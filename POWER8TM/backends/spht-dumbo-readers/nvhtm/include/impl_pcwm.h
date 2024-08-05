#pragma once

#include "rdtsc.h"
#include "spins.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

// use power2: 0 --> does on every sample: 1 --> does on none
// #define PCWM_NB_SAMPLES 0

#define LARGER_THAN(_TSi, _TSj, _i, _j) ((_TSi > _TSj) || (_TSi == _TSj && _i > _j))

typedef uintptr_t bit_array_t;

extern volatile __thread uint64_t PCWM_readClockVal;
extern volatile __thread int PCWM_writeLogStart, PCWM_writeLogEnd;
extern volatile __thread int PCWM_canJumpToWait;

// static volatile __thread uint64_t nbSamples;
// static volatile __thread uint64_t nbSamplesDone;

extern volatile __thread uint64_t PCWM_timeFlushTS1;
extern volatile __thread uint64_t PCWM_timeFlushTS2;
extern volatile __thread uint64_t PCWM_timeWaitingTS1;
extern volatile __thread uint64_t PCWM_timeWaitingTS2;
extern volatile __thread uint64_t PCWM_timeWaiting;
extern volatile __thread uint64_t PCWM_timeFlushing;
extern volatile __thread uint64_t PCWM_timeTX_upd;
extern volatile __thread uint64_t PCWM_timeTX_ro;
extern volatile __thread uint64_t PCWM_ro_durability_wait_time;
extern volatile __thread uint64_t PCWM_dur_commit_time;

extern __thread int PCWM_readonly_tx;

extern volatile __thread uint64_t PCWM_countUpdCommitPhases;
extern volatile __thread uint64_t PCWM_countROCommitPhases;

// volatile uint64_t incNbSamples;
extern volatile uint64_t PCWM_incCommitsPhases;
extern volatile uint64_t PCWM_incROCommitsPhases;
extern volatile uint64_t PCWM_incTimeTotal;
extern volatile uint64_t PCWM_incAfterTx;
extern volatile uint64_t PCWM_incWaiting;
extern volatile uint64_t PCWM_incFlushing;
extern volatile uint64_t PCWM_incTXTime_upd;
extern volatile uint64_t PCWM_incTXTime_ro;
extern volatile uint64_t PCWM_inc_ro_durability_wait_time;
extern volatile uint64_t PCWM_inc_dur_commit_time;

#ifdef DETAILED_BREAKDOWN_PROFILING
/* Breakdown of the main stages (JOAO) */
#define MAX_PROFILE_COUNT 10000
extern uint64_t **PCWM_ro_durability_wait_duration;
extern uint64_t *PCWM_ro_durability_wait_count;
extern uint64_t **PCWM_upd_after_commit_duration;
extern uint64_t **PCWM_upd_log_flush_duration;
extern uint64_t *PCWM_upd_after_commit_count;
#endif

#define MACRO_PCWM_fetch_log(threadId) ({                                                \
  write_log_thread[PCWM_writeLogEnd] = 0;                                                \
	write_log_thread[(PCWM_writeLogEnd + 8) & (gs_appInfo->info.allocLogSize - 1)] = 0;    \
})

