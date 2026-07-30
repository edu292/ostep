#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_CAP_ARGS 8

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
static char *path_dirs[] = {"/bin", NULL};
static bool running = true;

bool command_init(Command *c) {
  c->args.length = 1;
  c->args.capacity = 8;
  c->args.data = malloc(sizeof(char *) * 8);
  if (c->args.data == NULL) {
    return false;
  }

  return true;
}

void command_reset(Command *c) {
  c->name = NULL;
  c->args.length = 1;
}

void command_destroy(Command c) { free(c.args.data); }

size_t args_length(Command c) { return c.args.length - 1; }

bool command_add_argument(Command *c, char *val) {
  StringVector *v = &c->args;
  if (c->args.length == c->args.capacity) {
    size_t new_cap = v->capacity * 2;
    char **new_data = realloc(v->data, new_cap);
    if (new_data == NULL) {
      perror("Failed to insert");
      return false;
    }
    v->data = new_data;
    v->capacity = new_cap;
  }

  v->data[v->length++] = val;

  return true;
}

bool exit_handler(Command c) {
  if (args_length(c) != 1) {
    return false;
  }

  running = false;
  return true;
}

bool resolve_path(const char *name, char **full_path) {
  size_t base_len = strlen(name) + 2;
  size_t cap = 128;
  char *candidate = malloc(cap);
  size_t i = 0;
  char *dir = "";
  while (dir != NULL) {
    dir = path_dirs[i];
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
  int pid = fork();
  switch (pid) {
  case -1:
    perror("fork failed");
    return false;
  case 0:
    execv(c.name, c.args.data);
    perror("exec failed");
    return false;
  default:
    wait(NULL);
    free(c.name);
    return true;
  }
}

bool get_handler(Command *c, Handler **out) {
  if (strcmp(c->name, "exit") == 0) {
    *out = exit_handler;
  } else {
    char *full_path;
    if (!resolve_path(c->name, &full_path)) {
      return false;
    }

    c->name = full_path;
    *out = exec_handler;
  }
  c->args.data[0] = c->name;
  command_add_argument(c, NULL);

  return true;
}

int main(int argc, char *argv[]) {
  char *line_buf = NULL;
  size_t line_buf_size;

  size_t read;
  Command com;
  if (!command_init(&com)) {
    return 1;
  }

  goto prompt;
  while ((read = getline(&line_buf, &line_buf_size, stdin)) > 1) {
    line_buf[read - 1] = '\0';
    char *token;
    ParserState parser = NAME;

    while ((token = strsep(&line_buf, " ")) != NULL) {
      switch (parser) {
      case NAME:
        com.name = token;
        parser = ARGS;
        break;
      case ARGS:
        command_add_argument(&com, token);
      }
    }

    Handler *handler;
    if (!get_handler(&com, &handler)) {
      write(STDERR_FILENO, error_message, strlen(error_message));
    } else if (!handler(com)) {
      write(STDERR_FILENO, error_message, strlen(error_message));
    };

    command_reset(&com);
    if (!running) {
      break;
    }

  prompt:
    printf("wish> ");
  }

  free(line_buf);
  command_destroy(com);

  return 0;
}
