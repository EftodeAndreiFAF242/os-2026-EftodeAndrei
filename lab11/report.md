# Lab 11 - The Race and the Semaphore

Student: Eftode Andrei, FAF-242
Machine: Ubuntu 26.04.1 LTS in VirtualBox, gcc 15.2.0, 8 virtual CPUs
Folder: ~/os-lab11. Every program is compiled with `gcc -Wall -pthread`, without -O2.

## Part 1 - See the race

    $ ./race
    counter = 1560120 (expected 2000000)
    $ ./race
    counter = 2000000 (expected 2000000)
    $ ./race
    counter = 1825400 (expected 2000000)

**Observe 1.** `counter++` is three steps: load the value from memory into a
register, add 1 to the register, store the register back into memory.

**Observe 2.** The counter is 5. Thread A loads 5. Thread B loads 5. A adds 1 and
stores 6. B adds 1 and also stores 6. Two additions ran, but the counter grew by 1.

**Observe 3.** The scheduler decides when each thread runs and on which CPU. That
timing is different at every run, so the number of lost additions is different too.

**What I saw differently from the sheet.** The sheet says the result is never
2 000 000. On my VM, 23 of 100 runs gave exactly 2 000 000. One million additions
take only a few milliseconds, so one thread can finish before the other really
starts, and then nothing collides. With 100 000 000 additions the threads always
overlap: all 6 runs were wrong (120 to 135 million instead of 200 million). The
program is still wrong, it is only sometimes lucky.

## Part 2 - Fix it: mutex, then semaphore

    $ time ./race      counter = 2000000    real 0m0.032s
    $ time ./mutex     counter = 2000000    real 0m0.080s
    $ time ./sem       counter = 2000000    real 0m0.117s

Five runs of each: race 0.115 s, mutex 0.284 s, sem 0.693 s. The order is the
one on the sheet.

**Observe 1.** Yes. Three runs of `mutex` and three of `sem` all printed 2000000.

**Observe 2.** With `sem_init(&s, 0, 2)` two threads may be inside at the same
time, so nothing is protected. Five runs printed 1986236, 1977622, 1937254,
1987975 and 1966799: additions are lost again.

**Observe 3.** Every addition now also pays for a lock and an unlock. When the
other thread holds the lock, the thread has to wait, and that goes through the
kernel (the `sys` time grows).

## Part 3 - A semaphore as a signal

    $ ./order
    B: waiting for the message...
    A: message written
    B: I read "hello"

**Observe 1.** With start value 1, B reads an empty message:

    B: waiting for the message...
    B: I read ""
    A: message written

The semaphore already has one unit, so `sem_wait` does not wait. B reads before
A has written anything, and the global array is still all zeros.

**Observe 2.** A semaphore that starts at 0 means "it has not happened yet":
whoever calls `sem_wait` waits until someone else calls `sem_post`.

## Part 4 - A semaphore as a counter: the parking lot

Five runs together, counting every printed value of "inside":

    6 inside: 0
    15 inside: 1
    24 inside: 2
    15 inside: 3

**Observe 1.** The largest value is 3. The line that guarantees it is
`sem_wait(&places);`, because `sem_init(&places, 0, PLACES)` started the
semaphore at 3: the fourth car finds 0 and waits until a car calls `sem_post`.

**Observe 2.** With `PLACES` 1 the semaphore becomes a lock (a mutex): one car at
a time, "inside" is never above 1, and the six cars need 6.25 s.

## Optional - no lock at all

`atomic_fetch_add` does load, add and store as one indivisible step. `./atomic`
printed 2000000 in all 6 runs, in 0.053 s.
