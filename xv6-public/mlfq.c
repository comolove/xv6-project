#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "mlfq.h"

struct MLFQ mlfq;

//push and pop L0,L1 in circular queue
void
L0_push(struct proc* p) {
  mlfq.L0_proc[mlfq.L0_end++] = p;
	mlfq.L0_end %= LNPROC;
}

struct proc*
L0_pop() {
  struct proc* p = mlfq.L0_proc[mlfq.L0_start++];
  mlfq.L0_start %= LNPROC;
  cprintf("L0 %d %d\n",mlfq.L0_start, mlfq.L0_end);
  return p;
}

void
L1_push(struct proc* p) {
  mlfq.L1_proc[mlfq.L1_end++] = p;
	mlfq.L1_end %= LNPROC;
}

struct proc*
L1_pop() {
  struct proc* p = mlfq.L1_proc[mlfq.L1_start++];
  mlfq.L1_start %= LNPROC;
  cprintf("L1 %d %d\n",mlfq.L1_start, mlfq.L1_end);
  return p;
}

//push to leaf element and heapify.
void
L2_push(struct proc* p) {
  mlfq.L2_proc[++mlfq.L2_size] = p;
  uint cur_index = mlfq.L2_size;
  uint parent_index = cur_index/2;
  struct proc* cur_p,* parent_p;

  while(cur_index != 1) {
    cur_p = mlfq.L2_proc[cur_index];
    parent_p = mlfq.L2_proc[cur_index/2];

    if(cur_p->priority > parent_p->priority || (cur_p->priority == parent_p->priority && cur_p->time_enter > parent_p->time_enter)) 
      break;

    mlfq.L2_proc[cur_index] = parent_p;
    mlfq.L2_proc[parent_index] = cur_p;
    cur_index = parent_index;
    parent_index = cur_index/2;
  }
}

//pop root element and heapify remain elements.
struct proc*
L2_pop(void) {
  struct proc* pop_p = mlfq.L2_proc[1];
  struct proc* cur_p;
  uint cur_index = 1, smaller_child_index;
  mlfq.L2_proc[1] = mlfq.L2_proc[mlfq.L2_size--];
  while(cur_index < mlfq.L2_size) {
    cur_p = mlfq.L2_proc[cur_index];
    smaller_child_index = cur_index;

    if((2*cur_index <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index]->priority < cur_p->priority) ||
      (mlfq.L2_proc[2*cur_index]->priority == cur_p->priority && 
      mlfq.L2_proc[2*cur_index]->time_enter < cur_p->time_enter) ) 
      {
        smaller_child_index = 2*cur_index;
      }

    if((2*cur_index+1 <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index+1]->priority < mlfq.L2_proc[smaller_child_index]->priority) ||
      (mlfq.L2_proc[2*cur_index+1]->priority == mlfq.L2_proc[smaller_child_index]->priority && 
      mlfq.L2_proc[2*cur_index]->time_enter < mlfq.L2_proc[smaller_child_index]->time_enter)) 
      {
        smaller_child_index = 2*cur_index+1;
      }
    
    if(smaller_child_index == cur_index) break;
    
    struct proc* temp = cur_p;
    mlfq.L2_proc[cur_index] = mlfq.L2_proc[smaller_child_index];
    mlfq.L2_proc[smaller_child_index] = temp;

    cur_index = smaller_child_index;
  }

  cprintf("L2 %d %d\n",mlfq.L2_size,pop_p->pid);
  return pop_p;
}

int
L0_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0, index = 0;
  c->proc = 0;

  while(mlfq.L0_start != mlfq.L0_end){
    index = mlfq.L0_start;
    p = L0_pop();

    if(p->state != RUNNABLE) {
      mlfq.L0_proc[index] = 0;
      continue;
    }
    // Switch to chosen process.  It is the process's job
    // to release mlfq.lock and then reacquire it
    // before jumping back to us.
    proc_cnt++;
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
  }

  return proc_cnt;
}

int
L1_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0, index = 0;
  c->proc = 0;

  while(mlfq.L1_start != mlfq.L1_end){
    index = mlfq.L1_start;
    p = L1_pop();

    if(p->state != RUNNABLE) {
      mlfq.L1_proc[index] = 0;
      continue;
    }
    // Switch to chosen process.  It is the process's job
    // to release mlfq.lock and then reacquire it
    // before jumping back to us.
    proc_cnt++;

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
  }

  return proc_cnt;
}

int 
L2_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0, index = 0;
  c->proc = 0;

  while(mlfq.L2_size > 0) {
    index = mlfq.L2_size;
    p = L2_pop();

    if(p->state != RUNNABLE) {
      mlfq.L2_proc[index] = 0;
      continue;
    }
    // Switch to chosen process.  It is the process's job
    // to release mlfq.lock and then reacquire it
    // before jumping back to us.
    proc_cnt++;

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
  }

  return proc_cnt;
}

//yield mode : 1
//else mode : 0
//because yield have to enqueue to higher level queue
void 
enqueue(struct proc* p) {
  uint queue_level = p->mlfq_level;
  cprintf("pid: %d L%d %dt\n",p->pid, p->mlfq_level,p->time_quantum);
  if(p->time_quantum == 2*queue_level+4) {
    queue_level++;
    p->time_quantum = 0;
    
    if(queue_level >= 2) {
      acquire(&tickslock);
      p->time_enter = ticks;
      release(&tickslock);

      if(queue_level > 2) {
        if(p->priority) p->priority--;
        queue_level--;
      }
    }

    p->mlfq_level = queue_level;
  }
  if(queue_level == 0) {
    L0_push(p);
  } else if (queue_level == 1) {
    L1_push(p);
  } else {
    L2_push(p);
  }

}

int 
getLevel(void) {
  if(myproc()) return myproc()->mlfq_level;
  return 1;
}