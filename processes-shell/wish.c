#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_CAP_VEC 8

typedef enum { UNITIALIZED, NAME, ARGS, REDIRECT, FINISHED } ParserState;
typedef enum { OK, INVALID, END } ParseCode;

typedef struct {
  char *op;
  bool found_operator;
  bool has_prefix;
} ParseOpResult;

typedef struct {
  size_t length;
  size_t capacity;
  char **data;
} StringVector;

typedef struct {
  StringVector args;
  char *name;
  char *redirect;
  bool background;
} Command;

typedef bool(Handler)(Command *);

static char error_message[] = "An error has occurred\n";
static StringVector *path_dirs;
static bool running = true;

bool vector_init(StringVector *v) {
  v->length = 0;
  v->capacity = DEFAULT_CAP_VEC;
  v->data = malloc(sizeof(char *) * DEFAULT_CAP_VEC);
  return v->data != NULL;
}

void vector_clear(StringVector *v) { v->length = 0; }

void vector_destroy(StringVector *v) {
  if (v != NULL) {
    free(v->data);
  }
}

bool vector_append(StringVector *v, char *val) {
  if (v->length == v->capacity) {
    size_t new_cap = v->capacity * 2;
    char **new_data = realloc(v->data, new_cap * sizeof(char *));
    if (new_data == NULL) {
      perror("failed to insert");
      return false;
    }
    v->data = new_data;
    v->capacity = new_cap;
  }

  v->data[v->length++] = val;

  return true;
}

bool vector_insert(StringVector *v, char *val, size_t idx) {
  if (idx >= v->length) {
    return false;
  }

  v->data[idx] = val;
  return true;
}

void command_clear(Command *c) {
  vector_clear(&c->args);
  c->redirect = NULL;
  c->background = false;
  c->name = NULL;
}

bool exit_handler(Command *c) {
  if (c->args.length != 1) {
    return false;
  }

  running = false;
  return true;
}

bool path_handler(Command *c) {
  vector_clear(path_dirs);
  for (size_t i = 1; i < c->args.length; i++) {
    if (!vector_append(path_dirs, strdup(c->args.data[i]))) {
      return false;
    }
  }

  return true;
}

bool cd_handler(Command *c) {
  if (c->args.length != 2) {
    return false;
  }

  if (chdir(c->args.data[1]) != 0) {
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
      char *new_candidate = realloc(candidate, cap);
      if (new_candidate == NULL) {
        perror("failed to allocate path string");
        free(candidate);
        return false;
      }
      candidate = new_candidate;
    }

    snprintf(candidate, len, "%s/%s", dir, name);

    if (access(candidate, X_OK) == 0) {
      *full_path = candidate;
      return true;
    };
  }

  return false;
}

