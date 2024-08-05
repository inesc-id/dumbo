#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE   600
#define _BSD_SOURCE     1
#define _GNU_SOURCE     1
#define _DEFAULT_SOURCE 1

#include <unistd.h>
#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h> // USE_VECT is now the only option

#include "htm_impl.h"

// // sort undecided threads
// #define TYPE uint64_t
// #include "array_utils.h"

// #define TYPE int
// #include "array_utils.h"

// #undef TYPE

#define LARGER_THAN(_TSi, _TSj, _i, _j) ((_TSi > _TSj) || (_TSi == _TSj && _i > _j))

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t timeScanningTS1 = 0;
static volatile __thread uint64_t timeScanningTS2 = 0;
static volatile __thread uint64_t timeScanning = 0;
static volatile __thread uint64_t nbSnapshotRestarts = 0;

static volatile __thread uint64_t countCommitPhases = 0;

#ifndef NDEBUG
static volatile __thread uint64_t lastPhysicalClock = 0;
#endif

static volatile uint64_t incSnapshotRestarts = 0;
static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;
static volatile uint64_t incScanning = 0;

static volatile __thread int writeLogStart, writeLogEnd;
static volatile __thread int32_t allocLogSize;
static volatile __thread int32_t nbAbortsOnTX = 0;

// local array with all TX I must wait for
static volatile __thread int* whoIsBehindMe; // assess who is the thread I've to wait for
static volatile __thread int whoIsBehindMeCount = 0;
static volatile __thread uint64_t *myObservedTS;
static volatile __thread uint64_t *myVersion;
static volatile __thread uint64_t myPreviousTS;
static volatile __thread uint64_t readClockTSC;

// TODO: hacked, use "extern volatile int useFastPCWC;" then set it to 1
/* extern */volatile int useFastPCWC = 0;

// static char dumpFileName[] = "/mnt/nvram1/dumpfile"; // + threadId
// static char dumpFileName[] = "dumpfile"; // + threadId
// static __thread FILE* dumpFile = NULL;

int PCWC_haltSnoopAfterAborts = 0;

void install_bindings_pcwc()
{
  on_before_htm_begin  = on_before_htm_begin_pcwc;
  on_htm_abort         = on_htm_abort_pcwc;
  on_before_htm_write  = on_before_htm_write_8B_pcwc;
  on_before_htm_commit = on_before_htm_commit_pcwc;
  on_after_htm_commit  = on_after_htm_commit_pcwc;

  wait_commit_fn = wait_commit_pcwc;
}

void state_gather_profiling_info_pcwc(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
  __sync_fetch_and_add(&incScanning, timeScanning);
  __sync_fetch_and_add(&incSnapshotRestarts, nbSnapshotRestarts);
}

void state_fprintf_profiling_info_pcwc(char *filename)
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
              "TIME_WAIT",
              "TIME_SCANNING",
              "NB_SNAPSHOT_RESTARTS");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incWaiting, incScanning, incSnapshotRestarts);
  fclose(fp);
}

void on_htm_abort_pcwc(int threadId)
{
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), rdtscp(), __ATOMIC_RELEASE);
#endif
  readClockTSC = rdtsc();
  __atomic_store_n(myObservedTS, onesBit63(readClockTSC), __ATOMIC_RELEASE);
}

static inline uintptr_t fetch_log(int threadId)
{
  // TODO: this does not seem a problem in the other solution
  volatile uintptr_t res = 0;
  /*res += (uintptr_t) */write_log_thread[writeLogEnd] = 0;
  /*res += (uintptr_t) */write_log_thread[(writeLogEnd + 8) % allocLogSize] = 0;
  // /*res += (uintptr_t) */write_log_thread[(writeLogEnd + 16) % allocLogSize] = 0;
  // /*res += (uintptr_t) */write_log_thread[(writeLogEnd + 24) % allocLogSize] =  0;
  return res;
}

