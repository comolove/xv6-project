struct MLFQ {
  struct proc* L0_proc[LNPROC];
	struct proc* L1_proc[LNPROC];
	struct proc* L2_proc[LNPROC];
	uint L0_start, L0_end;
	uint L1_start, L1_end;
	uint L2_size;
};