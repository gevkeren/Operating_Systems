#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
// #include "kthread.h"


struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    for (struct kthread *kt = p->kthreads; kt < &p->kthreads[NKT]; kt++) {
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) ((p - proc) * NKT + (kt - p->kthreads)));
      kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      kthreadinit(p);
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void){
  push_off();
  struct kthread* kt = mykthread();
  pop_off();
  return kt->parent_pcb;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.

static struct proc*
allocproc(void)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

  found:
    // struct kthread* kt;
    p->pid = allocpid();
    p->state = USED;

    if((p->base_trapframes = (struct trapframe *)kalloc()) == 0){
      freeproc(p);
      release(&p->lock);
      return 0;
    }

    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
      freeproc(p);
      release(&p->lock);
      return 0;
    }
    
    acquire(&p->thread_id_lock);
    p->thread_id_counter = 1;
    release(&p->thread_id_lock);

    allockthread(p); // also acquires the &kt->lock
    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  
  if(p->base_trapframes)
    kfree((void*)p->base_trapframes);
  p->base_trapframes = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
    

  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->killed = 0;
  p->exit_status = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->thread_id_counter = 0;
  p->state = UNUSED;
  
  for (struct kthread *kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
    free_kthread(kt);
  }
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME(0), PGSIZE,
              (uint64)(p->base_trapframes), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME(0), 1, 0);  
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  p = allocproc();
  initproc = p;

  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  struct kthread *first_kernel_thread = p->kthreads;

  first_kernel_thread->trapframe->epc = 0; // user program_counter (PC)
  first_kernel_thread->trapframe->sp = PGSIZE; // user stack_pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  first_kernel_thread->tstate = KT_RUNNABLE;

  release(&first_kernel_thread->tlock);
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct kthread *kt = mykthread();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    // release(&kt->tlock);
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  if (&(np->kthreads) == 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  // copy saved user registers.
  *(np->kthreads[0].trapframe) = *(kt->trapframe);
  // Cause fork to return 0 in the child.
  np->kthreads[0].trapframe->a0 = 0;

  // np->kthreads->channel = kt->channel;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->kthreads->tlock);
  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  
  acquire(&np->kthreads[0].tlock);
  np->kthreads[0].tstate = KT_RUNNABLE;
  release(&np->kthreads[0].tlock);

  release(&np->lock);

  return pid;
}


// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();


  // Terminating all thread except mykthread
  int kill_tid = 0;
  for(;;){
    acquire(&p->lock);
    struct kthread * kt;
    for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
      struct kthread * current_thread = mykthread();
      if (kt != current_thread){
        acquire(&kt->tlock);
        kill_tid = kt->tid;
        release(&kt->tlock);
        break;
      }
    }
    release(&p->lock);

    if(kill_tid > 0){
      kthread_kill(kill_tid);
      kthread_join(kill_tid, 0);
    }
    else{
      break;
    }
  }
  
  if(p == initproc)
    panic("Init Exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->exit_status = status;
  p->state = ZOMBIE;
  
  struct kthread* kt;
  for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
    acquire(&kt->tlock);
    kt->tstate = KT_ZOMBIE;
    release(&kt->tlock);
  }
  

  acquire(&mykthread()->tlock);

  release(&p->lock);
  release(&wait_lock);

  // Jump into the scheduler, never to return.
  // acquire(&mykthread()->tlock);
  sched();
  panic("Zombie Exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p;
  p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->exit_status,
                                  sizeof(pp->exit_status)) < 0) {
            // freeproc(pp);
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }
    
    int killed = 0;
    struct kthread *kt;
    kt = mykthread();
    acquire(&kt->tlock);
    killed = kt->tkilled;
    release(&kt->tlock);

    // No point waiting if we don't have any children.
    if(killed || !havekids){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  struct kthread *kt;
  
  c->curr_thread = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    for(p = proc; p < &proc[NPROC]; p++) {
      if (p->state == USED){
        for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
          acquire(&kt->tlock);
          if(kt->tstate == KT_RUNNABLE){
            kt->tstate = KT_RUNNING;
            c->curr_thread = kt;
            swtch(&c->c_context, &kt->context);
            c->curr_thread = 0;
          }
          release(&kt->tlock);
        }
      }
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct kthread *kt = mykthread();

  if(!holding(&kt->tlock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(kt->tstate == KT_RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&kt->context, &mycpu()->c_context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct kthread *kt = mykthread();
  acquire(&kt->tlock);
  kt->tstate = KT_RUNNABLE;
  sched();
  release(&kt->tlock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&mykthread()->tlock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct kthread *kt = mykthread();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&kt->tlock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  kt->channel = chan;
  kt->tstate = KT_SLEEPING;

  sched();

  // Tidy up.
  kt->channel = 0;

  // Reacquire original lock.
  release(&kt->tlock);
  acquire(lk);
}



// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;
  struct kthread *kt ;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == USED){
      for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
        struct kthread* my_thread;
        my_thread = mykthread();
        if(kt != my_thread){
          acquire(&kt->tlock);
          if(kt->channel == chan && kt->tstate == KT_SLEEPING){
            kt->tstate = KT_RUNNABLE;
          }
          release(&kt->tlock);
        }
      }
    }
    release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).



//Find the proc to kill based on its pid and wake up all of the proc sleeping 
//kernel threads after setting the process’ killed flag value to 1.
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      struct kthread *kt;
      for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
        acquire(&kt->tlock);
        kt->tkilled = 1;
        if(kt->tstate == KT_SLEEPING){
          kt->tstate = KT_RUNNABLE;
        }
        release(&kt->tlock);
      }
      
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int 
kthread_create(uint64 start_func, uint64 stack, int stack_size)
{
  // printf("enter kthread_create\n");
  
  struct proc *p;
  struct kthread *kt;
  int thread_id;

  p = myproc();
  acquire(&p->lock);
  // printf("\tprocess - %d - lock acquired\n", p->pid);
  kt = allockthread(p); // acquires kt->tlock !!!
  // printf("\t\tthread - %d - allocated\n", kt->tid);
  if (!kt){ // Thread was not allocated
    // printf("\t\t\t\tkt == 0\n");
    release(&p->lock);
    return -1;
  }
  else{
    // Thread was allocated
    kt->tstate = KT_RUNNABLE;
    thread_id = kt->tid;
    
    kt->trapframe->epc = (uint64)start_func;
    kt->trapframe->sp = (uint64)(stack + stack_size);
    // printf("\t\t\tAll fields updated\n");
    release(&kt->tlock);
    // printf("\t\tkthread - %d - lock released inside kthread_create\n", kt->tid);
    release(&p->lock);
    // printf("\tproccess -%d - lock released inside kthread_create\n", p->pid);
    return thread_id;
  }

  
  
}

