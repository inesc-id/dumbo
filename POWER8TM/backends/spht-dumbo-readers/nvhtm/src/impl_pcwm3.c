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
static volatile __thread uint64_t myPreviousClock;

static volatile __thread int nbPrevThreads;
static volatile __thread int *prevThreadsArray;
static volatile __thread int *logPosArray;
// static volatile __thread int *prevLogPosArray;
static volatile __thread uint64_t *TSArray;
// static volatile __thread uint64_t *prevTSArray;
// static volatile __thread uint64_t *stableTSArray;

static volatile __thread int prevThread = -1;
static volatile __thread int prevLogPos = -1;
static volatile __thread uint64_t prevTS = 0;

static volatile __thread
  int writeLogStart, writeLogEnd;

static volatile __thread int canJumpToWait = 0;

static volatile __thread uint64_t timeFlushTS1 = 0;
static volatile __thread uint64_t timeFlushTS2 = 0;

static volatile __thread uint64_t timeScanTS1 = 0;
static volatile __thread uint64_t timeScanTS2 = 0;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;
static volatile __thread uint64_t timeFlushing = 0;
static volatile __thread uint64_t timeScanning = 0;
static volatile __thread uint64_t timeTX = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;
static volatile uint64_t incFlushing = 0;
static volatile uint64_t incScanning = 0;
static volatile uint64_t incTXTime_upd = 0;

void install_bindings_pcwm3()
{
  on_before_htm_begin  = on_before_htm_begin_pcwm3;
  on_htm_abort         = on_htm_abort_pcwm3;
  on_before_htm_write  = on_before_htm_write_8B_pcwm3;
  on_before_htm_commit = on_before_htm_commit_pcwm3;
  on_after_htm_commit  = on_after_htm_commit_pcwm3;

  wait_commit_fn = wait_commit_pcwm3;
}

void state_gather_profiling_info_pcwm3(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
  __sync_fetch_and_add(&incFlushing, timeFlushing);
  __sync_fetch_and_add(&incScanning, timeScanning);
  __sync_fetch_and_add(&incTXTime_upd, timeTX);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedUpdTX);
  __sync_fetch_and_add(&timeAbortedTX_global, timeAbortedROTX);


  timeSGL = 0;
  timeAbortedTX = 0;
  timeTX = 0;
  timeAfterTXSuc = 0;
  timeWaiting = 0;
  timeTotal = 0;
  countCommitPhases = 0;
}

void state_fprintf_profiling_info_pcwm3(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_TX",
              "TIME_WAIT",
              "TIME_SGL",
              "TIME_ABORTED_TX",
              "TIME_AFTER_TX_FAIL",
              "TIME_FLUSHING",
              "TIME_SCANNING");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incTXTime_upd, incWaiting, timeSGL_global, timeAbortedTX_global, 0L,
    incFlushing, incScanning);
}

static inline void fetch_log(int threadId)
{
  write_log_thread[writeLogEnd] = 0;
  write_log_thread[(writeLogEnd + 8) & (gs_appInfo->info.allocLogSize - 1)] = 0;
}

void on_before_htm_begin_pcwm3(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  write_log_thread = &(P_write_log[threadId][0]);
  
  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;

  fetch_log(threadId);
  myPreviousClock = zeroBit63(gs_ts_array[threadId].pcwm.ts);
  
  // TODO: write atomically these 2
  __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 0, __ATOMIC_RELEASE);

  write_log_thread[writeLogStart] = 0; // clears logPos values
  __m256i data256i = {
    rdtsc(),
    gs_ts_array[threadId].pcwm.prevTS,
    writeLogEnd,
    gs_ts_array[threadId].pcwm.prevLogPos
  };
  _mm256_store_si256((__m256i*)&gs_ts_array[threadId].pcwm, data256i);

  // __atomic_store_n(&gs_ts_array[threadId].pcwm.logPos, writeLogEnd, __ATOMIC_RELEASE);
  // __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE);

  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);

}

void on_htm_abort_pcwm3(int threadId)
{
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE);
}

void on_before_htm_write_8B_pcwm3(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
}

void on_before_htm_commit_pcwm3(int threadId)
{
  readClockVal = rdtscp();
}

