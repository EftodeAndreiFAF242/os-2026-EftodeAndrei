# Lab 18 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 18: Build a User-Level Thread Switch.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It explains what a thread switch really is, every part of `uthread.c` and `uthread_switch.S`, and follows the program from its first line to its last. The figure numbers are the same as in the README and in the Word report.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [The idea, with a bookmark](#2-the-idea-with-a-bookmark)
3. [The data: context and thread](#3-the-data-context-and-thread)
4. [thread_init and thread_create](#4-thread_init-and-thread_create)
5. [thread_schedule](#5-thread_schedule)
6. [thread_switch line by line](#6-thread_switch-line-by-line)
7. [Why ra continues the other thread](#7-why-ra-continues-the-other-thread)
8. [The test threads](#8-the-test-threads)
9. [The whole run, step by step](#9-the-whole-run-step-by-step)
10. [The Makefile rule](#10-the-makefile-rule)
11. [The limits of these threads](#11-the-limits-of-these-threads)
12. [Test yourself](#12-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| register | a tiny, very fast storage place inside the CPU |
| `ra` | return address: where the current function goes back to |
| `sp` | stack pointer: the top of the current stack |
| `s0` to `s11` | "saved" registers: a function must give them back unchanged |
| `a0`, `a1` | the first two arguments of a function |
| stack | the memory where a function keeps its local data; it grows down |
| context | the saved registers of a thread that is not running |
| callee-saved | the called function must keep these registers unchanged |
| caller-saved | the calling function saves these itself if it still needs them |
| yield | a thread gives the CPU to another thread by itself |

## 2. The idea, with a bookmark

You are reading three books, a few pages at a time. When you stop one, you put a bookmark in it. When you come back, the bookmark tells you exactly where to continue.

A thread is one of those books. The **context** is its bookmark. `thread_switch` does two things: it puts the bookmark in the book you are leaving, and it opens the next book at its bookmark.

The CPU has only one set of registers, and those registers **are** "where the program is right now". Change them, and the CPU is suddenly somewhere else.

## 3. The data: context and thread

```c
struct context {
  uint64 ra;
  uint64 sp;
  uint64 s0;
  /* s1 to s10 */
  uint64 s11;
};
```

There are 14 fields of 8 bytes, so a context is 112 bytes. The order matters, because the assembly reaches each field by its distance from the start:

| Field | Offset | Field | Offset |
|---|---|---|---|
| `ra` | 0 | `s5` | 56 |
| `sp` | 8 | `s6` | 64 |
| `s0` | 16 | `s7` | 72 |
| `s1` | 24 | `s8` | 80 |
| `s2` | 32 | `s9` | 88 |
| `s3` | 40 | `s10` | 96 |
| `s4` | 48 | `s11` | 104 |

```c
struct thread {
  struct context ctx;
  char stack[STACK_SIZE] __attribute__((aligned(16)));
  int state;
};
```

Each thread has its saved registers, its **own stack** of 8192 bytes, and a state. `aligned(16)` places the stack at an address divisible by 16, which is what RISC-V expects from `sp`.

```c
struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
```

There are 4 slots: `main` plus three threads. Globals start at zero, and 0 is `FREE`, so all slots start free. `current_thread` points to the one running now.

## 4. thread_init and thread_create

```c
current_thread = &all_thread[0];
current_thread->state = RUNNING;
```

`main` itself becomes thread 0.

```c
for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
  if (t->state == FREE) break;
}
t->state = RUNNABLE;
```

Finds the first free slot and marks it ready to run.

```c
t->ctx.ra = (uint64)func;
t->ctx.sp = (uint64)t->stack + STACK_SIZE;
```

This is the trap of the sheet. A new thread never ran, so nobody saved registers for it. We write them by hand:

- `ra = func`: the first `ret` will "return" to the start of the function.
- `sp = stack + STACK_SIZE`: the **end** of the array, because a stack grows from high addresses to low ones. If `sp` pointed at the start of the array, the first push would write outside it.

Without these two lines, `ra` is 0 and the first switch jumps to address 0.

## 5. thread_schedule

```c
next_thread = 0;
t = current_thread + 1;
for(int i = 0; i < MAX_THREAD; i++){
  if(t >= all_thread + MAX_THREAD)
    t = all_thread;
  if(t->state == RUNNABLE) {
    next_thread = t;
    break;
  }
  t = t + 1;
}
```

This is **round robin**. It starts at the slot after the current one, goes around at the end of the array, and takes the first thread that is ready.

```c
if (next_thread == 0) {
  printf("thread_schedule: no runnable threads\n");
  exit(-1);
}
```

If nobody is ready, the program ends. This is how `uthread` finishes.

```c
if (current_thread != next_thread) {
  struct thread *old = current_thread;
  next_thread->state = RUNNING;
  current_thread = next_thread;
  thread_switch(&old->ctx, &next_thread->ctx);
}
```

`old` remembers the thread we are leaving, because the next line changes `current_thread`. Then the switch happens. When this thread is chosen again later, it continues right after this `thread_switch` call.

## 6. thread_switch line by line

```asm
	.text
	.globl thread_switch
thread_switch:
```

`.text` means "this is code". `.globl` lets `uthread.c` see the name. The C call puts `&old->ctx` in `a0` and `&next_thread->ctx` in `a1`.

```asm
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	...
	sd s11, 104(a0)
```

`sd` means "store doubleword", 8 bytes. `sd sp, 8(a0)` writes `sp` to the address `a0 + 8`, which is the `sp` field of the old context. After these 14 lines, the old thread is safely saved.

```asm
	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	...
	ld s11, 104(a1)
```

`ld` means "load doubleword". These lines fill the CPU with the new thread's values. As soon as `sp` is loaded, we are on the new thread's stack.

```asm
	ret
```

`ret` jumps to the address in `ra`. It is the line where the CPU really lands in the other thread.

**Why only these 14?** By the RISC-V calling rules, a called function may destroy `t0`-`t6` and `a0`-`a7`, so the compiler already saved anything it needs from them before calling `thread_switch`. It expects `sp` and `s0`-`s11` to come back unchanged, and `ra` is needed to return. Saving exactly these 14 is enough.

## 7. Why ra continues the other thread

Normally, a function call works like this: `call f` puts the next address in `ra` and jumps to `f`, and `ret` in `f` jumps back to `ra`.

`thread_switch` plays a trick. It is called by thread A, so `ra` points back into A. But before `ret`, it replaces `ra` with the value saved earlier by thread B. So `ret` "returns" to a place in B, as if B's own call to `thread_switch` had just finished. Thread B does not notice that anything happened.

The pair matters:

- `ra` says **where** to continue.
- `sp` says **on which stack**, so the local variables are the right ones.

## 8. The test threads

```c
printf("thread_a started\n");
a_started = 1;
while(b_started == 0 || c_started == 0)
  thread_yield();
```

Thread a announces itself, then waits until the other two have started. While waiting, it keeps giving the CPU away.

```c
for (i = 0; i < 100; i++) {
  printf("thread_a %d\n", i);
  a_n += 1;
  thread_yield();
}
```

It prints one line, counts it, and gives the CPU to the next thread.

```c
printf("thread_a: exit after %d\n", a_n);
current_thread->state = FREE;
thread_schedule();
```

The thread is done. It frees its slot and calls the scheduler, which never comes back to it. A thread function must **not** simply return here, because nobody called it, so there is no caller to go back to. Its saved `ra` is still its own first address, so a plain `return` would start the function again.

The flags are `volatile`, which tells the compiler to really read them from memory each time, because another thread changes them.

## 9. The whole run, step by step

1. `main` becomes slot 0. Threads a, b and c take slots 1, 2 and 3.
2. `main` marks itself `FREE` and calls the scheduler. Slot 1 is next, so **a** starts.
3. a prints `thread_a started`. b has not started, so a yields. **b** runs.
4. b prints `thread_b started`. c has not started, so b yields. **c** runs.
5. c prints `thread_c started`. a and b have started, so c skips its wait and prints **`thread_c 0`**. Then it yields.
6. After slot 3 the scheduler wraps around. Slot 0 is free, so **a** continues inside its wait loop, sees that everyone started, and prints `thread_a 0`.
7. b does the same and prints `thread_b 0`.
8. c continues and prints `thread_c 1`, and the order stays c, a, b.

This is why **c** prints first, even though a started first: c is the only one that never had to wait.

At the end, c prints `thread_c: exit after 100` first and frees itself, then a, then b. Nobody is left, so the scheduler prints `no runnable threads` and the program exits. That last line is the normal end, not an error.

## 10. The Makefile rule

```make
$U/_uthread: $U/uthread.o $U/uthread_switch.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $U/_uthread $U/uthread.o $U/uthread_switch.o $(ULIB)
	$(OBJDUMP) -S $U/_uthread > $U/uthread.asm
```

- The first line says what `_uthread` is built from.
- The normal xv6 rule links **one** `.o` file with the library. `uthread` has two, so it needs its own rule.
- `$(LD)` is the linker. It joins the object files into a program.
- `$(OBJDUMP) -S` writes a readable listing of the machine code into `uthread.asm`.
- The two command lines must start with a **Tab**, or `make` stops with an error.

`$U/_uthread\` in `UPROGS` is what puts the program on the xv6 disk.

## 11. The limits of these threads

- **They must cooperate.** If a thread never calls `thread_yield`, the others never run. Nothing can interrupt it inside this library.
- **The kernel sees one process.** If one thread blocks in a system call, all of them stop.
- **They use one CPU at a time**, because the process is a single kernel thread.
- **`thread_create` does not check for a full table.** With more than 4 threads it would write outside the array. The sheet's program uses exactly 4 slots.

In exchange, a switch is extremely cheap: 29 instructions and no system call.

## 12. Test yourself

1. Which 14 registers does `thread_switch` save?
2. Why does it not save `t0` or `a0`?
3. Why does `thread_create` set `sp` to the **end** of the stack array?
4. What would happen without `t->ctx.ra = (uint64)func`?
5. Which instruction actually moves the CPU into the other thread?
6. Why does `thread_schedule` keep the variable `old`?
7. Why does thread c print its 0 before thread a?
8. Is `no runnable threads` an error?
9. What happens if one thread never calls `thread_yield`?
10. Why does `uthread` need its own Makefile rule?

**Answers.** (1) `ra`, `sp`, `s0` to `s11`. (2) They are caller-saved: the compiler already kept what it needs. (3) A stack grows down. (4) `ra` stays 0, so the first `ret` jumps to address 0. (5) `ret`. (6) `current_thread` already points to the new thread, so the old one must be remembered. (7) a and b had to wait for others to start, and c did not. (8) No, it is the normal end when all threads have finished. (9) The others never run again. (10) It links two object files, and the normal rule links only one.
