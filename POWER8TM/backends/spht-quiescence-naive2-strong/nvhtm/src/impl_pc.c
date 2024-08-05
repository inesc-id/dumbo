#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE   600
#define _BSD_SOURCE     1
#define _GNU_SOURCE     1
#define _DEFAULT_SOURCE 1

#include <unistd.h>

#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

#define LARGER_THAN(_TSi, _TSj, _i, _j) ((_TSi > _TSj) || (_TSi == _TSj && _i > _j))

typedef uintptr_t bit_array_t;

static volatile __thread uint64_t readClockVal;

static volatile __thread
  int writeLogStart, writeLogEnd;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;
static volatile __thread uint64_t timeTX_upd = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;
static volatile uint64_t incTXTime_upd= 0;
extern __thread uint64_t timeAbortedTX;

static inline void bitarray_clean(bit_array_t *bitArray)
{
  *bitArray ^= *bitArray;
}

static inline void bitarray_set(bit_array_t *bitArray, int pos)
{
  *bitArray |= (1 << pos);
}

// different from 0 means "is set"
static inline int bitarray_is_set(bit_array_t *bitArray, int pos)
{
  return (*bitArray & (1 << pos));
}

// different from 0 means "is set"
static inline int bitarray_any_is_set(bit_array_t *bitArray)
{
  return *bitArray;
}

void install_bindings_pc()
{
  on_before_htm_begin  = on_before_htm_begin_pc;
  on_htm_abort         = on_htm_abort_pc;
  on_before_htm_write  = on_before_htm_write_8B_pc;
  on_before_htm_commit = on_before_htm_commit_pc;
  on_after_htm_commit  = on_after_htm_commit_pc;

  wait_commit_fn = wait_commit_pc_simple;
}

void state_gather_profiling_info_pc(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
  __sync_fetch_and_add(&incTXTime_upd, timeTX_upd);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedUpdTX);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedROTX);


  timeSGL = 0;
  timeAbortedUpdTX = 0;
  timeAbortedROTX = 0;
  timeTX_upd = 0;
  timeAfterTXSuc = 0;
  timeWaiting = 0;
  timeTotal = 0;
  countCommitPhases = 0;
}

void state_fprintf_profiling_info_pc(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_TX",
              "TIME_WAIT",
              "TIME_SGL",
              "TIME_ABORTED_TX",
              "TIME_AFTER_TX_FAIL");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incTXTime_upd, incWaiting, timeSGL_global, timeAbortedTX_global, 0L);
}

static inline void fetch_log(int threadId)
{
  write_log_thread[writeLogEnd] = 0;
  write_log_thread[(writeLogEnd + 8) & (gs_appInfo->info.allocLogSize - 1)] = 0;
}

void on_before_htm_begin_pc(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  write_log_thread = &(P_write_log[threadId][0]);

  MEASURE_TS(timeTotalTS1);

  // -------------------------------------------------------
  // TODO: block if there isn't at least 2 cache lines available in the log
#ifdef CONCURRENT_CHECKPOINTER
  // BUG: log empty --> start == next
  int preLogNext = G_next[threadId].log_ptrs.write_log_next;
  volatile int start = G_next[threadId].log_ptrs.write_log_start;
  const int TXSizeBudget = 512; // log entries
  int target = (preLogNext + TXSizeBudget) & (gs_appInfo->info.allocLogSize - 1);
  while (!(preLogNext == start) && preLogNext != target) {
    preLogNext = (preLogNext + 1) & (gs_appInfo->info.allocLogSize - 1);
    if (start == preLogNext) {
      printf("[%i] NO MORE SPACE IN LOG!\n", threadId);
      while ((start = __atomic_load_n(&G_next[threadId].log_ptrs.write_log_start, __ATOMIC_ACQUIRE)) == preLogNext) {
        usleep(1);
        // printf("[end=%i i=%i] Log smash detected!\n", G_next[threadId].log_ptrs.write_log_next, i);
      }
      printf("[%i] Checkpointer released space!\n", threadId);
    }
  }
#endif /* CONCURRENT_CHECKPOINTER */
  // -------------------------------------------------------

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  fetch_log(threadId);
  gs_ts_array[threadId].comm.ts = rdtsc();
}

void on_htm_abort_pc(int threadId)
{
  gs_ts_array[threadId].comm.ts = rdtsc();
}

void on_before_htm_write_8B_pc(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
}

void on_before_htm_commit_pc(int threadId)
{
  readClockVal = rdtscp();
}

void on_after_htm_commit_pc(int threadId)
{
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, timeTX_upd);

  gs_ts_array[threadId].comm.ts = readClockVal; // currently has the TS of begin

  if (writeLogStart == writeLogEnd) {
    // tells the others to move on
    __atomic_store_n(&gs_ts_array[threadId].comm.ts, onesBit63(readClockVal), __ATOMIC_RELEASE);
    goto ret;
  }

  // flush log entries
  writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) & (gs_appInfo->info.allocLogSize - 1);
  FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
    &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  FENCE_PREV_FLUSHES();

  wait_commit_fn(threadId);

  /* Commits the write log (commit marker) */
  write_log_thread[writeLogEnd] = onesBit63(readClockVal);
  FLUSH_CL(&write_log_thread[writeLogEnd]);
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  FENCE_PREV_FLUSHES();

  // flush done!
  __atomic_store_n(&gs_ts_array[threadId].comm.ts, onesBit63(readClockVal), __ATOMIC_RELEASE);
  
  // wait_commit_fn(threadId); // CANNOT DO THE WAITING HERE!!! need to ensure commit marker order
ret:
  MEASURE_INC(countCommitPhases);
}

void wait_commit_pc_simple(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  volatile uintptr_t myTS = gs_ts_array[threadId].comm.ts;
  volatile uint64_t snapshotTS[gs_appInfo->info.nbThreads];
  volatile uint64_t snapshotDiscard[gs_appInfo->info.nbThreads];
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    snapshotDiscard[i] = 0;
  }

  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    snapshotTS[i] = gs_ts_array[i].pcwm.ts; // puts too much presure on this cache line
    if (LARGER_THAN(zeroBit63(snapshotTS[i]), myTS, i, threadId) || isBit63One(snapshotTS[i])) {
      snapshotDiscard[i] = 1;
    }
  }

  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId || snapshotDiscard[i] == 1) continue;
    do {
      snapshotTS[i] = gs_ts_array[i].pcwm.ts;
      // _mm_pause();
    } while (!(LARGER_THAN(zeroBit63(snapshotTS[i]), myTS, i, threadId) || isBit63One(snapshotTS[i]))
      /* && !gs_appInfo->info.isExit */);
  }

  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}

void wait_commit_pc_bitmap(int threadId)
{
  bit_array_t bitArray;
  volatile uintptr_t myTS = gs_ts_array[threadId].comm.ts;

  MEASURE_TS(timeWaitingTS1);
  bitarray_clean(&bitArray);
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    if (myTS > gs_ts_array[i].comm.ts) bitarray_set(&bitArray, i);
  }

  // if (!bitarray_any_is_set(&bitArray)) return; // luckly we are next

  // we are unlucky and need to wait the next one
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId || !bitarray_is_set(&bitArray, i)) continue;

    while (myTS > gs_ts_array[i].comm.ts && !gs_appInfo->info.isExit) {
      // wait threads with SMALLER ts, as they must commit first
      // _mm_pause();
      __atomic_thread_fence(__ATOMIC_ACQUIRE);
    }
  }
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
