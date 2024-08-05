#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

// /* DEBUG */static const char log_replay_print_file_name[1024] = "DEBUG_worker";
// /* DEBUG */static FILE *log_replay_print_file = NULL;

typedef uintptr_t bit_array_t;

/* extern */volatile cc_htm_queue_s *ccHTM_Q;

static volatile __thread uint64_t readClockVal;

static volatile __thread uint64_t myId;
uint64_t __thread advertiseEntry;

static volatile __thread
  int writeLogStart, writeLogEnd;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeTX_upd = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incTXTime_upd = 0;
static volatile uint64_t incWaiting = 0;
extern __thread uint64_t timeAbortedTX;

void install_bindings_ccHTM()
{
  on_before_htm_begin  = on_before_htm_begin_ccHTM;
  on_htm_abort         = on_htm_abort_ccHTM;
  on_before_htm_write  = on_before_htm_write_8B_ccHTM;
  on_before_htm_commit = on_before_htm_commit_ccHTM;
  on_after_htm_commit  = on_after_htm_commit_ccHTM;

  wait_commit_fn = wait_commit_ccHTM;
}

void state_gather_profiling_info_ccHTM(int threadId)
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

void state_fprintf_profiling_info_ccHTM(char *filename)
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

void on_before_htm_begin_ccHTM(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  MEASURE_TS(timeTotalTS1);

  // if (log_replay_print_file == NULL) {
  //   log_replay_print_file = fopen(log_replay_print_file_name, "w");
  // }

  write_log_thread = &(P_write_log[threadId][0]);  
  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;

  myId = __sync_add_and_fetch(&(ccHTM_Q->txCounter), 1);
  ccHTM_Q->entries[myId & (CCHTM_QUEUE_SIZE - 1)].nextRedIdx = CC_HTM_RED_Q_END; // by default
  __atomic_store_n(&ccHTM_Q->entries[myId & (CCHTM_QUEUE_SIZE - 1)].startTS, myId, __ATOMIC_RELEASE);
  write_log_thread[writeLogEnd] = myId;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  fetch_log(threadId); // prefetches a bit of log before starting the TX
}

void on_htm_abort_ccHTM(int threadId) { /* empty */ }

void on_before_htm_write_8B_ccHTM(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
}

void on_before_htm_commit_ccHTM(int threadId)
{
  readClockVal = rdtscp();
}

// must return the parent node:
//   - traverse the queue
//   - when the TS is larger than myTS return the parent
static inline uint64_t find_insert_node(uint64_t token, uint64_t persistentTS)
{
  volatile uint64_t head = ccHTM_Q->redHeadIdx;
  if (head == CC_HTM_RED_Q_HEAD) {
    return CC_HTM_RED_Q_HEAD;
  }

  if (isBit63One(head)) {
    head = zeroBit63(head);
  }

  volatile uint64_t idx = head;
  volatile uint64_t next_idx = idx;
  volatile uint64_t pTS = ccHTM_Q->entries[idx].persistTS;
  while (!isBit63One(next_idx) && pTS < persistentTS) { // TODO: same TS transactions?
    next_idx = ccHTM_Q->entries[idx].nextRedIdx;
    if (!isBit63One(next_idx)) {
      idx = next_idx;
      pTS = ccHTM_Q->entries[idx].persistTS;
    }
  }
  // return the parent node
  return idx;
}