static inline void scan_others(int threadId)
{
  // ------------------------------------------
  // before flagging, find the previous TX and write there my log position
  prevTS = gs_ts_array[threadId].pcwm.prevTS;
  prevThread = threadId;
  prevLogPos = gs_ts_array[threadId].pcwm.prevLogPos;
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  for (int i = gs_appInfo->info.nbThreads - 1; i >= 0; i--) {
    if (i == threadId) continue;

    __m256i data = _mm256_load_si256((__m256i *)&gs_ts_array[i].pcwm);
    // order matters !!!
    volatile uint64_t cpyTS = data[0];
    volatile uint64_t cpyPrevTS = data[1];
    volatile uint64_t cpyLogPos = data[2];
    volatile uint64_t cpyPrevLogPos = data[3];

    // volatile uint64_t cpyPrevTS = gs_ts_array[i].pcwm.prevTS;
    // volatile uint64_t cpyLogPos = gs_ts_array[i].pcwm.logPos;
    // volatile uint64_t cpyPrevLogPos = gs_ts_array[i].pcwm.prevLogPos;

    if (LARGER_THAN(readClockVal, zeroBit63(cpyTS), threadId, i)) {
      if (isBit63One(cpyTS)) { // is persistent (is stable)
        if (LARGER_THAN(zeroBit63(cpyTS), prevTS, i, prevThread)) {
          prevTS = zeroBit63(cpyTS);
          prevThread = i;
          prevLogPos = cpyLogPos;
        }
#ifndef DISABLE_PCWM_OPT
        if (LARGER_THAN(zeroBit63(cpyTS), readClockVal, i, threadId)) {
          canJumpToWait = 1;
        }
#endif
      } else { // is unstable
        prevThreadsArray[nbPrevThreads] = i;
        // prevLogPosArray[nbPrevThreads] = cpyPrevLogPos;
        logPosArray[nbPrevThreads] = cpyLogPos;
        TSArray[nbPrevThreads] = cpyTS;
        // prevTSArray[nbPrevThreads] = cpyPrevTS;

        nbPrevThreads++;
      }
    }

    if (LARGER_THAN(readClockVal, cpyPrevTS, threadId, i) && LARGER_THAN(cpyPrevTS, prevTS, i, prevThread)) {
      prevTS = zeroBit63(cpyPrevTS);
      prevThread = i;
      prevLogPos = cpyPrevLogPos;
    }
  }
}

// static __thread FILE *debug_log = NULL;

static inline void smart_close_log_pcwm(uint64_t marker, uint64_t *marker_pos)
{
  intptr_t lastCL  = ((uintptr_t)(&write_log_thread[writeLogEnd]) >> 6) << 6;
  intptr_t firstCL = ((uintptr_t)(&write_log_thread[writeLogStart]) >> 6) << 6;

  void *logStart = (void*) (write_log_thread + 0);
  void *logEnd   = (void*) (write_log_thread + gs_appInfo->info.allocLogSize);

  if (firstCL == lastCL) {
    // same cache line
    *marker_pos = marker;
    FLUSH_RANGE(firstCL, lastCL, logStart, logEnd);
  } else {
    intptr_t lastCLMinus1;
    if (lastCL == (uintptr_t)logStart) {
      lastCLMinus1 = (uintptr_t)logEnd;
    } else {
      lastCLMinus1 = lastCL - 1;
    }
    FLUSH_RANGE(firstCL, lastCLMinus1, logStart, logEnd);
    FENCE_PREV_FLUSHES();
    *marker_pos = marker;
    FLUSH_CL((void*)lastCL);
  }
}

