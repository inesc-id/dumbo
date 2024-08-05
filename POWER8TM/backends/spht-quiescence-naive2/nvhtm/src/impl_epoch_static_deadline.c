#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

#define TYPE int
#include "array_utils.h"

#define TYPE uint64_t
#include "array_utils.h"

#define DEADLINE_MASK 0x0fff // use all 1's for the next math to work
#define DEADLINE_BITS 12 // number of 1's previously used (DANGEROUS!)
#define MIN_TS_IN_EPOCH(_ts) (((_ts)&(~DEADLINE_MASK)))
#define DEADLINE(_ts) (MIN_TS_IN_EPOCH(_ts) + (DEADLINE_MASK+1)) // gives the next deadline for a given TS
#define TS_TO_EPOCH(_ts, _mod) (((_ts)>>(DEADLINE_BITS)) % _mod)

static volatile __thread
  int writeLogStart, writeLogEnd;
static volatile __thread int32_t allocLogSize;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;

static volatile __thread uint64_t *myObservedTS;
static volatile __thread uint64_t myEpochTS;
static volatile __thread uint64_t minTSInEpoch;
static volatile __thread uint64_t maxTSInEpoch;
static volatile __thread int myEpoch;
static volatile __thread int iAmTheFlusher;
static volatile __thread int didFlush[128];

// ############################################################################

static inline uint64_t
writeEpoch(int epoch)
{
  /* TODO: how to write an epoch */
  return -1;
}

// ############################################################################

void install_bindings_epoch_static_deadline()
{
  on_before_htm_begin  = on_before_htm_begin_epoch_static_deadline;
  on_htm_abort         = on_htm_abort_epoch_static_deadline;
  on_before_htm_write  = on_before_htm_write_8B_epoch_static_deadline;
  on_before_htm_commit = on_before_htm_commit_epoch_static_deadline;
  on_after_htm_commit  = on_after_htm_commit_epoch_static_deadline;

  wait_commit_fn       = wait_commit_epoch_static_deadline;
  try_prune_log_fn     = try_prune_log_epoch_static_deadline;
  prune_log_fn         = prune_log_forward_epoch_static_deadline;
}

void on_htm_abort_epoch_static_deadline(int threadId) { /* empty */ }

void on_before_htm_begin_epoch_static_deadline(int threadId)
{
  /* TODO */
}

void on_before_htm_write_8B_epoch_static_deadline(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
}

void on_before_htm_commit_epoch_static_deadline(int threadId)
{
  if (writeLogStart != writeLogEnd) { // read-only transaction do not need to be logged
    *myObservedTS = -1;
    // TODO: rdtscp causes a segmentation fault if myObservedTS is written before (using -O3)
    *myObservedTS = rdtsc();
  }
}

static inline int scanOtherTXs(int threadId)
{
  int previousEpoch = -1; 
  uint64_t previousEpochMaxTS = 0;
  uint64_t ts_array[gs_appInfo->info.nbThreads];
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    ts_array[i] = G_observed_ts[i].ts;
  }

  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (threadId == i) continue;
    didFlush[i] = 1;
    // only evaluate to false the threads from the same epoch that did not flush yet
    if (ts_array[i] > minTSInEpoch && ts_array[i] < maxTSInEpoch) {
      didFlush[i] = P_epoch_ts[i][myEpoch] >= minTSInEpoch;
      if (ts_array[i] < myEpochTS) {
        iAmTheFlusher = 0; // the one with smaller TS flushes
      }
    }

    // TODO: this is boggus 76203 <--> 76202 waiting on each other
    if (ts_array[i] < minTSInEpoch && ts_array[i] > previousEpochMaxTS) {
      previousEpoch = TS_TO_EPOCH(ts_array[i], gs_appInfo->info.allocEpochs);
      if (previousEpoch >= myEpoch)
        printf("error! previous epoch %i (%lx) >= my epoch %i (%lx)\n",
          previousEpoch, ts_array[i], myEpoch, ts_array[threadId]);
      previousEpochMaxTS = ts_array[i];
    }
  }
  return previousEpoch;
}

