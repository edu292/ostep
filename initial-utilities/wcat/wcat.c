#include <stdio.h>
#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  char buf[BUFFER_SIZE];
  FILE *file;
  char *filename;
  for (int file_index = 1; file_index < argc; file_index++) {
    filename = argv[file_index];
    file = fopen(filename, "r");
    if (file == NULL) {
      printf("wcat: cannot open file\n");
      return 1;
    }

    while (fgets(buf, BUFFER_SIZE, file) != NULL) {
      printf("%s", buf);
    }

    fclose(file);
  }
}
