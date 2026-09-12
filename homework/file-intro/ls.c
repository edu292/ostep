#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void get_rwx(char rwx[11], mode_t mode) {
  if (S_ISREG(mode)) {
    rwx[0] = '-';
  } else if (S_ISDIR(mode)) {
    rwx[0] = 'd';
  } else if (S_ISLNK(mode)) {
    rwx[0] = 'l';
  } else if (S_ISCHR(mode)) {
    rwx[0] = 'c';
  } else if (S_ISBLK(mode)) {
    rwx[0] = 'b';
  } else if (S_ISFIFO(mode)) {
    rwx[0] = 'p';
  } else if (S_ISSOCK(mode)) {
    rwx[0] = 's';
  }

  const char template[] = "rwx";
  for (int i = 0; i < 9; i++) {
    rwx[i + 1] = (mode & 0400) ? template[i % 3] : '-';
    mode <<= 1;
  }

  rwx[10] = '\0';
}

int main(int argc, char *argv[]) {
  bool verbose = false;
  int opt = 0;
  while ((opt = getopt(argc, argv, "l")) != -1) {
    switch (opt) {
    case 'l':
      verbose = true;
      break;
    default:
      fprintf(stderr, "Usage: ls [-l] [directory]\n");
      return EXIT_FAILURE;
    }
  }

  int remaining = argc - optind;
  const char *dir_name = nullptr;

  if (remaining == 0) {
    dir_name = ".";
  } else if (remaining == 1) {
    dir_name = argv[optind];
  } else {
    fprintf(stderr, "Error: too many arguments.\n");
    fprintf(stderr, "Usage: %s [-l] [directory]\n", argv[0]);
    return EXIT_FAILURE;
  }

  DIR *dir = opendir(dir_name);
  if (dir == nullptr) {
    perror("opendir");
    return EXIT_FAILURE;
  }

  int dir_fd = dirfd(dir);
  if (dir_fd < 0) {
    perror("dirfd");
    return EXIT_FAILURE;
  }

  struct dirent *entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (!verbose) {
      printf("%s  ", entry->d_name);
      continue;
    }

    static struct stat st;
    static char rwx[11];
    static char time_buf[13];
    static struct tm tm;
    char *filename = entry->d_name;

    if (fstatat(dir_fd, filename, &st, 0) < 0) {
      fprintf(stderr, "failed to get stat for %s\n", filename);
      perror("");
      continue;
    };

    get_rwx(rwx, st.st_mode);
    struct passwd *user = getpwuid(st.st_uid);
    struct group *group = getgrgid(st.st_gid);

    localtime_r(&st.st_mtim.tv_sec, &tm);
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%m", &tm);

    printf("%s %lu %s %s %5zu %s %s\n", rwx, st.st_nlink, user->pw_name,
           group->gr_name, st.st_size, time_buf, filename);
  }

  if (!verbose) {
    printf("\n");
  }

  closedir(dir);
  return EXIT_SUCCESS;
}