void on_before_htm_begin_pcwc(int threadId)
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
  write_log_thread = &(P_write_log[threadId][0]);
  myObservedTS = &G_observed_ts[threadId].comm.ts;
  myVersion = &G_observed_ts[threadId].comm.version;
  myPreviousTS =  G_observed_ts[threadId].comm.LPC;

  MEASURE_TS(timeTotalTS1);

  readClockTSC = rdtsc();
  __atomic_store_n(myObservedTS, onesBit63(readClockTSC), __ATOMIC_RELEASE);

#ifndef DISABLE_FLAG_IS_IN_TX
  __atomic_store_n(&(gs_ts_array[threadId].comm2.isInTX), 1, __ATOMIC_RELEASE);
#endif
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), rdtscp(), __ATOMIC_RELEASE); // TODO: magic scheduling
#endif

  writeLogEnd = writeLogStart = G_next[threadId].log_ptrs.write_log_next;
  fetch_log(threadId);
  // __asm__ __volatile__("mfence" ::: "memory");

  // wait busy snaphot TXs (it is not 100% accurate though)
#ifndef DISABLE_FLAG_SNAPSHOT
int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    while (gs_ts_array[i].comm2.waitSnapshot);
  }
#endif
}

void on_before_htm_write_8B_pcwc(int threadId, void *addr, uint64_t val)
{
  write_log_thread[writeLogEnd] = (uint64_t)addr;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
  write_log_thread[writeLogEnd] = (uint64_t)val;
  writeLogEnd = (writeLogEnd + 1) % allocLogSize;
}

void on_before_htm_commit_pcwc(int threadId)
{
  if (writeLogStart != writeLogEnd) { // read-only transaction do not need to be logged
    // if (HTM_SGL_budget > 0) {
    //   *myVersion += 2;
    //   volatile uint64_t tsc = rdtscp();
    //   *myObservedTS = tsc;
    //   // __atomic_thread_fence(__ATOMIC_RELEASE);
    //   __asm__ __volatile__("mfence" ::: "memory");
    //   tsc = rdtscp();
    //   *myObservedTS = tsc; // make sure this order is guarateeded
    //   // rdtscp(); // clean the pipeline
    //   // __atomic_thread_fence(__ATOMIC_RELEASE); // TODO: I guess these are not needed...
    // } else {
    //   *myObservedTS = rdtscp();
    // }
    readClockTSC = rdtscp();
  }
}

// for test purposes only!
static inline void scanTransactions_fast(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
  int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    volatile uint64_t ts = G_observed_ts[i].comm.ts; // DANGER: this may cause aborts
    myPCWCinfo[i].TS = ts;
    myPCWCinfo[i].LLC = ts; // hack here
    myPCWCinfo[i].LPC = ts; //
  }
}

static inline void scanTransactionsVect(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
  // TODO: wait phase (.comm2.isInTX) should be done outside the snapshot 
#ifndef DISABLE_FLAG_IS_IN_TX
int i;
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    while (__atomic_load_n(&(gs_ts_array[i].comm2.isInTX), __ATOMIC_ACQUIRE) == 1);
  }
#endif

// ------------------------------------------ TODO: enable HTM snapshot
  // TODO: in terms of aborts/throughput this does not seem to help
  // HTM_STATUS_TYPE status;
  // int retries = 0;
  // const int MAX_RETRIES = 2;

  // while (1) {
  //   if (HTM_begin(status) != HTM_CODE_SUCCESS) {
  //     nbSnapshotRestarts++;
  //     if (retries > MAX_RETRIES) {
  //       break;
  //     }
  //     retries++;
  //     continue;
  //   }
  //   for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //     // __m256i data = _mm256_load_si256((__m256i *)&G_observed_ts[i]);
  //     // _mm256_store_si256((__m256i *)(&myPCWCinfo[i]), data);
  //     myPCWCinfo[i].LLC = G_observed_ts[i].comm.LLC;
  //     myPCWCinfo[i].TS = G_observed_ts[i].comm.ts;
  //     myPCWCinfo[i].LPC = G_observed_ts[i].comm.LPC;
  //   }
  //   HTM_commit();
  //   break;
  // }

  // if (retries < MAX_RETRIES) return;
