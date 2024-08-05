#pragma once

#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

#define LARGER_THAN(_TSi, _TSj, _i, _j) ((_TSi > _TSj) || (_TSi == _TSj && _i > _j))

typedef uintptr_t bit_array_t;

extern __thread int PCWM2_readonly_tx;

extern volatile __thread uint64_t PCWM2_readClockVal;
extern volatile __thread uint64_t PCWM2_myPreviousClock;

extern volatile __thread int PCWM2_nbPrevThreads;
extern volatile __thread int *PCWM2_prevThreadsArray;
extern volatile __thread int *PCWM2_logPosArray;
// extern volatile __thread int *prevLogPosArray;
extern volatile __thread uint64_t *PCWM2_TSArray;
// extern volatile __thread uint64_t *prevTSArray;

extern volatile __thread int PCWM2_prevThread;
extern volatile __thread int PCWM2_prevLogPos;
extern volatile __thread uint64_t PCWM2_prevTS;

extern volatile __thread
  int PCWM2_writeLogStart, PCWM2_writeLogEnd;

extern volatile __thread int PCWM2_canJumpToWait;

extern volatile __thread uint64_t PCWM2_timeFlushTS1;
extern volatile __thread uint64_t PCWM2_timeFlushTS2;

extern volatile __thread uint64_t PCWM2_timeScanTS1;
extern volatile __thread uint64_t PCWM2_timeScanTS2;

extern volatile __thread uint64_t PCWM2_timeWaitingTS1;
extern volatile __thread uint64_t PCWM2_timeWaitingTS2;
extern volatile __thread uint64_t PCWM2_timeWaiting;
extern volatile __thread uint64_t PCWM2_timeFlushing;
extern volatile __thread uint64_t PCWM2_timeScanning;
extern volatile __thread uint64_t PCWM2_timeTX_upd;

extern volatile __thread uint64_t PCWM2_countCommitPhases;

extern volatile uint64_t PCWM2_incCommitsPhases;
extern volatile uint64_t PCWM2_incTimeTotal;
extern volatile uint64_t PCWM2_incAfterTx;
extern volatile uint64_t PCWM2_incWaiting;
extern volatile uint64_t PCWM2_incFlushing;
extern volatile uint64_t PCWM2_incScanning;
extern volatile uint64_t PCWM2_incTXTime_upd;

extern volatile __thread uint64_t PCWM2_durability_RO_spins;

extern __thread uint64_t timeAbortedTX;

typedef struct {
  uint64_t a[4];
} __m256i;
#define _mm256_load_si256(_m256i_addr) \
  *(_m256i_addr)
// TODO: it is not atomic in POWER8
#define _mm256_store_si256(_m256i_addr, _m256i_val) \
  *(_m256i_addr) = _m256i_val

// TODO: this does not seem to be called
#define MACRO_PCWM2_on_before_sgl_commit_pcwm2(threadId)                       \
  smart_close_log_pcwm2(/* commit value */ onesBit63(PCWM2_readClockVal),      \
                       /* marker position */ (uint64_t *)&(                    \
                           write_log_thread[PCWM2_writeLogEnd]));              \
  FENCE_PREV_FLUSHES();                                                        \
  /* emulating durmarker flush */                                              \
  FLUSH_CL(&P_last_safe_ts->ts);                                               \
  FENCE_PREV_FLUSHES();                                                        \
  /* just to be safe */                                                        \
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(0),               \
                   __ATOMIC_RELEASE);                                          \
//

#define MACRO_PCWM2_fetch_log(threadId)                                        \
  write_log_thread[PCWM2_writeLogEnd] = 0;                                     \
  write_log_thread[(PCWM2_writeLogEnd + 8) &                                   \
                   (gs_appInfo->info.allocLogSize - 1)] = 0;                   \
  //

