#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

#define CRAFTY_REDO 1

/* extern */int isCraftySet;

static volatile __thread uint64_t readClockVal;

static volatile __thread long logSize;
static volatile __thread int completedInSGL = 0;
static volatile __thread int lastValFailed = 0;

static volatile __thread
  long writeLogStart, writeLogEnd;

volatile __thread int crafty_isValidate = 0; // if redo fails go to validate

#define MAX_REDO_LOG_SIZE 2097152

static volatile __thread uint64_t redoLog[MAX_REDO_LOG_SIZE];
static volatile __thread int redoLogStart, redoLogEnd;

static __thread jmp_buf *respawn_point;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;
static volatile __thread uint64_t timeTX_upd = 0;
static volatile __thread uint64_t redoHTMCommits = 0;
static volatile __thread uint64_t redoFallbacks = 0;
static volatile __thread uint64_t redoAborts = 0;
static volatile __thread uint64_t normalHTMTXs = 0;
static volatile __thread uint64_t validationHTMTXs = 0;
static volatile __thread uint64_t validationHTMAborts = 0;
static volatile __thread uint64_t startCommitPhase = 0;
volatile __thread uint64_t successCommitPhase = 0;
volatile __thread uint64_t failCommitPhase = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE))) cache_line_s padded_gLastRedoTS;
static volatile __thread uint64_t *gLastRedoTS = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incNbTransactions = 0;
static volatile uint64_t incAfterTxSuc = 0;
static volatile uint64_t incAfterTxFail = 0;
static volatile uint64_t incWaiting = 0;
static volatile uint64_t incTXTime_upd = 0;
static volatile uint64_t incRedoHTMCommits = 0;
static volatile uint64_t incRedoFallbacks = 0;
static volatile uint64_t incRedoAborts = 0;
static volatile uint64_t incNormalHTMTXs = 0;
static volatile uint64_t incValidationHTMTXs = 0;
static volatile uint64_t incValidationHTMAborts = 0;
static volatile uint64_t incStartCommitPhase = 0;
static volatile uint64_t incFailCommitPhase = 0;
static volatile uint64_t incSuccessCommitPhase = 0;
extern __thread uint64_t timeAbortedTX;

void install_bindings_crafty()
{
  on_before_htm_begin  = on_before_htm_begin_crafty;
  on_htm_abort         = on_htm_abort_crafty;
  on_before_htm_write  = on_before_htm_write_8B_crafty;
  on_before_htm_commit = on_before_htm_commit_crafty;
  on_after_htm_commit  = on_after_htm_commit_crafty;
}

void state_gather_profiling_info_crafty(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incStartCommitPhase, startCommitPhase);
  __sync_fetch_and_add(&incFailCommitPhase, failCommitPhase);
  __sync_fetch_and_add(&incSuccessCommitPhase, successCommitPhase);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incNbTransactions, nbTransactions);
  __sync_fetch_and_add(&incAfterTxSuc, timeAfterTXSuc);
  __sync_fetch_and_add(&incAfterTxFail, timeAfterTXFail);
  __sync_fetch_and_add(&incTXTime_upd, timeTX_upd);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
  __sync_fetch_and_add(&incRedoHTMCommits, redoHTMCommits);
  __sync_fetch_and_add(&incRedoFallbacks, redoFallbacks);
  __sync_fetch_and_add(&incRedoAborts, redoAborts);
  __sync_fetch_and_add(&incNormalHTMTXs, normalHTMTXs);
  __sync_fetch_and_add(&incValidationHTMTXs, validationHTMTXs);
  __sync_fetch_and_add(&incValidationHTMAborts, validationHTMAborts);
  __sync_fetch_and_add(&timeSGL_global, timeSGL);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedUpdTX);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedROTX);


  countCommitPhases = 0;
  startCommitPhase = 0;
  successCommitPhase = 0;
  failCommitPhase = 0;
  timeTotal = 0;
  nbTransactions = 0;
  timeAfterTXSuc = 0;
  timeAfterTXFail = 0;
  timeWaiting = 0;
  redoHTMCommits = 0;
  redoHTMCommits = 0;
  redoFallbacks = 0;
  redoAborts = 0;
  normalHTMTXs = 0;
  validationHTMTXs = 0;
  validationHTMAborts = 0;
  timeSGL = 0;
  timeAbortedUpdTX = 0;
  timeAbortedROTX = 0;
}

void state_fprintf_profiling_info_crafty(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_TX",
              "TIME_WAIT",
              "TIME_SGL",
              "TIME_ABORTED_TX",
              "TIME_AFTER_TX_FAIL",
              "REDO_HTM_COMMIT",
              "REDO_FALLBACK",
              "REDO_ABORTS");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTxSuc, incTXTime_upd, incWaiting, timeSGL_global, timeAbortedTX_global, incAfterTxFail,
    incRedoHTMCommits, incRedoFallbacks, incRedoAborts);
}