// ------------------------------------------
  
  int restart;
  // int isDone[gs_appInfo->info.nbThreads];

  // memset(isDone, 0, gs_appInfo->info.nbThreads * sizeof(int));

#ifndef DISABLE_FLAG_SNAPSHOT
  __atomic_store_n(&(gs_ts_array[threadId].comm2.waitSnapshot), 1, __ATOMIC_RELEASE);
#endif
  // __asm__ __volatile__("mfence" ::: "memory");
  while (1) {
    restart = 0;
    int i;
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      // if (isDone[i]/* || counter[i]*/) {
      //   continue;
      // }
      // TODO: It seems that waiting before yields more throughput
      // NOTE: if you enable it it causes a deadlock due to waitSnapshot
      // isDone[i] = 1;
      __m256i data = _mm256_load_si256((__m256i *)&G_observed_ts[i]);
      _mm256_store_si256((__m256i *)(&myPCWCinfo[i]), data);
    }
    __asm__ __volatile__("mfence" ::: "memory");
    for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      uint64_t newVersion = __atomic_load_n(&G_observed_ts[i].comm.version, __ATOMIC_ACQUIRE);
      if (myPCWCinfo[i].version != newVersion || (isBit63One(myPCWCinfo[i].TS) && myPCWCinfo[i].TS != -1)) {
        restart = 1;
	      // isDone[i] = 0;
      }
    }
  
    if (!restart) break;
    nbSnapshotRestarts++; // restarted
  }
#ifndef DISABLE_FLAG_SNAPSHOT
  __atomic_store_n(&(gs_ts_array[threadId].comm2.waitSnapshot), 0, __ATOMIC_RELEASE);
#endif
}

static inline void computeNewLLC_fast(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
  myPCWCinfo[threadId].LLC = myPCWCinfo[threadId].TS;
  myPCWCinfo[threadId].LPC = myPCWCinfo[threadId].TS;
  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    // compute the thread that is behind me
    // TODO: must either flag the guy behind is done waiting OR wait for all the threads behind me
    if (myPCWCinfo[i].TS != -1 && LARGER_THAN(myPCWCinfo[threadId].TS, myPCWCinfo[i].TS, threadId, i)) {
      whoIsBehindMe[whoIsBehindMeCount] = i;
      whoIsBehindMeCount++;
    }
  }
}

// results in myPCWCinfo[threadId]
static inline void computeNewLLC(int threadId, volatile pcwc_info_s *myPCWCinfo)
{
  int futureTX_id = -1;
  pcwc_info_s futureTX = { .LPC = 0 };
  int base = 0;
  int offset;

  for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
    if (i == threadId) continue;
    if (myPCWCinfo[i].TS == -1 && LARGER_THAN(myPCWCinfo[i].LPC, myPCWCinfo[threadId].TS, i, threadId)) {
      // futureTX is TX with TS = -1 && min(LPC), s.t., LPC > myTS // next futureTX
      if (futureTX_id == -1 || LARGER_THAN(futureTX.LPC, myPCWCinfo[i].LPC, futureTX_id, i)) {
        futureTX.LPC = myPCWCinfo[i].LPC;
        futureTX.LLC = myPCWCinfo[i].LLC;
        futureTX_id = i;
      }
    }
    // compute the thread that is behind me
    // TODO: must either flag the guy behind is done waiting OR wait for all the threads behind me
    if (myPCWCinfo[i].TS != -1 && LARGER_THAN(myPCWCinfo[threadId].TS, myPCWCinfo[i].TS, threadId, i)) {
      whoIsBehindMe[whoIsBehindMeCount] = i;
      whoIsBehindMeCount++;
    }
  }

  if (futureTX_id == -1) { // no futureTX
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
  } else {
    // base = futureTX.LLC
    // offset = -1 - ( count TXs with TS!=-1 && TS > myTS && futureTX.LPC > TS)
    offset = -1;
    base =  futureTX.LLC;
    for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      if (myPCWCinfo[i].TS != -1 &&
          LARGER_THAN(myPCWCinfo[i].TS, myPCWCinfo[threadId].TS, i, threadId)
          && LARGER_THAN(futureTX.LPC, myPCWCinfo[i].TS, futureTX_id, i)) {
        offset--;
      }
    }
  }

  // copy this to the public cache lines
  myPCWCinfo[threadId].LLC = base + offset;
  myPCWCinfo[threadId].LPC = myPCWCinfo[threadId].TS;
}

