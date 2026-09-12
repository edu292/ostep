#define _GNU_SOURCE
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>

typedef struct {
  char *type;
  uint16_t octal;
  char rwx[11];
} FileSignature;

void init_file_signature(FileSignature *fs, mode_t mode) {
  fs->type = "unknown";
  if (S_ISREG(mode)) {
    fs->type = "regular file";
    fs->rwx[0] = '-';
  } else if (S_ISDIR(mode)) {
    fs->type = "directory";
    fs->rwx[0] = 'd';
  } else if (S_ISLNK(mode)) {
    fs->type = "symbolic link";
    fs->rwx[0] = 'l';
  } else if (S_ISCHR(mode)) {
    fs->type = "character device";
    fs->rwx[0] = 'c';
  } else if (S_ISBLK(mode)) {
    fs->type = "block device";
    fs->rwx[0] = 'b';
  } else if (S_ISFIFO(mode)) {
    fs->type = "FIFO/pipe";
    fs->rwx[0] = 'p';
  } else if (S_ISSOCK(mode)) {
    fs->type = "socket";
    fs->rwx[0] = 's';
  }

  fs->octal = mode & 0777;

  const char template[] = "rwx";
  for (int i = 0; i < 9; i++) {
    fs->rwx[i + 1] = (mode & 0400) ? template[i % 3] : '-';
    mode <<= 1;
  }
  fs->rwx[10] = '\0';
}

void print_timestamp(const char *label, struct statx_timestamp ts) {
  char buf[36];
  struct tm tm;

  time_t sec = (time_t)ts.tv_sec;
  localtime_r(&sec, &tm);

  size_t off = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.", &tm);

  off += snprintf(buf + off, sizeof(buf) - off, "%09ld ", ts.tv_nsec);

  strftime(buf + off, sizeof(buf) - off, "%z", &tm);

  printf("%6s: %s\n", label, buf);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: stat <file>\n");
    return EXIT_FAILURE;
  }

  char *file = argv[1];
  struct statx st;
  if (statx(AT_FDCWD, file, 0, STATX_BASIC_STATS | STATX_BTIME, &st) < 0) {
    perror("faled to get stat");
    return EXIT_FAILURE;
  }

  printf("%6s: %s\n", "File", file);

  printf("%6s: %llu\t", "Size", st.stx_size);
  printf("Blocks: %llu\t", st.stx_blocks);
  printf("IO Block: %u\t", st.stx_blksize);
  FileSignature fs;
  init_file_signature(&fs, st.stx_mode);
  printf("%s\n", fs.type);

  printf("Device: %u,%u\t", st.stx_dev_major, st.stx_dev_minor);
  printf("Inode: %llu\t", st.stx_ino);
  printf("Links: %u\n", st.stx_nlink);

  printf("Access: (%04o/%s)\t", fs.octal, fs.rwx);

  uid_t uid = st.stx_uid;
  struct passwd *user = getpwuid(uid);

  printf("Uid: ( %u/ %s)\t", uid, user->pw_name);

  gid_t gid = st.stx_gid;
  struct group *group = getgrgid(gid);
  printf("Gid: ( %u/ %s)\n", gid, group->gr_name);

  print_timestamp("Access", st.stx_atime);
  print_timestamp("Modify", st.stx_mtime);
  print_timestamp("Change", st.stx_ctime);
  print_timestamp("Birth", st.stx_btime);

  return EXIT_SUCCESS;
}