int
kthread_id(void)
{
  struct kthread *kt;
  int thread_id;

  kt = mykthread();

  acquire(&kt->tlock);
  thread_id = kt->tid;
  release(&kt->tlock);

  return thread_id;
}

int 
kthread_kill(int ktid)
{
  struct proc *p;
  struct kthread *kt;

  p = myproc();
  
  acquire(&p->lock);
  for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
    acquire(&kt->tlock);
    if (kt->tid != ktid){
      release(&kt->tlock);
    }
    else{ //kt->tid == ktid
      kt->tkilled = 1;
      if (kt->tstate == KT_SLEEPING){
        kt->tstate = KT_RUNNABLE;
      }
      // Releasing both locks before successfull return (0)
      release(&kt->tlock);
      release(&p->lock);
      return 0;
    }
  }
  // Didn't find the thread to kill
  // Releasing process lock before unsuccessfull return (-1)
  release(&p->lock);
  return -1;
}

void
kthread_exit(int status)
{
  struct proc *p;
  struct kthread *current_thread;
  struct kthread *kt;
  int flag = 0; // 0 if no active or sleeping threads, 1 otherwise

  p = myproc();
  current_thread = mykthread();

  acquire(&p->lock);
  for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
    acquire(&kt->tlock);
    if (kt->tstate == KT_RUNNABLE || kt->tstate == KT_RUNNING || kt->tstate == KT_SLEEPING || kt->tstate == KT_USED){
      // Thread is sort of active
      flag++;
    }
    release(&kt->tlock);
  }
  release(&p->lock);

  if (flag != 1){
    acquire(&wait_lock);
    wakeup(current_thread);

    acquire(&current_thread->tlock); // acquiring thread lock before entering scheduler
    
    current_thread->texit_status = status;
    current_thread->tstate = KT_ZOMBIE;

    release(&wait_lock);

    sched();
    panic("Zombie Exit!");
  }
  else{
    exit(status);
  }
}

int 
kthread_join(int ktid, uint64 status)
{
  acquire(&wait_lock);
  // printf("wait lock acquired\n");
  for(;;){
    // Find the thread
    struct proc *p;
    struct kthread *kt;
    int found = 0;

    p = myproc();

    acquire(&p->lock);
    // printf("\tproccess -%d - lock acquired inside kthread_join\n", p->pid);
    for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
      acquire(&kt->tlock);
      // printf("\t\tthread -%d - lock acquired inside kthread_join\n", kt->tid);
      if (kt->tid != ktid){
        release(&kt->tlock);
      }
      else{ //Found
        found = 1;
        release(&kt->tlock);
        release(&p->lock);
        break;
      }
    }

    if (found){
      acquire(&kt->tlock);
      // printf("\t\thread -%d - lock acquired inside kthread_join\n", kt->tid);
      // printf("state is %d\n", kt->tstate);
      if (kt->tstate == KT_ZOMBIE){
        // Waiting for thread with ktid to finish - return -1
        if (status != 0 && copyout(myproc()->pagetable, status, (char *)&kt->texit_status, sizeof(kt->texit_status)) < 0){
          release(&kt->tlock);
          // printf("\t\thread -%d - lock released inside kthread_join\n", kt->tid);
          release(&wait_lock);
          // printf("wait lock released\n");
          return -1;
        }
        else{
          free_kthread(kt);
          release(&kt->tlock);
          // printf("\t\thread -%d - lock released inside kthread_join\n", kt->tid);
          release(&wait_lock);
          // printf("wait lock released inside kthread_join\n", kt->tid);
          return 0;
        }
        // Success - freeing thread and returning 0
        
      }
      // Releasing thread lock before going to sleep on wait_lock
      release(&kt->tlock);
      sleep(kt, &wait_lock);
    }

    struct kthread *current_thread;
    int killed = 0;

    current_thread = mykthread();

    acquire(&current_thread->tlock);
    // printf("\t\tmythread -%d - lock acquired inside kthread_join\n", current_thread->tid);
    killed = kt->tkilled;
    release(&current_thread->tlock);
    // printf("\t\tmythread -%d - lock released inside kthread_join\n", current_thread->tid);

    if (kt == 0 || killed == 1){
      release(&wait_lock);
      return -1;
    }
  }
}