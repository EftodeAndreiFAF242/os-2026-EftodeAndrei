# Lab 12 - Producer, Consumer and the Other Tools

Student: Eftode Andrei, FAF-242
Machine: Ubuntu 26.04.1 LTS in VirtualBox, gcc 15.2.0, 8 virtual CPUs
Folder: ~/os-lab12. Every program is compiled with `gcc -Wall -pthread`.

## Part 1 - Producer and consumer

    put  1   (in buffer: 1)
                take 1   (in buffer: 0)
    put  2   (in buffer: 1)
    put  3   (in buffer: 2)
    put  4   (in buffer: 3)
    put  5   (in buffer: 4)
                take 2   (in buffer: 3)
    put  6   (in buffer: 4)
    ...
                take 8   (in buffer: 0)

**Observe 1.** `sem_wait(&empty)` stops the producer. `empty` counts the free
slots; when it reaches 0, the producer sleeps until the consumer calls
`sem_post(&empty)`.

**Observe 2.** The consumer waits at the start, in `sem_wait(&full)`: `full`
starts at 0 because the buffer has nothing to take.

**Observe 3.** With the mutex locked before `sem_wait(&empty)`, the program
printed put 1 to put 4 and then hung until Ctrl-C. When the buffer is full, the
producer sleeps in `sem_wait(&empty)` while it holds the mutex. The consumer needs
that mutex to take an item, and only the consumer can post `empty`. Each waits for
the other forever: a deadlock.

## Part 2 - The condition variable

    B: not ready, I sleep
    A: message written
    B: I read "hello"

**Observe 1.** The variable `ready` remembers it. The condition variable itself
remembers nothing: it is only the place where B sleeps.

**Observe 2.** Yes, with `if (!ready)` it still printed the same three lines here,
because there is one writer and one reader, and B was woken only after `ready = 1`.

**Observe 3.** `pthread_cond_wait` is allowed to return with no signal at all (a
spurious wakeup). Also, with several readers, another thread can run first and
set `ready` back to 0. So after waking, the thread must test the condition again.

## Part 3 - Readers and writers

    writer 1 writes 10  (alone)
    reader 1 reads 10   (readers inside: 1)
    reader 2 reads 10   (readers inside: 2)
    reader 3 reads 10   (readers inside: 3)
    writer 2 writes 20  (alone)

**Observe 1.** `if (++readers == 1) sem_wait(&room);` and
`if (--readers == 0) sem_post(&room);`. The first reader takes the room for the
whole group, the others walk in, and the last one out gives it back.

**Observe 2.** `++readers` is three steps, like `counter++` in Lab 11. Without
`rlock`, two readers could both see 0 and both call `sem_wait(&room)`: the second
would sleep although a reader is inside. An update could also be lost, and then
the count never returns to 0 and the writers wait forever.

**Observe 3.** Writer 2 can wait forever. As long as one reader is inside, new
readers go straight in, `readers` never reaches 0 and `room` is never given back.
This is starvation.

## Part 4 - Build a deadlock, then remove it

    $ timeout 5 ./deadlock; echo "exit code: $?"
    two: I have B, I want A
    one: I have A, I want B
    exit code: 124

**Observe 1.** Thread one holds A and waits for B. Thread two holds B and waits
for A:

    one --holds--> A --wanted by--> two
     ^                               |
     |                             holds
     +------- wanted by <--- B <-----+

**Observe 2.** No. Without the two `sleep(1)` lines, 10 runs out of 10 finished.
In 200 more runs, 189 finished and 11 hung. That is worse: the bug is still there,
but it passes the tests and appears only sometimes, for example on the day of the
demo.

**Observe 3.** If every thread takes A before B, a thread that holds B can never
be waiting for A, so the circle of waiting cannot close.

The fixed version takes A first in both threads:

    one: I have A, I want B
    one: I have both
    two: I have B, I want A
    two: I have both
    done
    exit code: 0

**A note on timing.** In this VM `sleep(1)` really takes 1.0 to 2.6 s. So the fixed
program needs 2.0 to 6.2 s, and one run was cut by the 5-second `timeout` (exit
code 124) while it was still working. With a longer timeout, 10 runs out of 10
ended with exit code 0.
