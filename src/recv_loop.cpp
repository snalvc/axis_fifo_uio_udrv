/*
AXIS-FIFO loop receiving program in cooperate w/ UIO driver
DOESN'T HANDLE ENDIANESS!!! Litte endian is assumed
*/

#include <fstream>
#include <iomanip>
#include <iostream>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>

#include "axis_fifo.hpp"

#define dsb(scope) asm volatile("dsb " #scope : : : "memory")
#define AXI
#define N_W32_IN_AXIS_FRAME 8
#define AXIS_FIFO_DEPTH 8192

using namespace std;
using namespace axis_fifo;

static int global_stop = 0;

void TerminationHandler(int signum) { global_stop = 1; }

int main(int argc, char const *argv[]) {
  if (argc < 3) {
    cerr << "Usage: " << argv[0] << " <FIFO_NAME> <DUMP_FILE>" << endl;
    return 1;
  }

  // Register SIGTERM handler
  struct sigaction new_action;
  new_action.sa_handler = TerminationHandler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  if (sigaction(SIGTERM, &new_action, NULL) != 0) {
    cerr << "Failed to register signal handler" << endl;
    exit(1);
  }

  AXIS_FIFO fifo(argv[1]);
  ios state(nullptr);
  int len;
  uint32_t dummy;
  uint32_t buffer[AXIS_FIFO_DEPTH];
  ofstream ofs(argv[2], ios::binary);

  state.copyfmt(cout);
  cout << hex << uppercase << setw(8) << setfill('0');

  cout << "ISR: 0x" << fifo[ISR_OFFSET] << endl;
  cout << "Clear ISR" << endl;
  fifo[ISR_OFFSET] = 0xFFFFFFFF;
  dsb(st);
  cout << "ISR: 0x" << fifo[ISR_OFFSET] << endl;

  cout << "Reseting receive FIFO" << endl;
  fifo[IER_OFFSET] = ISR_RRC;
  dsb(st);
  fifo[RDFR_OFFSET] = 0xA5;
  dsb(st);
  // Wait dor Receive Reset Complete interrupt
  len = read(fifo.get_uio_fd(), &dummy, sizeof(dummy));
  // Clear ISR
  fifo[ISR_OFFSET] = 0xFFFFFFFF;
  dsb(st);
  cout << "Receive FIFO reset complete" << endl;

  cout << "Begin receive loop" << endl;
  uint32_t good_events = ISR_RC;  // Receive Complete
  uint32_t bad_events = ISR_RFPF; // Receive FIFO Programmable Full
  uint32_t interrupts = good_events | bad_events;
  fifo[IER_OFFSET] = interrupts;
  dsb(st);

  // Setup reading w/ timeout
  fd_set read_fds, active_fds;
  struct timeval timeout;
  int rv;
  uint32_t byte_acc = 0;

  uint32_t packet_count = 0;

  FD_ZERO(&active_fds);
  FD_SET(fifo.get_uio_fd(), &active_fds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000; // 10 ms timeout

  cout << dec;
  read_fds = active_fds;
  rv = select(fifo.get_uio_fd(), &read_fds, NULL, NULL, &timeout);
  // cout << "rv: " << rv  << endl;
  // rv = select(fifo.get_uio_fd(), &read_fds, NULL, NULL, NULL);
  while (rv >= 0) {
    // len = read(fifo.get_uio_fd(), &dummy, sizeof(dummy));
    // if (len <= 0)
    //   break;
    // Clear ISR
    uint32_t isr = fifo[ISR_OFFSET];
    if (isr) {
      fifo[ISR_OFFSET] = isr;
      dsb(st);
    }

    // Stop on global synchonize flag
    if (global_stop)
      break;

    // Retreive data
    uint32_t byte_cnt = fifo[RLR_OFFSET];

    if (((byte_cnt >> 23) & 0xFF) == 0) {

      // Partial bit asserted
      if (byte_cnt & (1UL << 31)) {
        byte_cnt &= ~(1UL << 31);
        byte_acc += byte_cnt;
      } else {
        cout << "packet_count: " << packet_count << " ";
        cout << "byte_cnt: " << byte_cnt << ", byte_acc: " << byte_acc << endl;
        byte_cnt = byte_cnt - byte_acc;
        byte_acc = 0;
        packet_count++;
      }
      // uint32_t w32_cnt = fifo[RDFO_OFFSET];
      uint32_t w32_cnt = byte_cnt / 4;
      // w32_cnt -=
      //     (w32_cnt % N_W32_IN_AXIS_FRAME) ? (w32_cnt % N_W32_IN_AXIS_FRAME) :
      //     0;
      if (w32_cnt > 0) {
        cout << dec << w32_cnt << "," << byte_cnt << endl;
        for (int i = 0; i < (int32_t)w32_cnt; i++) {
          buffer[i] = fifo.pop();
        }
        ofs.write((const char *)buffer, w32_cnt * sizeof(uint32_t));
      }
    } else {
      cout << "byte_cnt: " << byte_cnt << ", byte_acc: " << byte_acc << endl;
      // break;
    }

    read_fds = active_fds;

    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10 ms timeout
    rv = select(fifo.get_uio_fd(), &read_fds, NULL, NULL, &timeout);
    // cout << "rv: " << rv  << endl;
  }

  // do {
  //   fifo[IER_OFFSET] = interrupts;
  //   dsb(st);
  //   len = read(fifo.get_uio_fd(), &dummy, sizeof(dummy));
  //   std::chrono::steady_clock::time_point tstart =
  //       std::chrono::steady_clock::now();
  //   if (len <= 0)
  //     break;
  //   uint32_t isr = fifo[ISR_OFFSET];
  //   // if (isr & bad_events) {
  //   //   if (isr & ISR_RFPF) {
  //   //     cout << "Receive FIFO is full!" << endl;
  //   //   }
  //   // }
  //   // Clear ISR
  //   fifo[ISR_OFFSET] = isr;
  //   dsb(st);
  //   // Retreive data
  //   uint32_t w32_cnt = fifo[RDFO_OFFSET];
  //   w32_cnt -=
  //       (w32_cnt % N_W32_IN_AXIS_FRAME) ? (w32_cnt % N_W32_IN_AXIS_FRAME) :
  //       0;
  //   for (int i = 0; i < (int32_t)w32_cnt; i++) {
  //     buffer[i] = fifo.pop();
  //   }
  //   cout << dec << w32_cnt << ", "
  //        << std::chrono::duration_cast<std::chrono::microseconds>(
  //               std::chrono::steady_clock::now() - tstart)
  //               .count()
  //        << endl;
  //   ofs.write((const char *)buffer, w32_cnt * sizeof(uint32_t));

  // } while (len);
  cout << "End of receive loop" << endl;
  cout.copyfmt(state);
  return 0;
}
