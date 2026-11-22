# Lab 12 – Producer, Consumer and the Other Tools

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Track A: the real OS, from the outside (Linux).

This page explains everything that was done in the lab, step by step: every command, what it printed, and why. The figure numbers are the same as in the Word report, so the two can be read side by side.

## What is in this folder

| File | What it is |
|---|---|
| `Lab12_EftodeAndrei_FAF-242.docx` | The report: title page, 22 screenshots, the answers to every Observe box |
| `prodcons.c` | Producer and consumer around a buffer of 4 slots |
| `condvar.c` | The order problem of Lab 11, solved with a condition variable |
| `readers.c` | Readers and writers with two semaphores |
| `deadlock.c` | Two threads and two mutexes, **the fixed version** (the original is in the git history) |
| `report.md` | The answers the sheet asks to write "in report.md" |
| `full_explanations.md` | The slow version of this page: every line of code, worked examples, a self-test |
| `screenshots/` | The terminal screenshots (`fig-1-1.png` is Figure 1.1, and so on) |

## How to run it

```bash
mkdir -p ~/os-lab12 && cd ~/os-lab12     # put the .c files here
gcc -Wall -pthread -o prodcons prodcons.c && ./prodcons
gcc -Wall -pthread -o condvar condvar.c   && ./condvar
gcc -Wall -pthread -o readers readers.c   && ./readers
gcc -Wall -pthread -o deadlock deadlock.c
timeout 15 ./deadlock; echo "exit code: $?"
```

## Words used on this page

- **Buffer**: a small piece of memory where items wait between the one who makes them and the one who uses them.
- **Condition variable**: a place where a thread sleeps until another thread says "something changed".
- **Deadlock**: threads wait for each other in a circle, so none of them can ever continue.
- **Starvation**: a thread could run, but others always go first, so it waits forever.
- **Exit code**: the number a program returns when it ends. `$?` holds the last one. 0 means success, and `timeout` returns 124 when it had to kill the program.

## Before the lab

![Figure 0.1 – The working folder with the four programs of the sheet](screenshots/fig-0-1.png)

## Part 1 – Producer and consumer

A fast producer puts 8 items into a buffer of 4 slots, and a slow consumer takes them out. Three tools work together: `empty` counts the free slots (starts at 4), `full` counts the filled slots (starts at 0), and a mutex protects the buffer itself.

![Figure 1.1 – prodcons.c, first half](screenshots/fig-1-1.png)

![Figure 1.2 – prodcons.c, second half](screenshots/fig-1-2.png)

![Figure 1.3 – The run: "in buffer" never goes above 4](screenshots/fig-1-3.png)

The order of the lines is not exactly the one on the sheet, because the scheduler decides who runs first. The rule still holds: never more than 4 items in the buffer.

For Observe 3, this `sed` swaps the two first lines of the producer, in a copy of the file:

```bash
sed '/sem_wait(&empty)/{N;s/\(.*\)\n\(.*\)/\2\n\1/}' prodcons.c > prodcons_bad.c
```

![Figure 1.4 – The mutex locked before sem_wait(&empty). Four items, then stuck until Ctrl-C](screenshots/fig-1-4.png)

**Observe.**

1. `sem_wait(&empty)` stops the producer when no slot is free.
2. The consumer waits at the start, in `sem_wait(&full)`, because `full` starts at 0.
3. The producer sleeps in `sem_wait(&empty)` while holding the mutex. The consumer needs that mutex to take an item, and only the consumer can post `empty`. Each waits for the other forever.

## Part 2 – The condition variable

The state is a normal variable, `ready`, protected by a mutex. The condition variable is only the place where B sleeps.

![Figure 2.1 – condvar.c, first half](screenshots/fig-2-1.png)

![Figure 2.2 – condvar.c, second half](screenshots/fig-2-2.png)

![Figure 2.3 – The expected three lines](screenshots/fig-2-3.png)

The `sleep(1)` took more than one second here. `sleep` promises **at least** that long, and this VM is sometimes slow to wake up. Four more runs:

![Figure 2.4 – Four more runs: always the right message, in 1.1 to 2.4 s](screenshots/fig-2-4.png)

![Figure 2.5 – With if (!ready) it still works here](screenshots/fig-2-5.png)

**Observe.**

1. The variable `ready` remembers it. The condition variable is only the place where B sleeps.
2. Yes, because there is one writer and one reader, and B was woken only after `ready = 1`.
3. `pthread_cond_wait` may return with no signal (a spurious wakeup), and with several readers another thread can run first and change `ready` back. So the condition must be tested again.

## Part 3 – Readers and writers

Many readers may read together, but a writer must be alone. `room` is 1 when the room is free, and `rlock` protects the counter `readers`. The first reader in locks the room, and the last one out unlocks it.

![Figure 3.1 – readers.c, first half](screenshots/fig-3-1.png)

![Figure 3.2 – readers.c, second half](screenshots/fig-3-2.png)

![Figure 3.3 – Three runs: three readers inside together, each writer alone](screenshots/fig-3-3.png)

**Observe.**

1. `if (++readers == 1) sem_wait(&room);` and `if (--readers == 0) sem_post(&room);`.
2. `++readers` is three steps. Without `rlock`, two readers could both see 0 and both wait for the room, or an update could be lost and the count would never return to 0.
3. Writer 2 can wait forever: while one reader is inside, new readers walk in and `room` is never given back. This is starvation.

## Part 4 – Build a deadlock, then remove it

Two threads, two mutexes, taken in opposite order.

![Figure 4.1 – deadlock.c, thread one: A, then B](screenshots/fig-4-1.png)

![Figure 4.2 – The original thread two: B, then A, the opposite order](screenshots/fig-4-2.png)

```bash
timeout 5 ./deadlock; echo "exit code: $?"
```

![Figure 4.3 – The deadlock: both threads print one line and wait, until timeout kills them (exit code 124)](screenshots/fig-4-3.png)

For Observe 2, `grep -v` copies the file without the two `sleep(1)` lines. The loops run it many times and count the exit codes:

```bash
grep -v "sleep(1)" deadlock_orig.c > nosleep.c
for i in $(seq 10); do timeout 1 ./nosleep > /dev/null; echo -n "$? "; done; echo
for i in $(seq 200); do timeout 0.3 ./nosleep > /dev/null; echo $?; done | sort | uniq -c
```

![Figure 4.4 – Without sleep(1): 10 of 10 runs finish, but 11 of 200 hang](screenshots/fig-4-4.png)

The fix: thread two also takes A first.

![Figure 4.5 – The fix: thread two also takes A first, and unlocks in the reverse order](screenshots/fig-4-5.png)

![Figure 4.6 – The fixed program prints every line. This one run was cut by the 5-second timeout; the next five end with 0](screenshots/fig-4-6.png)

**Why a fixed program got exit code 124 once.** It was not a deadlock. The program needs two `sleep(1)`, and in this VM one `sleep(1)` really takes 1.0 to 2.6 s (Figure 4.7). So the whole program needs 2.0 to 6.2 s, and that run went over the 5 seconds of `timeout`. With a longer timeout, it ended with 0 in 10 runs out of 10 (Figure 4.8).

![Figure 4.7 – In this VM sleep(1) really takes 1.0 to 2.6 s](screenshots/fig-4-7.png)

![Figure 4.8 – With a longer timeout the fixed program ends with exit code 0 in 10 runs out of 10](screenshots/fig-4-8.png)

**Observe.**

1. Thread one holds A and waits for B. Thread two holds B and waits for A. The circle: one → B → two → A → one.
2. No, most runs finish. That is worse: the bug passes the tests and appears only sometimes.
3. If everyone takes A before B, nobody can hold B while waiting for A, so the circle cannot close.

## Conclusions

![Figure 5.1 – The folder at the end, with the variants I made for the Observe questions](screenshots/fig-5-1.png)

Semaphores count, a mutex protects, and a condition variable is a place to sleep. The order of the calls matters as much as the calls: one swapped line in the producer hung the program. A deadlock needs a circle of waiting, so one global lock order removes it.
