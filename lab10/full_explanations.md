# Lab 10 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 10: Add Your Own System Call: trace.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It follows one system call from the user program, through the trap into the kernel, to the handler and back; explains every edit of the lab and every line of `trace.c`; shows how to read a mask bit by bit; explains the optional `sysinfo` call; and ends with a self-test. It is written for someone who has never read kernel code. The figure numbers are the same as in the README and in the Word report.

Lab 9 ([`../lab9/full_explanations.md`](../lab9/full_explanations.md)) watched system calls on Linux from the outside, with `strace`. This lab builds the same idea inside xv6.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [The journey of one system call in xv6](#2-the-journey-of-one-system-call-in-xv6)
3. [Part 1: the number and the user side](#3-part-1-the-number-and-the-user-side)
4. [Check 1: what the message means](#4-check-1-what-the-message-means)
5. [Part 2: the kernel side](#5-part-2-the-kernel-side)
6. [Check 2: why 1023, 965, 453, 0](#6-check-2-why-1023-965-453-0)
7. [Part 3: reading a mask](#7-part-3-reading-a-mask)
8. [Optional: sysinfo](#8-optional-sysinfo)
9. [The commits and how to rebuild](#9-the-commits-and-how-to-rebuild)
10. [trace and strace side by side](#10-trace-and-strace-side-by-side)
11. [Test yourself](#11-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| kernel | the operating system itself; in xv6, the folder `kernel/` |
| user program | a program that runs on top of the kernel: `sh`, `grep`, `echo`, my `trace`; in xv6, the folder `user/` |
| user mode / supervisor mode | the two modes of the RISC-V processor; in user mode the program cannot touch devices or kernel memory |
| register | a tiny storage cell inside the processor; RISC-V has 32, named `a0`-`a7`, `sp`, `ra`... |
| `ecall` | the instruction that jumps from user mode into the kernel ("environment call") |
| trap | any jump into the kernel: a system call, an interrupt from a device, an error |
| trapframe | a page where the kernel saves all the user registers of a process during a trap |
| `struct proc` | the kernel's record of one process: state, pid, memory, open files, name, and now the mask |
| `myproc()` | the kernel function that returns the `struct proc` of the process running on this CPU |
| mask | a number used as a row of on/off switches, one bit per system call |
| `printk` | the kernel's print function; it writes straight to the console |

## 2. The journey of one system call in xv6

Take `read(fd, buf, n)` in `grep`:

| Step | Where | What happens |
|---|---|---|
| 1 | `user/grep.c` | `grep` calls `read(fd, buf + m, sizeof(buf) - m - 1)`. The C calling convention puts the three arguments in `a0`, `a1`, `a2` |
| 2 | `user/usys.S` (made by `usys.pl`) | the stub `read:` does `li a7, SYS_read` (5 into `a7`), then `ecall` |
| 3 | the processor | `ecall` switches to supervisor mode and jumps to the address in register `stvec`: `uservec` |
| 4 | `kernel/trampoline.S`, `uservec` | saves all 32 user registers into the process's trapframe, switches to the kernel page table and stack, jumps to `usertrap()` |
| 5 | `kernel/trap.c`, `usertrap()` | sees `scause == 8` ("environment call from user mode"), moves the saved program counter 4 bytes forward (past the `ecall`), calls `syscall()` |
| 6 | `kernel/syscall.c`, `syscall()` | `num = p->trapframe->a7` (5), checks it, runs `syscalls[5]()` = `sys_read()`, stores the result in `p->trapframe->a0`. **The lab adds the print here.** |
| 7 | `kernel/sysfile.c`, `sys_read()` | reads its arguments from the trapframe with `argint`/`argaddr`, does the work, returns the number of bytes |
| 8 | `kernel/trap.c`, `prepare_return()`; `trampoline.S`, `userret` | restore the user registers from the trapframe (now `a0` holds the result) and `sret` back to user mode |
| 9 | `user/usys.S` | the stub does `ret`; `read()` returns the value of `a0` to `grep` |

The lab adds a new call by filling every step of this path for the number 23.

## 3. Part 1: the number and the user side

### 3.1 kernel/syscall.h (Figure 1.1)

```c
#define SYS_trace  23      // new
```

A `#define` is a name for a number, replaced by the compiler. This file is included both by the kernel (`syscall.c`) and by the generated stubs (`usys.S` starts with `#include "kernel/syscall.h"`), so both sides use the same 23. The sheet warns that the MIT page uses 22: in our newer xv6, 22 is already `sync`. Two calls with the same number would mean one of them is unreachable.

### 3.2 user/user.h (Figure 1.2)

```c
int trace(int);         // new
```

A declaration: "there is a function `trace` taking an `int` and returning an `int`". The function itself is the stub in `usys.S`; the declaration lets the C compiler check calls to it. xv6 is compiled with `-Werror`, so a call to an undeclared function would stop the build.

### 3.3 user/usys.pl (Figure 1.3)

```perl
entry("trace");         # new
```

The script's `entry` function (its body is at the top of Figure 1.3) prints, for each name:

```
.global trace
trace:
 li a7, SYS_trace
 ecall
 ret
```

- `.global trace`: make the label visible to other files (the linker can connect `trace.c` to it).
- `li a7, SYS_trace`: "load immediate": put 23 in register `a7`.
- `ecall`: into the kernel.
- `ret`: back to the caller; whatever the kernel left in `a0` is the return value.

The argument (the mask) is already in `a0`, because the C compiler put it there when calling `trace(mask)`. The stub does not touch it. Figure 1.7 shows the real generated lines 113-117 of `user/usys.S`.

### 3.4 user/trace.c, line by line (Figure 1.4)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
```

The usual three headers of an xv6 program: the type names (`uint`, `uint64`), `struct stat` (needed by `user.h`), and the declarations of the system calls and helpers.

```c
int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "usage: trace mask command [args...]\n");
    exit(1);
  }
```

`trace` needs at least a mask and a command. `fprintf(2, ...)` writes the message to descriptor 2, standard error.

```c
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "trace: failed\n");
    exit(1);
  }
```

`atoi` ("ASCII to integer") turns the text `"32"` into the number 32. `trace(32)` is the new system call; it sets the mask of **this** process. Before Part 2 the kernel answers -1, hence "trace: failed".

```c
  exec(argv[2], argv + 2);
  fprintf(2, "trace: exec %s failed\n", argv[2]);
  exit(1);
}
```

`argv + 2` is the same array of strings starting two places later: `{"grep", "hello", "README", 0}`. `exec` loads `grep` into this same process. It keeps the pid, the open files and the whole `struct proc` (only the memory and registers change), so the mask stays and `grep` is traced. If `exec` works it never returns; the last two lines run only if it fails (for example `trace 32 nosuchprogram`).

### 3.5 The Makefile (Figure 1.5)

`UPROGS` is the list of programs that `mkfs` copies onto the xv6 disk image `fs.img`. Each line of the list starts with a TAB and ends with a backslash, which tells `make` that the list continues on the next line. The command:

```bash
sed -i 's|^\t$U/_zombie\\$|&\n\t$U/_trace\\|' Makefile
```

| Piece | Meaning |
|---|---|
| `sed -i` | edit the file in place |
| `s\|A\|B\|` | substitute `A` by `B` (`\|` is used as separator instead of `/`, because the text contains `/`) |
| `^\t$U/_zombie\\$` | `A`: start of line, TAB, `$U/_zombie`, a backslash (`\\` is one literal `\`), end of line |
| `&` | in `B`: the whole text that matched, i.e. the zombie line itself |
| `\n\t$U/_trace\\` | then a new line, TAB, `$U/_trace` and a backslash |

`grep -n -A2 '_zombie' Makefile | cat -A` checks it: `^I` is how `cat -A` shows a TAB and `$` marks the end of the line. The new line sits between `zombie` and my `run` of Lab 8.

## 4. Check 1: what the message means

```
$ trace 32 grep hello README
3 trace: unknown sys call 23
trace: failed
```

The kernel's `syscall()` (before Part 2):

```c
  num = p->trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printk("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
```

`NELEM(syscalls)` is the number of places in the table, 23 (indexes 0 to 22). 23 is not smaller than 23, so it goes to `else`: the kernel prints pid 3, the process name `trace` and the number, and returns -1. Why pid 3? `init` is 1, the shell `sh` is 2, and the first command typed after boot is 3. This message proves that steps 1 to 6 of the journey already work for 23.

## 5. Part 2: the kernel side

### 5.1 kernel/proc.h (Figure 2.1)

```c
  int trace_mask;              // new: which calls to print
```

One `int` has 32 bits, enough for calls 0 to 31. `proc[NPROC]` is a global array, and global variables in C start at 0, so every process slot starts with mask 0. Every kernel file that uses `struct proc` must be recompiled, because the structure changed size; the Makefile knows it from the `.d` dependency files that `gcc -MD` writes.

### 5.2 kernel/sysproc.c (Figure 2.2)

```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}
```

- Handlers take no C arguments: the user's arguments are in the saved registers. `argint(0, &mask)` is in `kernel/syscall.c`: it calls `argraw(0)`, which returns `p->trapframe->a0`, and stores it in `mask` as an `int`.
- `myproc()->trace_mask = mask;` writes the mask into the caller's `struct proc`.
- `return 0;` becomes `p->trapframe->a0`, then the return value of `trace()` in user space.

### 5.3 kernel/syscall.c: the table (Figure 2.3)

```c
extern uint64 sys_trace(void);   // new
...
  [SYS_trace]   = sys_trace,       // new
```

`syscalls` is declared as `static uint64 (*syscalls[])(void)`: an array of pointers to functions that take nothing and return `uint64`. `[SYS_trace] = sys_trace` is a "designated initializer": it puts the address of `sys_trace` at index 23, whatever order the lines are in. The array becomes 24 places long, so `NELEM(syscalls)` is now 24 and 23 passes the check.

### 5.4 kernel/syscall.c: names and the print (Figure 2.4)

```c
static char *syscallnames[] = {
  [SYS_fork] "fork",   [SYS_exit] "exit",     [SYS_wait] "wait",
  ...
};
```

An array of strings indexed by call number: `syscallnames[5]` is `"read"`. (`[index] value` without `=` is an old GNU form of the designated initializer; gcc accepts it.) Place 0 is empty, since no call has number 0. It is `static`: visible only inside `syscall.c`. It must come **above** `syscall()`, because C reads a file from top to bottom and a name must be declared before it is used.

```c
    p->trapframe->a0 = syscalls[num]();
    if ((p->trace_mask >> num) & 1)                    // new
      printk("%d: syscall %s -> %ld\n",                // new
             p->pid, syscallnames[num], p->trapframe->a0);
```

- `p->trace_mask >> num`: shift the bits of the mask `num` places to the right; bit `num` arrives at position 0.
- `& 1`: keep only position 0. The result is 1 if bit `num` was on, else 0.
- The print is **after** the call, so `p->trapframe->a0` already holds the result. `%ld` prints a `long` (64 bits). `a0` is a `uint64`, but `%ld` reads it as signed, so -1 prints as `-1` and not as 18446744073709551615.
- `printk`, not `printf`: in this xv6 the kernel print function was renamed to `printk`, so that it is not confused with the `printf` of user programs.

This is where I made a mistake (README, 2.4): the three lines first landed after `} else {`, where they would print only for unknown calls. In the `else` branch `syscallnames[num]` could even read outside the table. I moved them before building.

### 5.5 kernel/proc.c (Figure 2.5)

```c
  safestrcpy(np->name, p->name, sizeof(p->name));
  np->trace_mask = p->trace_mask;      // new
```

In this xv6 the kernel side of `fork` is called `kfork()` (the `k` separates kernel functions from the user ones). It allocates `np`, copies the memory (`uvmcopy`), the registers (`*(np->trapframe) = *(p->trapframe)`), the open files (`filedup`), the current folder and the name. Nothing copies the mask unless this line does.

What about `exec`? `kexec()` in `kernel/exec.c` replaces the memory and the registers, and copies the new program name into `p->name`, but it does not touch `trace_mask`. That is why `trace` can set the mask and then `exec` the command.

## 6. Check 2: why 1023, 965, 453, 0

`user/grep.c`:

```c
char buf[1024];
...
  m = 0;
  while ((n = read(fd, buf + m, sizeof(buf) - m - 1)) > 0) {
    m += n;
    buf[m] = '\0';
    p = buf;
    while ((q = strchr(p, '\n')) != 0) {   // for every complete line
      ...                                  // match it, print it if it matches
      p = q + 1;
    }
    if (m > 0) {
      m -= p - buf;                        // bytes of the unfinished last line
      memmove(buf, p, m);                  // move them to the front
    }
  }
```

| Read | Asks for | Gets | Why |
|---|---|---|---|
| 1 | 1024 - 0 - 1 = 1023 | 1023 | the file is longer; one byte is kept for `'\0'` |
| 2 | 1023 - m = 965 | 965 | m = 58: the last line of the first piece was cut, its 58 bytes were moved to the front |
| 3 | (1023 - the new m) | 453 | only 2441 - 1023 - 965 = 453 bytes are left |
| 4 | ... | 0 | end of file: the loop stops |

The numbers depend only on `README` (2441 bytes) and on `grep.c`, so they are the same on every machine. Only the pid may differ.

## 7. Part 3: reading a mask

### 7.1 Bits and powers of two

| Call | Number N | Mask with only this bit: 2^N |
|---|---|---|
| fork | 1 | 2 |
| read | 5 | 32 |
| exec | 7 | 128 |
| open | 15 | 32768 |
| write | 16 | 65536 |
| close | 21 | 2097152 |
| trace | 23 | 8388608 |
| sysinfo | 24 | 16777216 |

To trace several calls, add their masks: open + close = 32768 + 2097152 = **2129920**. In binary: `1000001000000000000000`, a 1 at position 21 and a 1 at position 15 (counting from 0 on the right).

`2147483647` = 2^31 - 1 = 31 ones in binary: every call from 0 to 30. (It is also the largest positive `int`; 2^31 would not fit in an `int`.)

### 7.2 All bits on (Figure 3.1)

```
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
...
4: syscall close -> 0
```

- `trace -> 0`: the call that sets the mask is printed too, because the check runs after the call, when the mask is already set.
- `exec -> 3`: in xv6, `exec` returns `argc` (here 3: `grep`, `hello`, `README`), which becomes the first argument of `main` in the new program.
- `open -> 3`: `README` gets descriptor 3 (0, 1, 2 are the console, like on Linux).
- No `write`: no line of `README` contains "hello", so `grep` prints nothing.
- `close -> 0`, and then `exit`, which never returns, so it is never printed.

### 7.3 forkforkfork (Figures 3.2, 3.3)

From `user/usertests.c`:

```c
  int pid = fork();
  if (pid == 0) {
    while (1) {
      int fd = open("stopforking", 0);
      if (fd >= 0) exit(0);                         // the parent said stop
      if (fork() < 0)                               // table full:
        close(open("stopforking", O_CREATE | O_RDWR));  // tell everyone to stop
    }
  }
  pause(20);                                         // two seconds
  close(open("stopforking", O_CREATE | O_RDWR));
  wait(0);
```

- `usertests` (pid 3) runs each test in a child: `3: syscall fork -> 4`.
- The test (pid 4) forks the looping child: `4: syscall fork -> 5`.
- pid 5 and every child it makes run the same loop, so they all fork. With 3 CPUs, the ones that get to run print lines: 5, 6, later 34.
- xv6 has 64 slots (`NPROC` in `kernel/param.h`); pids 1 and 2 are `init` and `sh`. When the table is full, `fork` returns -1: `5: syscall fork -> -1`. That child creates the file `stopforking`, every looping process sees it and exits, and the test passes.

Without `np->trace_mask = p->trace_mask;` only pid 3 would print (its one `fork`), because 4, 5, 6... would start with mask 0.

### 7.4 echo hi (Figure 3.4)

`user/echo.c` writes each argument and then either a space or the new line:

```c
    write(1, argv[i], strlen(argv[i]));
    if (i + 1 < argc) write(1, " ", 1);
    else              write(1, "\n", 1);
```

So: `write(1, "hi", 2)`; the kernel puts "hi" on the console and then prints `6: syscall write -> 2\n` right after it, on the same line. Then `write(1, "\n", 1)`: the new line alone makes an empty line. Then the trace line of that second write: `6: syscall write -> 1`.

## 8. Optional: sysinfo

### 8.1 The pieces

| File | Added |
|---|---|
| `kernel/sysinfo.h` | `struct sysinfo { uint64 freemem; uint64 nproc; };` |
| `kernel/kalloc.c` | `freemem()` |
| `kernel/proc.c` | `nproc()` |
| `kernel/defs.h` | the two declarations, so `sysproc.c` can call them |
| `kernel/sysproc.c` | `#include "sysinfo.h"` and `sys_sysinfo()` |
| `kernel/syscall.h`, `kernel/syscall.c` | number 24, `extern`, table entry, name |
| `user/user.h`, `user/usys.pl` | `struct sysinfo;`, `int sysinfo(struct sysinfo *);`, `entry("sysinfo");` |
| `user/sysinfo.c`, `Makefile` | the test program and `$U/_sysinfo\` |

### 8.2 freemem()

```c
uint64
freemem(void)
{
  struct run *r;
  uint64 n = 0;

  acquire(&kmem.lock);
  for (r = kmem.freelist; r; r = r->next)
    n += PGSIZE;
  release(&kmem.lock);
  return n;
}
```

xv6 keeps every free 4096-byte page of physical memory in a linked list: the first bytes of each free page hold a pointer to the next free page (`struct run`). `kalloc()` takes the first page of the list, `kfree()` puts a page back. `freemem()` walks the list and adds `PGSIZE` (4096) per page. The lock: with 3 CPUs, another CPU could call `kalloc` in the middle of the walk and change the list; holding `kmem.lock` stops that for a moment.

### 8.3 nproc()

```c
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED)
      n++;
    release(&p->lock);
  }
```

Every slot of `proc[]` whose state is not `UNUSED` is a process in use (`USED`, `SLEEPING`, `RUNNABLE`, `RUNNING` or `ZOMBIE`). `p->lock` protects `p->state`.

### 8.4 sys_sysinfo() and copyout

```c
  argaddr(0, &addr);
  info.freemem = freemem();
  info.nproc = nproc();
  if (copyout(p->pagetable, p->sz, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
```

The user gives the **address** of its own `struct sysinfo`. That is a user virtual address: it means something only in the page table of that process. The kernel runs with its own page table, so it cannot write `*(struct sysinfo *)addr = info`. `copyout` walks the user page table to find the physical page behind `addr`, checks that the address is below `MAXVA`, that it belongs to the process and is writable, and copies the 16 bytes there. A bad address gives -1 instead of a kernel crash. (`argaddr` does no check by itself; its comment in `syscall.c` says "copyin/copyout will do that".)

### 8.5 The results (Figure 4.7)

| Situation | free memory | processes |
|---|---|---|
| after boot | 133 251 072 bytes (32 532 pages) | 3: init, sh, sysinfo |
| with `sleep 300 &` | 133 210 112 bytes (32 522 pages) | 4 |

QEMU gives xv6 128 MB; the kernel code and data, the page tables and the processes use the rest. The sleeping process costs 10 pages: its code and data, its stack and the guard page below it, its trapframe, and the pages of its page table.

## 9. The commits and how to rebuild

Inside the VM, branch `lab` of `~/xv6-riscv`:

```
aba8602 Lab 10: notes.txt
453cf78 Lab 10 optional: sysinfo system call
be310d9 Lab 10: trace system call
7e6e111 Lab 8: run, pingpong and primes
8cab251 Lab 6: mystat and find
5186256 Lab 4: sleep calls pause(), args program
06aad25 Add killzero test            (the course revision)
```

`lab10.patch` in this folder is `git format-patch --stdout 7e6e111..HEAD`: the three commits as text. The folder itself holds the changed files in their places, so copying `kernel/`, `user/` and `Makefile` over a fresh clone at `06aad25` gives the same tree (the README shows the commands).

## 10. trace and strace side by side

| | `strace` (Lab 9, Linux) | `trace` (Lab 10, xv6) |
|---|---|---|
| Where it runs | a separate user program | inside the kernel |
| How | `ptrace`: the kernel stops the traced program at every call and lets `strace` read its registers | `syscall()` checks a bit and prints with `printk` |
| What it shows | name, decoded arguments, result | pid, name, result |
| Choosing calls | `-e trace=read,write` | a bit mask: 32 for read |
| Children | only with `-f` | always: `kfork` copies the mask |
| Cost | the program stops twice per call | one shift and one AND per call |

## 11. Test yourself

1. Which register holds the system call number when `ecall` runs? Which one holds the result after?
2. Why did Check 1 print `unknown sys call 23` and not crash?
3. What would happen if `SYS_trace` were 22?
4. Why must the print in `syscall()` come after `p->trapframe->a0 = syscalls[num]();`?
5. Which mask traces only `exec` and `exit`?
6. `trace 32 sh`, then `cat README` in that shell: is `cat` traced? Why?
7. Why can `sys_sysinfo` not simply do `*(struct sysinfo *)addr = info;`?
8. Without the line in `kfork`, how many lines would `trace 2 usertests forkforkfork` print?

**Answers.** 1: `a7` for the number, `a0` for the result. 2: the number is outside the table, so `syscall()` takes the `else` branch: it prints the message and returns -1. 3: it would clash with `sync`: one of the two table entries would overwrite the other, so `sync()` would run `sys_trace` or the other way round. 4: before it, `a0` still holds the first argument, not the result. 5: `exec` is 7 and `exit` is 2: 2^7 + 2^2 = 128 + 4 = 132 (but `exit` never returns, so its line is never printed). 6: yes: `sh` forks a child for `cat`, the child copies the mask, then `exec` keeps it. 7: `addr` is a user virtual address, which means nothing in the kernel's page table; `copyout` translates and checks it. 8: one line, `3: syscall fork -> 4`, from `usertests` itself; all the children would have mask 0.
