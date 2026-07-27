#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("wgrep: searchterm [file ...]\n");
    return 1;
  }

  FILE *file;
  if (argc == 3) {
    file = fopen(argv[2], "r");
    if (file == NULL) {
      printf("wgrep: cannot open file\n");
      return 1;
    }
  } else {
    file = stdin;
  }

  char *searchterm = argv[1];
  char *line = NULL;
  size_t line_buf_size = 0;
  while (getline(&line, &line_buf_size, file) != -1) {
    if (strstr(line, searchterm) != NULL) {
      printf("%s", line);
    }
  }

  free(line);
  fclose(file);
}
