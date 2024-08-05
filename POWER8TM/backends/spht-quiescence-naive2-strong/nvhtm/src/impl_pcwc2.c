#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "htm_impl.h"

// sort undecided threads
#define TYPE uint64_t
#include "array_utils.h"

#define TYPE int
#include "array_utils.h"

#undef TYPE

#define LARGER_THAN(_TSi, _TSj, _i, _j) ((_TSi > _TSj) || (_TSi == _TSj && _i > _j))

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t timeScanningTS1 = 0;
static volatile __thread uint64_t timeScanningTS2 = 0;
static volatile __thread uint64_t timeScanning = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;
static volatile uint64_t incScanning = 0;

static volatile __thread int writeLogStart, writeLogEnd;
static volatile __thread int32_t allocLogSize;
static volatile __thread int32_t nbAbortsOnTX = 0;
static volatile __thread int writeLogStartSave;

// local array with all TX I must wait for
static volatile __thread int* whoIsBehindMe; // assess who is the thread I've to wait for
static volatile __thread int whoIsBehindMeCount = 0;
static volatile __thread uint64_t *myObservedTS;
static volatile __thread uint64_t myPreviousTS;
static volatile __thread uint64_t readClockTSC;

#ifndef NDEBUG
static volatile __thread uint64_t lastPhysicalClock = 0;
#endif

// static char dumpFileName[] = "/mnt/nvram1/dumpfile"; // + threadId
// static __thread FILE* dumpFile = NULL;

int PCWC2_haltSnoopAfterAborts = 0;

void install_bindings_pcwc2()
{
  on_before_htm_begin  = on_before_htm_begin_pcwc2;
  on_htm_abort         = on_htm_abort_pcwc2;
  on_before_htm_write  = on_before_htm_write_8B_pcwc2;
  on_before_htm_commit = on_before_htm_commit_pcwc2;
  on_after_htm_commit  = on_after_htm_commit_pcwc2;

  wait_commit_fn = wait_commit_pcwc2;
}

void state_gather_profiling_info_pcwc2(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
  __sync_fetch_and_add(&incScanning, timeScanning);
}

void state_fprintf_profiling_info_pcwc2(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_WAIT",
              "TIME_SCANNING");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incWaiting, incScanning);
}

void on_htm_abort_pcwc2(int threadId)
{
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), rdtsc(), __ATOMIC_RELEASE);
#endif
  readClockTSC = rdtsc();
  __atomic_store_n(myObservedTS, onesBit63(readClockTSC), __ATOMIC_RELEASE);
  // *myObservedTS = onesBit63(readClockTSC);
}

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

void on_before_htm_begin_pcwc2(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  write_log_thread = &(P_write_log[threadId][0]);

  // if (!dumpFile) {
  //   char dumpFileName2[128];
  //   sprintf(dumpFileName2, "%s%i", dumpFileName, threadId);
  //   dumpFile = fopen(dumpFileName2, "w");
  // }

#ifndef NDEBUG
  lastPhysicalClock = G_observed_ts[threadId].comm.LPC;
#endif

  allocLogSize = gs_appInfo->info.allocLogSize;
  myObservedTS = &G_observed_ts[threadId].comm.ts;
  myPreviousTS =  G_observed_ts[threadId].comm.LPC;

  MEASURE_TS(timeTotalTS1);

  readClockTSC = rdtsc();
  __atomic_store_n(myObservedTS, onesBit63(readClockTSC), __ATOMIC_RELEASE);
  // *myObservedTS = onesBit63(readClockTSC);

  // __m128i thrInfo = (__m128i){ rdtsc(), 1 };
  // _mm_stream_si128((__m128i*)&(gs_ts_array[threadId].comm.ts), thrInfo);

  // __atomic_store_n(&(gs_ts_array[threadId].comm2.isReturnToApp), 0, __ATOMIC_RELEASE);
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), rdtsc(), __ATOMIC_RELEASE); // TODO: magic scheduling
#endif

  // set on 10 aborts
  // nbAbortsOnTX = 0;
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //   while (gs_ts_array[i].comm2.waitSnapshot);
  // } // NOTE: if you wait after it causes a deadlock
#ifndef DISABLE_FLAG_IS_IN_TX
  __atomic_store_n(&(gs_ts_array[threadId].comm2.isInTX), 1, __ATOMIC_RELEASE);
#endif

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  fetch_log(threadId);
}

