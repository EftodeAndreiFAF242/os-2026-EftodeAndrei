# Lab 17 - Threads Share Memory, and Race

Student: Eftode Andrei, FAF-242
Machine: Ubuntu 26.04.1 LTS in VirtualBox, gcc 15.2.0, 8 virtual CPUs
Folder: ~/os-lab17. Every program is compiled with `gcc -Wall -Wextra -pthread`.

## Part 1 - Shared and racing

    $ ./fire
    thread 1 sees counter = 0
    thread 2 sees counter = 598641
    expected: 2000000
    counter:  1925698   (lost updates: the race)

**Observe 1.** Thread 2 starts by printing a number it never wrote itself. Only
thread 1 could have put it there. This proves that the two threads share the
same memory: there is one `counter`, and both see it.

**Observe 2.** `counter++` is three steps: read the value from memory, add 1,
write it back. The counter is 5. Thread 1 reads 5. Thread 2 reads 5. Thread 1
writes 6. Thread 2 also writes 6. Two additions ran, but the counter grew by 1.
The scheduler times the threads differently at each run, so the number of these
collisions, and the final result, changes every time.

**What I saw differently from the sheet.** The sheet says the result is always
less than 2000000. On my VM some runs are exactly 2000000 (2 of the 8 runs in
Figure 1.3). The loop takes only a few milliseconds, so sometimes the two loops
do not overlap and nothing collides. The program is still wrong, it is only
sometimes lucky.

## Part 2 - Make it worse, then think

`fire4.c` is the same program with four threads, so it expects 4000000.

    $ ./fire4
    expected: 4000000
    counter:  1566009   (lost updates: the race)

`lost.sh` runs a program 20 times and measures the loss. I ran two rounds:

    ./fire:  expected 2000000, lost on average 433455 (21%), least 0, most 993982
    ./fire4: expected 4000000, lost on average 1846785 (46%), least 426966, most 2739771
    ./fire:  expected 2000000, lost on average 410930 (20%), least 0, most 963030
    ./fire4: expected 4000000, lost on average 1857658 (46%), least 1072118, most 2661576

**Does the loss get bigger?** Yes. Two threads lose about 20% of the updates, and
four threads lose about 46%. With more threads, more of them collide on the same
variable.

**Observe 3.** A lock (mutex) around `counter++` would stop the lost updates,
because only one thread at a time could do the read, the add and the write.
