#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

void
L0_push(struct proc* p) {
  ptable.L0_proc[ptable.L0_end++] = p;
	ptable.L0_end %= LNPROC;
}

struct proc*
L0_pop() {
  struct proc* p = ptable.L0_proc[ptable.L0_start++];
  ptable.L0_start %= LNPROC;
  return p;
}

void
L1_push(struct proc* p) {
  ptable.L1_proc[ptable.L1_end++] = p;
	ptable.L1_end %= LNPROC;
}

struct proc*
L1_pop() {
  struct proc* p = ptable.L1_proc[ptable.L1_start++];
  ptable.L1_start %= LNPROC;
  return p;
}

void
L2_push(struct proc* p) {
  ptable.L2_proc[++ptable.L2_size] = p;
  uint cur_index = ptable.L2_size;
  uint parent_index = cur_index/2;
  struct proc* cur_p,* parent_p;

  while(cur_index != 1) {
    cur_p = ptable.L2_proc[cur_index];
    parent_p = ptable.L2_proc[cur_index/2];

    if(cur_p->priority >= parent_p->priority) 
      break;

    ptable.L2_proc[cur_index] = parent_p;
    ptable.L2_proc[parent_index] = cur_p;
    cur_index = parent_index;
    parent_index = cur_index/2;
  }
}

struct proc*
L2_pop(void) {
  struct proc* pop_p = ptable.L2_proc[1];
  struct proc* cur_p;
  uint cur_index = 1, smaller_child_index;
  ptable.L2_proc[1] = ptable.L2_proc[ptable.L2_size--];
  while(cur_index < ptable.L2_size) {
    cur_p = ptable.L2_proc[cur_index];
    smaller_child_index = cur_index;

    if(2*cur_index <= ptable.L2_size &&
      ptable.L2_proc[2*cur_index]->priority < cur_p->priority) {
        smaller_child_index = 2*cur_index;
      }

    if(2*cur_index+1 <= ptable.L2_size &&
      ptable.L2_proc[2*cur_index+1]->priority < ptable.L2_proc[smaller_child_index]->priority) {
        smaller_child_index = 2*cur_index+1;
      }
    
    if(smaller_child_index == cur_index) break;
    
    struct proc* temp = cur_p;
    ptable.L2_proc[cur_index] = ptable.L2_proc[smaller_child_index];
    ptable.L2_proc[smaller_child_index] = temp;

    cur_index = smaller_child_index;
  }
}

int
L0_scheduling(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  int proc_cnt = 0;
  c->proc = 0;

  while(ptable.L0_start != ptable.L0_end){
    p = L0_pop();

    if(p->state != RUNNABLE)
      continue;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
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

  while(ptable.L1_start != ptable.L1_end){
    p = L1_pop();

    if(p->state != RUNNABLE)
      continue;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
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

  while(ptable.L2_size > 0) {
    p = L2_pop();

    if(p->state != RUNNABLE)
      continue;

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
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