void on_before_htm_write_8B_pcwc2(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
}

void on_before_htm_commit_pcwc2(int threadId)
{
  if (writeLogStart != writeLogEnd) { // read-only transaction do not need to be logged
    // // __asm__ __volatile__("sfence" ::: "memory");
    // *myObservedTS = myPreviousTS; // needed in the SGL
    // // TODO: rdtscp causes a segmentation fault if myObservedTS is written before (using -O3)
    // __asm__ __volatile__("mfence" ::: "memory");
    // volatile uint64_t tsc = rdtscp();
    // *myObservedTS = tsc;
    readClockTSC = rdtscp();
  }
}

static inline void scanTransactions_useVect(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
#ifndef DISABLE_FLAG_IS_IN_TX
  int counter[gs_appInfo->info.nbThreads];
  int count = 0;
  memset(counter, 0, gs_appInfo->info.nbThreads * sizeof(int));
#endif
  
  // __asm__ __volatile__("mfence" ::: "memory");
  // put while loop here...
  // __atomic_store_n(&(gs_ts_array[threadId].comm2.waitSnapshot), 1, __ATOMIC_RELEASE);
#ifndef DISABLE_FLAG_IS_IN_TX
  while(1) {
#endif
    for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
#ifndef DISABLE_FLAG_IS_IN_TX
      // TODO: if this load is not atomic, it should be done an aditional load AFTER obtaining the version
      if (counter[i]) {
        count++;
        continue;
      }
      if (__atomic_load_n(&(gs_ts_array[i].comm2.isInTX), __ATOMIC_ACQUIRE) == 1) continue; // TODO: magic scheduling
      counter[i] = 1;
      count++;
#endif
      __m256i data = _mm256_load_si256((__m256i *)&G_observed_ts[i]);
      _mm256_store_si256((__m256i *)(&myPCWCinfo[i]), data);
    }
#ifndef DISABLE_FLAG_IS_IN_TX
    if (count >= gs_appInfo->info.nbThreads) break;
    count = 0;
  }
#endif
  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (myPCWCinfo[i].TS != -1 && isBit63One(myPCWCinfo[i].TS) && zeroBit63(myPCWCinfo[i].TS) < myPCWCinfo[threadId].TS) {
      uint64_t newTS;
      do {
        while (isBit63One(newTS = __atomic_load_n(&G_observed_ts[i].comm.ts, __ATOMIC_ACQUIRE)) && newTS != -1);
        __m256i data = _mm256_load_si256((__m256i *)&G_observed_ts[i]);
        _mm256_store_si256((__m256i *)(&myPCWCinfo[i]), data);
      } while (isBit63One(newTS = __atomic_load_n(&G_observed_ts[i].comm.ts, __ATOMIC_ACQUIRE)) && newTS != -1);
    }
  }
  // __atomic_store_n(&(gs_ts_array[threadId].comm2.waitSnapshot), 0, __ATOMIC_RELEASE);
}