void on_after_htm_commit_pcwm3(int threadId)
{
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, timeTX);
  int didTheFlush = 0;
  __m256i storeStableData;

  // if (!debug_log) {
  //   char logFileName[512];
  //   sprintf(logFileName, "%s%i", "debug_log_", threadId);
  //   debug_log = fopen(logFileName, "w");
  // }

  int _prevThreadsArray[gs_appInfo->info.nbThreads];
  int _logPosArray[gs_appInfo->info.nbThreads];
  // int _prevLogPosArray[gs_appInfo->info.nbThreads];
  uint64_t _TSArray[gs_appInfo->info.nbThreads];
  // uint64_t _prevTSArray[gs_appInfo->info.nbThreads];
  // uint64_t _stableTSArray[gs_appInfo->info.nbThreads];
  uint64_t advertiseEntry;
  // uint64_t advertiseEntryVal;

  nbPrevThreads = 0;
  prevThreadsArray = _prevThreadsArray;
  logPosArray = _logPosArray;
  // prevLogPosArray = _prevLogPosArray;
  TSArray = _TSArray;
  // stableTSArray = _stableTSArray;
  // prevTSArray = _prevTSArray;

  // gs_ts_array[threadId].pcwm.ts = readClockVal; // currently has the TS of begin
  // tells the others my TS taken within the TX
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, readClockVal, __ATOMIC_RELEASE);
  
  // TODO: check if it does not break anything
  if (((writeLogStart + 1) & (gs_appInfo->info.allocLogSize - 1)) == writeLogEnd) {
    // tells the others to move on
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, (uint64_t)-1, __ATOMIC_RELEASE);
    goto ret;
  }

  __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 1, __ATOMIC_RELEASE);

  MEASURE_TS(timeFlushTS1);

  // tells the following TXs my stable TS
  __atomic_store_n(&write_log_thread[writeLogStart], onesBit63(readClockVal), __ATOMIC_RELEASE);

  MEASURE_TS(timeScanTS1);
  scan_others(threadId);
  MEASURE_TS(timeScanTS2);

  // int prevWriteLogEnd = writeLogEnd;
  // writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1); // needed
  smart_close_log_pcwm(
    /* commit value */ -1,
    /* marker position */ (uint64_t*)&(write_log_thread[writeLogEnd])
  );
  // writeLogEnd = prevWriteLogEnd;

  // -------------------
  /** OLD */
  // // flush log entries
  // writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) & (gs_appInfo->info.allocLogSize - 1);
  // FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
  //   &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  // writeLogEnd = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  // FENCE_PREV_FLUSHES();
  // /* Commits the write log (commit marker) */
  // write_log_thread[writeLogEnd] = (uint64_t)-1; // onesBit63(readClockVal);
  // FLUSH_CL(&write_log_thread[writeLogEnd]);
  /** OLD */
  // -------------------

  INC_PERFORMANCE_COUNTER(timeScanTS1, timeScanTS2, timeScanning);
  
// at this point you know the earliest tx that precedes you and that has already durably committed..
// there may still be txs preceding you that have not durably committed yet
// ------------------------------------------
  
  // tells the others flush done!
  FENCE_PREV_FLUSHES();
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(readClockVal), __ATOMIC_RELEASE);

  MEASURE_TS(timeFlushTS2);
  INC_PERFORMANCE_COUNTER(timeFlushTS1, timeFlushTS2, timeFlushing);

  // now: is it safe to return to the application?
  // first wait preceding TXs
  canJumpToWait = 0;

  wait_commit_fn(threadId);
  // all preceding TXs flushed their logs, and we know the intention

  if (canJumpToWait) goto waitTheMarker;

  // second verify it the checkpointer will reproduce our log
  uint64_t oldVal, oldVal2;
putTheMarker:
  if ((oldVal = __atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE)) < readClockVal) {
    int success = 0;
    // it will not be reproduced, need to change it
    while (__atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE) < readClockVal) {
      oldVal2 = __sync_val_compare_and_swap(&P_last_safe_ts->ts, oldVal, readClockVal);
      success = (oldVal2 == oldVal);
      oldVal = oldVal2;
    }
    if (success) {
      FLUSH_CL(&P_last_safe_ts->ts);
      FENCE_PREV_FLUSHES();
      didTheFlush = 1;
// -------------------------------
      // tells the others that I've managed to flush up to my TS
      // __atomic_store_n(&gs_ts_array[threadId].comm2.globalMarkerTS, readClockVal, __ATOMIC_RELEASE);
      // TODO: now using the same cacheline
      __atomic_store_n(&gs_ts_array[threadId].pcwm.flushedMarker, readClockVal, __ATOMIC_RELEASE);
// -------------------------------
      // this may fail, I guess a more recent transaction will update the TS...
      // at this point it should be guaranteed that the checkpointer sees:
      //  P_last_safe_ts->ts >= readClockVal (it ignores bit 63)
      // TODO: while enabling this do not forget zeroBit63 whenever you read P_last_safe_ts
      // __sync_val_compare_and_swap(&P_last_safe_ts->ts, readClockVal, onesBit63(readClockVal));
    }
  }
waitTheMarker:
  if (!didTheFlush) {
    // tried the following (seems to be slightly faster):
// -------------------------------
    volatile uint64_t spinCount = 0;
    while (1) {
      // while (P_last_safe_ts->ts < readClockVal) {
      //   _mm_pause();
      //   spinCount++;
      //   if (spinCount > 10000000) goto putTheMarker;
      // }; // need to wait
      // // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      spinCount++;
      for (int i = gs_appInfo->info.nbThreads - 1; i >= 0; i--) {
        if (__atomic_load_n(&gs_ts_array[i].pcwm.flushedMarker, __ATOMIC_ACQUIRE) >= readClockVal) {
          goto outerLoop;
        }
      }
      _mm_pause();
      if (spinCount > 10000) {
        // goto putTheMarker;
      }
    }
// -------------------------------
    // // need to be sure it was flushed
    // while (!isBit63One(P_last_safe_ts->ts)); // TODO: the code before seems slightly faster
  }
