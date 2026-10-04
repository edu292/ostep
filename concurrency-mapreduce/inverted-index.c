#include "mapreduce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Map(char *file_name) {
  FILE *fp = fopen(file_name, "r");
  if (fp == nullptr) {
    return;
  }

  char *line = nullptr;
  size_t size = 0;
  int line_num = 1;

  while (getline(&line, &size, fp) != -1) {
    char *dummy = line;
    char *token;
    while ((token = strsep(&dummy, " \t\n\r")) != nullptr) {
      if (*token != '\0') {
        char loc[256];
        snprintf(loc, sizeof(loc), "%s:%d", file_name, line_num);
        MR_Emit(token, loc);
      }
    }
    line_num++;
  }

  free(line);
  fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
  printf("%s:", key);
  char *val;
  while ((val = get_next(key, partition_number)) != nullptr) {
    printf(" %s", val);
  }
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return EXIT_FAILURE;
  }

  int mappers = 4;
  int reducers = 4;

  if (getenv("MAPPERS")) {
    mappers = atoi(getenv("MAPPERS"));
  }
  if (getenv("REDUCERS")) {
    reducers = atoi(getenv("REDUCERS"));
  }

  MR_Run(argc, argv, Map, mappers, Reduce, reducers, MR_DefaultHashPartition);
  return 0;
}
