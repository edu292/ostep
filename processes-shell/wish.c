#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_CAP_ARGS 8

typedef enum { NAME } ParserState;

typedef struct {
  size_t length;
  size_t capacity;
  char **data;
} StringArray;

typedef struct {
  char *name;
  StringArray args;
} Command;

typedef bool(Handler)(Command);

static char error_message[30] = "An error has occurred\n";
static char *path_dirs[] = {"/bin", NULL};

bool command_init(Command *c) {
  c->args.length = 0;
  c->args.capacity = 8;
  c->args.data = malloc(sizeof(char *) * 8);
  if (c->args.data == NULL) {
    return false;
  }

  return true;
}

bool exit_handler(Command c) {
  if (c.args.length != 0) {
    return false;
  }

  exit(0);
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
    perror("failed to exec");
    return false;
  default:
    wait(NULL);
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
    c->args.data[0] = full_path;
    *out = exec_handler;
  }

  return true;
}

int main(int argc, char *argv[]) {
  char *line;
  size_t line_size = 0;

  printf("wish> ");
  size_t read;
  while ((read = getline(&line, &line_size, stdin)) > 0) {
    line[read - 1] = '\0';
    char *token;
    ParserState state = NAME;
    Command current;
    if (!command_init(&current)) {
      return 1;
    }

    while ((token = strsep(&line, " ")) != NULL) {
      switch (state) {
      case NAME:
        current.name = token;
        break;
      }
    }

    Handler *handler;
    if (!get_handler(&current, &handler)) {
      write(STDERR_FILENO, error_message, strlen(error_message));
    }

    if (!handler(current)) {
      write(STDERR_FILENO, error_message, strlen(error_message));
    };

    printf("wish> ");
  }

  return 0;
}
