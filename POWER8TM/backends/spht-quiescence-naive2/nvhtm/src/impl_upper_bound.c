#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

static volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t gs_globalClock = 0;

static __thread uint64_t readClockVal;

static volatile __thread
  int writeLogStart, writeLogEnd;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;

void install_bindings_ub()
{
  on_before_htm_begin  = on_before_htm_begin_ub;
  on_before_htm_write  = on_before_htm_write_8B_ub;
  on_before_htm_commit = on_before_htm_commit_ub;
  on_after_htm_commit  = on_after_htm_commit_ub;
}

void state_gather_profiling_info_ub(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
}

void state_fprintf_profiling_info_ub(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_WAIT");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incWaiting);
}

static inline void fetch_log(int threadId)
{
  P_write_log[threadId][writeLogEnd] = 0;
  P_write_log[threadId][(writeLogEnd + 8) % gs_appInfo->info.allocLogSize] = 0;
}

void on_before_htm_begin_ub(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;

  MEASURE_TS(timeTotalTS1);

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  __atomic_store_n(&(gs_ts_array[threadId].comm.isInTX), 1, __ATOMIC_RELEASE);
  fetch_log(threadId);
}

void on_before_htm_write_8B_ub(int threadId, void *addr, uint64_t val)
{
  P_write_log[threadId][writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;
  P_write_log[threadId][writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;
}

void on_before_htm_commit_ub(int threadId) { /* empty */ }

void on_after_htm_commit_ub(int threadId)
{
  MEASURE_TS(timeAfterTXTS1);

  if (writeLogEnd == writeLogStart) goto ret; // read-only TX
  gs_ts_array[threadId].comm.ts = __atomic_add_fetch(&readClockVal, 1, __ATOMIC_RELEASE);

  /* TODO: flush the read-set*/
  writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) % gs_appInfo->info.allocLogSize;
  FLUSH_RANGE(&P_write_log[threadId][writeLogStart], &P_write_log[threadId][writeLogEnd],
    &P_write_log[threadId][0], P_write_log[threadId] + gs_appInfo->info.allocLogSize);
  writeLogEnd = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;

  FENCE_PREV_FLUSHES(); // Log must hit memory before the commit marker

  P_write_log[threadId][writeLogEnd] = readClockVal;
  FLUSH_CL(&P_write_log[threadId][writeLogEnd]);
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) % gs_appInfo->info.allocEpochs;

  FENCE_PREV_FLUSHES();

  // wait_commit_fn(threadId);

  __atomic_store_n(&(gs_ts_array[threadId].comm.isInTX), 0, __ATOMIC_RELEASE);

ret:
  MEASURE_INC(countCommitPhases);
}

void wait_commit_ub(int threadId) { /* empty */ }
