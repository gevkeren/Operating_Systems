#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  char exit_msg[32];
  argstr(1, exit_msg, 32);
  //struct proc *p = myproc();
  exit(n, exit_msg);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  uint64 p_2;
  argaddr(0, &p);
  argaddr(1, &p_2);
  return wait(p,p_2);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
// Our implemented System call
uint64
sys_memsize(void)
{
  int myProcSize = myproc()->sz;
  return myProcSize;
}
//implemetion of set priority
uint64
sys_set_ps_priority(void){
  int priority;
  argint(0, &priority);
  return set_ps_priority(priority);
  // 0 - good 
  //-1 - bad
}

uint64
sys_set_cfs_priority(void){
  int priority;
  argint(0, &priority);
  return set_cfs_priority(priority); 
  // 0 - good 
  //-1 - bad
}
uint64
sys_get_cfs_stats(void)
{
  int proc_id;
  uint64 priority, rtime, stime, retime;
  argint(0, &proc_id);
  argaddr(1, &priority);
  argaddr(3, &stime);
  argaddr(2, &rtime);
  argaddr(4, &retime);
  return get_cfs_stats(proc_id, priority, stime, rtime, retime);
}

uint64
sys_set_policy(void)
{
  int policy;
  argint(0, &policy);
  return set_policy(policy);
}
