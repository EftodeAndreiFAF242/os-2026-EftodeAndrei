#include "kernel/types.h"
#include "user/user.h"

// the primes from 2 to 35 with a chain of processes (the sieve of the MIT util lab)

void stage(int left) __attribute__((noreturn));

// one stage of the chain: it reads numbers from the pipe "left"
void
stage(int left)
{
  int p, n;

  if(read(left, &p, sizeof(p)) != sizeof(p)){  // nothing came: the chain ends here
    close(left);
    exit(0);
  }
  printf("prime %d\n", p);                     // the first number is always a prime

  int right[2];
  pipe(right);
  if(fork() == 0){                             // the next stage reads from right[0]
    close(right[1]);
    close(left);
    stage(right[0]);
  }
  close(right[0]);
  while(read(left, &n, sizeof(n)) == sizeof(n)){
    if(n % p != 0)                             // forward only what p does not divide
      write(right[1], &n, sizeof(n));
  }
  close(left);
  close(right[1]);                             // the next stage gets end-of-file
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];

  pipe(p);
  if(fork() == 0){
    close(p[1]);
    stage(p[0]);
  }
  close(p[0]);
  for(int i = 2; i <= 35; i++)
    write(p[1], &i, sizeof(i));
  close(p[1]);
  wait(0);
  exit(0);
}
