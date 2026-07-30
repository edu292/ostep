#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_CAP_VEC 8

typedef enum { NAME, ARGS } ParserState;

typedef struct {
  size_t length;
  size_t capacity;
  char **data;
} StringVector;

typedef struct {
  char *name;
  StringVector args;
} Command;

typedef bool(Handler)(Command);

static char error_message[30] = "An error has occurred\n";
static StringVector *path_dirs;
static bool running = true;

bool vector_init(StringVector *v) {
  v->length = 0;
  v->capacity = DEFAULT_CAP_VEC;
  v->data = malloc(sizeof(char *) * DEFAULT_CAP_VEC);
  if (v->data == NULL) {
    return false;
  }

  return true;
}

void vector_clear(StringVector *v) {
  for (size_t i = 0; i < v->length; i++) {
    free(v->data[i]);
  }

  v->length = 0;
}

void vector_destroy(StringVector *v) {
  if (v != NULL) {
    vector_clear(v);
    free(v->data);
  }
}

bool vector_append(StringVector *v, char *val) {
  if (v->length == v->capacity) {
    size_t new_cap = v->capacity * 2;
    char **new_data = realloc(v->data, new_cap);
    if (new_data == NULL) {
      perror("failed to insert");
      return false;
    }
    v->data = new_data;
    v->capacity = new_cap;
  }

  v->data[v->length++] = val != NULL ? strdup(val) : NULL;

  return true;
}

bool vector_insert(StringVector *v, char *val, size_t idx) {
  if (idx >= v->length) {
    return false;
  }

  free(v->data[idx]);
  v->data[idx] = val != NULL ? strdup(val) : NULL;
  return true;
}

bool exit_handler(Command c) {
  if (c.args.length != 1) {
    return false;
  }

  running = false;
  return true;
}

bool path_handler(Command c) {
  vector_clear(path_dirs);
  for (size_t i = 1; i < c.args.length; i++) {
    if (!vector_append(path_dirs, c.args.data[i])) {
      return false;
    }
  }

  return true;
}

bool cd_handler(Command c) {
  if (c.args.length != 2) {
    return false;
  }

  if (chdir(c.args.data[1]) != 0) {
    return false;
  }

  return true;
}

bool resolve_path(const char *name, char **full_path) {
  size_t base_len = strlen(name) + 2;
  size_t cap = 128;
  char *candidate = malloc(cap);
  for (size_t i = 0; i < path_dirs->length; i++) {
    char *dir = path_dirs->data[i];
    size_t len = base_len + strlen(dir);
    if (len > cap) {
      cap = len;
      candidate = realloc(candidate, cap);
      if (candidate == NULL) {
        perror("failed to allocate path string");
        return false;
      }
    }

    snprintf(candidate, len, "%s/%s", dir, name);

    if (access(candidate, X_OK) == 0) {
      *full_path = candidate;
      return true;
    };

    i++;
  }

  return false;
}

bool exec_handler(Command c) {
  char *full_path;
  if (!resolve_path(c.name, &full_path)) {
    return false;
  }

  int pid = fork();
  switch (pid) {
  case -1:
    return false;
  case 0:
    vector_insert(&c.args, full_path, 0);
    vector_append(&c.args, NULL);
    execv(full_path, c.args.data);
    return false;
  default:
    wait(NULL);
    free(full_path);
    return true;
  }
}

Handler *get_handler(Command *c) {
  if (strcmp(c->name, "exit") == 0) {
    return exit_handler;
  } else if (strcmp(c->name, "path") == 0) {
    return path_handler;
  } else if (strcmp(c->name, "cd") == 0) {
    return cd_handler;
  } else {
    return exec_handler;
  }
}

int main(int argc, char *argv[]) {
  char *line_buf = NULL;
  size_t line_buf_size;
  path_dirs = malloc(sizeof(StringVector));
  if (!vector_init(path_dirs)) {
    return 1;
  }

  vector_append(path_dirs, "/bin");

  size_t read;
  Command com;
  if (!vector_init(&com.args)) {
    return 1;
  }

  goto prompt;
  while ((read = getline(&line_buf, &line_buf_size, stdin)) > 1) {
    line_buf[read - 1] = '\0';
    char *token;
    ParserState parser = NAME;

    char *line_pos = line_buf;
    while ((token = strsep(&line_buf, " \t")) != NULL) {
      if (*token == '\0') {
        continue;
      }

      switch (parser) {
      case NAME:
        com.name = token;
        vector_append(&com.args, token);
        parser = ARGS;
        break;
      case ARGS:
        vector_append(&com.args, token);
      }
    }

    Handler *handler = get_handler(&com);
    if (!handler(com)) {
      write(STDERR_FILENO, error_message, strlen(error_message));
    };

    vector_clear(&com.args);
    if (!running) {
      break;
    }

  prompt:
    printf("wish> ");
  }

  free(line_buf);
  vector_destroy(&com.args);
  vector_destroy(path_dirs);
  free(path_dirs);

  return 0;
}
