#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct Line {
  char *data;
  size_t length;
  struct Line *previous;
} Line;

int main(int argc, char *argv[]) {
  if (argc > 3) {
    fprintf(stderr, "usage: reverse <input> <output>\n");
    return EXIT_FAILURE;
  }

  FILE *out = stdout;
  FILE *in = stdin;
  if (argc == 3) {
    struct stat sb_in;
    struct stat sb_out;
    if (stat(argv[1], &sb_in) == 0 && stat(argv[2], &sb_out) == 0) {
      if (sb_in.st_dev == sb_out.st_dev && sb_in.st_ino == sb_out.st_ino) {
        fprintf(stderr, "reverse: input and output file must differ\n");
        return EXIT_FAILURE;
      }
    }

    char *out_filename = argv[2];
    out = fopen(out_filename, "w");
    if (out == nullptr) {
      fprintf(stderr, "reverse: cannot open file '%s'\n", out_filename);
      return EXIT_FAILURE;
    }
  }

  if (argc >= 2) {
    char *in_filename = argv[1];
    in = fopen(in_filename, "r");
    if (in == nullptr) {
      fprintf(stderr, "reverse: cannot open file '%s'\n", in_filename);
      return EXIT_FAILURE;
    }
  }

  char *line_buf = nullptr;
  size_t line_buf_size = 0;
  ssize_t line_len = 0;
  Line *line = nullptr;
  while ((line_len = getline(&line_buf, &line_buf_size, in)) != -1) {
    Line *newLine = malloc(sizeof(Line));
    if (newLine == nullptr) {
      fprintf(stderr, "malloc failed\n");
      return EXIT_FAILURE;
    }

    newLine->data = strdup(line_buf);
    newLine->length = (size_t)line_len;
    newLine->previous = line;
    line = newLine;
  }

  while (line != nullptr) {
    fwrite(line->data, line->length, 1, out);
    Line *old = line;
    line = old->previous;
    free(old->data);
    free(old);
  }
}
