#include <stddef.h>
#include <stdio.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
  unsigned int length;
  char character;
} Run;
#pragma pack(pop)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("wunzip: file1 [file2 ...]\n");
    return 1;
  }

  Run run;
  for (int file_index = 1; file_index < argc; file_index++) {
    FILE *file = fopen(argv[file_index], "r");
    if (file == NULL) {
      printf("wunzip: cannot open file\n");
      return 1;
    }

    while (fread(&run, sizeof(run), 1, file) == 1) {
      for (int i = 0; i < run.length; i++) {
        fputc(run.character, stdout);
      }
    }
  }
}
