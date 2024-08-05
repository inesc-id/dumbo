/**
 * Implementation of the epoch impatient algorithm  
 * 
 * Functions with state_* are called in main, there are not meant to be used in
 * the final prototype, instead they call the implementation of epoch_impa.
 * 
 * For modularity constraints, there are pointer to the implementations of the
 * TM events, i.e., before begin (on_before_htm_begin), on before TX writes
 * (on_before_htm_write), on before TX commits (on_before_htm_commit) and on
 * after TX commits (on_after_htm_commit).
 * 
 * Some extra functions are defined for the wait commit protocol
 * (wait_commit_fn) and pruning the log (prune_log_fn currently only does a
 * simple check).
 */
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

/* LOG_EMPTY_SPACE must be proportional to the number of threads
 * reason: threads increment others epochs */
#define LOG_EMPTY_SPACE 128

static volatile __thread
  int writeLogStart, writeLogEnd, myEpoch;
static volatile __thread int32_t allocLogSize;

static volatile __thread
  uint64_t myEpochTS;

static volatile __thread
  uint64_t * myObservedTS;

static volatile __thread uint64_t timeLookingForSlotTS1 = 0;
static volatile __thread uint64_t timeLookingForSlotTS2 = 0;
static volatile __thread uint64_t timeLookingForSlot = 0;

// static volatile __thread uint64_t timeAfterLookingForSlotTS1 = 0; // uses timeLookingForSlotTS2
static volatile __thread uint64_t timeAfterLookingForSlotTS2 = 0;
static volatile __thread uint64_t timeAfterLookingForSlot = 0;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incLookingForSlot = 0;
static volatile uint64_t incAfterLookingForSlot = 0;
static volatile uint64_t incWaiting = 0;

// static volatile __thread uint64_t timeStealing = 0;

// ############################################################################

static inline uint64_t writeEpoch(int epoch);
static inline int lockEpoch(int threadId, int epoch);
static inline int unlockEpoch(int threadId, int epoch);

// ############################################################################

void state_gather_profiling_info_epoch_impa(int threadId)
{
  // printf("t%i time=%.0fms afterCommit=%.3fus, lookingForSlot=%.3fus, afterLooking=%.3fus waiting=%.3fus\n", threadId,
  //   timeTotal/CPU_FREQ,
  //   timeAfterTX/CPU_FREQ / countCommitPhases * 1000,
  //   timeLookingForSlot/CPU_FREQ / countCommitPhases * 1000,
  //   timeAfterLookingForSlot/CPU_FREQ / countCommitPhases * 1000,
  //   timeWaiting/CPU_FREQ / countCommitPhases * 1000);
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incLookingForSlot, timeLookingForSlot);
  __sync_fetch_and_add(&incAfterLookingForSlot, timeAfterLookingForSlot);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
}

void state_fprintf_profiling_info_epoch_impa(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_LOOKING_FOR_SLOT",
              "TIME_AFTER_LOOKING",
              "TIME_WAIT");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incLookingForSlot,
    incAfterLookingForSlot, incWaiting);
}

void install_bindings_epoch_impa()
{
  on_before_htm_begin  = on_before_htm_begin_epoch_impa;
  on_before_htm_write  = on_before_htm_write_8B_epoch_impa;
  on_htm_abort         = on_htm_abort_epoch_impa;
  on_before_htm_commit = on_before_htm_commit_epoch_impa;
  on_after_htm_commit  = on_after_htm_commit_epoch_impa;

  // extra
  wait_commit_fn   = wait_commit_epoch_impa;
  prune_log_fn     = prune_log_forward_epoch_impa;
  try_prune_log_fn = try_prune_log_epoch_impa;
}

void on_before_htm_begin_epoch_impa(int threadId)
{
  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
}

void on_before_htm_write_8B_epoch_impa(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
}

void on_htm_abort_epoch_impa(int threadId) { /* empty */ }

void on_before_htm_commit_epoch_impa(int threadId)
{
  uint64_t ts = rdtscp();
  *myObservedTS = ts; // within the HTM TX
  // if (   G_observed_ts[threadId].comm.ts == 0
  //     || G_observed_ts[threadId].comm.ts == 1
  //     || G_observed_ts[threadId].comm.ts == -2) {
  //   G_observed_ts[threadId].comm.ts = 1; /* TODO: fix for wrap arounds */
  // }
}

