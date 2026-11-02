# Operating Systems – laboratory works

- **Student:** Eftode Andrei, group FAF-242
- **Course:** Operating Systems, FCIM / FAF, Technical University of Moldova, 2026-2027

Each lab has its own folder. Every folder holds the Word report, the files the lab asks for, and a `README.md` that explains what was done. From Lab 3 on, each lab also has a `full_explanations.md`, which takes every command and every line of code apart, piece by piece.

| Folder | Lab | Topic | What is inside |
|---|---|---|---|
| [`lab1/`](lab1/) | 1 | The OS as a resource manager (Linux) | Report, `report.md`, screenshots |
| [`lab2/`](lab2/) | 2 | Meet the OS you will build (xv6) | Report, `sleep.c`, `pingpong.c`, `Makefile`, `notes.md`, screenshots |
| [`lab3/`](lab3/) | 3 | The command line: shell, pipes and text filters | Report, `oneliners.sh`, `report.sh`, screenshots |
| [`lab4/`](lab4/) | 4 | The xv6 shell from the outside | Report, `sleep.c`, `args.c`, `args_linux.c`, `Makefile`, screenshots |
| [`lab5/`](lab5/) | 5 | The file system with your own hands (Linux) | Report, `report.md`, screenshots |
| [`lab6/`](lab6/) | 6 | Inside the xv6 file system | Report, `mystat.c`, `find.c`, `Makefile`, `notes.md`, screenshots (plus `sleep.c`, `args.c` from Lab 4, which the Makefile also builds) |
| [`lab7/`](lab7/) | 7 | Three small programs: processes, redirection, a pipe (Linux) | Report, `run.c`, `zombie.c`, `redirect.c`, `pipeline.c`, `pipeline3.c`, `report.md`, screenshots |
| [`lab8/`](lab8/) | 8 | The same calls, inside your own OS (xv6) | Report, `run.c`, `pingpong.c`, `primes.c`, `Makefile`, `notes.md`, screenshots (plus `sleep.c`, `args.c`, `mystat.c`, `find.c` from Labs 4 and 6, which the Makefile also builds) |
| [`lab9/`](lab9/) | 9 | Watching system calls with strace (Linux) | Report, `syscalls.c`, `report.md`, screenshots |
| [`lab10/`](lab10/) | 10 | Add your own system call: trace (xv6) | Report, the changed xv6 files in `kernel/` and `user/` (`trace.c`, and the optional `sysinfo`), `Makefile`, `lab10.patch`, `notes.txt`, screenshots (plus the programs of Labs 4, 6 and 8, which the Makefile also builds) |

The labs follow two tracks. Track A looks at a real operating system, Linux, from the outside (Labs 1, 3, 5, 7 and 9). Track B builds on a small teaching operating system, xv6 (Labs 2, 4, 6, 8 and 10).

## Where the work is done

- A Windows 11 laptop with a Linux virtual machine: **Ubuntu 26.04.1 LTS** in VirtualBox (user `vboxuser`, computer name `os`).
- The machine was prepared with the course script `os2026-setup.sh`, which installs the tools, downloads xv6 into `~/xv6-riscv`, builds it and boots it once. Its last line was **All set.** (see [`lab3/README.md`](lab3/README.md#preparing-the-laptop)).
- Tools: `riscv64-linux-gnu-gcc` 15.2.0, QEMU 10.2.1, GNU Make 4.4.1, git, `strace` 6.19.
- xv6: revision `06aad25` of [mit-pdos/xv6-riscv](https://github.com/mit-pdos/xv6-riscv), the one the course uses, on a branch named `lab`. Lab 2 was redone on a fresh clone of the current xv6 (revision `f780d78`), so its `sleep.c` calls `pause()`, the current name of the system call.
- Every xv6 folder (`lab2/`, `lab4/`, `lab6/`, `lab8/`) builds on its own: copy its `.c` files into `user/` and its `Makefile` into a fresh xv6 tree, then run `make qemu`. This was checked on revision `06aad25` and on the current `f780d78`.
- Lab 10 changes the kernel, so `lab10/` keeps the layout of the tree: copy its `kernel/`, `user/` and `Makefile` over a fresh xv6 tree at `06aad25`, then run `make qemu`.

## How a lab is added

```bash
git add lab7 README.md && git commit -m "Lab 7" && git push
```