void on_after_htm_commit_epoch_static_deadline(int threadId)
{
  MEASURE_TS(timeAfterTXTS1);
  
  // TODO: check TSC wrap arounds 
  if (writeLogStart == writeLogEnd) goto ret; // read only TX

  myEpochTS = *myObservedTS;
  myEpoch = TS_TO_EPOCH(myEpochTS, gs_appInfo->info.allocEpochs);
  minTSInEpoch = MIN_TS_IN_EPOCH(myEpochTS);
  maxTSInEpoch = DEADLINE(myEpochTS);
  iAmTheFlusher = 1; // then the scan may set this off

  /* Commits the write log (commit marker) */
  P_write_log[threadId][writeLogEnd] = onesBit63(*myObservedTS);

  /* Flush write-set here (no fence) */
  FLUSH_RANGE(&P_write_log[threadId][writeLogStart], &P_write_log[threadId][writeLogEnd],
    &P_write_log[threadId][0], P_write_log[threadId] + gs_appInfo->info.allocLogSize);
  int nextLogNext = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;
  G_next[threadId].log_ptrs.write_log_next = nextLogNext;
  FENCE_PREV_FLUSHES();

  // notifies is done flusing the logs
  __atomic_store_n(&(P_epoch_ts[threadId][myEpoch]), myEpochTS, __ATOMIC_RELEASE);

  // wait for deadline
  while (rdtsc() < maxTSInEpoch);

  __atomic_thread_fence(__ATOMIC_RELAXED);

  int lastEpochToWait = scanOtherTXs(myEpoch);

  // waits the last epoch to be persistent
  while (lastEpochToWait != -1 &&
      zeroBit63(P_epoch_persistent[lastEpochToWait]) < MIN_TS_IN_EPOCH(lastEpochToWait))
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

  __atomic_thread_fence(__ATOMIC_RELAXED);

  wait_commit_fn(threadId);

  // 1 thread does this
  if (iAmTheFlusher) { // TODO: cache line alignment
    P_epoch_persistent[myEpoch] = myEpochTS; // epoch flush done
    FLUSH_CL(&(P_epoch_persistent[myEpoch]));
    FENCE_PREV_FLUSHES();
    __atomic_store_n(&(P_epoch_persistent[myEpoch]), onesBit63(myEpochTS), __ATOMIC_RELEASE);
  } else {
    while (!isBit63One(P_epoch_persistent[myEpoch]));
  }

ret:
  MEASURE_INC(countCommitPhases);
}

void wait_commit_epoch_static_deadline(int threadId)
{
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (threadId == i || didFlush[i]) continue;
    while (P_epoch_ts[i][myEpoch] < minTSInEpoch) {
      __atomic_thread_fence(__ATOMIC_ACQUIRE);
    }
  }
}

void try_prune_log_epoch_static_deadline(int threadId)
{
  volatile int upToEpoch = EPOCH_LAST(threadId);
  if (gs_log_data.log.who_is_pruning != -1 || upToEpoch == P_start_epoch) return; // someone is doing it
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    uint64_t remoteTS = zeroBit63(EPOCH_READ(i));
    if (remoteTS > myEpochTS) {
      // if (zeroBit63(remoteTS) == 0) printf("[%i] some thread is idle\n", threadId);
      return; // not the last one
    }
  }

  upToEpoch = (upToEpoch + gs_appInfo->info.allocEpochs - 1) % gs_appInfo->info.allocEpochs;

  if (upToEpoch == P_start_epoch || !IS_CLOSE_TO_END(upToEpoch, gs_log_data.log.epoch_end)) return;

  // upToEpoch = upToEpoch + gs_appInfo->info.allocEpochs - 1;

  // // ensure there is an epoch between the beginning and end
  // if (upToEpoch == P_start_epoch || gs_appInfo->info.isExit) return;

  if (__atomic_load_n(&gs_log_data.log.who_is_pruning, __ATOMIC_ACQUIRE) == -1) {
    __sync_val_compare_and_swap(&gs_log_data.log.who_is_pruning, -1, threadId);
    if (gs_log_data.log.who_is_pruning == threadId) {   // ensures single pruner
      prune_log_fn(upToEpoch);
      __atomic_store_n(                                 // unlocks pruner thread
        &gs_log_data.log.who_is_pruning, -1, __ATOMIC_RELEASE);
    }
  }
}

void prune_log_forward_epoch_static_deadline(int upToEpoch)
{
  // clean for everyone up to myEpoch
  int startPtr = (gs_log_data.log.epoch_end + 1) % gs_appInfo->info.allocEpochs;
  int endPtr   = upToEpoch; // (upToEpoch + 1) % gs_appInfo->info.allocEpochs; // shouldn't be +1 ?
  int epochsPruned = 0;
  // void *flushAddr;
  // uint64_t epochLastTS = 0;

  // printf("writting back %i to %i (end = %i)\n", startPtr, endPtr, gs_log_data.log.epoch_end);
  int j;
  for (j = startPtr; j != endPtr; j = (j + 1) % gs_appInfo->info.allocEpochs) {
    // /*epochLastTS = */writeEpoch(j);
    epochsPruned++;
  }

  P_start_epoch = (P_start_epoch + epochsPruned) % gs_appInfo->info.allocEpochs;
  FLUSH_CL(&P_start_epoch);
  FENCE_PREV_FLUSHES();

  // volatile version of P_start_epoch --> must be updated after
  IMPATIENT_EPOCH_END_INC(epochsPruned); // increments gs_log_data.log.epoch_end
}

void state_gather_profiling_info_epoch_static_deadline(int threadId)
{
  // printf("t%i time=%.0fms afterCommit=%.3fus, waiting=%.3fus\n", threadId,
  //   timeTotal/CPU_FREQ,
  //   timeAfterTX/CPU_FREQ / countCommitPhases * 1000,
  //   timeWaiting/CPU_FREQ / countCommitPhases * 1000);
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
}

void state_fprintf_profiling_info_epoch_static_deadline(char *filename)
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