void on_after_htm_commit_ccHTM(int threadId)
{
  MEASURE_INC(countCommitPhases);
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, timeTX_upd);

  uint64_t currentTxCounter;
  uint64_t idx;
  uint64_t isReadOnly = (writeLogEnd == ((writeLogStart + 1) & (gs_appInfo->info.allocLogSize - 1)));

  currentTxCounter = ccHTM_Q->txCounter;
  idx = myId & (CCHTM_QUEUE_SIZE - 1);

  write_log_thread[writeLogEnd] = onesBit63(currentTxCounter); 
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  write_log_thread[writeLogEnd] = onesBit63(readClockVal); // log as enough info to reconstruct the table

  FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
    &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);

  // notify end
  ccHTM_Q->entries[idx].persistTS = readClockVal;
  while (1) {
    uint64_t i = find_insert_node(idx, readClockVal);
    volatile uint64_t head = ccHTM_Q->redHeadIdx;
    volatile uint64_t changeNext;
    
    if (!isBit63One(i)) {
      changeNext = ccHTM_Q->entries[i].nextRedIdx; // many not be needed
      if (changeNext == CC_HTM_RED_Q_EMPTY) continue; // BUG
    }

    if (i != CC_HTM_RED_Q_HEAD && i != CC_HTM_RED_Q_END && i != CC_HTM_RED_Q_EMPTY && isBit63One(i)) {
      // printf("checkpointer is using it - 1\n");
      continue;
    }

    if (i != CC_HTM_RED_Q_HEAD && i != CC_HTM_RED_Q_END && i != CC_HTM_RED_Q_EMPTY &&
        ((changeNext != CC_HTM_RED_Q_EMPTY && changeNext != CC_HTM_RED_Q_END && isBit63One(changeNext))
        || changeNext == CC_HTM_RED_Q_BUSY)) {
      // checkpointer is using it
      // printf("checkpointer is using it - 2\n");
      continue;
    }

    if (head != CC_HTM_RED_Q_HEAD && isBit63One(head)) {
      // printf("checkpointer is using it - 3\n");
      continue;
    }

    if (i != CC_HTM_RED_Q_HEAD) {
      ccHTM_Q->entries[idx].nextRedIdx = changeNext; // by default it has an END
    } else {
      ccHTM_Q->entries[idx].nextRedIdx = CC_HTM_RED_Q_END;
    }

    __atomic_thread_fence(__ATOMIC_RELEASE);
    
    // assert(i != CC_HTM_RED_Q_END);
    if (i == CC_HTM_RED_Q_END) {
      printf("BUG!!!\n");
    }

    if (i == CC_HTM_RED_Q_HEAD) {
      if (__sync_bool_compare_and_swap(&(ccHTM_Q->redHeadIdx), CC_HTM_RED_Q_HEAD, idx)) {
        // fprintf(log_replay_print_file, "[%i] changed empty head to idx=%li\n", threadId, idx);
        break;
      }
    } else if (__sync_bool_compare_and_swap(&(ccHTM_Q->entries[i].nextRedIdx), ccHTM_Q->entries[idx].nextRedIdx, idx)) {
      // fprintf(log_replay_print_file, "[%i] idx=%li changed from %lx to idx=%li, head is = %li (before = %li)\n",
      //   threadId, i, ccHTM_Q->entries[idx].nextRedIdx, idx, ccHTM_Q->redHeadIdx, head);
      break;
    }
  }
  __atomic_store_n(&(ccHTM_Q->entries[idx].endTS), currentTxCounter, __ATOMIC_RELEASE);

  // commit
  writeLogStart = (writeLogStart + 1) & (gs_appInfo->info.allocLogSize - 1); // moves the startTS entry
  advertiseEntry = ((uint64_t)threadId << 32) | writeLogStart;
  advertiseEntry = onesBit63(advertiseEntry);
  FENCE_PREV_FLUSHES(); // makes sure the CLWB finished
  __atomic_store_n(&(ccHTM_Q->entries[myId & (CCHTM_QUEUE_SIZE - 1)].logPtr), advertiseEntry, __ATOMIC_RELEASE);

  if (!isReadOnly) {
    MEASURE_TS(timeWaitingTS1);
    wait_commit_fn(threadId);
    MEASURE_TS(timeWaitingTS2);
    INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
  }

  // advances 2 cachelines to avoid conflicts with the checkpointer
  // writeLogEnd = (writeLogEnd + 8) & (gs_appInfo->info.allocLogSize - 1);

  G_next[threadId].log_ptrs.write_log_next = writeLogEnd;
}

void wait_commit_ccHTM(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  // TODO: wait the applier thread

  while (!gs_appInfo->info.isExit &&
      __atomic_load_n(&(ccHTM_Q->entries[myId & (CCHTM_QUEUE_SIZE - 1)].logPtr), __ATOMIC_ACQUIRE) == advertiseEntry) {
    // wait the applier to apply the TX
  }
  
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
