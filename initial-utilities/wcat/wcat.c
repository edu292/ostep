#include <stdio.h>
#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  char buf[BUFFER_SIZE];
  for (int file_index = 1; file_index < argc; file_index++) {
    char *filename = argv[file_index];
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
      printf("wcat: cannot open file\n");
      return 1;
    }

    while (fgets(buf, BUFFER_SIZE, file) != NULL) {
        printf("%s", buf);
    }
  }
}