void on_after_htm_commit_epoch_impa(int threadId)
{
  MEASURE_TS(timeAfterTXTS1);

  // printf("thread %i after commit\n", threadId);
  int epoch, isFenceDone = 0;

  // BUG: G_observed_ts[threadId].ts changes to -1 before waiting
  myEpochTS = G_observed_ts[threadId].ts;
  DEBUG_ASSERT(G_observed_ts[threadId].ts != -1, "Got -1 timestamp tid=%i\n", threadId);

  epoch = LOOK_UP_FREE_SLOT(threadId); // updates write_log_next

  if (writeLogStart == writeLogEnd) goto ret; // read only TX

  // printf("thread %i is writeTX\n", threadId);

  /* Commits the write log (commit marker) */
  P_write_log[threadId][writeLogEnd] = onesBit63(myEpochTS);

  /* Flush write-set here (no fence) */
  FLUSH_RANGE(&P_write_log[threadId][writeLogStart], &P_write_log[threadId][writeLogEnd],
    &P_write_log[threadId][0], P_write_log[threadId] + gs_appInfo->info.allocLogSize);
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) % gs_appInfo->info.allocEpochs;
  
  MEASURE_TS(timeLookingForSlotTS1);
  while (!gs_appInfo->info.isExit /* may ignore last TX */) { // TODO: it may spin on log prune after threads exit
    epoch = G_next[threadId].log_ptrs.epoch_next;
    if (IS_SLOT_OCCUPIED(epoch, threadId)) {
      try_prune_log_fn(threadId); /* log is full */
      LOOK_UP_FREE_SLOT(threadId);
      continue;
    }
    if (!isFenceDone) {
      isFenceDone = 1; /* fences the flush of the log */
      FENCE_PREV_FLUSHES();
    }
    uint64_t * volatile addrTS = (uint64_t * volatile) &P_epoch_ts[threadId][epoch];
    if (lockEpoch(threadId, epoch)) {
      LOOK_UP_FREE_SLOT(threadId); // makes sure some other guy did not steal in the meanwhile
      volatile int epochInLock = G_next[threadId].log_ptrs.epoch_next;
      if (epoch != epochInLock) {
        unlockEpoch(threadId, epoch);
        continue;
      }
      MEASURE_TS(timeLookingForSlotTS2);
      INC_PERFORMANCE_COUNTER(timeLookingForSlotTS1, timeLookingForSlotTS2, timeLookingForSlot);
      
      // epoch is equal to epochInLock;
      // printf("[%i] got slot %i (%lx -> %lx) \n", threadId, epoch, P_epoch_ts[threadId][epoch], G_observed_ts[threadId].ts);

      DEBUG_ASSERT(G_observed_ts[threadId].ts != -1, "Got -1 timestamp epoch=%i\n", epoch);
      myEpoch = epoch;

      // P_epoch_ts[threadId][epoch] = myEpochTS
      __atomic_store_n(addrTS, myEpochTS, __ATOMIC_RELEASE);
      unlockEpoch(threadId, epoch);

      __atomic_store_n(myObservedTS, (uint64_t)-1, __ATOMIC_RELEASE);

      FLUSH_CL(&addrTS);
      // LOOK_UP_FREE_SLOT(threadId);
      FENCE_PREV_FLUSHES(); // TODO: this should be before the CAS

      // flush done
      __atomic_store_n(addrTS, onesBit62(myEpochTS), __ATOMIC_RELEASE);

      // // __atomic_store_n(&G_next[threadId].log_ptrs.flush_epoch, myEpoch, __ATOMIC_RELEASE);
      // volatile int flushEpoch = __atomic_load_n(&G_next[threadId].log_ptrs.flush_epoch, __ATOMIC_ACQUIRE);
      // // needs to notify that the flush is done 
      // while (flushEpoch < myEpoch) {
      //   __sync_val_compare_and_swap(&G_next[threadId].log_ptrs.flush_epoch, flushEpoch, myEpoch);
      //   flushEpoch = __atomic_load_n(&G_next[threadId].log_ptrs.flush_epoch, __ATOMIC_ACQUIRE);
      // }

      // printf("[%i] thread %i (%lx)\n", myEpoch, threadId, myEpochTS);

      MEASURE_TS(timeAfterLookingForSlotTS2);
      INC_PERFORMANCE_COUNTER(timeLookingForSlotTS2, timeAfterLookingForSlotTS2, timeAfterLookingForSlot);
      break;
    }
  }

  __atomic_thread_fence(__ATOMIC_RELEASE);

  // TODO: wait other threads
  wait_commit_fn(threadId);
  // TODO: flush the log
  try_prune_log_fn(threadId);

ret:
  MEASURE_INC(countCommitPhases);
}

