#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

int showinum; // set by the optional third argument -n

// Print every path under dir whose last component is name.
void
find(char *dir, char *name)
{
  char buf[MAXPATH], *p; // the kernel refuses longer paths anyway
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(dir, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", dir);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", dir);
    close(fd);
    return;
  }
  if (st.type != T_DIR) {
    fprintf(2, "find: %s is not a directory\n", dir);
    close(fd);
    return;
  }
  if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }
  strcpy(buf, dir);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) // free slot
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;
    if (stat(buf, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }
    if (strcmp(p, name) == 0) {
      if (showinum)
        printf("%s %d\n", buf, st.ino);
      else
        printf("%s\n", buf);
    }
    if (st.type == T_DIR)
      find(buf, name);
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "-n") != 0)) {
    fprintf(2, "usage: find dir name [-n]\n");
    exit(1);
  }
  showinum = (argc == 4);
  find(argv[1], argv[2]);
  exit(0);
}
