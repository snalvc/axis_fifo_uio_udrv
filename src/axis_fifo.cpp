#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "axis_fifo.hpp"

namespace {
std::string read_from_file(const std::filesystem::path &p) {
  std::ifstream ifs(p);
  std::string ret;
  ifs >> ret;
  return ret;
}
} // namespace

UIO::UIO(const uint32_t idx) {
  std::string sp("/sys/class/uio/uio");
  sp += std::to_string(idx);
  std::filesystem::path p(sp);
  std::string uio_name = read_from_file(p / "name");
  this->uio_name = uio_name;
  this->path = std::filesystem::path(sp);
  this->retrieve_maps(p);
}

UIO::UIO(const std::string &name) {
  for (const auto &file :
       std::filesystem::directory_iterator("/sys/class/uio")) {
    std::string uio_name = read_from_file(file.path() / "name");
    if (uio_name == name) {
      this->uio_name = uio_name;
      this->path = file.path();
      this->retrieve_maps(file.path());
      break;
    }
  }
}

void UIO::retrieve_maps(const std::filesystem::path &p) {
  auto map_dir = p / "maps";
  for (const auto &file : std::filesystem::directory_iterator(map_dir)) {
    auto map_p = file.path();
    auto addr_p = map_p / "addr";
    auto name_p = map_p / "name";
    auto offset_p = map_p / "offset";
    auto size_p = map_p / "size";
    std::string name_s = read_from_file(name_p);
    std::string addr_s = read_from_file(addr_p);
    std::string offset_s = read_from_file(offset_p);
    std::string size_s = read_from_file(size_p);
    uint64_t addr = std::stoull(addr_s, 0, 0);
    uint64_t offset = std::stoull(offset_s, 0, 0);
    uint64_t size = std::stoull(size_s, 0, 0);
    this->maps.emplace_back(name_s, addr, offset, size);
  }
}

const UIO::Map_Essential *UIO::get_map(const uint32_t idx) {
  if (idx > this->maps.size()) {
    throw std::out_of_range("get_map()");
  }
  return &(this->maps[idx]);
}

const UIO::Map_Essential *UIO::get_map(const std::string &name) {
  for (auto &e : this->maps) {
    if (e.name == name) {
      return &e;
    }
  }
  return nullptr;
}

namespace axis_fifo {

const std::unordered_map<std::string, uint32_t> AXIS_FIFO::reg_offset{
    {std::string("ISR"), 0x0},    {std::string("IER"), 0x4},
    {std::string("TDFR"), 0x8},   {std::string("TDFV"), 0xc},
    {std::string("TDFD"), 0x10},  {std::string("TLR"), 0x14},
    {std::string("RDFR"), 0x18},  {std::string("RDFO"), 0x1c},
    {std::string("RDFD"), 0x20},  {std::string("RLR"), 0x24},
    {std::string("SRR"), 0x28},   {std::string("TDR"), 0x2c},
    {std::string("RDR"), 0x30},   {std::string("TID"), 0x34},
    {std::string("TUSER"), 0x38}, {std::string("RID"), 0x3c},
    {std::string("RUSER"), 0x40}};

const std::unordered_map<std::string, uint32_t> AXIS_FIFO::isr_masks{
    {"RFPE", 1 << 19}, {"RFPF", 1 << 20}, {"TFPE", 1 << 21}, {"TFPF", 1 << 22},
    {"RRC", 1 << 23},  {"TRC", 1 << 24},  {"TSE", 1 << 25},  {"RC", 1 << 26},
    {"TC", 1 << 27},   {"TPOE", 1 << 28}, {"RPUE", 1 << 29}, {"RPORE", 1 << 30},
    {"RPURE", 1 << 31}};

AXIS_FIFO::AXIS_FIFO(const uint32_t uio_idx) {
  this->p_uio = std::make_unique<UIO>(uio_idx);
  this->setup();
}

AXIS_FIFO::AXIS_FIFO(const std::string &name) {
  this->p_uio = std::make_unique<UIO>(name);
  this->setup();
}

AXIS_FIFO::~AXIS_FIFO() {
  if (this->axi_full_space != NULL)
    munmap(this->axi_full_space,
           this->p_uio->get_map(static_cast<uint32_t>(1))->size);
  if (this->reg_space != NULL)
    munmap(this->reg_space,
           this->p_uio->get_map(static_cast<uint32_t>(0))->size);
  if (this->uio_fd)
    close(this->uio_fd);
}

void AXIS_FIFO::setup() {
  std::regex suio("\\/sys\\/class\\/uio\\/(uio\\d+)");
  std::smatch sm;
  std::string sys_uio_path = this->p_uio->get_path().string();
  std::string uio_dev_path("/dev/");
  if (!std::regex_match(sys_uio_path, sm, suio)) {
    throw std::invalid_argument("UIO sys class path: format error");
  }
  uio_dev_path += sm[1].str();

  this->uio_fd = open(uio_dev_path.c_str(), O_RDWR | O_SYNC);
  if (this->uio_fd < 0) {
    throw std::system_error(std::error_code(errno, std::generic_category()),
                            "open()");
  }
  uint32_t size = this->p_uio->get_map(static_cast<uint32_t>(0))->size;
  this->reg_space = (uint32_t *)mmap(NULL, size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, this->uio_fd, 0);
  if (this->reg_space == MAP_FAILED) {
    throw std::system_error(std::error_code(errno, std::generic_category()),
                            "mmap()");
  }
  // AXI-Full exists
  if (this->p_uio->n_maps() > 1) {
    uint32_t addr = this->p_uio->get_map(static_cast<uint32_t>(1))->addr;
    uint32_t size = this->p_uio->get_map(static_cast<uint32_t>(1))->size;
    this->axi_full_space =
        (uint32_t *)mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED,
                         this->uio_fd, 1 * getpagesize());
    if (this->axi_full_space == MAP_FAILED) {
      throw std::system_error(std::error_code(errno, std::generic_category()),
                              "mmap()");
    }
    this->read_addr = this->axi_full_space + 0x1000 / 4;
  } else {
    this->read_addr = this->reg_space + (RDFD_OFFSET / 4);
  }
}

uint32_t AXIS_FIFO::operator[](const uint32_t idx) const {
  return *(reg_space + idx / 4);
}

uint32_t &AXIS_FIFO::operator[](const uint32_t idx) {
  return *(reg_space + idx / 4);
}

uint32_t AXIS_FIFO::pop() { return *(this->read_addr); }

} // namespace axis_fifo