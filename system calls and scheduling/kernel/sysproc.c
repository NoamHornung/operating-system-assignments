#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)   //ass1 task3
{
  int n;
  argint(0, &n);
  char msg[32]; 
  argstr(1, msg,32);
  exit(n,msg);
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
sys_wait(void) //ass1 task3
{
  uint64 p;
  uint64 m;
  argaddr(0, &p);
  argaddr(1, &m);
  return wait(p,m);
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


//ass1 task2
uint64
sys_memsize(void)
{
  uint64 size;
  size = myproc()->sz;
  return size;
}

//ass1 task5
uint64
sys_set_ps_priority(void)
{
  int n;
  argint(0, &n);
  return set_ps_priority(n);
}

//ass1 task6
uint64
sys_set_cfs_priority(void)
{
  int priority;
  argint(0, &priority);
  return set_cfs_priority(priority);
}

//ass1 task6
uint64
sys_get_cfs_stats(void)
{
  int pid;
  argint(0, &pid);
  uint64 cfs_priority;
  uint64 rtime;
  uint64 stime;
  uint64 retime;
  argaddr(1, &cfs_priority);
  argaddr(2, &rtime);
  argaddr(3, &stime);
  argaddr(4, &retime);
  return get_cfs_stats(pid, cfs_priority, rtime, stime, retime);
}

//ass1 task7
uint64
sys_set_policy(void)
{
  int policy;
  argint(0, &policy);
  return set_policy(policy);
}