#include "kernel/types.h"
#include "user/user.h"

// pingpong: a parent and its child exchange one byte over two pipes.
int
main(int argc, char *argv[])
{
  int p2c[2];   // parent -> child
  int c2p[2];   // child -> parent
  char byte = 'p';

  if(pipe(p2c) < 0 || pipe(c2p) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "pingpong: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // child: wait for the ping, answer with the pong
    close(p2c[1]);
    close(c2p[0]);
    if(read(p2c[0], &byte, 1) != 1){
      fprintf(2, "pingpong: child read failed\n");
      exit(1);
    }
    printf("%d: received ping\n", getpid());
    write(c2p[1], &byte, 1);
    exit(0);
  }

  // parent: send the ping, wait for the pong
  close(p2c[0]);
  close(c2p[1]);
  write(p2c[1], &byte, 1);
  if(read(c2p[0], &byte, 1) != 1){
    fprintf(2, "pingpong: parent read failed\n");
    exit(1);
  }
  printf("%d: received pong\n", getpid());
  wait(0);
  exit(0);
}
