#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

static volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t gs_globalClock = 0;

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
static volatile uint64_t incTXTime_upd = 0;

void install_bindings_lc()
{
  on_before_htm_begin  = on_before_htm_begin_lc;
  on_before_htm_write  = on_before_htm_write_8B_lc;
  on_before_htm_commit = on_before_htm_commit_lc;
  on_after_htm_commit  = on_after_htm_commit_lc;
}

void state_gather_profiling_info_lc(int threadId)
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

void state_fprintf_profiling_info_lc(char *filename)
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

void on_before_htm_begin_lc(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  write_log_thread = &(P_write_log[threadId][0]);

  MEASURE_TS(timeTotalTS1);
  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  __atomic_store_n(&(gs_ts_array[threadId].comm.isInTX), 1, __ATOMIC_RELEASE);
  fetch_log(threadId);
}

void on_before_htm_write_8B_lc(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
}

void on_before_htm_commit_lc(int threadId)
{
  if (writeLogStart != writeLogEnd) {
    readClockVal = ++gs_globalClock;
  }
}

void on_after_htm_commit_lc(int threadId)
{
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, timeTX_upd);

  if (writeLogStart == writeLogEnd) {
    goto ret;
  }
  
  gs_ts_array[threadId].comm.ts = readClockVal;

  /* TODO: flush the read-set*/
   writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) & (gs_appInfo->info.allocLogSize - 1);
  FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
    &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);

  FENCE_PREV_FLUSHES(); // Log must hit memory before the commit marker

  write_log_thread[writeLogEnd] = onesBit63(readClockVal);
  FLUSH_CL(&write_log_thread[writeLogEnd]);
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);

  FENCE_PREV_FLUSHES();
  __atomic_store_n(&(gs_ts_array[threadId].comm.isInTX), 0, __ATOMIC_RELEASE);

  // Log replayer can detect "holes" in the log and discard those TXs
  wait_commit_fn(threadId); // Done after the commit marker is persistent

ret:
  __atomic_store_n(&(gs_ts_array[threadId].comm.isInTX), 0, __ATOMIC_RELEASE);
  MEASURE_INC(countCommitPhases);
}

void wait_commit_lc(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  volatile uintptr_t myTS = gs_ts_array[threadId].comm.ts;
  volatile uint64_t snapshotTS[gs_appInfo->info.nbThreads];
  volatile uint64_t snapshotIsInTx[gs_appInfo->info.nbThreads];
  int targetWaitFor = -1; // the thread that got myTS-1

  // __atomic_thread_fence(__ATOMIC_ACQUIRE);
  while (!gs_appInfo->info.isExit) {
    int numDoneThrs = 1; // self
    uint64_t otherTS, isInTX;
    int i;
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (i == threadId || (targetWaitFor != -1 && targetWaitFor != i)) continue;
      snapshotTS[i] = gs_ts_array[i].comm.ts;
      snapshotIsInTx[i] = gs_ts_array[i].comm.isInTX;
    }
    if (targetWaitFor != -1) {
      otherTS = snapshotTS[targetWaitFor];
      isInTX = snapshotIsInTx[targetWaitFor];
      if (myTS < otherTS || (myTS > otherTS && !isInTX)) {
        numDoneThrs = gs_appInfo->info.nbThreads;
      }
    } else {
      int i;
      for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
        if (i == threadId) continue;
        
        otherTS = snapshotTS[i];
        isInTX = snapshotIsInTx[i];

        if (otherTS == myTS-1) {
          targetWaitFor = i; // only need to load this one
        }

        if (myTS < otherTS || (myTS > otherTS && !isInTX)) {
          numDoneThrs++; // smaller TS is done with the wait phase
        }
        // } else while (myTS > otherTS && isInTX) {
        //   otherTS = gs_ts_array[i].comm.ts;
        //   isInTX = gs_ts_array[i].comm.isInTX;
      }
    }
    if (numDoneThrs == gs_appInfo->info.nbThreads) break;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
  }
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
