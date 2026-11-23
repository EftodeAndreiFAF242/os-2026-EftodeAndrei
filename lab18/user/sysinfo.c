#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct sysinfo info;

  if (sysinfo(&info) < 0) {
    fprintf(2, "sysinfo: failed\n");
    exit(1);
  }
  printf("free memory: %lu bytes (%lu pages)\n", info.freemem, info.freemem / 4096);
  printf("processes in use: %lu\n", info.nproc);

  // an address the process does not own must give -1, not crash the kernel
  printf("with a bad pointer sysinfo returns %d\n", sysinfo((struct sysinfo *)0xffffffffffUL));
  exit(0);
}
