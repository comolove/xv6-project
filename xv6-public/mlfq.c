#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct MLFQ {
  struct proc* L0_proc[LNPROC];
	struct proc* L1_proc[LNPROC];
	struct proc* L2_proc[LNPROC];
	uint L0_start, L0_end;
	uint L1_start, L1_end;
	uint L2_size;
} mlfq;

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
L2_push(struct proc* p) {
  mlfq.L2_proc[++mlfq.L2_size] = p;
  uint cur_index = mlfq.L2_size;
  uint parent_index = cur_index/2;
  struct proc* cur_p,* parent_p;

  while(cur_index != 1) {
    cur_p = mlfq.L2_proc[cur_index];
    parent_p = mlfq.L2_proc[cur_index/2];

    if(cur_p->priority >= parent_p->priority) 
      break;

    mlfq.L2_proc[cur_index] = parent_p;
    mlfq.L2_proc[parent_index] = cur_p;
    cur_index = parent_index;
    parent_index = cur_index/2;
  }
}

struct proc*
L2_pop(void) {
  struct proc* pop_p = mlfq.L2_proc[1];
  struct proc* cur_p;
  uint cur_index = 1, smaller_child_index;
  mlfq.L2_proc[1] = mlfq.L2_proc[mlfq.L2_size--];
  while(cur_index < mlfq.L2_size) {
    cur_p = mlfq.L2_proc[cur_index];
    smaller_child_index = cur_index;

    if(2*cur_index <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index]->priority < cur_p->priority) {
        smaller_child_index = 2*cur_index;
      }

    if(2*cur_index+1 <= mlfq.L2_size &&
      mlfq.L2_proc[2*cur_index+1]->priority < mlfq.L2_proc[smaller_child_index]->priority) {
        smaller_child_index = 2*cur_index+1;
      }
    
    if(smaller_child_index == cur_index) break;
    
    struct proc* temp = cur_p;
    mlfq.L2_proc[cur_index] = mlfq.L2_proc[smaller_child_index];
    mlfq.L2_proc[smaller_child_index] = temp;

    cur_index = smaller_child_index;
  }

  return pop_p;
}

int
L0_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0;
  c->proc = 0;

  while(mlfq.L0_start != mlfq.L0_end){
    p = L0_pop();

    if(p->state != RUNNABLE)
      continue;

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
    p->time_quantum++;
    if(p->time_quantum == L0TIMEMAX) {
      L1_push(p);
      p->time_quantum = 0;
    }
    c->proc = 0;
  }

  return proc_cnt;
}

int
L1_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0;
  c->proc = 0;

  while(mlfq.L1_start != mlfq.L1_end){
    p = L1_pop();

    if(p->state != RUNNABLE)
      continue;

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
    p->time_quantum++;
    if(p->time_quantum == L1TIMEMAX) {
      L2_push(p);
      p->time_quantum = 0;
    }
    c->proc = 0;
  }

  return proc_cnt;
}

int 
L2_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0;
  c->proc = 0;

  while(mlfq.L2_size > 0) {
    p = L2_pop();

    if(p->state != RUNNABLE)
      continue;

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