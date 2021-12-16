#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct target_s {
  uint16_t h_angle;
  uint16_t v_angle;
  float w_data;
  uint16_t range_idx;
  uint16_t velocity_idx;
  uint32_t timestamp;
  uint16_t frame_id;
  uint16_t flags; // flag[0]: target valid, flag[1]: EOF
} __attribute__((packed)) target_t;

int main(int argc, char *argv[]) {
  int fd;
  void *reg, *axi_full;

  printf("sizeof(target_t) = %d\n", sizeof(target_t));
  return 0;
}
