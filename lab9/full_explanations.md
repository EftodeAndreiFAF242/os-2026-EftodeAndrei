# Lab 9 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 9: Watching System Calls with strace.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It explains what a system call really is, how to read every line `strace` prints, every line of `syscalls.c`, every command typed in the terminal, and why the same program makes a different number of `write` calls on a terminal and into a file. It is written for someone who has never used Linux. The figure numbers are the same as in the README and in the Word report.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [What a system call is](#2-what-a-system-call-is)
3. [How to read a strace line](#3-how-to-read-a-strace-line)
4. [Part 1: the calls of ls -l](#4-part-1-the-calls-of-ls--l)
5. [Part 2: syscalls.c line by line](#5-part-2-syscallsc-line-by-line)
6. [File descriptors and the number 3](#6-file-descriptors-and-the-number-3)
7. [Part 3: counting, and why printf is not write](#7-part-3-counting-and-why-printf-is-not-write)
8. [From strace to Lab 10](#8-from-strace-to-lab-10)
9. [Test yourself](#9-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| kernel | the core of the operating system; the only program that may touch the hardware |
| user program | every other program: `bash`, `ls`, `seq`, my `syscalls` |
| system call | a request from a user program to the kernel, like "open this file" or "write these bytes" |
| `strace` | a tool that prints every system call of a program |
| file descriptor | a small number (0, 1, 2, 3...) that a process uses to name an open file |
| C library (glibc) | the library every C program uses: `printf`, `strlen`, and small wrappers like `open()` that make the real system call |
| buffer | memory where data waits, to be sent in one piece later |
| `errno` | the error code of the last failed call, e.g. `ENOENT` = "no such file" |
| `/proc` | a pretend folder made by the kernel; its files describe the running processes |
| shell | the program that reads my commands (`bash`) and starts other programs |

## 2. What a system call is

A program runs in **user mode**: it can compute, but it cannot touch the disk, the screen or another program's memory. The processor itself forbids it. Only the kernel runs in **kernel mode**, where everything is allowed.

So when `ls` wants to print, it cannot write to the screen itself. It asks the kernel:

1. It puts the number of the service in a register (on x86-64 Linux, `write` is number 1, `openat` is 257) and the arguments in other registers (descriptor, address of the bytes, how many).
2. It executes the special instruction `syscall`. The processor switches to kernel mode and jumps to a fixed place in the kernel.
3. The kernel looks at the number, runs its `write` function, puts the result (bytes written, or a negative error) in a register.
4. It switches back to user mode, right after the `syscall` instruction.

That trip is the "door" of the lecture. In Lab 10 the same door is built by hand in xv6 (there the instruction is called `ecall` and the number goes into register `a7`).

`strace` stands next to the door. It uses another system call, `ptrace`, which lets one process watch another: the kernel stops the watched program at every entry and exit of a system call, and `strace` reads the registers and prints them.

## 3. How to read a strace line

```
openat(AT_FDCWD, "note.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
└name┘ └──────────────── arguments ────────────────────┘   └result
```

- **name**: which system call.
- **arguments**: decoded into words when `strace` knows them. A string is shown in quotes, cut after 32 characters with `...`. Flags like `O_WRONLY|O_CREAT` are bits joined with `|`.
- **result** after `=`: what the kernel returned. For `openat` it is the new descriptor, for `read`/`write` the number of bytes, for `close` 0.
- **errors**: `= -1 ENOENT (No such file or directory)`. The call returned -1 and the C library put the code `ENOENT` in `errno`.
- `= ?` means the call never returned (`exit_group` ends the process).
- `+++ exited with 0 +++` is not a call: it tells that the process ended with status 0.

The options used in this lab:

| Option | Meaning |
|---|---|
| `-f` | follow children too (lines get a PID in front when there can be several processes) |
| `-e trace=openat,read,...` | show only these calls |
| `-o file` | write the trace into `file` instead of the screen |
| `-c` | do not print the calls; print a table with counts at the end |
| `-S calls` | sort the `-c` table by the number of calls |

## 4. Part 1: the calls of ls -l

### 4.1 The command

```bash
cd ~
strace -f -e trace=openat,read,write,close ls -l
```

`ls -l` lists the folder in "long" format: permissions, number of links, owner, group, size, date, name. To print one line per entry it needs, for each entry, the information the kernel keeps about it, and the names of the owner and the group (the kernel knows only the numbers, 1000 and 1000).

### 4.2 The trace, in the order it happens (Figures 1.2 and 1.1)

| Lines | What happens |
|---|---|
| `openat(... "/etc/ld.so.cache" ...) = 3`, `close(3)` | the loader opens the index of the shared libraries |
| `openat(... "libselinux.so.1") = 3`, `read(3, "\177ELF...", 832)`, `close(3)` | it opens a library and reads its first 832 bytes, the ELF header; the rest is mapped with `mmap` (not shown) |
| the same for `libgcc_s`, `libm`, `libc`, `libpcre2` | five libraries in all |
| `openat(... "/proc/filesystems")` and two `read` | `libselinux` checks whether SELinux is mounted |
| `openat(... "/proc/self/maps")` and five `read` | the Rust runtime reads the memory map of its own process |
| `openat(... "uucore/en-US.ftl")` = 3, `read` | the messages of the uutils programs (Ubuntu 26.04's `ls` is written in Rust) |
| `openat(... "ls/en-US.ftl") = -1 ENOENT` | a second message file, missing: a failed call |
| `openat(AT_FDCWD, ".", ...O_DIRECTORY) = 3` | the folder to list |
| `openat("/etc/nsswitch.conf")`, `/etc/passwd`, `/etc/group` | turning the numbers 1000 into the names `vboxuser` |
| 16 times `openat(... "/usr/share/zoneinfo/...", O_DIRECTORY) = 4`, `close(4)` | this `ls` walks the time zone folders |
| `openat(... "Etc/UTC") = 4`, `read(4, "TZif2...")` | the time zone of the machine, to print the dates |
| `close(3)` | done with the folder |
| `write(1, "total 116\n...", 1326) = 1326` | **the whole listing, one call** |

Only one line prints. Everything else prepares the program or collects information.

### 4.3 The counting commands (Figure 1.3)

```bash
grep -v '+++' ~/os-lab9/ls-l.trace | awk '{print $2}' | cut -d'(' -f1 | sort | uniq -c
```

Each `|` sends the output of one command into the next. Take one line of the file, `5834  openat(AT_FDCWD, "/etc/ld.so.cache", ...) = 3`:

| Step | What it does | The line becomes |
|---|---|---|
| `grep -v '+++'` | drops lines that contain `+++` (the "exited" line) | unchanged |
| `awk '{print $2}'` | keeps the second word (words are split at spaces) | `openat(AT_FDCWD,` |
| `cut -d'(' -f1` | cuts at `(` and keeps field 1 | `openat` |
| `sort` | puts equal names next to each other | |
| `uniq -c` | merges equal neighbours and counts them | `31 openat` |

My first try was `grep -c "^openat("` (count lines that **start** with `openat(`); it printed 0 for everything, because with `-f -o` every line starts with the PID. That is why the PID is skipped with `awk`.

`ls -l /proc/$$/fd`: `$$` is replaced by bash with its own PID; `/proc/<pid>/fd` is a folder with one entry per open descriptor, each pointing to what is open. For the shell, 0, 1, 2 (and 255, a copy bash keeps for itself) are `/dev/pts/0`.

### 4.4 Why one write?

A program that prints through a line-buffered output (Part 3) makes one `write` per line on a terminal. The uutils `ls` does not: it builds the whole text in memory and writes it once, even on a terminal. The kernel does not care how the text is cut; the screen shows the same thing.

## 5. Part 2: syscalls.c line by line

```c
#include <fcntl.h>      /* open */
#include <unistd.h>     /* write, read, close */
#include <string.h>     /* strlen */
#include <stdio.h>      /* printf */
```

Headers only declare the functions (name, arguments, return type), so the compiler can check the calls. `fcntl.h` also defines the flags `O_CREAT`, `O_WRONLY`...

```c
const char *msg = "hello, kernel\n";
```

A pointer to 15 bytes in memory: 14 characters (`\n` is one character, the new line) and the final `'\0'` that marks the end of a C string. `strlen(msg)` counts until the `'\0'`: 14.

```c
int fd = open("note.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
```

- `O_WRONLY`: open for writing only.
- `O_CREAT`: create the file if it does not exist.
- `O_TRUNC`: if it exists, cut it to 0 bytes first.
- The flags are bits; `|` (bitwise OR) joins them into one number.
- `0644` is the permission of a new file, in octal: `rw-` for me (6), `r--` for the group (4) and for others (4). The `umask` of the shell can remove bits, but 0644 is already within the usual umask 002, so `ls -l` shows `-rw-r--r--` (Figure 2.2).
- The result is the descriptor, or -1 on error.

```c
if (fd < 0) { perror("open"); return 1; }
```

`perror` prints "open: " and the text of `errno`, e.g. "open: Permission denied". `return 1` from `main` ends the program with status 1.

```c
printf("open returned descriptor %d\n", fd);
```

`%d` is replaced by the integer `fd`. This is a library call; whether and when it becomes a `write` is the subject of Part 3.

```c
ssize_t n = write(fd, msg, strlen(msg));
```

"Write 14 bytes from the address `msg` into descriptor `fd`." The result is the number of bytes really written. `ssize_t` is a signed size type, so it can also hold -1.

```c
printf("write sent %zd bytes\n", n);
close(fd);
```

`%zd` is the format for a `ssize_t`. `close` frees descriptor 3; from now on 3 is free again.

```c
fd = open("note.txt", O_RDONLY);
char buf[64];
n = read(fd, buf, sizeof(buf) - 1);
buf[n] = '\0';
```

Open for reading. `buf` is 64 bytes on the stack. `read` asks for at most 63 bytes (`sizeof(buf)` is 64), keeping one byte for the `'\0'` that `printf("%s")` needs to know where the text ends. The file has 14 bytes, so `read` returns 14 and `buf[14] = '\0'`.

The program does not check this second `open`; if `note.txt` disappeared in between, `read(-1, ...)` would fail with `EBADF` and `buf[-1]` would be wrong. The sheet keeps the program short; a real program would check every call.

```c
printf("read got %zd bytes: %s", n, buf);
close(fd);
return 0;
```

No `\n` in the format: the new line comes from the file content itself.

### 5.1 The C names and the system call names

| C function | System call on x86-64 Linux |
|---|---|
| `open(path, flags, mode)` | `openat(AT_FDCWD, path, flags, mode)` |
| `write`, `read`, `close` | `write`, `read`, `close` (same name) |
| `printf` | none by itself; later `write(1, ...)` |
| `return 0` from `main` | `exit_group(0)` |

`AT_FDCWD` means "a path relative to the current working folder". The newer `openat` can also open relative to another open folder; glibc uses it for every `open`.

## 6. File descriptors and the number 3

The kernel keeps, for every process, a small table: descriptor number → open file. `open` puts the file in the **lowest free slot** and returns its number. That rule is in POSIX, so programs can rely on it (Lab 7's `redirect.c` used it to put a file in slot 1).

When bash starts a program, it does `fork` (copy itself, with its table) and then `exec` (load the new program, **keeping** the table). Bash's own 0, 1, 2 point to the terminal, so every program starts with 0, 1, 2 already taken, and the first `open` gets 3.

The two proofs of Figure 2.5:

- `ls -l /proc/self/fd`: `self` is the process that reads the folder, here `ls`. It has 0, 1, 2 on the terminal, and 3: the folder `/proc/<pid>/fd` it opened to list. `ls` too got 3 first.
- `./syscalls <&-`: in bash, `<&-` closes descriptor 0 for that one command. The program starts with only 1 and 2; `open` takes the lowest free slot, 0. The program works the same with 0 instead of 3.

## 7. Part 3: counting, and why printf is not write

### 7.1 The table of strace -c (Figure 3.1)

| Column | Meaning |
|---|---|
| `% time` | share of the total time spent in this call |
| `seconds` | total time in this call |
| `usecs/call` | average microseconds per call |
| `calls` | how many times it was called |
| `errors` | how many of them failed |
| `syscall` | the name |

The times are not reliable under `strace` (stopping the program at every call slows everything down), but the counts are exact.

### 7.2 Three kinds of buffering

The C library keeps a buffer for standard output (usually 4096 bytes or more). `printf` only formats the text into it. The buffer is sent with `write`:

| Mode | When the buffer is sent | Used when |
|---|---|---|
| unbuffered | at once, every time | standard error |
| line buffered | at every `\n`, or when full | output is a terminal |
| fully buffered | only when full, or at the end of the program | output is a file or a pipe |

So the same `./syscalls`:

| Output goes to | `write` calls | Which |
|---|---|---|
| the terminal | 4 | 3 lines of `printf` + my `write(3, ...)` |
| `out.txt` | 2 | 1 buffer of 80 bytes at exit + my `write(3, ...)` |

27 + 20 + 33 = 80: the three lines of Figure 2.4 are exactly the 80 bytes of Figure 3.3.

My own `write(fd, msg, 14)` is always 1 call: I called the system call myself, with no library buffer in between.

### 7.3 How the library knows (Figure 3.5)

Before the first output, the C library calls `fstat(1, ...)`, which describes what descriptor 1 is:

- `st_mode=S_IFCHR|0600, st_rdev=makedev(0x88, 0)`: a character device, number 136 / 0. Linux gives the numbers 136 to 143 to pseudo-terminals (`/dev/pts/N`). glibc recognises them as terminals directly: line buffering.
- `st_mode=S_IFREG|0664, st_size=0`: a regular file (just emptied by `> out.txt`): full buffering.
- `st_mode=S_IFCHR|0666, st_rdev=makedev(0x1, 0x3)`: a character device, but 1/3 is `/dev/null`. glibc is not sure, so it asks with `ioctl(1, TCGETS2, ...)` ("give me your terminal settings"). The kernel answers `-1 ENOTTY`, "inappropriate ioctl for device": not a terminal, so full buffering. This is the failed `ioctl` counted in Figure 3.4.

### 7.4 seq and the cost of a system call

`seq 1 100000` writes 588 895 bytes. With line buffering that would be 100 000 `write` calls; into `/dev/null` it makes 72, of about 8 KB each. Every system call costs the trip into the kernel and back (saving registers, switching modes, checking arguments), so fewer, larger calls are faster. That is the whole reason buffers exist.

In the last command:

```bash
strace -e trace=write seq 1 100000 2>&1 > /dev/null | head -n 3
```

Redirections are read from left to right: `2>&1` first makes descriptor 2 (where `strace` writes) a copy of descriptor 1, which at that moment is the pipe to `head`; then `> /dev/null` changes descriptor 1 alone to `/dev/null`. Result: the trace goes to `head`, the numbers to `/dev/null`.

### 7.5 The whole of ./syscalls (Figure 3.4)

41 calls of 19 kinds; the ones that are not mine:

| Call | Why |
|---|---|
| `execve` | bash's child becomes `./syscalls` |
| `brk` | moves the end of the heap: memory for `malloc` (the buffer of `printf`) |
| `mmap`, `mprotect`, `munmap` | map `libc.so.6` into memory and set which parts are code (read + execute) |
| `access` (1 error) | the loader checks for `/etc/ld.so.preload`, which does not exist |
| `openat`, `read`, `pread64`, `fstat`, `close` | the loader opening and reading `libc.so.6` |
| `arch_prctl`, `set_tid_address`, `set_robust_list`, `rseq`, `prlimit64`, `getrandom` | the C library preparing threads, the stack limit and random numbers for security |
| `ioctl` (1 error) | the terminal check of 7.3 |

## 8. From strace to Lab 10

`strace` watches the door from outside, using `ptrace`. In Lab 10 the door is built from inside, in the xv6 kernel: a new system call `trace(mask)` stores a mask in the process, and the kernel itself prints a line after each system call whose bit is set in the mask. `trace 32 grep hello README` in xv6 is the small twin of `strace -e trace=read grep hello README` on Linux.

## 9. Test yourself

1. What does `= -1 ENOENT` at the end of a strace line mean?
2. Why does `ls -l` open `/etc/passwd`?
3. In `strace -f -o file`, why does every line start with a number?
4. A program closes descriptor 1 and then opens a file. Which descriptor does it get, and where will its `printf` output go?
5. Why does `read(3, buf, 63)` return 14?
6. `./syscalls > out.txt` makes 2 `write` calls. Which two?
7. Would `fprintf(stderr, ...)` three times make 1 or 3 `write` calls into a file? Why?
8. Why is `printf("x"); _exit(0);` dangerous on a file, but not on a terminal... or is it?

**Answers.** 1: the call failed, the file does not exist. 2: to turn the owner number 1000 into the name `vboxuser`. 3: with `-f` there can be several processes, so each line says which PID made the call. 4: descriptor 1, the lowest free one; `printf` writes to 1, so the output goes into the file (this is how `>` works, Lab 7). 5: it asked for at most 63 bytes but the file has only 14. 6: `write(3, "hello, kernel\n", 14)` from my code and one `write(1, ..., 80)` with the whole `printf` buffer at exit. 7: 3, standard error is unbuffered. 8: it is dangerous on both. `_exit` ends the process without flushing the C library buffers, and `"x"` has no `\n`, so even on a terminal it is still in the buffer and is lost.