#define MACRO_PCWM2_on_before_htm_begin_pcwm2(threadId, ro)                    \
{                                                                              \
  write_log_thread = &(P_write_log[threadId][0]);                              \
  PCWM2_writeLogEnd = PCWM2_writeLogStart =                                    \
    G_next[threadId].log_ptrs.write_log_next;                                  \
  MACRO_PCWM2_fetch_log(threadId);                                             \
  if (gs_ts_array[threadId].pcwm.isUpdate != 0)                                \
    __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 0, __ATOMIC_RELEASE);\
  PCWM2_readonly_tx = ro;                                                      \
  if (log_replay_flags & LOG_REPLAY_CONCURRENT)                                \
  {                                                                            \
    /* check space in the log */                                               \
    const int MIN_SPACE_IN_LOG = 128;                                          \
    long totSize = gs_appInfo->info.allocLogSize;                              \
    volatile long start = G_next[threadId].log_ptrs.write_log_start;           \
    long next = G_next[threadId].log_ptrs.write_log_next;                      \
    long nextExtra = (next + MIN_SPACE_IN_LOG) & (totSize - 1);                \
    volatile long size = next >= start ? next - start : totSize - (start - next);\
    long extraSize = nextExtra > next ? nextExtra - next : totSize - (next - nextExtra);\
    while (size + extraSize > totSize) {                                       \
      /* wait the background replayer to gather some transactions */           \
      start = G_next[threadId].log_ptrs.write_log_start;                       \
      size = next >= start ? next - start : totSize - (start - next);          \
      extraSize = nextExtra > next ? nextExtra - next : totSize - (next - nextExtra);\
    }                                                                          \
  }                                                                            \
  PCWM2_myPreviousClock = zeroBit63(gs_ts_array[threadId].pcwm.ts);            \
  if (!PCWM2_readonly_tx)                                                      \
  {                                                                            \
    write_log_thread[PCWM2_writeLogStart] = 0; /* clears logPos values */      \
    __m256i data256i = {                                                       \
      rdtsc(),                                                                 \
      gs_ts_array[threadId].pcwm.prevTS,                                       \
      PCWM2_writeLogEnd,                                                       \
      gs_ts_array[threadId].pcwm.prevLogPos                                    \
    };                                                                         \
    _mm256_store_si256((__m256i*)&gs_ts_array[threadId].pcwm, data256i);       \
  }                                                                            \
  /* __atomic_store_n(&gs_ts_array[threadId].pcwm.logPos, PCWM2_writeLogEnd, __ATOMIC_RELEASE);\
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE); */\
  PCWM2_writeLogEnd = (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);\
}                                                                              \
//

#define MACRO_PCWM2_on_htm_abort_pcwm2(threadId) \
  if (!PCWM2_readonly_tx) \
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE);\
//

#define MACRO_PCWM2_on_before_htm_write_8B_pcwm2(threadId, addr, val)          \
  write_log_thread[PCWM2_writeLogEnd] = (uint64_t)addr;                        \
  PCWM2_writeLogEnd =                                                          \
    (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);             \
  write_log_thread[PCWM2_writeLogEnd] = (uint64_t)val;                         \
  PCWM2_writeLogEnd =                                                          \
    (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);             \
//

#define MACRO_PCWM2_on_before_htm_commit_pcwm2(threadId) \
  PCWM2_readClockVal = rdtscp(); \
//