static inline void fetch_log(int threadId)
{
  write_log_thread[writeLogEnd] = 0;
  write_log_thread[(writeLogEnd + 8) & (logSize - 1)] = 0;
  redoLog[redoLogEnd] = 0;
  redoLog[(redoLogEnd + 8) & (MAX_REDO_LOG_SIZE - 1)] = 0;
}

void crafty_set_respawn_point(jmp_buf *env)
{
  respawn_point = env;
}

inline static void rollback(int threadId, int doRedo)
{
#ifdef CRAFTY_REDO
  if (doRedo) {
    int i;
    for (i = writeLogStart; ; i = ((i + 2) & (logSize - 1))) {
      void *addr = (uint64_t*)(write_log_thread[i]);
      if (addr == NULL || isBit63One((uint64_t)addr)) break;
      // store redo entry
      redoLog[redoLogEnd] = (uint64_t)addr;
      redoLogEnd = (redoLogEnd + 1) & (MAX_REDO_LOG_SIZE - 1);
      redoLog[redoLogEnd] = *(uint64_t*)addr;
      redoLogEnd = (redoLogEnd + 1) & (MAX_REDO_LOG_SIZE - 1);
    }
  }
#endif /* CRAFTY_REDO */
int i;
  for (i = ((writeLogEnd - 2) & (logSize - 1));
            i != ((writeLogStart - 2) & (logSize - 1));
            i = ((i - 2) & (logSize - 1))) {
    // undo
    // printf("[%i] undo %p <-- %lx\n", threadId, (uint64_t*)(write_log_thread[i]), (uint64_t)write_log_thread[(i + 1) & (logSize - 1)]);
    *(uint64_t*)(write_log_thread[i]) = (uint64_t)write_log_thread[(i + 1) & (logSize - 1)];
  }
}

void on_before_htm_begin_crafty(int threadId)
{
  gLastRedoTS = (uint64_t*)&(padded_gLastRedoTS.ts);
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  logSize = gs_appInfo->info.allocLogSize;
  write_log_thread = &(P_write_log[threadId][0]);

  if (crafty_isValidate) {
    validationHTMTXs++;
    // /*if (threadId==0) */printf("[%i] BeginTX isValidate\n", threadId);
  } else {
    // /*if (threadId==0) */printf("[%i] BeginTX isUndo\n", threadId);
    fetch_log(threadId);
    normalHTMTXs++;
  }

  if (lastValFailed && !crafty_isValidate) {
    lastValFailed = 0;
    HTM_SGL_budget = 0;
  }

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  redoLogEnd = redoLogStart = 0;
}

void on_before_htm_write_8B_crafty(int threadId, void *addr, uint64_t val)
{
  if (!crafty_isValidate) {
    // printf("%li: %p <- %lx (redo value = %lx)\n", writeLogEnd, addr, *(uint64_t*)addr, val);
    write_log_thread[writeLogEnd] = (uint64_t)addr;
    writeLogEnd = (writeLogEnd + 1) & (logSize - 1);
    write_log_thread[writeLogEnd] = *(uint64_t*)addr;
    writeLogEnd = (writeLogEnd + 1) & (logSize - 1);
  } else {
    // printf("[%i] %li: addr %p <-- %lx (redo: %lx)\n", threadId, writeLogEnd, addr, *(uint64_t*)addr, val);
    if (addr != (uint64_t*)(write_log_thread[writeLogEnd]) ||
        (uint64_t)(write_log_thread[(writeLogEnd + 1) & (logSize - 1)]) != *(uint64_t*)addr) {
      // printf("[%i] failed commit %p <-- %lx (log: %lx) | SGL=%i\n", threadId, addr, *(uint64_t*)addr,
      //   (uint64_t)(write_log_thread[(writeLogEnd + 1) & (logSize - 1)]), completedInSGL);
      // check failed: there was a write in between, must abort
      if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) {
        HTM_named_abort(0x1);
      } else {
        // if (threadId==0) printf("[%i] aborted 3 \n", threadId);
        crafty_isValidate = 0;
        lastValFailed = 1;
        rollback(threadId, 0);
        HTM_exit_fallback();
        if (respawn_point != NULL) {
          longjmp(*respawn_point, 0x1);
        }
      }
    }
    writeLogEnd = (writeLogEnd + 2) & (logSize - 1);
  }
}

