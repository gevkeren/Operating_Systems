#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

extern void forkret(void);

void kthreadinit(struct proc *p) {
  initlock(&p->thread_id_lock, "kthread_id");
  for (struct kthread *kt = p->kthreads; kt < &p->kthreads[NKT]; kt++)
  {
    initlock(&kt->tlock, "kthread");
    // acquire(&kt->tlock);
    kt->tstate = KT_UNUSED;
    kt->parent_pcb = p;
    // release(&kt->tlock);
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthreads)));
  }
}


struct kthread *mykthread() {
  push_off();
  struct cpu *c = mycpu();
  struct kthread* k_thread = c->curr_thread;
  pop_off();
  return k_thread;
}

int allockthreadid(struct proc *p) {
  int kthread_id;
  acquire(&p->thread_id_lock);
  kthread_id = p->thread_id_counter;
  p->thread_id_counter++;
  release(&p->thread_id_lock);
  return kthread_id;
}


struct kthread* allockthread(struct proc *p) {
  struct kthread *kt;
  // int num_of_unused = 0;
  for (kt = p->kthreads; kt < &p->kthreads[NKT]; kt++){
    acquire(&kt->tlock);
    // printf("kt lock acquired inside allockthread\n");
    if(kt->tstate == KT_UNUSED) {
      // num_of_unused++;
      goto found;
    } 
    else {
      release(&kt->tlock);
      // printf("kt lock released inside allockthread\n");
    }
  }
  return 0;
  found:
    if((kt->trapframe = get_kthread_trapframe(p, kt)) == 0){
      free_kthread(kt);
      release(&kt->tlock);
      // printf("kt lock released inside allockthread\n");
      return 0;
    }
    kt->tid = allockthreadid(p);
    kt->parent_pcb = p;
    kt->tstate = KT_USED;
    
    memset(&kt->context, 0, sizeof(kt->context));
    kt->context.ra = (uint64)forkret;
    kt->context.sp = kt->kstack + PGSIZE;
    return kt;
}


void
free_kthread(struct kthread *kt) {
  kt->trapframe = 0;
  kt->tid = 0;
  kt->channel = 0;
  kt->texit_status = 0;
  kt->tkilled = 0;
  kt->parent_pcb = 0;
  
  kt->tstate = KT_UNUSED;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt) {
  return p->base_trapframes + ((int)(kt - p->kthreads));
}