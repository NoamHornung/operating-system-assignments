#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];


void kthreadinit(struct proc *p)
{
  
  initlock(&p->counter_lock, "counter_tid");
  for(struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    initlock(&kt->kt_lock, "thread");
    kt->kt_state = T_UNUSED;
    kt->kt_proc = p;
    
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

struct kthread *mykthread()
{
  
  //return &myproc()->kthread[0];

  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->kthread;
  pop_off();
  return kt;
}

int alloctid(struct proc *p)
{
  acquire(&p->counter_lock);
  int tid = p->counter;
  p->counter = p->counter + 1;
  release(&p->counter_lock);

  return tid;
}

struct kthread* allockthread(struct proc *p)
{
  struct kthread *kt;

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->kt_lock);
    if(kt->kt_state == T_UNUSED) {
      goto found;
    } else {
      release(&kt->kt_lock);
    }
  }
  return 0;

  found:
  kt->tid = alloctid(p);
  kt->kt_state = T_USED;

  //Allocate a trapframe page.
  if((kt->trapframe = (struct trapframe *)get_kthread_trapframe(p, kt)) == 0){
    freekthread(kt);
    release(&kt->kt_lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = kt->kstack + PGSIZE;
  return kt;
}


// kt->kt_lock must be held.
void
freekthread(struct kthread *kt)
{
  kt->trapframe = 0;
  kt->tid = 0;
  kt->chan = 0;
  kt->kt_killed = 0;
  kt->kt_xstate = 0;
  kt->kt_state = T_UNUSED;
}

  

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}


int kthread_create(void*(*start_func)(), void* stack,uint stack_size){
  //printf("kthread_create\n");
  struct kthread *kt= allockthread(myproc());
  if(kt==0)
    return -1;
  kt->trapframe->epc = (uint64)start_func;
  kt->trapframe->sp = (uint64)(stack + stack_size);
  kt->kt_state = RUNNABLE;
  int tid= kt->tid;
  release(&kt->kt_lock);
  return tid;
}

int kthread_kill(int tid){
  //printf("kthread_kill tid: %d\n", tid);
  struct proc *p= myproc();
  for(struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->kt_lock);
    if(kt->tid == tid) {
      kt->kt_killed = 1;
      if(kt->kt_state == SLEEPING)
        kt->kt_state = RUNNABLE;
      release(&kt->kt_lock);
      return 0;
    }
    release(&kt->kt_lock);
  }
  return -1;
}

int kthread_killed(struct kthread *kt){
  //printf("kthread_killed\n");
  int killed;
  acquire(&kt->kt_lock);
  killed= kt->kt_killed;
  release(&kt->kt_lock);
  return killed;
}


void kthread_exit(int status){
  //printf("kthread_exit %d\n", mykthread()->tid);

  struct proc *p= myproc();
  struct kthread *kt= mykthread();
  struct kthread *t;
  int numOfThreads=0;
  for(t = p->kthread; t < &p->kthread[NKT]; t++){
    if(t==kt)
      continue;
    acquire(&t->kt_lock);
    if(t->kt_state != T_UNUSED && t->kt_state != T_ZOMBIE){
      numOfThreads=1;
      release(&t->kt_lock);
      break;
    }
    release(&t->kt_lock);
  }

  acquire(&kt->kt_lock);
  kt->kt_xstate= status;
  kt->kt_state= T_ZOMBIE;
  release(&kt->kt_lock);

  acquire(&p->lock);
  wakeup(kt);
  release(&p->lock);

  //printf("after wakeup\n");

  if(numOfThreads == 0){
    exit(status);
  }
  

  acquire(&kt->kt_lock);
  sched();
  printf("kthread_exit: zombie exit\n");
  panic("zombie exit");
}

int kthread_join(int tid, int *status){
  //printf("kthread_join on tid=%d\n", tid);
  struct proc *p = myproc();
  struct kthread *found=0;

  acquire(&p->lock);

  for(;;){
    for(struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
      acquire(&kt->kt_lock);
      if(kt->tid == tid){
        found=kt;
        if(kt->kt_state==T_ZOMBIE){
          if(status != 0 && copyout(p->pagetable, (uint64) status, (char *)&kt->kt_xstate,
                                  sizeof(kt->kt_xstate)) < 0) {
            release(&kt->kt_lock);
            release(&p->lock);
            return -1;
          }
          freekthread(kt);
          release(&kt->kt_lock);
          release(&p->lock);
          return 0;
        }
        release(&kt->kt_lock); // found but not zombie
        break;
      }
      release(&kt->kt_lock);
    }

    if(kthread_killed(mykthread()) || found==0){
      release(&p->lock);
      return -1;
    }
    //printf("kthread_join: sleeping\n");
    sleep(found, &p->lock);
    //printf("kthread_join: woken up\n");
  }
}

  


