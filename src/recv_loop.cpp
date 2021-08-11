/*
AXIS-FIFO loop receiving program in cooperate w/ UIO driver
DOESN'T HANDLE ENDIANESS!!! Litte endian is assumed
*/

#include <fstream>
#include <iomanip>
#include <iostream>

#include <unistd.h>

#include "axis_fifo.hpp"

#define dsb(scope) asm volatile("dsb " #scope : : : "memory")
#define AXI
#define N_W32_IN_AXIS_FRAME 8
#define AXIS_FIFO_DEPTH 8192

using namespace std;
using namespace axis_fifo;

int main(int argc, char const *argv[]) {
  AXIS_FIFO fifo;
  ios state(nullptr);
  int len;
  uint32_t dummy;
  uint32_t buffer[AXIS_FIFO_DEPTH];
  ofstream ofs("dump.bin", ios::binary);

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

  do {
    fifo[IER_OFFSET] = interrupts;
    dsb(st);
    len = read(fifo.get_uio_fd(), &dummy, sizeof(dummy));
    if (len <= 0)
      break;
    uint32_t isr = fifo[ISR_OFFSET];
    if (isr & bad_events) {
      if (isr & ISR_RFPF) {
        cout << "Receive FIFO is full!" << endl;
      }
    }
    // Clear ISR
    fifo[ISR_OFFSET] = isr;
    dsb(st);
    // Retreive data
    uint32_t w32_cnt = fifo[RDFO_OFFSET];
    w32_cnt -=
        (w32_cnt % N_W32_IN_AXIS_FRAME) ? (w32_cnt % N_W32_IN_AXIS_FRAME) : 0;
    for (int i = 0; i < (int32_t)w32_cnt; i++) {
      buffer[i] = fifo.pop();
    }
    cout << dec << "received " << w32_cnt << " 32-bit word(s)" << endl;
    ofs.write((const char *)buffer, w32_cnt * sizeof(uint32_t));

  } while (len);
  cout << "End of receive loop" << endl;
  cout.copyfmt(state);
  return 0;
}