// results in myPCWCinfo[threadId]
static inline void computeNewLLC(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
  int base = 0;
  int offset;

  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    // compute the thread that is behind me
    // TODO: must either flag the guy behind is done waiting OR wait for all the threads behind me
    if (myPCWCinfo[i].TS != -1 && LARGER_THAN(myPCWCinfo[threadId].TS, myPCWCinfo[i].TS, threadId, i)) {
      whoIsBehindMe[whoIsBehindMeCount] = i;
      whoIsBehindMeCount++;
    }
  }

  // TODO: these loops can be merged
  // base = max among all the LLCs, filtering the threads whose LPC>myTS
  // base_LPC = LPC of the thread whose base we have used
  // offset = +1 + count in how many threads myTS > TS && TS != -1 && TS > base_LPC
  uint64_t base_LPC = 0;
  int base_LPC_i = -1;
  offset = 1;
  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (LARGER_THAN(myPCWCinfo[i].LPC, myPCWCinfo[threadId].TS, i, threadId)) continue;
    if (myPCWCinfo[i].LLC > base) {
      base = myPCWCinfo[i].LLC;
      base_LPC = myPCWCinfo[i].LPC;
      base_LPC_i = i;
    }
  }
  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (myPCWCinfo[i].TS != -1 &&
        LARGER_THAN(myPCWCinfo[threadId].TS, myPCWCinfo[i].TS, threadId, i)
        && LARGER_THAN(myPCWCinfo[i].TS, base_LPC, i, base_LPC_i)) {
      offset++;
    }
  }

  // copy this to the public cache lines
  myPCWCinfo[threadId].LLC = base + offset;
  myPCWCinfo[threadId].LPC = myPCWCinfo[threadId].TS;

  // TODO: write my spot on prevTX log
  // if (G_observed_ts[threadId].comm.LLC + 1 == myPCWCinfo[threadId].LLC) {
  //   // 2 TXs in a row
  //   int logPos = G_next[threadId].log_ptrs.write_log_next;
  //   logPos = (logPos + gs_appInfo->info.allocLogSize - 2) % gs_appInfo->info.allocLogSize;
  //   uint64_t logInfo = ((uint64_t)threadId << 32) | writeLogStart;
  //   P_write_log[threadId][logPos] = onesBit63(logInfo);
  //   return;
  // }

  // int prevThread = -1;
  // uint64_t maxTS = 0;
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //   if (i == threadId) continue;
  //   if (myPCWCinfo[i].LLC == myPCWCinfo[threadId].LLC - 1) {
  //     prevThread = -1;
  //     break;
  //   }
  //   // else, the TS must be set (is a TX that is behind)
  //   if (myPCWCinfo[i].TS != -1 && LARGER_THAN(myPCWCinfo[threadId].TS, myPCWCinfo[i].TS, threadId, i)) {
  //     if (myPCWCinfo[i].TS > maxTS) {
  //       prevThread = i;
  //       maxTS = myPCWCinfo[i].TS;
  //     }
  //   }
  // }
  // if (prevThread == -1) {
  //   printf("[%i] did not find prevThread\n", threadId);
  //   return;
  // }

  // int logPos = G_next[prevThread].log_ptrs.write_log_next;
  // while (!isBit63One(P_write_log[prevThread][logPos])) {
  //   logPos = (logPos + gs_appInfo->info.allocLogSize - 2) % gs_appInfo->info.allocLogSize;
  //   if (isBit63One(P_write_log[prevThread][logPos]) && zeroBit63(P_write_log[prevThread][logPos]) > myPCWCinfo[threadId].LLC) {
  //     logPos = (logPos + gs_appInfo->info.allocLogSize - 2) % gs_appInfo->info.allocLogSize;
  //   }
  // }
  // uint64_t prevTXts = zeroBit63(P_write_log[prevThread][logPos]);
  // if (myPCWCinfo[threadId].LLC != prevTXts - 1) {
  //   printf("[%i] invalid TS\n", threadId);
  //   return;
  // }
  // logPos = (logPos + gs_appInfo->info.allocLogSize - 1) % gs_appInfo->info.allocLogSize;
  // uint64_t logInfo = ((uint64_t)threadId << 32) | writeLogStart;
  // P_write_log[prevThread][logPos] = onesBit63(logInfo);
}

void on_after_htm_commit_pcwc2(int threadId)
{
  MEASURE_TS(timeAfterTXTS1);
  int localWhoIsBehindMe[gs_appInfo->info.nbThreads];

  // __m128i thrInfo = (__m128i){ rdtsc(), 0 };
  // _mm_stream_si128((__m128i*)&(gs_ts_array[threadId].comm.ts), thrInfo);


#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), *myObservedTS, __ATOMIC_RELEASE); // TODO: magic scheduling
#endif
#ifndef DISABLE_FLAG_IS_IN_TX
  __atomic_store_n(&(gs_ts_array[threadId].comm2.isInTX), 0, __ATOMIC_RELEASE); // TODO: magic scheduling
#endif

  if (writeLogStart == writeLogEnd) {
    __atomic_store_n(&(G_observed_ts[threadId].comm.ts), -1, __ATOMIC_RELEASE);
    goto ret;
  }

  __atomic_store_n(myObservedTS, readClockTSC, __ATOMIC_RELEASE); // externalizes the clock taken inside the TX

  whoIsBehindMeCount = 0;
  whoIsBehindMe = (int*)localWhoIsBehindMe;

  // flush log entries
  writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) % gs_appInfo->info.allocLogSize;
  FLUSH_RANGE(&write_log_thread[writeLogStart], &write_log_thread[writeLogEnd],
    &write_log_thread[0], write_log_thread + gs_appInfo->info.allocLogSize);
  writeLogEnd = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;

  // snapshot all threads (this cost is amortized in the flush)
  MEASURE_TS(timeScanningTS1);
  scanTransactions_useVect(threadId, gs_pcwc_info[threadId]);

  computeNewLLC(threadId, gs_pcwc_info[threadId]);
  MEASURE_TS(timeScanningTS2);
  INC_PERFORMANCE_COUNTER(timeScanningTS1, timeScanningTS2, timeScanning);

  FENCE_PREV_FLUSHES();

  /* Commits the write log (commit marker) */
  // int startCommitMarker = writeLogEnd;

  //
  write_log_thread[writeLogEnd] = onesBit63(gs_pcwc_info[threadId][threadId].LLC);
  FLUSH_CL(&write_log_thread[writeLogEnd]);
  int nextLogNext = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;
  
  // -------------------------------------------------------
  // TODO: block if there isn't at least 2 cache lines available in the log