void on_after_htm_commit_pcwc(int threadId)
{
  MEASURE_TS(timeAfterTXTS1);
  int localWhoIsBehindMe[gs_appInfo->info.nbThreads];

  // __atomic_store_n(myObservedTS, readClockTSC, __ATOMIC_RELEASE);

   // NOTE: cannot take a new clock, must really use this one, because future TXs may be tricked and continue
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), G_observed_ts[threadId].comm.ts, __ATOMIC_RELEASE);
#endif
#ifndef DISABLE_FLAG_IS_IN_TX
  __atomic_store_n(&(gs_ts_array[threadId].comm2.isInTX), 0, __ATOMIC_RELEASE);
#endif

  if (writeLogStart == writeLogEnd) {
    __atomic_store_n(&(G_observed_ts[threadId].comm.ts), -1, __ATOMIC_RELEASE);
    goto ret; // read only TX
  }

  __m256i data256i = { readClockTSC, gs_pcwc_info[threadId][threadId].LLC,
    gs_pcwc_info[threadId][threadId].LPC, gs_pcwc_info[threadId][threadId].version + 2};
  _mm256_store_si256((__m256i*)&G_observed_ts[threadId].comm, data256i);
  
  whoIsBehindMeCount = 0;
  whoIsBehindMe = (int*)localWhoIsBehindMe;

  // flush log entries
  writeLogEnd = (writeLogEnd + gs_appInfo->info.allocLogSize - 1) % gs_appInfo->info.allocLogSize;
  FLUSH_RANGE(&P_write_log[threadId][writeLogStart], &P_write_log[threadId][writeLogEnd],
    &P_write_log[threadId][0], P_write_log[threadId] + gs_appInfo->info.allocLogSize);
  writeLogEnd = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;

  // snapshot all threads (this cost is amortized in the flush)
  MEASURE_TS(timeScanningTS1);
  if (useFastPCWC == 0) {
    scanTransactionsVect(threadId, gs_pcwc_info[threadId]);
  } else if (useFastPCWC == 2) {
    scanTransactionsVect(threadId, gs_pcwc_info[threadId]);
  } else {
    scanTransactions_fast(threadId, gs_pcwc_info[threadId]);
  }

  if (useFastPCWC == 0 || useFastPCWC == 2) {
    computeNewLLC(threadId, gs_pcwc_info[threadId]);
  } else {
    computeNewLLC_fast(threadId, gs_pcwc_info[threadId]);
  }

  MEASURE_TS(timeScanningTS2);
  INC_PERFORMANCE_COUNTER(timeScanningTS1, timeScanningTS2, timeScanning);

  FENCE_PREV_FLUSHES();

  /* Commits the write log (commit marker) */
  // int startCommitMarker = writeLogEnd;

  write_log_thread[writeLogEnd] = onesBit63(gs_pcwc_info[threadId][threadId].LLC);
  FLUSH_CL(&write_log_thread[writeLogEnd]);
  int nextLogNext = (writeLogEnd + 1) % gs_appInfo->info.allocLogSize;


  FENCE_PREV_FLUSHES();

  // // wait busy snaphot TXs (it is not 100% accurate though)
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //   while (gs_ts_array[i].comm2.waitSnapshot);
  // }

  // TODO: check the version algorithm
  // G_observed_ts[threadId].comm.version = gs_pcwc_info[threadId][threadId].version + 1;
  // __asm__ __volatile__("mfence" ::: "memory");
  data256i = (__m256i){ -1, gs_pcwc_info[threadId][threadId].LLC,
    gs_pcwc_info[threadId][threadId].LPC, gs_pcwc_info[threadId][threadId].version + 2};
  _mm256_store_si256((__m256i*)&G_observed_ts[threadId].comm, data256i);
  // G_observed_ts[threadId].comm.version = gs_pcwc_info[threadId][threadId].version + 2;
  // __asm__ __volatile__("mfence" ::: "memory");

  // __atomic_thread_fence(__ATOMIC_SEQ_CST);
  wait_commit_fn(threadId); // with PCWC it is possible to wait after flushing the commit marker
  // fprintf(dumpFile, "  LLC=%lu (budget=%i)\n", gs_pcwc_info[threadId][threadId].LLC, HTM_SGL_budget);
  // for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //   fprintf(dumpFile, "[%i] <LLC=%li,LPC=%lx,CPC=%lx,version=%i>\n", i, gs_pcwc_info[threadId][i].LLC, gs_pcwc_info[threadId][i].LPC, gs_pcwc_info[threadId][i].TS, gs_pcwc_info[threadId][i].version);
  // }
  // __atomic_thread_fence(__ATOMIC_SEQ_CST);

  __atomic_store_n(&G_next[threadId].log_ptrs.write_log_next, nextLogNext, __ATOMIC_RELEASE);
