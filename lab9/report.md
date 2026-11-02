# Lab 9 - Watching System Calls with strace

Student: Eftode Andrei, FAF-242
Machine: Ubuntu 26.04.1 LTS in VirtualBox, kernel 7.0.0-38-generic,
gcc 15.2.0, strace 6.19
Folder: ~/os-lab9

## Part 1 - The calls of a real command

    $ cd ~
    $ strace -f -e trace=openat,read,write,close ls -l
    openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
    ...
    write(1, "total 116\ndrwxr-xr-x  2 vboxuser"..., 1326) = 1326
    +++ exited with 0 +++

Count (strace -o, then grep): 86 calls of these four kinds
(31 openat, 20 read, 1 write, 34 close), and 324 system calls of
every kind (strace -c says 323, plus the final exit_group).
Almost all of them come before the listing: 5 libraries are loaded
(libselinux, libgcc_s, libm, libc, libpcre2), then /proc/filesystems,
/proc/self/maps, the message file of ls, /etc/nsswitch.conf,
/etc/passwd and /etc/group (to turn uid 1000 into "vboxuser"), and the
time zone (this ls even opens 16 folders of /usr/share/zoneinfo).

Observe: ls prints with the system call write. It writes to descriptor
1, standard output, which is my terminal (/dev/pts/0). The whole
listing, 1326 bytes, goes out in one single write: the ls of Ubuntu
26.04 (the Rust "uutils" one) builds the text first, then writes once.

## Part 2 - A program that calls the kernel directly

    $ gcc -Wall -Wextra -o syscalls syscalls.c
    $ ./syscalls
    open returned descriptor 3
    write sent 14 bytes
    read got 14 bytes: hello, kernel

    $ strace -e trace=openat,write,read,close ./syscalls
    openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
    close(3)                                = 0
    openat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
    read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0 \250\2\0\0\0\0\0"..., 832) = 832
    close(3)                                = 0
    openat(AT_FDCWD, "note.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
    write(1, "open returned descriptor 3\n", 27) = 27
    write(3, "hello, kernel\n", 14)         = 14
    write(1, "write sent 14 bytes\n", 20)   = 20
    close(3)                                = 0
    openat(AT_FDCWD, "note.txt", O_RDONLY)  = 3
    read(3, "hello, kernel\n", 63)          = 14
    write(1, "read got 14 bytes: hello, kernel"..., 33) = 33
    close(3)                                = 0
    +++ exited with 0 +++

My six calls are there, as on the sheet. Between them are three more
write(1, ...): those are the three printf lines going to the terminal.

Observe: 0, 1 and 2 are standard input, standard output and standard
error. The shell opened them on the terminal and the program got them
through fork and exec, so they are taken before main starts
(ls -l /proc/self/fd shows 0, 1, 2 -> /dev/pts/0). open always gives
the smallest free number, so 3. Proof: with standard input closed,
./syscalls <&- prints "open returned descriptor 0".

## Part 3 - Count one kind of call

(The sheet gives only the title of this part; I counted with strace -c.)

    $ strace -c -S calls -o ls-l.count ls -l > /dev/null
      44 listxattr, 34 close, 34 getdents64, 31 openat (1 error),
      29 fstat, 29 statx, 27 mmap, 20 read ... 1 write
      323 calls in total, 13 of them errors
    $ strace -c -e trace=openat ls -l > /dev/null
      31 openat, 1 error (the missing message file of ls)
    $ strace -c -e trace=write ./syscalls
      4 write
    $ strace -c -e trace=write ./syscalls > out.txt
      2 write
    $ strace -c -e trace=write seq 1 100000 > /dev/null
      72 write for 588895 bytes (about 8190 bytes each)
    $ strace -c ./syscalls > /dev/null
      41 calls of 19 kinds; only 6 of them are the calls I wrote
      (2 openat, 1 write, 1 read, 2 close), plus 1 write for printf;
      1 ioctl fails: the C library asks "is stdout a terminal?"
    $ strace -e trace=fstat,ioctl ./syscalls      (and > out.txt, > /dev/null)
      terminal:  fstat(1) = S_IFCHR, st_rdev 0x88 (a pseudo-terminal)
      out.txt:   fstat(1) = S_IFREG (a regular file)
      /dev/null: fstat(1) = S_IFCHR 0x1,0x3, then ioctl(1, TCGETS2) = ENOTTY

1. The most frequent call of ls -l is listxattr (44, about two per
   entry), not write (1). Listing costs more calls than printing:
   statx and listxattr for every entry, getdents64 to read folders.
2. ./syscalls makes 4 writes on the terminal but only 2 into a file.
   printf does not call write by itself: it fills a buffer in the C
   library. On a terminal the buffer is sent at every "\n" (3 writes);
   into a file it is sent once, at exit (1 write of 80 bytes). The
   library decides with fstat(1): a pseudo-terminal (device 0x88)
   gets line buffering, a regular file full buffering. For any other
   device (/dev/null) it also tries ioctl(1, TCGETS2), which fails
   with ENOTTY: that is the 1 error in strace -c. The
   write to note.txt is always exactly 1, because I call write myself.
3. seq prints 100000 lines with only 72 writes. Every system call is
   a trip into the kernel, so programs collect output and write it in
   big pieces.

## Conclusion

strace shows the door between a program and the kernel: every open,
read, write and close of the program is a system call with a number
in, a number out. Even "ls -l" makes about 320 of them, most to load
libraries and to look up names. My own program's calls appear exactly
as I wrote them, with descriptor 3, the smallest free one after 0, 1
and 2. printf is not a system call; the C library decides when to call
write, so the same program makes 4 writes on a terminal and 2 into a
file.