#ifndef NDEBUG
  int preLogNext = G_next[threadId].log_ptrs.write_log_next;
  int i = preLogNext; // TODO: throw this into debug
  while (i != nextLogNext) {
    i = (i + 1) % gs_appInfo->info.allocLogSize;
    if (G_next[threadId].log_ptrs.write_log_start == i) {
      printf("[end=%i i=%i] Log smash detected!\n", G_next[threadId].log_ptrs.write_log_next, i);
    }
  }
#endif /* NDEBUG */
  // -------------------------------------------------------

  G_next[threadId].log_ptrs.write_log_next = nextLogNext;

  FENCE_PREV_FLUSHES();

  // printf("T%2i commit TX=%3lu TS=%lx \n", threadId, counter, *myObservedTS);
  wait_commit_fn(threadId); // with PCWC it is possible to wait after flushing the commit marker

  // TODO: this does not seem to be the issue
  // gs_ts_array[threadId].ts = rdtsc(); // if this TS is larger than the snapshot ts then go on

  __m256i data256i = { -1, gs_pcwc_info[threadId][threadId].LLC, gs_pcwc_info[threadId][threadId].LPC, 0};
  _mm256_store_si256((__m256i*)&G_observed_ts[threadId].comm, data256i);

  // fprintf(dumpFile, "  LLC=%lu (budget=%i)\n", gs_pcwc_info[threadId][threadId].LLC, HTM_SGL_budget);
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //   fprintf(dumpFile, "[%i] <LLC=%li,LPC=%lx,CPC=%lx,version=%i>\n", i, gs_pcwc_info[threadId][i].LLC, gs_pcwc_info[threadId][i].LPC, gs_pcwc_info[threadId][i].TS, gs_pcwc_info[threadId][i].version);
  // }
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

ret:
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), -1, __ATOMIC_RELEASE);
#endif
  MEASURE_INC(countCommitPhases);
}

void wait_commit_pcwc2(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  uint64_t snapshot[gs_appInfo->info.nbThreads];
  uint64_t myTS = gs_pcwc_info[threadId][threadId].TS;
  // uint64_t myTS = G_observed_ts[threadId].comm.ts;
  // uint64_t myTS = gs_ts_array[threadId].comm2.ts;
  int countThreadsDone = 0;

  if (whoIsBehindMeCount == 0) return; // Done!
  // the preCounter could go on a different Cache Line from the HTM_TS

  while (countThreadsDone != whoIsBehindMeCount) {
    // take snapshot
    for (int i = whoIsBehindMeCount-1; i >= 0; i--) { // myth: this is faster than the below
    // for (int i = 0; i < whoIsBehindMeCount; ++i) {
      if (whoIsBehindMe[i] == -1) continue;
#ifndef DISABLE_APROX_WAIT_CLOCK
      snapshot[i] = __atomic_load_n(&(gs_ts_array[whoIsBehindMe[i]].comm2.ts), __ATOMIC_ACQUIRE);
#else
      snapshot[i] = __atomic_load_n(&(G_observed_ts[whoIsBehindMe[i]].comm.ts), __ATOMIC_ACQUIRE);
#endif
    }

    // evaluate (TODO: we should flag "I'm done" not in the TS, as this causes contention with HTM)
    for (int i = whoIsBehindMeCount-1; i >= 0; i--) {
    // for (int i = 0; i < whoIsBehindMeCount; ++i) {
      if (whoIsBehindMe[i] == -1) continue;
      if (snapshot[i] == -1 || LARGER_THAN(snapshot[i], myTS, whoIsBehindMe[i], threadId)) {
        whoIsBehindMe[i] = -1;
        countThreadsDone++;
      } 
    }
  }
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
