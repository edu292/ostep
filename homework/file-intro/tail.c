#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIN(a, b)                                                              \
  ({                                                                           \
    auto _a = (a);                                                             \
    auto _b = (b);                                                             \
    _a < _b ? _a : _b;                                                         \
  })

int main(int argc, char *argv[]) {
  char *endptr = nullptr;
  size_t wanted_lines = 10;
  int opt = 0;
  while ((opt = getopt(argc, argv, "n:")) != -1) {
    switch (opt) {
    case 'n':
      wanted_lines = strtoumax(optarg, &endptr, 10);
      break;
    default:
      fprintf(stderr, "Usage: %s [-n NUM] FILE\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (argc - optind != 1) {
    fprintf(stderr, "Error: wrong number of arguments.\n");
    fprintf(stderr, "Usage: %s [-n NUM] FILE\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *filename = argv[optind];
  int fd = open(filename, O_RDONLY);

  struct stat st;
  fstat(fd, &st);
  off_t offset = st.st_size - 1;
  size_t found_lines = 0;
  char buf[256];
  off_t print_start = 0;

  while (offset > 0) {
    size_t chunk = MIN(sizeof(buf), offset);
    offset -= (off_t)chunk;
    pread(fd, buf, chunk, offset);

    char *nl = nullptr;
    size_t search_len = chunk;
    while (found_lines < wanted_lines &&
           (nl = memrchr(buf, '\n', search_len)) != nullptr) {
      found_lines++;
      search_len = (size_t)(nl - buf);
    }

    if (found_lines == wanted_lines) {
      print_start = offset + (nl - buf) + 1;
      break;
    }
  }

  lseek(fd, print_start, SEEK_SET);
  ssize_t n = 0;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    write(STDOUT_FILENO, buf, (size_t)n);
  }

  close(fd);
  return EXIT_SUCCESS;
}