static inline void scan_others(int threadId)
{
  // ------------------------------------------
  // before flagging, find the previous TX and write there my log position
  PCWM2_prevTS = gs_ts_array[threadId].pcwm.prevTS;
  PCWM2_prevThread = threadId;
  PCWM2_prevLogPos = gs_ts_array[threadId].pcwm.prevLogPos;
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  for (int i = gs_appInfo->info.nbThreads - 1; i >= 0; i--) {
    if (i == threadId) continue;

    __m256i data = _mm256_load_si256((__m256i *)&gs_ts_array[i].pcwm);
    // order matters !!!
    volatile uint64_t cpyTS = data.a[0];
    volatile uint64_t cpyPrevTS = data.a[1];
    volatile uint64_t cpyLogPos = data.a[2];
    volatile uint64_t cpyPrevLogPos = data.a[3];

    // volatile uint64_t cpyPrevTS = gs_ts_array[i].pcwm.prevTS;
    // volatile uint64_t cpyLogPos = gs_ts_array[i].pcwm.logPos;
    // volatile uint64_t cpyPrevLogPos = gs_ts_array[i].pcwm.prevLogPos;

    if (LARGER_THAN(PCWM2_readClockVal, zeroBit63(cpyTS), threadId, i)) {
      if (isBit63One(cpyTS)) { // is persistent (is stable)
        if (LARGER_THAN(zeroBit63(cpyTS), PCWM2_prevTS, i, PCWM2_prevThread)) {
          PCWM2_prevTS = zeroBit63(cpyTS);
          PCWM2_prevThread = i;
          PCWM2_prevLogPos = cpyLogPos;
        }
#ifndef DISABLE_PCWM_OPT
        if (LARGER_THAN(zeroBit63(cpyTS), PCWM2_readClockVal, i, threadId)) {
          PCWM2_canJumpToWait = 1;
        }
#endif
      } else { // is unstable
        PCWM2_prevThreadsArray[PCWM2_nbPrevThreads] = i;
        // prevLogPosArray[PCWM2_nbPrevThreads] = cpyPrevLogPos;
        PCWM2_logPosArray[PCWM2_nbPrevThreads] = cpyLogPos;
        PCWM2_TSArray[PCWM2_nbPrevThreads] = cpyTS;
        // prevTSArray[PCWM2_nbPrevThreads] = cpyPrevTS;

        PCWM2_nbPrevThreads++;
      }
    }

    if (LARGER_THAN(PCWM2_readClockVal, cpyPrevTS, threadId, i) && LARGER_THAN(cpyPrevTS, PCWM2_prevTS, i, PCWM2_prevThread)) {
      PCWM2_prevTS = zeroBit63(cpyPrevTS);
      PCWM2_prevThread = i;
      PCWM2_prevLogPos = cpyPrevLogPos;
    }
  }
}

