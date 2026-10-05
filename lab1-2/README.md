# Labs 1 and 2

Course: Operating Systems, FCIM / FAF, UTM, 2026-2027.

Both labs are described in one report.

| File | What it is |
|---|---|
| `LAB 1 & Lab 2_FAF-242_Eftode_Andrei_v2.docx` | The report for both labs, with 48 screenshots |
| `report.md` | Lab 1: the written answers, as they were typed in the virtual machine |
| `notes.md` | Lab 2: the written answers |
| `sleep.c` | Lab 2: my first xv6 program. It goes in `user/sleep.c` of xv6 |
| `pingpong.c` | Lab 2, optional part: a parent and a child process pass one byte through two pipes |

## Lab 1 – The OS as a resource manager

Track A: the real OS, from the outside (Linux).

The lab watches Linux manage four resources from the terminal.

- **Files.** A file was created, copied, renamed and deleted; `ls -l` and `chmod` showed who owns a file and who may read or write it.
- **Processes.** `ps` and `top` listed the running programs; a `sleep` process was started in the background, found and stopped with `kill`. Process number 1 is `systemd`, the ancestor of all the others.
- **Memory.** `free -h` showed how much RAM is used. "Available" memory is larger than "free" memory, because Linux uses spare RAM as a cache.
- **Devices.** `df`, `lsblk` and `/dev` showed that the disk is `/dev/sda2`, mounted on `/`, and that devices appear as files.

`strace` counted 137 system calls for one `ls`. Programs never touch the hardware themselves: they ask the kernel, and the kernel decides.

## Lab 2 – Meet the OS you will build

Track B: build your own small OS (xv6).

- xv6 is a small teaching operating system from MIT, written for a RISC-V processor. It is built with a cross-compiler and runs in the QEMU emulator.
- `make qemu` built it from source and started it. Inside it I ran `ls`, `echo`, `cat`, `wc`, a pipe, and the test suite `usertests`, which ended with ALL TESTS PASSED.
- The source has two parts: `kernel/`, the operating system itself, and `user/`, the ordinary programs. I followed the system call `read` from `cat.c` to `sys_read` in `kernel/sysfile.c`.
- I wrote `user/sleep.c`, added it to the Makefile and tested it: `sleep 10` pauses for about one second.
- `pingpong.c` uses `fork` and two pipes to send one byte from a parent process to its child and back.

These two labs used xv6 revision `9374395`, where the system call is still named `sleep()`. From Lab 4 on, the work moves to the current revision, where it is `pause()`. See [`../lab4/README.md`](../lab4/README.md).
