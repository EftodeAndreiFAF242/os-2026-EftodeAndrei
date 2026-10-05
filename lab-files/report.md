# Lab 1 - The OS as a Resource Manager

Student: Eftode Andrei, FAF-242
Machine: Ubuntu 26.04.1 LTS in VirtualBox, kernel Linux 7.0.0-34-generic (x86_64), user vboxuser, host name os

    $ whoami
    vboxuser
    $ uname -a
    Linux os 7.0.0-34-generic #34-Ubuntu SMP PREEMPT_DYNAMIC Wed Sep  2 14:29:37 UTC 2026 x86_64 GNU/Linux
    $ uptime
     11:15:39 up 10 min,  1 user,  load average: 0.99, 1.25, 0.79

## Part 1 - Files and directories

1. The files I created are owned by user vboxuser and group vboxuser
   (uid=1000, gid=1000). A new file takes the user and the primary group
   of the process that created it.
2. After chmod 600 the ten characters are "-rw-------":
   - 1st character "-"  : a regular file (d = directory, l = symbolic link)
   - characters 2-4 "rw-": the owner may read and write, not execute
   - characters 5-7 "---": the group may do nothing
   - characters 8-10 "---": everybody else may do nothing
   Before the change it was "-rw-rw-r--" (664), which comes from my umask 0002.
3. /etc  holds the system-wide configuration files (adduser.conf, NetworkManager, X11 ...).
   /home holds the personal directories of the users (mine is /home/vboxuser).

Interesting output: chmod changed only the "Change" time in stat, not "Modify",
because permissions live in the inode and the content of the file was not touched.

    -rw-rw-r-- 1 vboxuser vboxuser 24 Oct  2 11:16 note.txt
    -rw------- 1 vboxuser vboxuser 24 Oct  2 11:16 note.txt
    -rw-r--r-- 1 vboxuser vboxuser 24 Oct  2 11:16 note.txt

## Part 2 - Processes

1. PID 1 is systemd (/usr/lib/systemd/systemd, /sbin/init is a link to it).
   It is the first user-space process the kernel starts (its parent is 0),
   it starts all the services and it is the ancestor of every other process.
2. About 244 processes: "ps aux | wc -l" printed 245 (one line is the header)
   and top showed "Tasks: 243 total, 1 running, 241 sleeping".
   Roughly half of them are kernel threads (names in [brackets]).
3. "State:  S (sleeping)" - the process waits for its timer and uses no CPU.

Interesting output: the background job and how kill ended it.

    $ sleep 300 &
    [1] 11723
    $ ps -ef | grep sleep
    vboxuser   11723    9766  0 11:17 pts/0    00:00:00 sleep 300
    $ kill 11723
    [1]+  Terminated              sleep 300

## Part 3 - Memory

1. Total RAM 5.5 GiB (MemTotal: 5776328 kB). Only 1.4 GiB is completely free,
   but 4.1 GiB is "available", because 3.0 GiB is buff/cache that the kernel
   can give back at once.
2. Swap is space on disk where the kernel can move memory pages that were not
   used recently, so that programs can use more memory than the physical RAM.
   On this VM no swap is configured: Swap 0B, "swapon --show" prints nothing.
3. VmRSS = 7532 kB, about 7.4 MB, for a program that only waits. It surprised me,
   but 6308 kB of it is RssFile (the program file and the shared libraries, which
   are shared with other processes) and only 1224 kB is its own private memory.

Interesting output:

                   total        used        free      shared  buff/cache   available
    Mem:           5.5Gi       1.4Gi       1.4Gi        46Mi       3.0Gi       4.1Gi
    Swap:             0B          0B          0B

## Part 4 - Devices and storage

1. "/" is mounted from /dev/sda2 (ext4, 49G, 17% used), the second partition
   of the virtual disk /dev/sda. /dev/sda1 is the small EFI partition (/boot/efi).
2. /dev/sda is the hard disk of the VM (block device 8,0).
   /dev/null is a character device (1,3) that throws away everything written to it.
3. "Everything is a file" means that disks, terminals, the random generator and
   even the kernel's data about processes (/proc) have names in the same directory
   tree and are used with the same open/read/write/close calls as normal files.

Interesting output: the snap packages are ordinary image files shown as disks (loop devices).

    sda      8:0    0  50.5G  0 disk
    |-sda1   8:1    0     1G  0 part /boot/efi
    `-sda2   8:2    0  49.4G  0 part /
    loop3    7:3    0 260.3M  1 loop /snap/firefox/8763

## Closing synthesis

The operating system manages four resources: files, processes, memory and devices.
I saw files with "ls -l" (owner, group, permissions), processes with "ps aux" (and
top, kill), memory with "free -h" and devices with "lsblk" (and df -h, ls -l /dev).
In every case my programs did not touch the hardware themselves; they asked the kernel
and the kernel decided, which is why "strace -c ls" counted 137 system calls for one ls.