void try_prune_log_epoch_impa(int threadId)
{
  if (gs_log_data.log.who_is_pruning != -1) return; // someone is doing it
int i;
  if (!IS_CLOSE_TO_END(G_next[threadId].log_ptrs.epoch_next, gs_log_data.log.epoch_end)) {
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (i == threadId) continue;
      uint64_t remoteTS = EPOCH_READ_PTR(myEpoch, i);
      if (zeroBit63(remoteTS) > myEpochTS) {
        // if (zeroBit63(remoteTS) == 0) printf("[%i] some thread is idle\n", threadId);
        return; // not the last one
      }
    }
  } // else I'm without log space just try to make space

  int lastSafeEpoch = FIND_LAST_SAFE_EPOCH();

  if (IS_CLOSE_TO_END(gs_log_data.log.epoch_end, lastSafeEpoch)) {
    // there is a straggler that does not move its pointer (too soon to checkpoint)
    return;
  }

  // BUG: sometimes lastSafeEpoch is incorrect
  lastSafeEpoch = (lastSafeEpoch + gs_appInfo->info.allocEpochs - 2) % gs_appInfo->info.allocEpochs;

  if (__atomic_load_n(&gs_log_data.log.who_is_pruning, __ATOMIC_ACQUIRE) == -1) {
    __sync_val_compare_and_swap(&gs_log_data.log.who_is_pruning, -1, threadId);
    if (gs_log_data.log.who_is_pruning == threadId) {   // ensures single pruner

      // printf("%i up to %i\n", threadId, lastSafeEpoch);
      prune_log_fn(lastSafeEpoch);

      __atomic_store_n(                                 // unlocks pruner thread
        &gs_log_data.log.who_is_pruning, -1, __ATOMIC_RELEASE);
    }
  }
}

void wait_commit_epoch_impa(int threadId)
{
  volatile uintptr_t ts1 = rdtscp();
  volatile uint64_t maxTS = myEpochTS;
  int volatile stragglerID[gs_appInfo->info.nbThreads];
  int volatile didSteal[gs_appInfo->info.nbThreads];
  volatile int nbOfStragglers = 0;
  uint64_t stealTS = onesBit63(myEpochTS);
  // uint64_t prevEpoch = (myEpoch + gs_appInfo->info.allocEpochs - 1) % gs_appInfo->info.allocEpochs;
  volatile uint64_t remoteTS;
  int canSteal = 1;

  MEASURE_TS(timeWaitingTS1);

  while (!gs_appInfo->info.isExit) { // TODO: some bug here (blocks if set to 1 due to thread exit)

    nbOfStragglers = 0;
    int i;
    for (i = (threadId + 1) % gs_appInfo->info.nbThreads;
             i != threadId;
             i = (i + 1) % gs_appInfo->info.nbThreads) {
      // this snooping will cause major problems if used with HTM
      remoteTS = zeroBit63(P_epoch_ts[i][myEpoch]);
      if (!IS_SLOT_OCCUPIED_TS(myEpoch, i, remoteTS)) {
        stragglerID[nbOfStragglers] = i;
        didSteal[nbOfStragglers] = 0;
        nbOfStragglers++;
      } else {
        // other thread is also waiting
        if (remoteTS != -1 && remoteTS > maxTS) maxTS = remoteTS;
      }
    }

    if (nbOfStragglers == 0) break; // cool, no stragglers! we are done

    // give some time to the stranglers to commit their TS
    if (nbOfStragglers > 0 && rdtsc() - ts1 < gs_appInfo->info.epochTimeout) {
      // still no timeout
      continue;
    }

    // timeout: let us try to steal some epoch slots
    if (lockEpoch(threadId, myEpoch)) {

      canSteal = 1;
      int i;
      for (i = 0; i < nbOfStragglers; ++i) {
        volatile int straggler = stragglerID[i];
        if (straggler == threadId) continue;
        if (!IS_SLOT_OCCUPIED(myEpoch, straggler)) {
          remoteTS = __atomic_load_n(&G_observed_ts[stragglerID[i]].ts, __ATOMIC_ACQUIRE);
          // zeroBit63(P_epoch_ts[stragglerID[i]][prevEpoch]) && remoteTS != (uint64_t)-1
          if (remoteTS != -1 && remoteTS <= maxTS) {
            // corner case in which stealing screws up, better retry later
            ts1 = rdtsc();
            canSteal = 0;
          }
        } else {
          stragglerID[i] = -1; // this straggler managed to write the entry
        }
      }

      if (canSteal) {
        int i;
        for (i = 0; i < nbOfStragglers; ++i) {
          volatile int straggler = stragglerID[i];
          if (straggler == threadId || straggler == -1) continue;
          if (!IS_SLOT_OCCUPIED(myEpoch, straggler)) {
            // this thread can go in the next epoch
            uint64_t * volatile addrTS = (uint64_t * volatile) &P_epoch_ts[stragglerID[i]][myEpoch];
            *addrTS = stealTS;
            didSteal[i] = 1;
          }
        }
      }

      unlockEpoch(threadId, myEpoch);

      if (canSteal) {
        
        for (i = 0; i < nbOfStragglers; ++i) {
          if (didSteal[i] == 1) {
            uint64_t * volatile addrTS = (uint64_t * volatile) &P_epoch_ts[stragglerID[i]][myEpoch];
            FLUSH_CL(addrTS);

            // // needs to notify, flush is done, but it may already be flushing a more recent epoch...
            // // LOOK_UP_FREE_SLOT(stragglerID[i]); // TODO: this is done concurrently
            // // __atomic_store_n(&G_next[threadId].log_ptrs.flush_epoch, myEpoch, __ATOMIC_RELEASE);
            // volatile int32_t flushEpoch = __atomic_load_n(&G_next[i].log_ptrs.flush_epoch, __ATOMIC_ACQUIRE);
            // while (flushEpoch < myEpoch) {
            //   __sync_val_compare_and_swap(&G_next[i].log_ptrs.flush_epoch, flushEpoch, myEpoch);
            //   flushEpoch = __atomic_load_n(&G_next[i].log_ptrs.flush_epoch, __ATOMIC_ACQUIRE);
            // }
          }
        }
        FENCE_PREV_FLUSHES();
        int i;
        for (i = 0; i < nbOfStragglers; ++i) {
          if (didSteal[i] == 1) {
            uint64_t * volatile addrTS = (uint64_t * volatile) &P_epoch_ts[stragglerID[i]][myEpoch];
            __atomic_store_n(addrTS, onesBit62(stealTS), __ATOMIC_RELEASE); // flush is done
          }
        }
      }
    }
  }
  int i;
  for (i = (threadId + 1) % gs_appInfo->info.nbThreads;
           i != threadId;
           i = (i + 1) % gs_appInfo->info.nbThreads) {
    volatile uint64_t ptr = P_epoch_ts[i][myEpoch];
    while (!isBit62One(ptr) && !gs_appInfo->info.isExit) { // when 1 thread exits it may forget to set this flag
      ptr = __atomic_load_n(&P_epoch_ts[i][myEpoch], __ATOMIC_ACQUIRE);
    }
  }
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}