#define MACRO_PCWM_on_before_htm_begin_pcwm(threadId, ro) ({                             \
  onBeforeWrite = on_before_htm_write;                                                   \
  onBeforeHtmCommit = on_before_htm_commit;                                              \
  write_log_thread = &(P_write_log[threadId][0]);                                        \
  /*nbSamples++;                                                                         \
  if ((nbSamples & (PCWM_NB_SAMPLES - 1)) == (PCWM_NB_SAMPLES - 1)) {                    \
    nbSamplesDone++;                                                                     \
  }*/                                                                                    \
  PCWM_writeLogEnd = PCWM_writeLogStart = G_next[threadId].log_ptrs.write_log_next;      \
  MACRO_PCWM_fetch_log(threadId);                                                                   \
  if (log_replay_flags & LOG_REPLAY_CONCURRENT) {                                        \
    /* check space in the log */                                                         \
    const int MIN_SPACE_IN_LOG = 128;                                                    \
    long totSize = gs_appInfo->info.allocLogSize;                                        \
    volatile long start = G_next[threadId].log_ptrs.write_log_start;                     \
    long next = G_next[threadId].log_ptrs.write_log_next;                                \
    long nextExtra = (next + MIN_SPACE_IN_LOG) & (totSize - 1);                          \
    volatile long size = next >= start ? next - start : totSize - (start - next);        \
    long extraSize = nextExtra > next ? nextExtra - next : totSize - (next - nextExtra); \
    while (size + extraSize > totSize) {                                                 \
      /* wait the background replayer to gather some transactions */                     \
      start = __atomic_load_n(&G_next[threadId].log_ptrs.write_log_start, __ATOMIC_ACQUIRE);\
      size = next >= start ? next - start : totSize - (start - next);                    \
      extraSize = nextExtra > next ? nextExtra - next : totSize - (next - nextExtra);    \
    }                                                                                    \
  }                                                                                      \
  if (gs_ts_array[threadId].pcwm.isUpdate != 0)                                          \
    __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 0, __ATOMIC_RELEASE);         \
  PCWM_readonly_tx = ro;                                                                 \
  if (!PCWM_readonly_tx)                                                                 \
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE);         \
  else                                                                                   \
    /* For RO txs (which run non-transactionally), we set their ts to infinity */        \
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(PCWM_readClockVal), __ATOMIC_RELEASE);\
})

#define MACRO_PCWM_on_htm_abort_pcwm(threadId) ({                                        \
  /*debug*/                                                                              \
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(0), __ATOMIC_RELEASE);      \
  /* if (!PCWM_readonly_tx) */                                                           \
  /*   __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, rdtsc(), __ATOMIC_RELEASE); */ /* TODO: debug: why? shouldn't it be infinity? */\
})

#define MACRO_PCWM_on_before_htm_write_8B_pcwm(threadId, addr, val)                      \
  write_log_thread[PCWM_writeLogEnd] = (uint64_t)addr;                                   \
  PCWM_writeLogEnd = (PCWM_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);       \
  write_log_thread[PCWM_writeLogEnd] = (uint64_t)val;                                    \
  PCWM_writeLogEnd = (PCWM_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);       \
//

#define MACRO_PCWM_on_before_htm_commit_pcwm(threadId)                                   \
  PCWM_readClockVal = rdtscp();                                                          \
//

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
smart_close_log_pcwm(
	uint64_t marker,
	uint64_t *marker_pos
){
  intptr_t lastCL  = ((uintptr_t)(&write_log_thread[PCWM_writeLogEnd]) >> 6) << 6;
  intptr_t firstCL = ((uintptr_t)(&write_log_thread[PCWM_writeLogStart]) >> 6) << 6;

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


#define MACRO_PCWM_on_before_sgl_commit_pcwm(threadId)                                   \
  smart_close_log_pcwm(                                                                  \
  /* commit value */ onesBit63(PCWM_readClockVal),                                       \
  /* marker position */ (uint64_t*)&(write_log_thread[PCWM_writeLogEnd])                 \
  );                                                                                     \
  FENCE_PREV_FLUSHES();                                                                  \
  FLUSH_CL(&P_last_safe_ts->ts);                                                         \
  FENCE_PREV_FLUSHES();                                                                  \
//

static __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__))
on_after_htm_commit_pcwm(int threadId)
{
  // if (PCWM_writeLogStart == PCWM_writeLogEnd)
  //   INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, PCWM_timeTX_ro);
  // else
  //   INC_PERFORMANCE_COUNTER(timeTotalTS1, timeAfterTXTS1, PCWM_timeTX_upd);

  int didTheFlush = 0;

  // gs_ts_array[threadId].pcwm.ts = PCWM_readClockVal; // currently has the TS of begin
  // tells the others my TS taken within the TX
  // TODO: remove the atomic
  if (!PCWM_readonly_tx) 
    __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, PCWM_readClockVal, __ATOMIC_RELEASE);

  // printf("[%i] did TX=%lx\n", threadId, PCWM_readClockVal);

  if (PCWM_writeLogStart == PCWM_writeLogEnd) {
    /* Read-only transaction is about to return */

    if (!PCWM_readonly_tx)
    // tells the others to move on
      __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(PCWM_readClockVal), __ATOMIC_RELEASE);

    /* RO durability wait (bug fix by Joao)*/
    /* debug! */
    // printf("will call RO_wait_for_durable_reads\n");
    // RO_wait_for_durable_reads(threadId, PCWM_readClockVal);
    // printf("returned\n");

    goto ret;
  }

  __atomic_store_n(&gs_ts_array[threadId].pcwm.isUpdate, 1, __ATOMIC_RELEASE);

