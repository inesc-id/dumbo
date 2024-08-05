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

// ############################################################################

static inline uint64_t
writeEpoch(int epoch)
{
  int      sortArrayID[gs_appInfo->info.nbThreads];
  uint64_t sortArrayTS[gs_appInfo->info.nbThreads];
  size_t sortArraySize = 0;
  uint64_t latestTS = 0;

  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    uint64_t * volatile addrTS = (uint64_t * volatile)&P_epoch_ts[i][epoch];
    volatile uint64_t       ts = __atomic_load_n(addrTS, __ATOMIC_ACQUIRE);
    uint64_t            tsZero = zeroBit63(ts);
    int where = sortedArray_uint64_t(sortArrayTS, sortArraySize, ts);
    insArrayPos_int(sortArrayID, sortArraySize, i, where);
    sortArraySize++;
    if (!IS_SLOT_OCCUPIED(epoch, i)) {
      printf("INVALID%i(PREV): %16lx %16lx\n", i,
        EPOCH_READ_PTR((epoch+gs_appInfo->info.allocEpochs-1)%gs_appInfo->info.allocEpochs, 0),
        EPOCH_READ_PTR((epoch+gs_appInfo->info.allocEpochs-1)%gs_appInfo->info.allocEpochs, 1));
      printf("INVALID%i(CURR): %16lx %16lx\n", i, EPOCH_READ_PTR(epoch, 0),
        EPOCH_READ_PTR(epoch, 1));
      if (IS_SLOT_OCCUPIED(epoch, i)) {
        printf("(next) %i %i (end=%i)\n", G_next[0].log_ptrs.epoch_next, G_next[1].log_ptrs.epoch_next,
          gs_log_data.log.epoch_end);
        printf("fixed by itself...\n");
      } else {
        printf("(next) %i %i (end=%i)\n", G_next[0].log_ptrs.epoch_next, G_next[1].log_ptrs.epoch_next,
          gs_log_data.log.epoch_end);
        printf("not fixed!\n");
      }
    }
    DEBUG_ASSERT(
      IS_SLOT_OCCUPIED(epoch, i),
      "invalid TS(%lx) on epoch %i\n", ts, epoch
    );
    if (tsZero > latestTS) {
      latestTS = tsZero;
    }
  }

  return latestTS;
}

// ############################################################################

void install_bindings_epoch_sync()
{
  on_before_htm_begin  = on_before_htm_begin_epoch_sync;
  on_htm_abort         = on_htm_abort_epoch_sync;
  on_before_htm_write  = on_before_htm_write_8B_epoch_sync;
  on_before_htm_commit = on_before_htm_commit_epoch_sync;
  on_after_htm_commit  = on_after_htm_commit_epoch_sync;

  wait_commit_fn       = wait_commit_epoch_sync;
  try_prune_log_fn     = try_prune_log_epoch_sync;
  prune_log_fn         = prune_log_forward_epoch_sync;
}

void on_htm_abort_epoch_sync(int threadId) { /* empty */ }

static inline uintptr_t fetch_log(int threadId)
{
  // TODO: this does not seem a problem in the other solution
  volatile uintptr_t res = 0;
  write_log_thread[writeLogEnd] = 0;
  write_log_thread[(writeLogEnd + 8) % allocLogSize] = 0;
  // write_log_thread[(writeLogEnd + 16) % allocLogSize] = 0;
  // write_log_thread[(writeLogEnd + 24) % allocLogSize] = 0;
  return res;
}

void on_before_htm_begin_epoch_sync(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  write_log_thread = &(P_write_log[threadId][0]);
  allocLogSize = gs_appInfo->info.allocLogSize;

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  fetch_log(threadId);
}

void on_before_htm_write_8B_epoch_sync(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
}

__thread uint64_t myEpochTS;
void on_before_htm_commit_epoch_sync(int threadId)
{
  myEpochTS = rdtscp(); // within the HTM TX
}

void on_after_htm_commit_epoch_sync(int threadId)
{
  if (myEpochTS == 0) myEpochTS = 1; /* minor fix */
  // if bit 63 is set this is also broken

  if (writeLogStart == writeLogEnd) goto ret; // read only TX

  /* Commits the write log (commit marker) */
  P_write_log[threadId][writeLogEnd] = onesBit63(myEpochTS);

  /* Flush write-set here (no fence) */
  FLUSH_RANGE(&P_write_log[threadId][writeLogStart], &P_write_log[threadId][writeLogEnd],
    &P_write_log[threadId][0], P_write_log[threadId] + gs_appInfo->info.allocLogSize);
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) % gs_appInfo->info.allocEpochs;

  FENCE_PREV_FLUSHES();

  volatile int epoch = EPOCH_LAST(threadId);
  while (gs_log_data.log.epoch_end == epoch) {
    try_prune_log_fn(threadId);
  }

  EPOCH_WRITE_VAL(threadId, myEpochTS, epoch); // externalizes after commit

  wait_commit_fn(threadId);

  EPOCH_FINALIZE(threadId);

ret:
  MEASURE_INC(countCommitPhases);
}

void state_gather_profiling_info_epoch_sync(int threadId)
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

void state_fprintf_profiling_info_epoch_sync(char *filename)
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

void wait_commit_epoch_sync(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  volatile int epoch = EPOCH_LAST(threadId);
  volatile uint64_t snapshotTS[gs_appInfo->info.nbThreads];
  volatile uint64_t snapshotPrevTS[gs_appInfo->info.nbThreads];
  volatile int threadDidFlush[gs_appInfo->info.nbThreads];
  // volatile int startEpoch = P_start_epoch;
  volatile int prevEpoch = (epoch + gs_appInfo->info.allocEpochs - 1) % gs_appInfo->info.allocEpochs;
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    threadDidFlush[i] = 0;
  }

  // stable
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (threadId == i || threadDidFlush[i]) continue; // avoid unecessary loads
    snapshotPrevTS[i] = P_epoch_ts[i][prevEpoch];
  }

  FENCE_PREV_FLUSHES();

  // Waits for the flush epoch
  __atomic_store_n(&P_epoch_ts[threadId][epoch], onesBit63(myEpochTS), __ATOMIC_RELEASE);

  while (!gs_appInfo->info.isExit) {
    int numEpochsDone = 1, numFlushesDone = 1; // avoid self
    int i;
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (threadId == i) continue;
      snapshotTS[i] = P_epoch_ts[i][epoch];
    }
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (threadId == i) continue;
      // TODO: it seems hopeless to optimize this, the bottleneck becomes the scanning either way
      // if(IS_SLOT_OCCUPIED_TS(epoch, i, snapshotTS[i])) {
      int threadIsOk = 0;
      if(zeroBit63(snapshotTS[i]) >= zeroBit63(snapshotPrevTS[i])) { // current slot has larger TS than previous
        numEpochsDone++;
        threadIsOk = 1;
      }
      /* maybe it is already in the next epoch */
      if (isBit63One(snapshotTS[i])) {
        numFlushesDone++;
        if (threadIsOk) threadDidFlush[i] = 1;
      }
    }
    if (numEpochsDone == gs_appInfo->info.nbThreads && numFlushesDone == gs_appInfo->info.nbThreads) break;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
  }

  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
  try_prune_log_fn(threadId);
}

void try_prune_log_epoch_sync(int threadId)
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

void prune_log_forward_epoch_sync(int upToEpoch)
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
