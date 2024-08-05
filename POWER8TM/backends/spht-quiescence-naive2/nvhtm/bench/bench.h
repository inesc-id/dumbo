#ifndef BENCH_H_GUARD_
#define BENCH_H_GUARD_
  
// TODO: put here some variables to control the benchmark parameters

// Put this inside a while loop
void* bench_stm_init(int sameMemPool, long size);
void bench_stm_exit();
void bench_stm_init_thread();
void bench_stm_exit_thread();
void bench_stm_print(int nbThreads, double duration);
void* bench_palloc(int id, long size);
void bench_no_conflicts(void *largeMemRegion, int threadId, long spinsExec, void(*inTxFn)(void* arg), void* arg);
void bench_no_conflicts_with_reads(void *largeMemRegion, long sizeRegion, int threadId, long nbReads, long nbWrites, void(*inTxFn)(void* arg), void* arg);
void bench_no_conflicts_with_reads_stm(void *largeMemRegion, long sizeRegion, int threadId, long nbReads, long nbWrites, void(*inTxFn)(void* arg), void* arg);
void bench_random_with_reads(void *largeMemRegion, int threadId, long nbReads, long nbWrites, void(*inTxFn)(void* arg), void* arg);

#endif /* BENCH_H_GUARD_ */
