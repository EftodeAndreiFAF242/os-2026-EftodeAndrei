# Lab 13 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 13: Semaphores in Your Own Kernel.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It follows one system call from a user program into the kernel and explains every line of `kernel/sem.c`. Read Labs 10 and 11 first: the first adds a system call, the second uses semaphores.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [The path of sem_down](#2-the-path-of-sem_down)
3. [Why sysinfo moved to 28](#3-why-sysinfo-moved-to-28)
4. [sem.c: the data](#4-semc-the-data)
5. [sys_sem_init](#5-sys_sem_init)
6. [sys_sem_down, the heart](#6-sys_sem_down-the-heart)
7. [The lost wakeup](#7-the-lost-wakeup)
8. [sys_sem_up](#8-sys_sem_up)
9. [semtest step by step](#9-semtest-step-by-step)
10. [mix and the critical section](#10-mix-and-the-critical-section)
11. [Sleeping with a spinlock](#11-sleeping-with-a-spinlock)
12. [sem_trydown](#12-sem_trydown)
13. [Test yourself](#13-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| system call | a request from a user program to the kernel, with a number |
| `ecall` | the RISC-V instruction that jumps from user mode into the kernel |
| register `a7` | holds the number of the system call |
| registers `a0`, `a1` | hold the arguments, and `a0` holds the result on the way back |
| spinlock | a lock where the CPU keeps trying in a loop until it is free |
| channel | the address a process sleeps on |
| `wakeup(chan)` | wakes every process sleeping on that address |
| panic | the kernel found something impossible and stops |

## 2. The path of sem_down

When `semtest` calls `sem_down(0)`:

1. `user/usys.pl` generated a small stub: put 25 (`SYS_sem_down`) in register `a7`, then `ecall`.
2. `ecall` jumps into the kernel.
3. `syscall()` in `kernel/syscall.c` reads `a7` = 25 and runs `syscalls[25]`, which is `sys_sem_down`.
4. `sys_sem_down` reads its argument with `argint(0, &id)`, which looks in register `a0`.
5. Its return value goes into `a0`, and the program continues after `ecall`.

That is why Part 1 alone gives "unknown sys call 24": the stub and the number exist, but `syscalls[24]` is still empty.

## 3. Why sysinfo moved to 28

The optional part of Lab 10 gave `sysinfo` the number 24. This sheet demands 24, 25 and 26, and its Check 1 must print "unknown sys call 24". If `sysinfo` stayed at 24, `sem_init` would call the wrong function.

So the first commit changes one line in `kernel/syscall.h`:

```c
#define SYS_sysinfo 28     // optional, was 24
```

Only this line changes, because every other file uses the name `SYS_sysinfo`, not the number. 27 stays free for the optional `sem_trydown`. Figure 1.7 and Figure 4.5 show that `sysinfo` still works.

## 4. sem.c: the data

```c
#define NSEM 8

struct sem {
  struct spinlock lock;   // a zeroed spinlock is a free spinlock
  int value;              // how many may still pass
};

static struct sem sems[NSEM];
```

- There are 8 semaphores, numbered 0 to 7, shared by **all** processes.
- `static` means the table is visible only inside this file.
- A global variable starts as all zeros, so every lock starts free and every value starts at 0.

## 5. sys_sem_init

```c
uint64
sys_sem_init(void)
{
  int id, value;
  argint(0, &id);           // first argument
  argint(1, &value);        // second argument
  if (id < 0 || id >= NSEM || value < 0)
    return -1;              // never trust numbers from a user program
  acquire(&sems[id].lock);
  sems[id].value = value;
  release(&sems[id].lock);
  return 0;
}
```

The check matters. Without it, `sem_init(100, 1)` would write outside the table, and a user program could damage the kernel's memory.

## 6. sys_sem_down, the heart

```c
acquire(&s->lock);
while (s->value == 0) {
  sleep_prepare(s);       // I will sleep on the address of s
  release(&s->lock);      // never sleep holding a spinlock
  sleep();                // returns at once if sem_up came in between
  acquire(&s->lock);
  if (killed(myproc())) { // kill must still work on a waiting process
    release(&s->lock);
    return -1;
  }
}
s->value--;
release(&s->lock);
return 0;
```

Line by line:

1. Take the spinlock, so nobody changes the value while I look at it.
2. While there is nothing to take:
   - `sleep_prepare(s)` writes the address of `s` into my process (`p->chan`).
   - Release the spinlock, so that `sem_up` can get in.
   - `sleep()` gives up the CPU. A sleeping process uses **no CPU**.
   - When I wake up, take the spinlock again.
   - If someone killed me, stop waiting.
3. Now the value is above 0 and I hold the lock: take one unit.

**Why `while` and not `if`?** `wakeup(s)` wakes **every** process sleeping on `s`, but `sem_up` added only **one** unit. The first to get the spinlock takes it. The others find 0 again, and the `while` sends them back to sleep. A process that never slept can also come along and take the unit first.

## 7. The lost wakeup

Look at the gap between `release` and `sleep()`. What if `sem_up` runs exactly there?

| Time | Process A (`sem_down`) | Process B (`sem_up`) |
|---|---|---|
| 1 | value is 0, `sleep_prepare(s)` | |
| 2 | `release(&s->lock)` | |
| 3 | | `acquire`, value = 1, `wakeup(s)`, `release` |
| 4 | `sleep()` | |

If A then went to sleep, it could sleep forever, although the value is 1. This is the classic **lost wakeup**.

This kernel solves it with the two steps. In `kernel/proc.c`:

```c
void
sleep(void)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  if (p->chan != 0) {       // still waiting?
    p->state = SLEEPING;
    sched();
  }
  release(&p->lock);
}
```

`wakeup(s)` at time 3 finds A's `p->chan == s` and sets it to 0. So at time 4, `sleep()` sees 0 and returns at once. That is what the comment "returns at once if sem_up came in between" means.

## 8. sys_sem_up

```c
acquire(&s->lock);
s->value++;
wakeup(s);
release(&s->lock);
return 0;
```

Add one unit, wake everyone who sleeps on this address, and release. It does not matter how many wake up, because each one re-checks the value in its `while`.

## 9. semtest step by step

**Test 1: order.** Semaphore 0 starts at 0.

| Step | Child | Parent |
|---|---|---|
| 1 | `sem_down(0)`: value is 0, sleeps | `pause(10)` |
| 2 | asleep | prints "parent: done" |
| 3 | asleep | `sem_up(0)`: value 1, wakes the child |
| 4 | takes the unit, prints "child: I ran" | `wait(0)` |

**Test 2: ping-pong.** Semaphore 1 (ping's turn) starts at 1, and semaphore 2 (pong's turn) at 0.

| Step | Ping (parent) | Pong (child) | sem 1 | sem 2 |
|---|---|---|---|---|
| start | | | 1 | 0 |
| 1 | `sem_down(1)`, prints ping 1 | waits on 2 | 0 | 0 |
| 2 | `sem_up(2)` | | 0 | 1 |
| 3 | waits on 1 | `sem_down(2)`, prints pong 1 | 0 | 0 |
| 4 | | `sem_up(1)` | 1 | 0 |

Each one hands the turn to the other, so they alternate strictly.

## 10. mix and the critical section

```c
for (int k = 0; k < 5; k++) {
  if (uselock)
    sem_down(3);
  for (char *p = s; *p; p++)
    write(1, p, 1);        // one letter, one system call
  if (uselock)
    sem_up(3);
}
```

One line has 30 letters and a new line, so it costs **31** system calls. Without the lock, the other process can write between any two of them, and `make qemu` uses 3 CPUs, so they really run together.

With `mix lock`, semaphore 3 starts at 1, so it is a lock. `sem_down(3)` and `sem_up(3)` make the whole line a critical section.

## 11. Sleeping with a spinlock

What if `sem_down` did not release the spinlock before sleeping?

1. A sleeps, holding `s->lock`.
2. B calls `sem_up`, and `acquire(&s->lock)` spins forever, with interrupts off.
3. Nobody can ever wake A.

I tried it in a separate copy of the tree, and the kernel refused even earlier (Figure 3.5):

```
$ semtest
panic: sched locks
```

The check is in `sched()`, in `kernel/proc.c`:

```c
if (mycpu()->noff != 1)
  panic("sched locks");
```

`noff` counts how many spinlocks this CPU holds. When a process gives up the CPU it must hold exactly one, its own `p->lock`. With `s->lock` as well, the count is 2, and the kernel panics.

## 12. sem_trydown

The optional call, number 27, never sleeps:

```c
acquire(&s->lock);
if (s->value == 0) {
  release(&s->lock);
  return -1;
}
s->value--;
release(&s->lock);
return 0;
```

Note the `release` before `return -1`. Returning with the lock held would block this semaphore forever.

`semtry` starts semaphore 4 at 1, so it shows both answers:

```
first try: 0        took the only unit
second try: -1      nothing left, and it did not sleep
after sem_up: 0     one unit is back
```

## 13. Test yourself

1. Which register holds the number of the system call?
2. Why did `sysinfo` have to move?
3. Why does `sys_sem_init` check `id` and `value`?
4. Why must the spinlock be released before `sleep()`?
5. What is a lost wakeup, and how does this kernel avoid it?
6. Why is the test `while (s->value == 0)` and not `if`?
7. How many system calls does one line of `mix` cost?
8. What does "panic: sched locks" mean?

**Answers.**

1. `a7`.
2. The sheet demands 24 for `sem_init`, and `sysinfo` was using it.
3. A user program could pass any number and write outside the table.
4. `sem_up` needs that lock to wake the sleeper.
5. A wakeup that comes just before the sleep. `sleep_prepare` registers the channel first, `wakeup` clears it, and `sleep()` then returns at once.
6. `wakeup` wakes everyone, but only one unit was added, so the others must test again.
7. 31: 30 letters and the new line.
8. A process tried to give up the CPU while holding a spinlock other than its own `p->lock`.
