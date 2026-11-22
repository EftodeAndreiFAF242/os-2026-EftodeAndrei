# Lab 12 – Full explanations

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027. Lab 12: Producer, Consumer and the Other Tools.

[`README.md`](README.md) tells what was done in the lab. This file goes slower. It explains the two classic problems, the condition variable and the deadlock, with every important line of code. Read Lab 11 first: threads, the mutex and the semaphore are explained there.

## Contents

1. [Words you need first](#1-words-you-need-first)
2. [Producer and consumer](#2-producer-and-consumer)
3. [The circular buffer](#3-the-circular-buffer)
4. [Why the order of two lines matters](#4-why-the-order-of-two-lines-matters)
5. [The condition variable](#5-the-condition-variable)
6. [Why while and not if](#6-why-while-and-not-if)
7. [Readers and writers](#7-readers-and-writers)
8. [Deadlock](#8-deadlock)
9. [Why the fixed program timed out once](#9-why-the-fixed-program-timed-out-once)
10. [The commands](#10-the-commands)
11. [Test yourself](#11-test-yourself)

## 1. Words you need first

| Word | Meaning |
|---|---|
| producer | the thread that makes items |
| consumer | the thread that uses them |
| buffer | memory where items wait between the two |
| condition variable | a place to sleep until someone says "something changed" |
| spurious wakeup | a thread wakes up from a wait although nobody signaled it |
| deadlock | threads wait for each other in a circle, forever |
| starvation | a thread could run, but others always go first |
| exit code | the number a program returns at the end; 0 = success |

## 2. Producer and consumer

The producer is fast and the consumer is slow, so a buffer sits between them. Two things can go wrong:

- the buffer is **full**: the producer must wait;
- the buffer is **empty**: the consumer must wait.

Three tools solve it, each with one job:

| Tool | Start value | Job |
|---|---|---|
| `empty` (semaphore) | 4 | counts the free slots; the producer takes one |
| `full` (semaphore) | 0 | counts the filled slots; the consumer takes one |
| `lock` (mutex) | free | only one thread touches the buffer at a time |

```c
void *producer(void *arg) {
    for (int item = 1; item <= ITEMS; item++) {
        sem_wait(&empty);               // wait for a free slot
        pthread_mutex_lock(&lock);      // enter the buffer
        buf[in] = item;                 // put the item
        in = (in + 1) % SIZE;           // next place, wraps around
        count++;                        // only for printing
        printf("put  %d   (in buffer: %d)\n", item, count);
        pthread_mutex_unlock(&lock);    // leave the buffer
        sem_post(&full);                // one more filled slot
    }
    return NULL;
}
```

The consumer is the mirror image: it waits on `full`, takes the item, and posts `empty`. After each item it sleeps 0.3 s (`usleep(300000)`, in microseconds), so it is slow.

The sheet says every pipe in Unix works this way inside the kernel. When a pipe is full, the writer sleeps. When it is empty, the reader sleeps.

## 3. The circular buffer

`buf` has 4 places, numbered 0 to 3. `in` is where the next item goes, and `out` is where the next item comes from. The line

```c
in = (in + 1) % SIZE;
```

adds 1 and wraps around: `%` is the remainder of the division, so the values go 0, 1, 2, 3, 0, 1, 2, 3...

| Item | `in` before | Place used | `in` after |
|---|---|---|---|
| 1 | 0 | 0 | 1 |
| 2 | 1 | 1 | 2 |
| 3 | 2 | 2 | 3 |
| 4 | 3 | 3 | 0 |
| 5 | 0 | 0 | 1 |

Item 5 goes in place 0 only after the consumer took item 1. The semaphore `empty` guarantees it.

## 4. Why the order of two lines matters

The correct code is:

```c
sem_wait(&empty);
pthread_mutex_lock(&lock);
```

In Observe 3 the two lines were swapped:

```c
pthread_mutex_lock(&lock);
sem_wait(&empty);
```

What happens when the buffer is full (Figure 1.4):

1. The producer takes the mutex.
2. `empty` is 0, so the producer sleeps, **still holding the mutex**.
3. The consumer wants to take an item, so it calls `pthread_mutex_lock` and waits.
4. Only the consumer could post `empty`, and it can never get in.

Both wait forever. **Rule: never go to sleep while holding a lock that the one who must wake you needs.**

## 5. The condition variable

A semaphore **remembers** a signal in its number. A condition variable remembers **nothing**. So the state is kept in a normal variable.

```c
int ready = 0;                                       // the state
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;    // protects ready
pthread_cond_t changed = PTHREAD_COND_INITIALIZER;   // the place to sleep
```

The writer changes the state and signals:

```c
pthread_mutex_lock(&lock);
strcpy(message, "hello");
ready = 1;
pthread_cond_signal(&changed);      // wake one waiter
pthread_mutex_unlock(&lock);
```

The reader tests the state and sleeps if needed:

```c
pthread_mutex_lock(&lock);
while (!ready) {
    pthread_cond_wait(&changed, &lock);
}
printf("B: I read \"%s\"\n", message);
pthread_mutex_unlock(&lock);
```

`pthread_cond_wait` does three things:

1. it **releases** the mutex, so the writer can get in;
2. it **sleeps** until a signal;
3. it **takes the mutex again** before it returns.

## 6. Why while and not if

With `if (!ready)` the program still worked (Figure 2.5), because there is one writer and one reader. It is still wrong in general, for two reasons:

1. **Spurious wakeup.** The POSIX standard allows `pthread_cond_wait` to return with no signal at all. With `if`, B would read a message that is not there.
2. **Someone else got there first.** With several readers, one can wake up and consume the item, and set `ready` back to 0, before the next one runs.

With `while`, the thread tests the condition again after every wakeup. If it is still false, it goes back to sleep.

## 7. Readers and writers

Reading does not change anything, so many readers can read together. A writer changes the data, so it must be alone.

```c
sem_wait(&rlock);
if (++readers == 1)     // I am the first reader in
    sem_wait(&room);    // take the room for all the readers
sem_post(&rlock);

// ... read ...

sem_wait(&rlock);
if (--readers == 0)     // I am the last reader out
    sem_post(&room);    // give the room back
sem_post(&rlock);
```

Think of a meeting room. The first reader locks the door against writers. The others walk in. The last one out unlocks it. A writer just does `sem_wait(&room)`, so it is alone inside.

**Why `rlock`?** `++readers` is load, add, store, the same problem as `counter++`. Without a lock, two readers could both see 0 and both become "the first". The second would then sleep on `room`, although readers are inside. Or an update could be lost, the count would never reach 0, and the writers would wait forever.

**Starvation.** If readers keep arriving, `readers` never reaches 0, and the room is never given back. Writer 2 waits forever even though nothing is broken.

## 8. Deadlock

```c
// thread one            // thread two (original)
lock(A);                 lock(B);
sleep(1);                sleep(1);
lock(B);   // waits      lock(A);   // waits
```

After the `sleep`, one holds A and wants B, and two holds B and wants A:

```
one --holds--> A --wanted by--> two
 ^                               |
 |                             holds
 +------- wanted by <--- B <-----+
```

A deadlock needs **four conditions together**:

1. **Mutual exclusion**: a lock has only one owner.
2. **Hold and wait**: a thread keeps one lock while it waits for another.
3. **No preemption**: nobody can take a lock away from a thread.
4. **Circular wait**: the waiting forms a circle.

Remove any one and the deadlock is impossible. **The fix removes the circle**: both threads take A first, then B. A thread that holds B has already got A, so it can never wait for A.

**Without the `sleep`**, the threads rarely collide: 10 of 10 runs finished, and only 11 of 200 hung (Figure 4.4). That is worse. The bug passes every quick test and then shows up rarely, on someone else's machine.

## 9. Why the fixed program timed out once

In Figure 4.6 the fixed program got exit code 124 once. It was not a deadlock: it had printed three of its lines and was still in its second `sleep(1)`.

`sleep(1)` promises **at least** one second. In this VM it took 1.0 to 2.6 s (Figure 4.7), so the whole program took 2.0 to 6.2 s (Figure 4.8). Sometimes that is more than the 5 seconds of `timeout`. With a longer timeout, all 10 runs ended with exit code 0.

## 10. The commands

| Command | What it does |
|---|---|
| `timeout 5 ./deadlock` | runs the program and kills it after 5 seconds |
| `echo "exit code: $?"` | prints the exit code of the last command |
| Ctrl-C | stops the program that is running in the terminal |
| `grep -v "text" a.c > b.c` | copies `a.c` without the lines that contain the text |
| `sed -n '1,30p' a.c` | prints only lines 1 to 30 |
| `sed -i '...' a.c` | changes the file in place |
| `diff a.c b.c` | prints the lines that differ between two files |
| `> /dev/null` | throws the output away |

## 11. Test yourself

1. What does each of `empty`, `full` and `lock` do?
2. Why must `sem_wait(&empty)` come before the mutex?
3. What does `pthread_cond_wait` do, in three steps?
4. Why do we write `while (!ready)`?
5. Why does the counter `readers` need its own lock?
6. What is starvation?
7. What are the four conditions of a deadlock?
8. Why does the same lock order remove the deadlock?

**Answers.**

1. `empty` counts free slots, `full` counts filled slots, `lock` protects the buffer.
2. Otherwise the producer sleeps holding the mutex, and the consumer can never get in to free a slot.
3. It releases the mutex, sleeps, and takes the mutex again.
4. A thread can wake up with the condition still false, so it must test again.
5. `++readers` is three steps and can lose an update, like `counter++`.
6. A thread that could run but always loses to others, so it waits forever.
7. Mutual exclusion, hold and wait, no preemption, circular wait.
8. Nobody can hold the second lock while waiting for the first, so the circle cannot form.
