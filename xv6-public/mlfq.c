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
  return p;
}

void
upper_heapify(uint cur_index) {
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

void
lower_heapify(uint cur_index) {
  struct proc* cur_p;
  uint smaller_child_index;
  while(cur_index < mlfq.L2_size) {
    cur_p = mlfq.L2_proc[cur_index];
    smaller_child_index = cur_index;

    if((2*cur_index <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index]->priority < cur_p->priority) ||
      (2*cur_index+1 <= mlfq.L2_size && mlfq.L2_proc[2*cur_index]->priority == cur_p->priority && 
      mlfq.L2_proc[2*cur_index]->time_enter < cur_p->time_enter) ) 
      {
        smaller_child_index = 2*cur_index;
      }

    if((2*cur_index+1 <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index+1]->priority < mlfq.L2_proc[smaller_child_index]->priority) ||
      (2*cur_index+1 <= mlfq.L2_size && mlfq.L2_proc[2*cur_index+1]->priority == mlfq.L2_proc[smaller_child_index]->priority && 
      mlfq.L2_proc[2*cur_index+1]->time_enter < mlfq.L2_proc[smaller_child_index]->time_enter)) 
      {
        smaller_child_index = 2*cur_index+1;
      }
    if(smaller_child_index == cur_index) break;
    
    struct proc* temp = cur_p;
    mlfq.L2_proc[cur_index] = mlfq.L2_proc[smaller_child_index];
    mlfq.L2_proc[smaller_child_index] = temp;

    cur_index = smaller_child_index;
  }
}

void
heapify(uint cur_index) {
  upper_heapify(cur_index);
  lower_heapify(cur_index);
} 

//push to leaf element and heapify.
//sort by priority, enter_time
void
L2_push(struct proc* p) {
  mlfq.L2_proc[++mlfq.L2_size] = p;
  upper_heapify(mlfq.L2_size);
}

//pop root element and heapify remain elements.
struct proc*
L2_pop(void) {
  struct proc* pop_p = mlfq.L2_proc[1];
  mlfq.L2_proc[1] = mlfq.L2_proc[mlfq.L2_size--];
  lower_heapify(1);
  return pop_p;
}

// find index with pid in L2
int
L2_find(uint pid, int cur_index) {
  struct proc* p = mlfq.L2_proc[cur_index];
  int ret = 0;
  if(p->pid == pid) return cur_index;

  if(2*cur_index <= mlfq.L2_size) {
    ret = L2_find(pid, 2*cur_index);
    if(ret) return ret;
  }
  if(2*cur_index+1 <= mlfq.L2_size) {
    ret = L2_find(pid, 2*cur_index+1);
    if(ret) return ret;
  }

  return ret;
}

//scheduling in L0 queue, return executed processes count
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

//scheduling in L1 queue, return executed processes count
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

//scheduling in L2 queue, return executed processes count
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

    // cprintf("pid: %d priority: %d state: %d time_quantum: %d\n",p->pid,p->priority,p->state,p->time_quantum);

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

// enqueue to ready queue
// determine where to go by looking process's status
void 
enqueue(struct proc* p) {
  uint queue_level = p->mlfq_level;
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

// just return process's ready queue level
int 
getLevel(void) {
  struct proc* p = myproc();
  if(p) return p->mlfq_level;
  return -1;
}