void prune_log_forward_epoch_impa(int upToEpoch)
{
  // clean for everyone up to myEpoch
  int startPtr = (gs_log_data.log.epoch_end + 1) % gs_appInfo->info.allocEpochs;
  int endPtr   = (upToEpoch + 1) % gs_appInfo->info.allocEpochs; // shouldn't be +1 ?
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
    // if (!IS_SLOT_OCCUPIED(epoch, i)) {
    //   printf("INVALID%i(PREV): %16lx %16lx\n", i,
    //     EPOCH_READ_PTR((epoch+gs_appInfo->info.allocEpochs-1)%gs_appInfo->info.allocEpochs, 0),
    //     EPOCH_READ_PTR((epoch+gs_appInfo->info.allocEpochs-1)%gs_appInfo->info.allocEpochs, 1));
    //   printf("INVALID%i(CURR): %16lx %16lx\n", i, EPOCH_READ_PTR(epoch, 0),
    //     EPOCH_READ_PTR(epoch, 1));
    //   if (IS_SLOT_OCCUPIED(epoch, i)) {
    //     printf("(next) %i %i (end=%i)\n", G_next[0].log_ptrs.epoch_next, G_next[1].log_ptrs.epoch_next,
    //       gs_log_data.log.epoch_end);
    //     printf("fixed by itself...\n");
    //   } else {
    //     printf("(next) %i %i (end=%i)\n", G_next[0].log_ptrs.epoch_next, G_next[1].log_ptrs.epoch_next,
    //       gs_log_data.log.epoch_end);
    //     printf("not fixed!\n");
    //   }
    // }
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

static inline int
lockEpoch(int threadId, int epoch)
{
  // try different strategies here
  return (int)__sync_bool_compare_and_swap(&G_epoch_lock[epoch], 0, threadId+1);
  // if (G_epoch_lock[epoch] == threadId+1) {
  //   return 1;
  // } else {
  //   return 0;
  // }
}

static inline int
unlockEpoch(int threadId, int epoch)
{
  if (G_epoch_lock[epoch] == threadId+1) {
    __atomic_store_n(&G_epoch_lock[epoch], 0, __ATOMIC_RELEASE);
    return 1;
  } else {
    return 0;
  }
}