#ifndef DISABLE_PCWM_OPT
  // says to the others that it intends to write this value in the marker
  // TODO: now using gs_ts_array[threadId].pcwm.ts as TS intention
  // __atomic_store_n(&gs_ts_array[threadId].comm2.globalMarkerIntent, PCWM_readClockVal, __ATOMIC_RELEASE);
#endif

  // if ((nbSamples & (PCWM_NB_SAMPLES - 1)) == (PCWM_NB_SAMPLES - 1)) {
    MEASURE_TS(PCWM_timeFlushTS1);
  // }

  // int prevPCWM_writeLogEnd = PCWM_writeLogEnd;
  // PCWM_writeLogEnd = (PCWM_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1); // needed
  smart_close_log_pcwm(
    /* commit value */ onesBit63(PCWM_readClockVal),
    /* marker position */ (uint64_t*)&(write_log_thread[PCWM_writeLogEnd])
  );
  // PCWM_writeLogEnd = prevPCWM_writeLogEnd;

  // -------------------
  /** OLD */
  // flush log entries
  // PCWM_writeLogEnd = (PCWM_writeLogEnd + gs_appInfo->info.allocLogSize - 1) & (gs_appInfo->info.allocLogSize - 1);
  // FLUSH_RANGE(&write_log_thread[PCWM_writeLogStart], &write_log_thread[PCWM_writeLogEnd],
  //   &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  // PCWM_writeLogEnd = (PCWM_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
  // FENCE_PREV_FLUSHES();
  // /* Commits the write log (commit marker) */
  // write_log_thread[PCWM_writeLogEnd] = onesBit63(PCWM_readClockVal);
  // FLUSH_CL(&write_log_thread[PCWM_writeLogEnd]);
  /** OLD */
  // -------------------

  // tells the others flush done!
  FENCE_PREV_FLUSHES();
  __atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(PCWM_readClockVal), __ATOMIC_RELEASE);

  // if ((nbSamples & (PCWM_NB_SAMPLES - 1)) == (PCWM_NB_SAMPLES - 1)) {
    MEASURE_TS(PCWM_timeFlushTS2);
  // }
  INC_PERFORMANCE_COUNTER(PCWM_timeFlushTS1, PCWM_timeFlushTS2, PCWM_timeFlushing);

  // now: is it safe to return to the application?
  // first wait preceding TXs
  PCWM_canJumpToWait = 0;
  wait_commit_fn(threadId);
  // all preceding TXs flushed their logs, and we know the intention

  if (PCWM_canJumpToWait) goto waitTheMarker;

  // second verify it the checkpointer will reproduce our log
  volatile uint64_t oldVal, oldVal2;