void on_before_htm_commit_crafty(int threadId)
{
  if (crafty_isValidate) {
    if (!isBit63One(write_log_thread[writeLogEnd]))
    {
      // validation failed
      if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) {
        HTM_named_abort(0x1);
      } else {
        // should not fail validation
        crafty_isValidate = 0;
        rollback(threadId, 0);
        HTM_exit_fallback();
        if (respawn_point != NULL) {
          longjmp(*respawn_point, 0x1);
        }
      }
    }
    
#ifdef CRAFTY_REDO
    __atomic_store_n(gLastRedoTS, 1, __ATOMIC_RELEASE); // makes sure the other does not miss the TSC
#endif /* CRAFTY_REDO */
    readClockVal = rdtscp();
#ifdef CRAFTY_REDO
    *gLastRedoTS = readClockVal;
#endif /* CRAFTY_REDO */

    writeLogEnd = (writeLogEnd + 1) & (logSize - 1);
    write_log_thread[writeLogEnd] = onesBit63(readClockVal); // undo log is closed, no longer used

  } else if (writeLogStart != writeLogEnd) {
    readClockVal = rdtsc();
    write_log_thread[writeLogEnd] = onesBit63(readClockVal);

    /* undo the TX */
    if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) {
      rollback(threadId, 1);
    } else {
      // flush written to
      int i;
      for (i = writeLogStart; ; i = ((i + 2) & (logSize - 1))) {
        // Flush the writes in the log
        void *addr = (uint64_t*)(write_log_thread[i]);
        if (addr == NULL || isBit63One((uint64_t)addr)) break;
        FLUSH_CL(addr);
      }
      completedInSGL = 1;
    }
  }
}

void on_after_htm_commit_crafty(int threadId)
{
  if (crafty_isValidate) {
    goto complete;
  }
  startCommitPhase++;
  if (writeLogStart == writeLogEnd) {
    goto complete;
  }
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, timeTX_upd);
  
  /* flush the undo log */
  FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
    &write_log_thread[0], write_log_thread + logSize);

  if (completedInSGL) {
    readClockVal = rdtsc();
    writeLogEnd = (writeLogEnd + 1) & (MAX_REDO_LOG_SIZE - 1);
    write_log_thread[writeLogEnd] = onesBit63(readClockVal); // COMMITTED
    FLUSH_CL(&(write_log_thread[writeLogEnd]));
    FENCE_PREV_FLUSHES(); // Log must hit memory before changes
    goto ret;
  }

  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, readClockVal, __ATOMIC_RELEASE);

#ifdef CRAFTY_REDO
  // it will be undone
  /* REDO TX BEGIN */
  HTM_SGL_budget = HTM_SGL_INIT_BUDGET; /* HTM_get_budget(); */
  while (1) {
    if (ENTER_HTM_COND(HTM_SGL_tid, HTM_SGL_budget)) {
      CHECK_SGL_NOTX();
      if (START_TRANSACTION(HTM_SGL_status)) {
        // HTM_SGL_budget = HTM_update_budget(HTM_SGL_budget, HTM_SGL_status); // TODO: stats
        UPDATE_BUDGET(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); // TODO: stats
        if (HTM_get_named(HTM_SGL_status) == 0x1) {
          // redo failed
redo_abort:
          redoAborts++;
          crafty_isValidate = 1;
          // retry the transaction in validate mode
          // writeLogEnd = writeLogStart; // it is reset up
          MEASURE_TS(timeTotalTS2); // it is going for validation....
          INC_PERFORMANCE_COUNTER(timeTotalTS1, timeTotalTS2, timeTotal);
          INC_PERFORMANCE_COUNTER(timeAfterTXTS1, timeTotalTS2, timeAfterTXFail);
          MEASURE_TS(timeTotalTS1);
          if (respawn_point != NULL) {
            longjmp(*respawn_point, 0x1);
          } else {
            return; // TODO
          }
        }
        continue;
      }
      CHECK_SGL_HTM();
    } else {
      ENTER_SGL(HTM_SGL_tid);
    }
    break;
  }
  /* ************* */

  // inside REDO transaction

  // Redo phase
  if (*gLastRedoTS >= readClockVal) {
    // abort
    if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) {
      HTM_named_abort(0x1);
    } else {
      EXIT_SGL();
      goto redo_abort;
    }
  }
  // check passed: redo
  int j;
  for (j = redoLogStart;
        j != redoLogEnd;
        j = (j + 2) & (MAX_REDO_LOG_SIZE - 1)) {
    uint64_t *addr = redoLog[j];
    uint64_t val = (uint64_t)redoLog[(j + 1) & (MAX_REDO_LOG_SIZE - 1)];
    *addr = val;
  }

  __atomic_store_n(gLastRedoTS, 1, __ATOMIC_RELEASE);
  readClockVal = rdtscp();
  writeLogEnd = (writeLogEnd + 1) & (logSize - 1);
  write_log_thread[writeLogEnd] = onesBit63(readClockVal); // COMMITTED

  *gLastRedoTS = readClockVal;

  /* REDO TX COMMIT */
  if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) {
      HTM_commit(); // TODO stats
      redoHTMCommits++;
    // printf("[%i] redo done in TX\n", threadId);
  } else {
      HTM_exit_fallback();
      redoFallbacks++;
    // printf("[%i] redo done in SGL\n", threadId);
  }
  FLUSH_CL(&(write_log_thread[writeLogEnd]));
  FENCE_PREV_FLUSHES();
  /* ************** */
