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
  while (getline(&line, &size, fp) != -1) {
    char *dummy = line;
    char *src = strsep(&dummy, " \t\r\n");
    char *dst = strsep(&dummy, " \t\r\n");

    if (src != nullptr && dst != nullptr && *src != '\0' && *dst != '\0') {
      MR_Emit(dst, src);
    }
  }

  free(line);
  fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
  int count = 0;
  while (get_next(key, partition_number) != nullptr) {
    count++;
  }
  printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return 0;
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
  return EXIT_SUCCESS;
}
