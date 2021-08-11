#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    int fd;
    void *reg;
    fd = open("/dev/uio0", O_RDWR);
    if (fd < 0) {
        perror("open()");
        return 1;
    }
    reg = mmap(NULL, 0x00010000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (reg == NULL) {
        perror("mmap()");
        return 1;
    }
    printf("Successfully mmaped uio0 through fd\n");
    printf("ISR: 0x%08X\n", *((uint32_t *)reg));
    printf("IER: 0x%08X\n", *((uint32_t *)(reg + 4)));
    munmap(reg, 0x00010000);
    return 0;
}
