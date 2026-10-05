# Lab 2 - Meet the OS You Will Build (xv6)

Student: Eftode Andrei, FAF-242
Tree: mit-pdos/xv6-riscv, branch lab2 = commit 9374395 (the last one before
upstream renamed sleep() to pause()), plus -march=rv64gc in the Makefile
(needed by the Ubuntu 26.04 cross-compiler, same change as upstream e90b257).
Tools: riscv64-linux-gnu-gcc 15.2.0, QEMU 10.2.1.

## Part 2 - Using xv6

1. Three programs xv6 ships with: ls, cat, echo
   (the others on the disk: grep, wc, sh, mkdir, rm, ln, kill, init, usertests ...).
2. For "ls | grep c" to work the OS must have:
   - processes: the shell has to create two processes (fork) and run a different
     program in each one (exec), so that ls and grep run at the same time;
   - communication between processes: the kernel's pipe, a buffer with a write end
     and a read end that are file descriptors. The shell puts the write end in
     place of the standard output of ls and the read end in place of the standard
     input of grep.
3. The xv6 shell works on the same idea as the Linux shell (commands, arguments,
   pipes, redirection) but it is much smaller: the prompt is only "$" and there is
   no history, no tab completion, no variables and no job control.

Output:

    $ ls | grep c
    cat            2 3 36488
    echo           2 4 35344
    wc             2 17 37472
    console        3 22 0
    $ wc README
    48 334 2425 README
    $ usertests -q
    ...
    ALL TESTS PASSED

## Part 3 - Reading the source

1. user/cat.c uses these system calls:
   - read(fd, buf, sizeof(buf)) : copy up to 512 bytes from the open file into buf
   - write(1, buf, n)           : send n bytes from buf to descriptor 1 (the console)
   - open(argv[i], O_RDONLY)    : find the file by name, give back a descriptor
   - close(fd)                  : give the descriptor back
   - exit(status)               : end the process and report the status
   (fprintf(2, ...) is a library function, in the end it also calls write.)
2. sys_read is implemented in kernel/sysfile.c at line 69
   (its return type uint64 is on line 68). sys_write is at line 83.
3. kernel/ is the operating system itself and runs privileged, with access to all
   the hardware and memory; user/ holds ordinary programs that run unprivileged
   and can get anything done only by asking the kernel through a system call.

## Part 4 - sleep

    $ sleep 10        (pauses about one second, then the prompt comes back)
    $ sleep
    usage: sleep <ticks>

sleep() goes to sys_sleep in kernel/sysproc.c (line 66), system call number 13.
One tick is about a tenth of a second (kernel/trap.c).
