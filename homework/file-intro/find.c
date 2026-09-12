#define _GNU_SOURCE
#include <dirent.h>
#include <fnmatch.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  TYPE_ALL = 0,
  TYPE_FILE = 'f',
  TYPE_DIR = 'd',
  TYPE_LINK = 'l'
} FileType;

typedef struct {
  const char *root;
  const char *pattern;
  off_t min_size;
  size_t maxdepth;
  FileType file_type;
} Settings;

bool init_settings(int argc, char *argv[], Settings *set) {
  set->root = ".";
  set->pattern = nullptr;
  set->min_size = 0;
  set->maxdepth = UINT64_MAX;
  set->file_type = TYPE_ALL;

  static struct option options[] = {
      {"name", required_argument, nullptr, 'n'},
      {"type", required_argument, nullptr, 't'},
      {"maxdepth", required_argument, nullptr, 'd'},
      {"size", required_argument, nullptr, 's'},
      {nullptr, no_argument, nullptr, 0}};

  int opt = -1;
  while ((opt = getopt_long_only(argc, argv, "", options, nullptr)) != -1) {
    switch (opt) {
    case 'n':
      set->pattern = optarg;
      break;
    case 't':
      switch (optarg[0]) {
      case TYPE_FILE:
      case TYPE_DIR:
      case TYPE_LINK:
        set->file_type = (FileType)optarg[0];
        break;
      default:
        fprintf(stderr, "Invalid type '%s' (expected f, d, or l)\n", optarg);
        return false;
      }
      break;
    case 'd':
      set->maxdepth = strtoumax(optarg, nullptr, 10);
      break;
    case 's':
      set->min_size = strtol(optarg, nullptr, 10);
      break;
    case '?':
      return false;
    default:
      break;
    }
  }

  int remaining = argc - optind;
  if (remaining > 1) {
    fprintf(stderr, "Error: Too many arguments.\n");
    return false;
  }
  if (remaining == 1) {
    set->root = argv[optind];
  }

  return true;
}

void traverse(Settings *set, const char *dir_path, size_t depth) {
  DIR *dir = opendir(dir_path);
  char child_path[PATH_MAX];
  char *filename = stpncpy(child_path, dir_path, sizeof(child_path));
  *filename++ = '/';
  size_t remaining = sizeof(child_path) - (size_t)(filename - child_path);
  struct stat st;
  struct dirent *entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    size_t nlen = strlen(entry->d_name);
    if (nlen >= remaining) {
      continue;
    }

    memcpy(filename, entry->d_name, nlen);
    filename[nlen] = '\0';

    bool is_dir = entry->d_type == DT_DIR;
    bool match = true;

    if (set->pattern != nullptr &&
        fnmatch(set->pattern, entry->d_name, 0) != 0) {
      match = false;
    }

    if (match && st.st_size < set->min_size) {
      match = false;
    }

    if (match && set->file_type != TYPE_ALL) {
      if ((set->file_type == TYPE_FILE && entry->d_type != DT_REG) ||
          (set->file_type == TYPE_DIR && !is_dir) ||
          (set->file_type == TYPE_LINK && entry->d_type != DT_LNK)) {
        match = false;
      }
    }

    if (match) {
      printf("%s\n", child_path);
    }

    if (is_dir && depth < set->maxdepth) {
      traverse(set, child_path, depth + 1);
    }
  }
  closedir(dir);
}

int main(int argc, char *argv[]) {
  Settings settings;
  if (!init_settings(argc, argv, &settings)) {
    return EXIT_FAILURE;
  };

  printf("%s\n", settings.root);
  traverse(&settings, settings.root, 1);
  return EXIT_SUCCESS;
}