bool exec_handler(Command *c) {
  char *full_path = NULL;
  if (!resolve_path(c->name, &full_path)) {
    return false;
  }

  int pid = fork();
  switch (pid) {
  case -1:
    return false;
  case 0:
    vector_insert(&c->args, full_path, 0);
    vector_append(&c->args, NULL);
    if (c->redirect != NULL) {
      int fd = open(c->redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        return false;
      }

      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    execv(full_path, c->args.data);
    _exit(1);
  default:
    if (!c->background) {
      waitpid(pid, NULL, 0);
    }

    free(full_path);
    return true;
  }
}

Handler *get_handler(Command *c) {
  if (strcmp(c->name, "exit") == 0) {
    return exit_handler;
  }
  if (strcmp(c->name, "path") == 0) {
    return path_handler;
  }
  if (strcmp(c->name, "cd") == 0) {
    return cd_handler;
  }

  return exec_handler;
}

static inline ParseOpResult operator_parse(char *token) {
  char *op = strpbrk(token, "&>");
  if (op == NULL) {
    return (ParseOpResult){
        .found_operator = false, .has_prefix = true, .op = NULL};
  }

  return (ParseOpResult){
      .found_operator = true, .has_prefix = op != token, .op = op};
}

static inline void commit_operator(char *op, char **buf_ptr, Command *out,
                                   ParserState *state) {
  switch (*op) {
  case '&':
    out->background = true;
    *state = FINISHED;
    break;
  case '>':
    *state = REDIRECT;
    break;
  }

  *op = '\0';
  if (*buf_ptr != NULL) {
    *(*buf_ptr - 1) = ' ';
  }
  *buf_ptr = op + 1;
}

ParseCode command_parse(char **buf_ptr, Command *out) {
  if (*buf_ptr == NULL || **buf_ptr == '\0') {
    return END;
  }

  ParserState state = UNITIALIZED;
  char *token = NULL;
  while ((token = strsep(buf_ptr, " \t\n")) != NULL) {
    if (*token == '\0') {
      continue;
    }

    ParseOpResult r = operator_parse(token);
    ;
    switch (state) {
    case UNITIALIZED:
      state = NAME;
      [[fallthrough]];
    case NAME:
      if (r.found_operator) {
        if (!r.has_prefix) {
          return INVALID;
        }

        commit_operator(r.op, buf_ptr, out, &state);
      } else {
        state = ARGS;
      }

      out->name = token;
      vector_append(&out->args, token);
      break;
    case ARGS:
      if (r.found_operator) {
        commit_operator(r.op, buf_ptr, out, &state);

        if (!r.has_prefix) {
          continue;
        }
      }

      vector_append(&out->args, token);
      break;
    case REDIRECT:
      out->redirect = token;
      state = FINISHED;
      break;
    case FINISHED:
      if (r.found_operator && !r.has_prefix) {
        commit_operator(r.op, buf_ptr, out, &state);
      } else {
        if (*buf_ptr != NULL) {
          *(*buf_ptr - 1) = ' ';
        }

        *buf_ptr = token;
      }

      goto finished;
    }
  }

finished:
  switch (state) {
  case UNITIALIZED:
    return END;
  case REDIRECT:
  case NAME:
    return INVALID;
  default:
    return OK;
  }
}

int main(int argc, char *argv[]) {
  FILE *input = NULL;
  bool batch_mode = false;
  switch (argc) {
  case 1:
    input = stdin;
    break;
  case 2:
    input = fopen(argv[1], "r");
    if (input == NULL) {
      write(STDERR_FILENO, error_message, strlen(error_message));
      return 1;
    }
    batch_mode = true;
    break;
  default:
    write(STDERR_FILENO, error_message, strlen(error_message));
    return 1;
  }

  char *line_buf = NULL;
  size_t line_buf_size = 0;
  path_dirs = malloc(sizeof(StringVector));
  if (!vector_init(path_dirs)) {
    return 1;
  }

  vector_append(path_dirs, "/bin");

  Command com;
  if (!vector_init(&com.args)) {
    return 1;
  }
  command_clear(&com);

  ssize_t read = 0;
  while (running) {
    if (!batch_mode) {
      printf("wish> ");
      fflush(stdout);
    }

    read = getline(&line_buf, &line_buf_size, input);
    if (read == -1) {
      break;
    }

    line_buf[read - 1] = '\0';
    char *line_pos = line_buf;
    ParseCode code = END;
    while ((code = command_parse(&line_pos, &com)) == OK) {
      Handler *handler = get_handler(&com);
      bool success = handler(&com);

      command_clear(&com);

      if (!success) {
        write(STDERR_FILENO, error_message, sizeof(error_message) - 1);
      };
    }
    if (code == INVALID) {
      command_clear(&com);
      write(STDERR_FILENO, error_message, sizeof(error_message) - 1);
    }

    while (wait(NULL) > 0) {
    }
  }

  free(line_buf);
  vector_destroy(&com.args);
  vector_destroy(path_dirs);
  free(path_dirs);
  fclose(input);

  return 0;
}
