#include "mapreduce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *g_pattern = "ERROR";

void Map(char *file_name) {
  FILE *fp = fopen(file_name, "r");
  if (fp == nullptr) {
    return;
  }

  char *line = nullptr;
  size_t size = 0;
  while (getline(&line, &size, fp) != -1) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strstr(line, g_pattern) != nullptr) {
      MR_Emit(g_pattern, line);
    }
  }

  free(line);
  fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
  char *val;
  while ((val = get_next(key, partition_number)) != nullptr) {
    printf("[%s] %s\n", key, val);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return 0;
  }

  if (getenv("MATCH_PATTERN")) {
    g_pattern = getenv("MATCH_PATTERN");
  }

  int mappers = 4;
  int reducers = 1;

  if (getenv("MAPPERS")) {
    mappers = atoi(getenv("MAPPERS"));
  }
  if (getenv("REDUCERS")) {
    reducers = atoi(getenv("REDUCERS"));
  }

  MR_Run(argc, argv, Map, mappers, Reduce, reducers, MR_DefaultHashPartition);
  return EXIT_SUCCESS;
}