static inline void smart_close_log_pcwm2(uint64_t marker, uint64_t *marker_pos)
{
  intptr_t lastCL  = ((uintptr_t)(&write_log_thread[PCWM2_writeLogEnd]) >> 6) << 6;
  intptr_t firstCL = ((uintptr_t)(&write_log_thread[PCWM2_writeLogStart]) >> 6) << 6;

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

//TO DO: this function is implemented in impl_pcwm.c (but it should be in some common c file)
#define MACRO_PCWM2_RO_wait_for_durable_reads(threadId, myPreCommitTS) \
\
  uint64_t otherTS; \
  /* printf("BEFORE thread %i on RO wait\n", threadId); */ \
  for (int i = 0; i < gs_appInfo->info.nbThreads; i++) { \
    if (i!=threadId) { \
      PCWM2_durability_RO_spins = 0; \
      do { \
        otherTS = zeroBit63(__atomic_load_n(&(gs_ts_array[i].pcwm.ts), __ATOMIC_ACQUIRE)); \
        PCWM2_durability_RO_spins ++; \
        /* if (PCWM2_durability_RO_spins > 100000UL && PCWM2_durability_RO_spins % 100001 == 0) \
          printf("Thread %i is blocked on tid %i (%li spins)!\n", threadId, i, PCWM2_durability_RO_spins); */ \
				if (otherTS == 0 /* corner case */) break; \
      } /* TODO: This does not work very well in POWER because the 256bit stores are not atomic */ \
      while (otherTS < myPreCommitTS && \
        !__atomic_load_n(&gs_appInfo->info.isExit, __ATOMIC_ACQUIRE)); \
    } \
  } \
  /* printf("AFTER thread %i on RO wait\n", threadId); */ \
\
//

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
on_after_htm_commit_pcwm2(int threadId)
{
  // assert(gs_ts_array[threadId].pcwm.ts == 0);

  /* printf("BEFORE on_after_htm_commit_pcwm2 thread %i PCWM2_readonly_tx %i\n", threadId, PCWM2_readonly_tx); */

  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, PCWM2_timeTX_upd);
  int didTheFlush = 0;
  volatile unsigned long new_ts = onesBit63(PCWM2_readClockVal);
  __m256i storeStableData;

  int _PCWM2_prevThreadsArray[gs_appInfo->info.nbThreads];
  int _logPosArray[gs_appInfo->info.nbThreads];
  // int _prevLogPosArray[gs_appInfo->info.nbThreads];
  uint64_t _TSArray[gs_appInfo->info.nbThreads];
  // uint64_t _prevTSArray[gs_appInfo->info.nbThreads];
  uint64_t advertiseEntry;
  // uint64_t advertiseEntryVal;

  PCWM2_nbPrevThreads = 0;
  PCWM2_prevThreadsArray = _PCWM2_prevThreadsArray;
  PCWM2_logPosArray = _logPosArray;
  // prevLogPosArray = _prevLogPosArray;
  PCWM2_TSArray = _TSArray;
  // prevTSArray = _prevTSArray;

  // gs_ts_array[threadId].pcwm.ts = PCWM2_readClockVal; // currently has the TS of begin
  // tells the others my TS taken within the TX
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, PCWM2_readClockVal, __ATOMIC_RELEASE);
  
  // TODO: check if it does not break anything
  if (((PCWM2_writeLogStart + 1) & (gs_appInfo->info.allocLogSize - 1)) == PCWM2_writeLogEnd) {
    // tells the others to move on
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, (uint64_t)-1, __ATOMIC_RELEASE);
    // gs_ts_array[threadId].pcwm.ts
    if (PCWM2_readonly_tx) // TODO: seems bogus
      { MACRO_PCWM2_RO_wait_for_durable_reads(threadId, PCWM2_readClockVal); }
    goto ret;
  }

  __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 1, __ATOMIC_RELEASE);

  MEASURE_TS(PCWM2_timeFlushTS1);

  MEASURE_TS(PCWM2_timeScanTS1);
  scan_others(threadId);
  MEASURE_TS(PCWM2_timeScanTS2);

  // int prevWriteLogEnd = PCWM2_writeLogEnd;
  // PCWM2_writeLogEnd = (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1); // needed
  smart_close_log_pcwm2(
    /* commit value */ onesBit63(PCWM2_readClockVal),
    /* marker position */ (uint64_t*)&(write_log_thread[PCWM2_writeLogEnd])
  );
  // PCWM2_writeLogEnd = prevWriteLogEnd;

  // -------------------
  /** OLD */
  // // flush log entries
  // PCWM2_writeLogEnd = (PCWM2_writeLogEnd + gs_appInfo->info.allocLogSize - 1) & (gs_appInfo->info.allocLogSize - 1);
  // FLUSH_RANGE(&write_log_thread[PCWM2_writeLogStart], &write_log_thread[PCWM2_writeLogEnd],
  //   &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  // PCWM2_writeLogEnd = (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  // FENCE_PREV_FLUSHES();
  // /* Commits the write log (commit marker) */
  // write_log_thread[PCWM2_writeLogEnd] = onesBit63(PCWM2_readClockVal);
  // FLUSH_CL(&write_log_thread[PCWM2_writeLogEnd]);
  /** OLD */
  // -------------------

  __atomic_store_n(&write_log_thread[PCWM2_writeLogStart], PCWM2_readClockVal, __ATOMIC_RELEASE);
  INC_PERFORMANCE_COUNTER(PCWM2_timeScanTS1, PCWM2_timeScanTS2, PCWM2_timeScanning);
  
// at this point you know the earliest tx that precedes you and that has already durably committed..
// there may still be txs preceding you that have not durably committed yet
// ------------------------------------------
  
  // tells the others flush done!
  FENCE_PREV_FLUSHES();
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, new_ts, __ATOMIC_RELEASE);

  MEASURE_TS(PCWM2_timeFlushTS2);
  INC_PERFORMANCE_COUNTER(PCWM2_timeFlushTS1, PCWM2_timeFlushTS2, PCWM2_timeFlushing);

  // now: is it safe to return to the application?
  // first wait preceding TXs
  PCWM2_canJumpToWait = 0;

  wait_commit_fn(threadId);
  // all preceding TXs flushed their logs, and we know the intention

  if (PCWM2_canJumpToWait) goto waitTheMarker;

  // second verify it the checkpointer will reproduce our log
  uint64_t oldVal, oldVal2;
