#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("wzip: file1 [file2 ...]\n");
    return 1;
  }

  int run_char = -1;
  unsigned int run_length;
  for (int file_index = 1; file_index < argc; file_index++) {
    FILE *file = fopen(argv[file_index], "r");
    if (file == NULL) {
      printf("wzip: cannot open file\n");
      return 1;
    }

    if (run_char == -1) {
      run_char = fgetc(file);
      run_length = 1;
    }

    int current;
    while ((current = fgetc(file)) != -1) {
      if (current != run_char) {
        fwrite(&run_length, sizeof(unsigned int), 1, stdout);
        fwrite(&run_char, sizeof(char), 1, stdout);
        run_char = current;
        run_length = 1;
        continue;
      }

      run_length++;
    }

    fclose(file);
  }

  fwrite(&run_length, sizeof(unsigned int), 1, stdout);
  fwrite(&run_char, sizeof(char), 1, stdout);
}
