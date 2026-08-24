#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct {
  uint32_t length;
  char character;
} Run;
#pragma pack(pop)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("wzip: file1 [file2 ...]\n");
    return 1;
  }

  Run run = {};
  for (int file_index = 1; file_index < argc; file_index++) {
    int fd = open(argv[file_index], O_RDONLY);
    if (fd < 0) {
      printf("wzip: cannot open file\n");
      return 1;
    }
    struct stat sb;
    fstat(fd, &sb);
    size_t len = (size_t)sb.st_size;

    char *data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(data, len, MADV_SEQUENTIAL);
    close(fd);

    char *current_ptr = data;
    if (file_index == 1) {
      run.character = *current_ptr++;
      run.length = 1;
    }

    for (; current_ptr < data + len; current_ptr++) {
      char current_char = *current_ptr;
      if (current_char != run.character) {
        fwrite(&run, sizeof(Run), 1, stdout);

        run.character = current_char;
        run.length = 1;
        continue;
      }

      run.length++;
    }

    munmap(data, len);
  }

  fwrite(&run, sizeof(Run), 1, stdout);
}
