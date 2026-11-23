# Lab 17 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 17: Threads Share Memory, and Race.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It explains what a thread is, every line of `fire.c`, how an update gets lost, and every command. It is written for someone who has never used threads. The figure numbers are the same as in the README and in the Word report.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [Threads, explained with a shared notebook](#2-threads-explained-with-a-shared-notebook)
3. [fire.c line by line](#3-firec-line-by-line)
4. [How an update gets lost](#4-how-an-update-gets-lost)
5. [Reading the output](#5-reading-the-output)
6. [Why some runs are exactly right](#6-why-some-runs-are-exactly-right)
7. [fire4.c: what changed](#7-fire4c-what-changed)
8. [lost.sh line by line](#8-lostsh-line-by-line)
9. [The idea that fixes it](#9-the-idea-that-fixes-it)
10. [The commands](#10-the-commands)
11. [Test yourself](#11-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| process | a running program, with its own memory |
| thread | one line of execution inside a process |
| global variable | a variable outside any function; there is one copy of it |
| local variable | a variable inside a function; each thread has its own copy |
| race | the result depends on which thread happens to run when |
| lost update | an addition that ran, but was overwritten by another thread |
| atomic | done as one step that nobody can cut into |
| scheduler | the part of the kernel that decides who runs, when, and on which CPU |
| lock (mutex) | lets only one thread at a time run a piece of code |

## 2. Threads, explained with a shared notebook

Two people work from one notebook, where a single number is written. Each must add 1 to it. To do that, a person looks at the number, computes the new one in their head, then writes it down.

If they take turns, everything is fine. If both look at the number at the same moment, both see 5, both compute 6, and both write 6. Two people worked, but the number grew by 1.

The notebook is the **shared memory**. The two people are the **threads**. The number in each person's head is a **CPU register**.

## 3. fire.c line by line

```c
#include <stdio.h>
#include <pthread.h>
```

`stdio.h` gives us `printf`. `pthread.h` gives us the thread functions.

```c
#define N 1000000
long counter = 0;
```

`N` is replaced by 1000000 before compiling. `counter` is **global**, so there is one copy and both threads use it.

```c
void *work(void *arg) {
```

This is the code each thread runs. A thread function must take one `void *` and return a `void *`. A `void *` is an address of "anything".

```c
    long id = (long)arg;
```

We passed a small number disguised as an address. This line turns it back into a number. `id` is **local**, so each thread has its own.

```c
    printf("thread %ld sees counter = %ld\n", id, counter);
```

Prints which thread this is, and the value of the shared counter at this moment. `%ld` prints a `long`.

```c
    for (int i = 0; i < N; i++)
        counter++;
```

Adds 1 to the shared counter, one million times. `i` is local. `counter` is not, and this line is the bug.

```c
    return NULL;
}
```

The thread ends. It has nothing to give back.

```c
int main(void) {
    pthread_t a, b;
```

`pthread_t` is the type of a thread's identifier. `a` and `b` will hold the two threads.

```c
    pthread_create(&a, NULL, work, (void *)1);
    pthread_create(&b, NULL, work, (void *)2);
```

Each call starts a new thread. The four arguments are: where to store its identifier, the options (`NULL` means the defaults), the function it runs, and the argument given to that function.

```c
    pthread_join(a, NULL);
    pthread_join(b, NULL);
```

`main` waits here until each thread finishes. Without this, `main` could print the result, or even end the whole process, while the threads are still working.

```c
    printf("expected: %d\n", 2 * N);
    printf("counter:  %ld   (lost updates: the race)\n", counter);
    return 0;
}
```

Prints what the result should be and what it really is. The text "lost updates" is printed every time, even in a lucky run.

## 4. How an update gets lost

Compiled as in this lab, without optimization, `counter++` becomes three separate steps:

1. **Read**: copy `counter` from memory into a register.
2. **Add**: add 1 to the register.
3. **Write**: copy the register back to memory.

Each thread has its own registers, so here is what can happen when the counter is 5:

| Time | Thread 1 | Thread 2 | counter in memory |
|---|---|---|---|
| 1 | reads 5 | | 5 |
| 2 | | reads 5 | 5 |
| 3 | adds 1 (has 6) | | 5 |
| 4 | | adds 1 (has 6) | 5 |
| 5 | writes 6 | | 6 |
| 6 | | writes 6 | 6 |

Two additions ran, and the counter grew by 1. One update is lost. A worse case is possible too: a thread can read a value, wait while the other thread adds many times, then write its old value back and erase all that work.

## 5. Reading the output

```text
thread 1 sees counter = 0
thread 2 sees counter = 598641
expected: 2000000
counter:  1925698   (lost updates: the race)
```

- Thread 1 starts first and sees 0.
- When thread 2 prints, thread 1 has already added about 600 thousand. Thread 2 never wrote that number, so this is the proof of **shared memory**.
- The result is 74302 too small. That many updates were lost.

The order of the first two lines can change. In some runs thread 2 prints first, because the scheduler decides who goes first.

## 6. Why some runs are exactly right

One million additions take only a few milliseconds. Starting a thread and calling `printf` also take time. So sometimes one thread finishes its whole loop before the other one starts its loop. Then they never touch the counter at the same time, and the result is exactly 2000000.

That does **not** mean the program is correct. It means this run was lucky. This is why race bugs are so hard to find: a program can pass a test many times and still be wrong.

## 7. fire4.c: what changed

Only what was needed for four threads (Figure 2.1):

```c
pthread_t a, b, c, d;
pthread_create(&c, NULL, work, (void *)3);
pthread_create(&d, NULL, work, (void *)4);
pthread_join(c, NULL);
pthread_join(d, NULL);
printf("expected: %d\n", 4 * N);
```

Each new thread must be started and also waited for. The expected value becomes 4 million.

The measured loss went from about 20% to about 46%. With four threads working at once, a value that a thread has just read is much more likely to be changed by another thread before it writes.

## 8. lost.sh line by line

```bash
prog=$1
expected=$2
```

`$1` and `$2` are the first two words after the script's name. In `./lost.sh ./fire 2000000`, `prog` is `./fire` and `expected` is `2000000`.

```bash
total=0
min=$expected
max=0
```

Starting values: the sum of the losses, the smallest loss, the largest loss.

```bash
for i in $(seq 20); do
```

`seq 20` prints the numbers 1 to 20, so the loop runs 20 times.

```bash
    value=$($prog | grep '^counter:' | awk '{print $2}')
```

Runs the program. `grep` keeps the line that starts with `counter:`. `awk` prints its second word, the number. `$( ... )` stores that output in `value`.

```bash
    lost=$((expected - value))
    total=$((total + lost))
```

`$(( ... ))` does arithmetic in bash. The loss of this run is added to the total.

```bash
    if [ "$lost" -lt "$min" ]; then min=$lost; fi
    if [ "$lost" -gt "$max" ]; then max=$lost; fi
```

`-lt` means "less than" and `-gt` means "greater than". These two lines remember the smallest and the largest loss.

```bash
average=$((total / 20))
percent=$((average * 100 / expected))
echo "$prog: expected $expected, lost on average $average ($percent%), least $min, most $max"
```

Computes the average and the percentage, then prints one line. Bash only does whole-number arithmetic, so the percentage is rounded down.

## 9. The idea that fixes it

The three steps must not be mixed between threads. A **lock** (mutex) makes that happen:

```text
take the lock
counter++        only one thread can be here
release the lock
```

If a thread finds the lock taken, it waits. So the read, the add and the write of one thread finish before another thread can start its own. The sheet says this is built next week, so this lab only proposes it.

## 10. The commands

| Command | What it does |
|---|---|
| `gcc -Wall -Wextra -pthread -o fire fire.c` | compiles `fire.c` into a program called `fire` |
| `-Wall -Wextra` | turn on the compiler's warnings |
| `-pthread` | builds with the thread library |
| `-o fire` | names the output file |
| `./fire` | runs the program from the current folder |
| `nproc` | prints the number of CPUs |
| `cat -n fire.c` | prints the file with line numbers |
| `diff fire.c fire4.c` | shows the lines that differ between two files |

In the `diff` output, `<` is a line of the first file and `>` is a line of the second file.

## 11. Test yourself

1. Why is `counter` shared, but `i` is not?
2. What are the three steps of `counter++`?
3. Thread 2 prints `counter = 598641`. What does that prove?
4. Why can the result be different at every run?
5. Why is a run that prints exactly 2000000 not proof that the program is correct?
6. What would happen if `main` did not call `pthread_join`?
7. Why did four threads lose more than two?
8. How would a lock stop the lost updates?

**Answers.** (1) `counter` is global, so there is one copy. `i` is local, so each thread has its own. (2) Read, add 1, write. (3) The threads share memory, because thread 2 sees a value it did not write. (4) The scheduler times the threads differently each run. (5) The loops may simply not have overlapped that time. (6) `main` could print too early or end the process while the threads still work. (7) More threads change the variable at once, so more collisions happen. (8) Only one thread at a time could do all three steps.