outerLoop:
  
// ------------------------------------------
  // TODO: non-atomic
  // __atomic_store_n(&gs_ts_array[threadId].pcwm.prevLogPos, gs_ts_array[threadId].pcwm.logPos, __ATOMIC_RELEASE);
  // __atomic_store_n(&gs_ts_array[threadId].pcwm.prevTS, readClockVal, __ATOMIC_RELEASE);
  // when zeroBit63(P_last_safe_ts->ts) >= readClockVal we can return to the app
  storeStableData = (__m256i){
    gs_ts_array[threadId].pcwm.ts, // does not change
    readClockVal,
    gs_ts_array[threadId].pcwm.logPos, // does not change
    gs_ts_array[threadId].pcwm.logPos
  };
  _mm256_store_si256((__m256i*)&gs_ts_array[threadId].pcwm, storeStableData);

  // first transaction cannot write!
  if (prevTS == 0) {
    // printf("[%i] did not find previous TX\n", threadId);
    goto ret_update;
  }

  // now go there and write the entry
  // advertiseEntryVal = P_write_log[prevThread][prevLogPos];
  advertiseEntry = ((uint64_t)prevThread << 32) | prevLogPos;

  // if (!__sync_bool_compare_and_swap(&P_write_log[prevThread][prevLogPos], advertiseEntryVal, onesBit63(advertiseEntry))) {
  //   printf("[%i] Cannot write on the previous TX log\n", threadId);
  // }
  // fprintf(debug_log, "myTS=%lx (idx=%i) prevTS=%lx (thread=%i idx=%i) unstable: %lx stable: %lx\n", 
  //   readClockVal, writeLogStart, prevTS, prevThread, prevLogPos, TSArray[0], stableTSArray[0]);
  P_write_log[threadId][writeLogEnd] = onesBit63(advertiseEntry);

  // }
  // advertiseEntry = ((uint64_t)threadId << 32) | writeLogStart;
  // P_write_log[prevThread][prevLogPos] = onesBit63(advertiseEntry);

// ------------------------------------------

ret_update:
  // place the next ptr after the TS
  G_next[threadId].log_ptrs.write_log_next = (writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
ret:
  MEASURE_INC(countCommitPhases);
}

void wait_commit_pcwm3(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  volatile uintptr_t myTS = gs_ts_array[threadId].pcwm.ts;
  volatile uint64_t snapshotTS[gs_appInfo->info.nbThreads];

  uint64_t ts;
  for (int j = 0; j < nbPrevThreads; ++j) {
    int i = prevThreadsArray[j];
    // either the unstable TS become stable OR is still unstable but larger than myTS
    // in the second case check the prevTS
    do {
      snapshotTS[i] = gs_ts_array[i].pcwm.ts;
      // _mm_pause();
    } while (!(LARGER_THAN(zeroBit63(snapshotTS[i]), myTS, i, threadId) || isBit63One(snapshotTS[i])));

#ifndef DISABLE_PCWM_OPT
    if (LARGER_THAN(zeroBit63(snapshotTS[i]), readClockVal, i, threadId)
        && __atomic_load_n(&gs_ts_array[i].pcwm.isUpdate, __ATOMIC_ACQUIRE) == 1) {
      canJumpToWait = 1;
    }
#endif

    ts = zeroBit63(P_write_log[i][logPosArray[j]]);
    // stableTSArray[j] = ts;

    // read-only transactions break this

    if (isBit63One(ts)) {
      continue;
    }

    if (LARGER_THAN(readClockVal, ts, threadId, i) && LARGER_THAN(ts, prevTS, i, prevThread)) {
      prevTS = ts;
      prevThread = i;
      prevLogPos = logPosArray[j];
    }
  }

  // if (nbPrevThreads > 0) {
  //   printf("[%i] TS %lx > %lx (t%i): %i unstable t%i pTS=%lx TS=%lx logPos = %lx/%lx\n",
  //     threadId, readClockVal & 0xFFFFFF, prevTS & 0xFFFFFF, prevThread, nbPrevThreads,
  //     prevThreadsArray[0], prevTSArray[0] & 0xFFFFFF, TSArray[0] & 0xFFFFFF,
  //     ts , P_write_log[prevThreadsArray[0]][logPosArray[0]]  & 0xFFFFFF);
  // } else {
  //   printf("[%i] TS %lx > %lx (t%i) \n",
  //     threadId, readClockVal & 0xFFFFFF, prevTS & 0xFFFFFF, prevThread);
  // }

  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
