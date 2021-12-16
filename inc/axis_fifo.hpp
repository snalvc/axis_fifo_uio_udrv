#ifndef AXIS_FIFO_HPP_
#define AXIS_FIFO_HPP_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class UIO {
public:
  class Map_Essential {
  public:
    Map_Essential() = delete;
    Map_Essential(std::string name, uint64_t addr, uint64_t offset,
                  uint64_t size)
        : name(name), addr(addr), offset(offset), size(size) {}
    const std::string name;
    const uint64_t addr;
    const uint64_t offset;
    const uint64_t size;
  };

public:
  UIO() = delete;
  UIO(const uint32_t idx);
  UIO(const std::string &name);
  UIO(const char *name) : UIO(std::string(name)) {}

  uint32_t n_maps() { return this->maps.size(); }
  std::string name() { return this->uio_name; }
  const UIO::Map_Essential *get_map(const uint32_t idx);
  const UIO::Map_Essential *get_map(const std::string &name);
  const UIO::Map_Essential *get_map(const char *name) {
    return this->get_map(std::string(name));
  }
  std::filesystem::path get_path() { return this->path; }

private:
  std::string uio_name;
  std::filesystem::path path;
  std::vector<UIO::Map_Essential> maps;

  void retrieve_maps(const std::filesystem::path &p);
};

namespace axis_fifo {

#define ISR_OFFSET 0x0
#define IER_OFFSET 0x4
#define TDFR_OFFSET 0x8
#define TDFV_OFFSET 0xc
#define TDFD_OFFSET 0x10
#define TLR_OFFSET 0x14
#define RDFR_OFFSET 0x18
#define RDFO_OFFSET 0x1c
#define RDFD_OFFSET 0x20
#define RLR_OFFSET 0x24
#define SRR_OFFSET 0x28
#define TDR_OFFSET 0x2c
#define RDR_OFFSET 0x30
#define TID_OFFSET 0x34
#define TUSER_OFFSET 0x38
#define RID_OFFSET 0x3c
#define RUSER_OFFSET 0x40

#define ISR_RFPE (1 << 19)
#define ISR_RFPF (1 << 20)
#define ISR_TFPE (1 << 21)
#define ISR_TFPF (1 << 22)
#define ISR_RRC (1 << 23)
#define ISR_TRC (1 << 24)
#define ISR_TSE (1 << 25)
#define ISR_RC (1 << 26)
#define ISR_TC (1 << 27)
#define ISR_TPOE (1 << 28)
#define ISR_RPUE (1 << 29)
#define ISR_RPORE (1 << 30)
#define ISR_RPURE (1 << 31)

class AXIS_FIFO {
public:
  AXIS_FIFO() : AXIS_FIFO("axis-fifo") {}
  AXIS_FIFO(const uint32_t uio_idx);
  AXIS_FIFO(const std::string &name);
  AXIS_FIFO(const char *name) : AXIS_FIFO(std::string(name)) {}
  ~AXIS_FIFO();

  static const std::unordered_map<std::string, uint32_t> reg_offset;
  static const std::unordered_map<std::string, uint32_t> isr_masks;

  int get_uio_fd() { return this->uio_fd; }
  uint32_t pop();

  uint32_t operator[](const uint32_t idx) const;
  uint32_t &operator[](const uint32_t idx);

private:
  std::unique_ptr<UIO> p_uio;
  int uio_fd = 0;
  uint32_t *reg_space = NULL;
  uint32_t *axi_full_space = NULL;
  uint32_t *read_addr = NULL;

  void setup();
};

} // namespace axis_fifo

#endif
