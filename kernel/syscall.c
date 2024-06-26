#include "rkt.h"
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_sbrkx(void);
extern uint64 sys_clone(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
    [SYS_fork] = sys_fork,     [SYS_exit] = sys_exit,
    [SYS_wait] = sys_wait,     [SYS_pipe] = sys_pipe,
    [SYS_read] = sys_read,     [SYS_kill] = sys_kill,
    [SYS_exec] = sys_exec,     [SYS_fstat] = sys_fstat,
    [SYS_chdir] = sys_chdir,   [SYS_dup] = sys_dup,
    [SYS_getpid] = sys_getpid, [SYS_sbrk] = sys_sbrk,
    [SYS_sleep] = sys_sleep,   [SYS_uptime] = sys_uptime,
    [SYS_open] = sys_open,     [SYS_write] = sys_write,
    [SYS_mknod] = sys_mknod,   [SYS_unlink] = sys_unlink,
    [SYS_link] = sys_link,     [SYS_mkdir] = sys_mkdir,
    [SYS_close] = sys_close,   [SYS_sbrkx] = sys_sbrkx,
    [SYS_clone] = sys_clone,
};

static int check_rkt_flags(struct proc *p, int syscall_num) {
  int unauthorized = 0;

  switch (syscall_num) {
  case SYS_fork:
    unauthorized = p->rktflags & RKT_RESTRICT_FORK;
    break;
  case SYS_pipe:
    unauthorized = p->rktflags & RKT_RESTRICT_PIPE;
    break;
  case SYS_read:
    unauthorized = p->rktflags & RKT_RESTRICT_READ;
    break;
  case SYS_kill:
    unauthorized = p->rktflags & RKT_RESTRICT_KILL;
    break;
  case SYS_exec:
    unauthorized = p->rktflags & RKT_RESTRICT_EXEC;
    break;
  case SYS_fstat:
    unauthorized = p->rktflags & RKT_RESTRICT_FSTAT;
    break;
  case SYS_chdir:
    unauthorized = p->rktflags & RKT_RESTRICT_CHDIR;
    break;
  case SYS_dup:
    unauthorized = p->rktflags & RKT_RESTRICT_DUP;
    break;
  case SYS_getpid:
    unauthorized = p->rktflags & RKT_RESTRICT_GETPID;
    break;
  case SYS_sbrk:
    unauthorized = p->rktflags & RKT_RESTRICT_SBRK;
    break;
  case SYS_uptime:
    unauthorized = p->rktflags & RKT_RESTRICT_UPTIME;
    break;
  case SYS_open:
    unauthorized = p->rktflags & RKT_RESTRICT_OPEN;
    break;
  case SYS_write:
    unauthorized = p->rktflags & RKT_RESTRICT_WRITE;
    break;
  case SYS_mknod:
    unauthorized = p->rktflags & RKT_RESTRICT_MKNOD;
    break;
  case SYS_unlink:
    unauthorized = p->rktflags & RKT_RESTRICT_UNLINK;
    break;
  case SYS_link:
    unauthorized = p->rktflags & RKT_RESTRICT_LINK;
    break;
  case SYS_mkdir:
    unauthorized = p->rktflags & RKT_RESTRICT_MKDIR;
    break;
  case SYS_close:
    unauthorized = p->rktflags & RKT_RESTRICT_CLOSE;
    break;
  case SYS_sbrkx:
    unauthorized = p->rktflags & RKT_RESTRICT_SBRKX;
    break;
  case SYS_clone:
    unauthorized = p->rktflags & RKT_RESTRICT_CLONE;
    break;
  }

  return unauthorized > 0;
}

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;

  if (p->rktflags) {
    int unauthorized = check_rkt_flags(p, num);
    
    if (unauthorized) {
      printf("%d %s: unauthorized syscall %d\n", p->pid, p->name, num);
      p->trapframe->a0 = -401; // 401 as in HTTP
      return;
    }
  }

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