putTheMarker:
  if ((oldVal = __atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE)) < PCWM2_readClockVal) {
    int success = 0;
    // it will not be reproduced, need to change it
    while (__atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE) < PCWM2_readClockVal) {
      oldVal2 = __sync_val_compare_and_swap(&P_last_safe_ts->ts, oldVal, PCWM2_readClockVal);
      success = (oldVal2 == oldVal);
      oldVal = oldVal2;
    }
    if (success) {
      FLUSH_CL(&P_last_safe_ts->ts);
      FENCE_PREV_FLUSHES();
      didTheFlush = 1;
// -------------------------------
      // tells the others that I've managed to flush up to my TS
      // __atomic_store_n(&gs_ts_array[threadId].comm2.globalMarkerTS, PCWM2_readClockVal, __ATOMIC_RELEASE);
      // TODO: now using the same cacheline
      __atomic_store_n(&gs_ts_array[threadId].pcwm.flushedMarker, PCWM2_readClockVal, __ATOMIC_RELEASE);
// -------------------------------
      // this may fail, I guess a more recent transaction will update the TS...
      // at this point it should be guaranteed that the checkpointer sees:
      //  P_last_safe_ts->ts >= PCWM2_readClockVal (it ignores bit 63)
      // TODO: while enabling this do not forget zeroBit63 whenever you read P_last_safe_ts
      // __sync_val_compare_and_swap(&P_last_safe_ts->ts, PCWM2_readClockVal, onesBit63(PCWM2_readClockVal));
    }
  }
waitTheMarker:
  if (!didTheFlush) {
    // tried the following (seems to be slightly faster):
// -------------------------------
    volatile uint64_t spinCount = 0;
    while (1) {
      // while (P_last_safe_ts->ts < PCWM2_readClockVal) {
      //   _mm_pause();
      //   spinCount++;
      //   if (spinCount > 10000000) goto putTheMarker;
      // }; // need to wait
      // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      spinCount++;
      for (int i = gs_appInfo->info.nbThreads - 1; i >= 0; i--) {
        if (__atomic_load_n(&gs_ts_array[i].pcwm.flushedMarker, __ATOMIC_ACQUIRE) >= PCWM2_readClockVal) {
          goto outerLoop;
        }
      }
      // _mm_pause();
      if (spinCount > 10000) {
        goto putTheMarker;
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
  // __atomic_store_n(&gs_ts_array[threadId].pcwm.prevTS, PCWM2_readClockVal, __ATOMIC_RELEASE);
  // when zeroBit63(P_last_safe_ts->ts) >= PCWM2_readClockVal we can return to the app
  storeStableData = (__m256i){
    gs_ts_array[threadId].pcwm.ts, // does not change
    PCWM2_readClockVal,
    gs_ts_array[threadId].pcwm.logPos, // does not change
    gs_ts_array[threadId].pcwm.logPos
  };
  _mm256_store_si256((__m256i*)&gs_ts_array[threadId].pcwm, storeStableData);

  // first transaction cannot write!
  if (PCWM2_prevTS == 0) {
    // printf("[%i] did not find previous TX\n", threadId);
    goto ret_update;
  }

  // now go there and write the entry
  // advertiseEntryVal = P_write_log[PCWM2_prevThread][prevLogPos];
  advertiseEntry = ((uint64_t)threadId << 32) | PCWM2_writeLogStart;

  // if (!__sync_bool_compare_and_swap(&P_write_log[PCWM2_prevThread][prevLogPos], advertiseEntryVal, onesBit63(advertiseEntry))) {
  //   printf("[%i] Cannot write on the previous TX log\n", threadId);
  // }
  P_write_log[PCWM2_prevThread][PCWM2_prevLogPos] = onesBit63(advertiseEntry);

  // }
  // advertiseEntry = ((uint64_t)threadId << 32) | PCWM2_writeLogStart;
  // P_write_log[PCWM2_prevThread][PCWM2_prevLogPos] = onesBit63(advertiseEntry);

// ------------------------------------------
ret_update:
  // place the next ptr after the TS
  G_next[threadId].log_ptrs.write_log_next = (PCWM2_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
ret:
// assert(gs_ts_array[threadId].pcwm.ts == 0);
  MEASURE_INC(PCWM2_countCommitPhases);
  /* printf("AFTER on_after_htm_commit_pcwm2 thread %i\n", threadId); */
}