putTheMarker:
  if ((oldVal = __atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE)) < PCWM_readClockVal) {
    int success = 0;
    // it will not be reproduced, need to change it
    while (__atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE) < PCWM_readClockVal) {
      oldVal2 = __sync_val_compare_and_swap(&P_last_safe_ts->ts, oldVal, PCWM_readClockVal);
      success = (oldVal2 == oldVal);
      oldVal = oldVal2;
    }
    if (success) {
      FLUSH_CL(&P_last_safe_ts->ts);
      FENCE_PREV_FLUSHES();
      didTheFlush = 1;
// -------------------------------
      // tells the others that I've managed to flush up to my TS
      // __atomic_store_n(&gs_ts_array[threadId].comm2.globalMarkerTS, PCWM_readClockVal, __ATOMIC_RELEASE);
      // TODO: now using the same cacheline
      __atomic_store_n(&gs_ts_array[threadId].pcwm.flushedMarker, PCWM_readClockVal, __ATOMIC_RELEASE);
// -------------------------------
      // this may fail, I guess a more recent transaction will update the TS...
      // at this point it should be guaranteed that the checkpointer sees:
      //  P_last_safe_ts->ts >= PCWM_readClockVal (it ignores bit 63)
      // TODO: while enabling this do not forget zeroBit63 whenever you read P_last_safe_ts
      // __sync_val_compare_and_swap(&P_last_safe_ts->ts, PCWM_readClockVal, onesBit63(PCWM_readClockVal));
    }
  }
waitTheMarker:
  if (!didTheFlush) {
    // tried the following (seems to be slightly faster):
// -------------------------------
    volatile uint64_t spinCount = 0;
    while (1) {
      // while (__atomic_load_n(&P_last_safe_ts->ts, __ATOMIC_ACQUIRE) < PCWM_readClockVal) {
      //   _mm_pause();
      //   spinCount++;
      //   if (spinCount > 10000) {
      //     // printf("goto putTheMarker\n");
      //     goto putTheMarker;
      //   }
      // }; // need to wait
      spinCount++;
      int i;
      for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
            // if (gs_ts_array[i].comm2.globalMarkerTS >= PCWM_readClockVal) {
        if (__atomic_load_n(&gs_ts_array[i].pcwm.flushedMarker, __ATOMIC_ACQUIRE) >= PCWM_readClockVal) {
          goto outerLoop;
        }
      }
      if (spinCount > 10000) {
        // goto putTheMarker;
      }
      //_mm_pause();
    }
// -------------------------------
    // // need to be sure it was flushed
    // while (!isBit63One(P_last_safe_ts->ts)); // TODO: the code before seems slightly faster
  }
outerLoop:
  
  G_next[threadId].log_ptrs.write_log_next = (PCWM_writeLogEnd + 1) & (gs_appInfo->info.allocLogSize - 1);
ret:

if (PCWM_readonly_tx) {
  MEASURE_INC(PCWM_countROCommitPhases);
} else {
  uint64_t ts2;
  MEASURE_TS(ts2);
  INC_PERFORMANCE_COUNTER(timeAfterTXTS1, ts2, PCWM_dur_commit_time);
  MEASURE_INC(PCWM_countUpdCommitPhases);
}

/* TODO: this might not be needed for every case (it's just for safety) */
__atomic_store_n(&gs_ts_array[threadId].pcwm.ts, onesBit63(0), __ATOMIC_RELEASE);


#ifdef DETAILED_BREAKDOWN_PROFILING
  if (!is_PCWM_readonly_tx) {
    uint64_t ts2;
    MEASURE_TS(ts2);

    int c = PCWM_upd_after_commit_count[threadId];
    if (c < MAX_PROFILE_COUNT) {
      PCWM_upd_after_commit_duration[threadId][c] = ts2 - timeAfterTXTS1;
      PCWM_upd_log_flush_duration[threadId][c] = PCWM_timeFlushTS2 - PCWM_timeFlushTS1;
      PCWM_upd_after_commit_count[threadId]++;
    }
  }
#endif

// printf("gs_ts_array[%d].pcwm.ts = %lu\n", threadId, gs_ts_array[threadId].pcwm.ts);
  
}
