#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint64_t key;
  char value[56];
} KeyValue;

typedef struct {
  size_t capacity;
  size_t count;
  KeyValue *data;
} SortedVector;

bool sorted_vector_init(SortedVector *sv, size_t capacity) {
  sv->capacity = capacity > 0 ? capacity : 128;
  sv->count = 0;
  sv->data = malloc(sv->capacity * sizeof(KeyValue));

  return sv->data == nullptr;
}

void sorted_vector_destroy(SortedVector *sv) { free(sv->data); }

bool sorted_vector_load(SortedVector *sv, FILE *f) {
  fread(sv, sizeof(size_t), 2, f);
  sv->data = malloc(sv->capacity * sizeof(KeyValue));
  if (sv->data == nullptr) {
    return false;
  }

  fread(sv->data, sizeof(KeyValue), sv->count, f);
  return true;
}

void sorted_vector_save(SortedVector *sv, FILE *f) {
  fwrite(sv, sizeof(size_t), 2, f);
  fwrite(sv->data, sizeof(KeyValue), sv->count, f);
}

size_t sorted_vector_index(SortedVector *sv, size_t key) {
  size_t low = 0;
  size_t high = sv->count;

  while (low < high) {
    size_t mid = low + ((high - low) / 2);
    if (sv->data[mid].key < key) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  return low;
}

bool sorted_vector_insert(SortedVector *sv, KeyValue val) {
  if (sv->count == sv->capacity) {
    size_t new_capacity = 2 * sv->capacity;
    KeyValue *new_data = realloc(sv->data, new_capacity * sizeof(KeyValue));
    if (new_data == nullptr) {
      return false;
    }

    sv->capacity = new_capacity;
    sv->data = new_data;
  }

  size_t idx = sorted_vector_index(sv, val.key);
  memmove(&sv->data[idx + 1], &sv->data[idx],
          (sv->count - idx) * sizeof(sv->data[0]));

  sv->data[idx] = val;
  sv->count++;
  return true;
}

bool sorted_vector_get(SortedVector *sv, size_t key, KeyValue *out) {
  size_t idx = sorted_vector_index(sv, key);
  if (idx >= sv->count || sv->data[idx].key != key) {
    return false;
  }

  *out = sv->data[idx];
  return true;
}

bool sorted_vector_delete(SortedVector *sv, size_t key) {
  size_t idx = sorted_vector_index(sv, key);
  if (idx >= sv->count || sv->data[idx].key != key) {
    return false;
  }

  memmove(&sv->data[idx], &sv->data[idx + 1],
          (sv->count - (idx + 1)) * sizeof(sv->data[0]));
  sv->count--;
  return true;
}

void sorted_vector_clear(SortedVector *sv) { sv->count = 0; }

typedef struct {
  uint64_t key;
  char *value;
} HandlerArgs;

typedef void (*Handler)(SortedVector *data, HandlerArgs args);

typedef enum {
  CMD_ARGS_NONE,
  CMD_ARGS_KEY,
  CMD_ARGS_KEY_VALUE
} CommandArgsMode;

typedef struct {
  Handler handler;
  CommandArgsMode args;
} CommandDef;

void handle_put(SortedVector *data, HandlerArgs args) {
  KeyValue pair = {.key = args.key};
  strlcpy(pair.value, args.value, sizeof(pair.value));
  if (!sorted_vector_insert(data, pair)) {
    perror("put failed");
    exit(EXIT_FAILURE);
  }
}

void handle_get(SortedVector *data, HandlerArgs args) {
  KeyValue pair;
  if (!sorted_vector_get(data, args.key, &pair)) {
    fprintf(stderr, "%lu not found\n", args.key);
    return;
  }

  printf("%lu,%s\n", pair.key, pair.value);
}

void handle_delete(SortedVector *data, HandlerArgs args) {
  if (!sorted_vector_delete(data, args.key)) {
    fprintf(stderr, "%lu not found\n", args.key);
  }
}

void handle_clear(SortedVector *data, HandlerArgs /*unused*/) {
  sorted_vector_clear(data);
}

void handle_all(SortedVector *data, HandlerArgs /*unused*/) {
  for (size_t i = 0; i < data->count; i++) {
    KeyValue pair = data->data[i];
    printf("%lu,%s\n", pair.key, pair.value);
  }
}

bool parse_command(const char *field, CommandDef *out_def) {
  if (field == NULL || field[0] == '\0' || field[1] != '\0') {
    return false;
  }

  static const CommandDef CMD_TABLE[256] = {
      ['p'] = {handle_put, CMD_ARGS_KEY_VALUE},
      ['g'] = {handle_get, CMD_ARGS_KEY},
      ['d'] = {handle_delete, CMD_ARGS_KEY},
      ['c'] = {handle_clear, CMD_ARGS_NONE},
      ['a'] = {handle_all, CMD_ARGS_NONE},
  };

  CommandDef def = CMD_TABLE[(unsigned char)field[0]];
  if (!def.handler) {
    return false;
  }

  *out_def = def;
  return true;
}

typedef enum {
  PARSER_COMMAND,
  PARSER_KEY,
  PARSER_VALUE,
  PARSER_END,
  PARSER_ERROR
} ParserState;

int main() {
  SortedVector data;

  FILE *f = fopen("database.txt", "rb");
  if (f != nullptr) {
    sorted_vector_load(&data, f);
    fclose(f);
  } else {
    sorted_vector_init(&data, 0);
  }

  char *line_buf = nullptr;
  size_t line_buf_size = 0;
  while (1) {
    printf("> ");
    fflush(stdout);
    ssize_t read = getline(&line_buf, &line_buf_size, stdin);
    if (read == -1) {
      break;
    }

    line_buf[read - 1] = '\0';
    char *cursor = line_buf;
    char *token;
    while ((token = strsep(&cursor, " ")) != nullptr) {
      ParserState parser_state = PARSER_COMMAND;
      HandlerArgs args;
      CommandDef def = {};
      char *field;
      while ((field = strsep(&token, ",")) != nullptr) {
        if (field[0] == '\0' || field[0] == ' ') {
          break;
        }

      process_state:
        switch (parser_state) {
        case PARSER_COMMAND:
          if (!parse_command(field, &def)) {
            fprintf(stderr, "bad command\n");
            parser_state = PARSER_ERROR;
            goto process_state;
          }

          parser_state = def.args == CMD_ARGS_NONE ? PARSER_END : PARSER_KEY;
          break;
        case PARSER_KEY:
          args.key = strtoumax(field, nullptr, 10);
          parser_state = def.args == CMD_ARGS_KEY ? PARSER_END : PARSER_VALUE;
          break;
        case PARSER_VALUE:
          args.value = field;
          parser_state = PARSER_END;
          [[fallthrough]];
        case PARSER_END:
        case PARSER_ERROR:
          goto next;
        }
      }
    next:
      if (parser_state == PARSER_END) {
        def.handler(&data, args);
      }
    }
  }

  f = fopen("database.tmp", "wb");
  if (f == nullptr) {
    goto exit;
  }

  sorted_vector_save(&data, f);
  fclose(f);
  rename("database.tmp", "database.txt");

exit:
  free(line_buf);
  sorted_vector_destroy(&data);
  return EXIT_SUCCESS;
}