#else /* !CRAFTY_REDO */
  if (!crafty_isValidate) {
    crafty_isValidate = 1;
    // retry the transaction in validate mode
    // printf("[%i][%li] Validate %i entries (addr=%p <-- %lx)\n", threadId, countCommitPhases,
    //   (writeLogEnd - writeLogStart + logSize) & (logSize - 1), (uint64_t*)write_log_thread[writeLogStart],
    //   *(uint64_t*)write_log_thread[writeLogStart]);
    writeLogEnd = writeLogStart;
    if (respawn_point != NULL) {
      longjmp(*respawn_point, 0x1);
    } else {
      return; // TODO
    }
  }
#endif

complete:

  // flush values written to (TODO: this should somewhere up, before the COMMITTED marker)
  if (writeLogStart != writeLogEnd) {
    int i;
    for (i = writeLogStart; ; i = ((i + 2) & (logSize - 1))) {
      // Flush the writes in the log
      void *addr = (uint64_t*)(write_log_thread[i]);
      if (addr == NULL || isBit63One((uint64_t)addr)) break;
      FLUSH_CL(addr);
    }
  }

ret:

  // wait preceding transactions
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, readClockVal, __ATOMIC_RELEASE);
  int canReturn = 0;
  while (!canReturn) {
    canReturn = 1;
    int i;
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (i == threadId) continue;
      unsigned long otherTs = __atomic_load_n(&gs_ts_array[i].pcwm.ts, __ATOMIC_ACQUIRE);
      if (!isBit63One(otherTs) && zeroBit63(otherTs) < readClockVal) {
        canReturn = 0;
        break;
      }
    }
  }
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(readClockVal), __ATOMIC_RELEASE);


  G_next[threadId].log_ptrs.write_log_next = writeLogEnd;

  if (crafty_isValidate) {
    // validate successful
    FLUSH_CL(&(write_log_thread[writeLogEnd]));
    FENCE_PREV_FLUSHES();
    G_next[threadId].log_ptrs.write_log_next = writeLogEnd;
  }
  // if (threadId==0) printf("[%i][%i] committed %i entries!\n", threadId, countCommitPhases,
  //   (writeLogEnd - writeLogStart + logSize) & (logSize - 1));
  countCommitPhases++;
  crafty_isValidate = 0;
  lastValFailed = 0;
  completedInSGL = 0;
}

void on_htm_abort_crafty(int threadId)
{
  // if (threadId==0) printf("[%i] aborted --- \n", threadId);
  if (lastValFailed && !crafty_isValidate) {
    lastValFailed = 0;
    HTM_SGL_budget = 0;
  }
  if (crafty_isValidate) {
    validationHTMAborts++;
    if (HTM_get_named(HTM_SGL_status) == 0x1) {
      // validate failed restart in undo mode
      failCommitPhase++;
      lastValFailed = 1;
      crafty_isValidate = 0;
      MEASURE_TS(timeTotalTS2);
      INC_PERFORMANCE_COUNTER(timeTotalTS1, timeTotalTS2, timeTotal);
      INC_PERFORMANCE_COUNTER(timeAfterTXTS1, timeTotalTS2, timeAfterTXFail);
      MEASURE_TS(timeTotalTS1);
      MEASURE_TS(timeAfterTXTS1);
      // // retry the transaction in validate mode
      // if (respawn_point != NULL) {
      //   longjmp(*respawn_point, 0x1);
      // } else {
      //   return; // TODO
      // }
    }
  }
}

void *craftyMalloc(int tid, long size)
{
  void * res = (void*)-1;
  if (!crafty_isValidate) {
    res = nvmalloc_local(tid, size);
    onBeforeWrite(HTM_SGL_tid, res, size);
  } else {
    // is validate
    res = (void*)(write_log_thread[writeLogEnd]);
    // printf("%li: %p <- %lx (redo value = ?)\n", writeLogEnd, res, *(uint64_t*)res);
    // long loggedSize = (void*)(write_log_thread[(writeLogEnd + 1) & (logSize - 1)]);
    // if (loggedSize != size) {
    //   printf("malloc size mismatch logged size=%li(logged) != %li\n", loggedSize, size);
    // }
    // printf("res=%p\n", res);
    writeLogEnd = (writeLogEnd + 2) & (logSize - 1);
  }
  return res;
}

