// Lab 10 optional: what the sysinfo system call fills in.
struct sysinfo {
  uint64 freemem;   // free memory, in bytes
  uint64 nproc;     // processes in use
};
