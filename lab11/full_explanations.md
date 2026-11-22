# Lab 11 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 11: The Race and the Semaphore.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It explains what a thread is, why `counter++` breaks, every line of the programs, and the three ways to use one semaphore. It is written for someone who has never used threads. The figure numbers are the same as in the README and in the Word report.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [What a thread is](#2-what-a-thread-is)
3. [race.c line by line](#3-racec-line-by-line)
4. [How an addition gets lost](#4-how-an-addition-gets-lost)
5. [Why race is sometimes right](#5-why-race-is-sometimes-right)
6. [The mutex](#6-the-mutex)
7. [The semaphore](#7-the-semaphore)
8. [order.c: a semaphore as a signal](#8-orderc-a-semaphore-as-a-signal)
9. [parking.c: a semaphore as a counter](#9-parkingc-a-semaphore-as-a-counter)
10. [atomic.c: no lock at all](#10-atomicc-no-lock-at-all)
11. [The commands](#11-the-commands)
12. [Test yourself](#12-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| process | a running program, with its own memory |
| thread | one line of execution inside a process; threads of a process share its memory |
| shared variable | a variable that two threads can both read and write, like a global |
| race condition | the result depends on which thread happens to run first |
| critical section | code that only one thread may run at a time |
| mutex | a lock with two states: free or taken |
| semaphore | a number that never goes below 0, with the operations wait and post |
| atomic | done as one step that nobody can cut into |
| scheduler | the part of the kernel that decides who runs, when, and on which CPU |

## 2. What a thread is

A normal program does one thing at a time. With `pthread_create` it starts a second line of execution, in the same memory. On a machine with several CPUs, both really run at the same moment.

Each thread has its **own** stack, so local variables like `i` are private. **Global** variables like `counter` exist once, so both threads touch the same one. That is where the trouble comes from.

## 3. race.c line by line

```c
#include <stdio.h>      // printf
#include <pthread.h>    // pthread_create, pthread_join
#define N 1000000       // N is replaced by 1000000 before compiling
long counter = 0;       // a global: one copy, shared by both threads
```

```c
void *add(void *arg) {
    for (int i = 0; i < N; i++)
        counter++;
    return NULL;
}
```

This is the code each thread runs. A thread function must take one `void *` and return a `void *`. This one needs neither, so it ignores `arg` and returns `NULL`. `i` is local, so each thread has its own.

```c
pthread_t a, b;                          // two variables that name the threads
pthread_create(&a, NULL, add, NULL);     // start a thread that runs add()
pthread_create(&b, NULL, add, NULL);     // start a second one
pthread_join(a, NULL);                   // wait until a has finished
pthread_join(b, NULL);                   // wait until b has finished
```

The four arguments of `pthread_create` are: where to store the thread's name, the options (`NULL` = default), the function to run, and the argument for that function.

Without `pthread_join`, `main` would return at once and kill both threads before they finish.

## 4. How an addition gets lost

`counter++` looks like one step. The CPU does three:

1. **load**: copy `counter` from memory into a register
2. **add**: add 1 to the register
3. **store**: copy the register back into memory

Each thread has its own registers. Here is one bad order, starting from 5:

| Time | Thread A | Thread B | `counter` in memory |
|---|---|---|---|
| 1 | load (A has 5) | | 5 |
| 2 | | load (B has 5) | 5 |
| 3 | add (A has 6) | | 5 |
| 4 | | add (B has 6) | 5 |
| 5 | store | | 6 |
| 6 | | store | **6** |

Two additions ran, but the counter grew by one. One addition is lost.

## 5. Why race is sometimes right

The sheet says the answer is never 2 000 000. On my VM, 23 of 100 runs gave exactly that (Figure 1.4).

A loop of one million additions takes only a few milliseconds. Starting a thread takes time too, so thread A can finish before thread B really begins. If they never overlap, nothing can collide.

The proof: with 100 million additions the threads must overlap, and all six runs were wrong, around 130 million instead of 200 million.

This is the most dangerous part of a race: **a wrong program can look right** when you test it.

## 6. The mutex

```c
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;   // a free lock
...
pthread_mutex_lock(&lock);      // free: take it. taken: wait
counter++;                      // the critical section
pthread_mutex_unlock(&lock);    // free it, let a waiting thread in
```

Now the three steps of `counter++` cannot be cut, because the other thread stops at `pthread_mutex_lock`. The answer is always 2 000 000.

The price: each addition now does extra work, and a thread that finds the lock taken has to wait, which goes through the kernel. That is why `mutex` is slower than `race` in Figure 2.5.

## 7. The semaphore

A semaphore is a number with two operations:

- `sem_wait(&s)`: if the number is above 0, subtract 1 and go on. If it is 0, **wait**.
- `sem_post(&s)`: add 1, and wake one thread that waits.

```c
sem_t s;
sem_init(&s, 0, 1);     // the 0 = shared between threads; the 1 = start value
sem_wait(&s);           // 1 -> 0, I go in
counter++;
sem_post(&s);           // 0 -> 1, the next one may go in
sem_destroy(&s);        // done with it
```

**The start value decides the job:**

| Start value | Job | Meaning |
|---|---|---|
| 1 | lock | one thread may pass at a time |
| 0 | signal | nobody passes until someone calls `sem_post` |
| N | counter | N threads may be inside at the same time |

With start value 2 in `sem.c`, both threads pass together, so the counter is unprotected again (Figure 2.8). A lock must start at exactly 1.

## 8. order.c: a semaphore as a signal

```c
void *writer(void *arg) {
    sleep(1);                    // pretend the work takes a while
    strcpy(message, "hello");    // write the message
    printf("A: message written\n");
    sem_post(&ready);            // 0 -> 1: wake up B
    return NULL;
}

void *reader(void *arg) {
    printf("B: waiting for the message...\n");
    sem_wait(&ready);            // waits while the value is 0
    printf("B: I read \"%s\"\n", message);
    return NULL;
}
```

`main` starts B first, on purpose. B prints its first line and stops at `sem_wait`, because the value is 0. One second later, A writes and posts. B wakes up and reads "hello".

With start value 1 (Figure 3.3), `sem_wait` passes at once. B reads `message` before A wrote it. A global array starts as all zeros, so B prints an empty string.

## 9. parking.c: a semaphore as a counter

The semaphore `places` starts at 3 and counts the free places.

```c
sem_wait(&places);              // take a place, or wait
pthread_mutex_lock(&lock);
inside++;
printf("car %ld enters  (inside: %d)\n", id, inside);
pthread_mutex_unlock(&lock);
sleep(1);                       // parked
```

Cars 1, 2 and 3 take the value from 3 to 0. Car 4 finds 0 and waits. When a car leaves, `sem_post(&places)` gives a place back and one waiting car goes in.

The **mutex** is a separate tool with a separate job: it keeps `inside++` and its `printf` together, because `inside` is a shared variable too.

`(void *)(i + 1)` sends the car's number to the thread through the `void *` argument, and `long id = (long)arg;` turns it back into a number.

With `PLACES` 1 the semaphore is a lock. Each car stays one second, one after the other, so the run takes about 6 seconds (Figure 4.4).

## 10. atomic.c: no lock at all

```c
atomic_long counter = 0;
atomic_fetch_add(&counter, 1);
```

The CPU has special instructions that do load, add and store as **one** step. No thread can cut in. This is fast (0.053 s here) but works only for simple operations on one variable. For anything bigger you still need a lock.

## 11. The commands

| Command | What it does |
|---|---|
| `gcc -Wall -pthread -o race race.c` | compiles `race.c` into the program `race`; `-Wall` shows warnings, `-pthread` adds the threads library |
| `./race` | runs the program from the current folder |
| `time ./race` | runs it and prints how long it took |
| `for i in 1 2 3; do ./race; done` | runs it three times |
| `seq 100` | prints the numbers 1 to 100 |
| `grep -c "text"` | counts the lines that contain the text |
| `grep -o "text"` | prints only the matching part of each line |
| `sort` then `uniq -c` | sorts the lines, then counts the equal ones |
| `sed 's/old/new/' a.c > b.c` | makes a copy of `a.c` with `old` replaced by `new`; in the new text, `\&` means a real `&` |

## 12. Test yourself

1. What are the three steps of `counter++`?
2. Why can a wrong program print the right answer?
3. What is a critical section?
4. What start value makes a semaphore a lock? A signal?
5. What happens with `sem_init(&s, 0, 2)` in `sem.c`?
6. Why does `main` call `pthread_join`?
7. In `parking.c`, why is there a mutex if there is already a semaphore?
8. Why is `-O2` not allowed in this lab?

**Answers.**

1. Load, add, store.
2. If the threads do not overlap, nothing collides. The bug is still there.
3. Code that only one thread may run at a time.
4. 1 for a lock, 0 for a signal.
5. Two threads can be inside, so additions are lost.
6. To wait for the threads. Otherwise `main` ends and kills them.
7. The semaphore limits the cars to 3. The mutex protects the shared `inside` and its `printf`.
8. The optimizer would replace the loop with one addition, and the race would disappear from view.