ret:
#ifndef DISABLE_APROX_WAIT_CLOCK
  __atomic_store_n(&(gs_ts_array[threadId].comm2.ts), -1, __ATOMIC_RELEASE);
#endif
  MEASURE_INC(countCommitPhases);
}

void wait_commit_pcwc(int threadId)
{
  MEASURE_TS(timeWaitingTS1);
  uint64_t snapshot[gs_appInfo->info.nbThreads];
  uint64_t myTS = gs_pcwc_info[threadId][threadId].TS;
  int countThreadsDone = 0;

  memset(snapshot, 0, sizeof(uint64_t)*gs_appInfo->info.nbThreads);

  if (whoIsBehindMeCount == 0) goto ret; // Done!
  // the preCounter could go on a different Cache Line from the HTM_TS

  while (countThreadsDone != whoIsBehindMeCount) {
    // take snapshot

    for (int i = 0; i < whoIsBehindMeCount; ++i) {
      if (whoIsBehindMe[i] == -1) continue;
#ifndef DISABLE_APROX_WAIT_CLOCK
      snapshot[i] = zeroBit63(__atomic_load_n(&(gs_ts_array[whoIsBehindMe[i]].comm2.ts), __ATOMIC_ACQUIRE));
#else
      snapshot[i] = zeroBit63(__atomic_load_n(&(G_observed_ts[whoIsBehindMe[i]].comm.ts), __ATOMIC_ACQUIRE));
#endif
    }

    for (int i = 0; i < whoIsBehindMeCount; ++i) {
      if (whoIsBehindMe[i] == -1) continue;
      if (snapshot[i] == -1 || LARGER_THAN(snapshot[i], myTS, whoIsBehindMe[i], threadId)) {
        whoIsBehindMe[i] = -1;
        countThreadsDone++;
      } 
    }
  }

ret:
  MEASURE_TS(timeWaitingTS2);
  INC_PERFORMANCE_COUNTER(timeWaitingTS1, timeWaitingTS2, timeWaiting);
}
