// semtry.c: shows both answers of sem_trydown (optional part of Lab 13).
#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  sem_init(4, 1);
  printf("first try: %d\n", sem_trydown(4));    // took the only unit
  printf("second try: %d\n", sem_trydown(4));   // nothing left, no sleep
  sem_up(4);
  printf("after sem_up: %d\n", sem_trydown(4)); // one unit is back
  exit(0);
}
