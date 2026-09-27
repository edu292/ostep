#include <bits/time.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

constexpr uint16_t DIVISOR = 0x8005;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }

  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    perror("open failed");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("stat failed");
    return EXIT_FAILURE;
  }

  size_t file_size = (size_t)st.st_size;

  if (file_size == 0) {
    fprintf(stderr, "input file size is too small\n");
  }

  const uint8_t *data =

      mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (close(fd) == -1) {
    perror("close failed");
    return EXIT_FAILURE;
  };

  if (data == MAP_FAILED) {
    perror("mmap failed");
    return EXIT_FAILURE;
  }

  struct timespec start;
  struct timespec end;
  uint16_t checksum = 0;
  clock_gettime(CLOCK_MONOTONIC, &start);
  const uint8_t *end_ptr = data + file_size;
  for (const uint8_t *ptr = data; ptr != end_ptr; ptr++) {
    checksum ^= (uint16_t)*ptr << 8;
    for (uint8_t b = 0; b < 8; b++) {
      if (checksum & 0x8000) {
        checksum = (uint16_t)(checksum << 1) ^ DIVISOR;
      } else {
        checksum <<= 1;
      }
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  munmap((void *)data, file_size);
  double elapsed_sec = (double)(end.tv_sec - start.tv_sec) +
                       ((double)(end.tv_nsec - start.tv_nsec) * 1e-9);

  double mib = (double)file_size / (1024.0 * 1024.0);
  double throughput = mib / elapsed_sec;

  printf("Elapsed:    %.6f s\n", elapsed_sec);
  printf("Throughput: %.2f MiB/s\n", throughput);

  printf("Checksum: %#018b | %#06x\n", checksum, checksum);
}
