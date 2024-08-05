#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE   600
#define _BSD_SOURCE     1
#define _GNU_SOURCE     1
#define _DEFAULT_SOURCE 1

#include "bench.h"
#include "spins.h"
#include "impl.h"
#include "htm_impl.h"
#include "global_structs.h"
#include <setjmp.h>
#include <signal.h>

#  include <mod_mem.h>
#  include <mod_stats.h>
#  include <stm.h>

#define RAND_R_FNC(seed) ({ \
    uint64_t next = seed; \
    uint64_t result; \
    next *= 1103515245; \
    next += 12345; \
    result = (uint64_t) (next / 65536) % 2048; \
    next *= 1103515245; \
    next += 12345; \
    result <<= 10; \
    result ^= (uint64_t) (next / 65536) % 1024; \
    next *= 1103515245; \
    next += 12345; \
    result <<= 10; \
    result ^= (uint64_t) (next / 65536) % 1024; \
    seed = next; \
    result; \
})

static __thread uint64_t seed = 12345;
static __thread uint64_t seedSet = 0;

void bench_no_conflicts(void *largeMemRegion, int threadId, long spinsExec, void(*inTxFn)(void* arg), void* arg)
{
  uint64_t * addr1 = (uint64_t *) (&((int8_t*)largeMemRegion)[256 * threadId]);
  uint64_t * addr2 = (uint64_t *) (&((int8_t*)largeMemRegion)[256 * threadId + 8]);

  NV_HTM_BEGIN(threadId);

  onBeforeWrite(threadId, addr1, threadId);
  *addr1 = threadId;

  onBeforeWrite(threadId, addr2, threadId);
  *addr2 = threadId;

  inTxFn(arg);
  spin_fn(spinsExec);

  NV_HTM_END(threadId);
}

__thread long bench_read_time = 0;
__thread long bench_tot_time = 0;
__thread long bench_read_time_samples = 0;
static __thread long sampleCount = 0;
#define SAMPLE_COUNT 32

void bench_no_conflicts_with_reads(
  void *largeMemRegion,
  long size_region,
  int threadId,
  long nbReads,
  long nbWrites,
  void(*inTxFn)(void* arg),
  void* arg)
{
  // each thread has 1MB

  // 2147483648
  // uint64_t threadArea = 16777216L / sizeof(uint64_t) / gs_appInfo->info.nbThreads;
  uint64_t threadArea = size_region / sizeof(uint64_t);
  uint64_t readAddrAcc = 0;
  int index;
  uint64_t *addr;
  volatile uint64_t ts0, ts1, ts2, ts3;

  if (!seedSet) {
    seed *= (threadId + 12345);
    seedSet = 1;
  }

  uint64_t save_seed = seed;
  // // prefetch
  // for (int i = 0; i < nbReads + nbWrites; ++i) {
  //   index = (RAND_R_FNC(seed) % threadArea);
  //   addr = &((uint64_t*)largeMemRegion)[index];
  //   __builtin_prefetch(addr, 1, 3);
  //   // readAddrAcc += *addr;
  //   // *addr = readAddrAcc; // TODO: writing here reduces spurious aborts...
  // }
  // // ---------

  sampleCount++;
  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts0 = rdtscp();
  }

  NV_HTM_BEGIN(threadId);
  seed = save_seed;

  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts1 = rdtscp();
  }
  readAddrAcc = 0;
  // index = (RAND_R_FNC(seed) % threadArea);
  int i;
  for (i = 0; i < nbReads; ++i) {
    index = (RAND_R_FNC(seed) % threadArea);
    addr = &((uint64_t*)largeMemRegion)[index];
    readAddrAcc += *addr;
    // printf("[%i] readAddrAcc = %lx\n", threadId, readAddrAcc);
    // index = (index + 1) % threadArea;
  }
  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts2 = rdtscp();
    bench_read_time += ts2 - ts1;
  }

  // if ((RAND_R_FNC(seed) % 1024) == 0) {
    for (i = 0; i < nbWrites; ++i) {
      index = (RAND_R_FNC(seed) % threadArea);
      // index /= 1024; // TODO: makes transactions abort less
      addr = &((uint64_t*)largeMemRegion)[index];
      uint64_t value_to_write = readAddrAcc + 1;
      onBeforeWrite(threadId, addr, value_to_write);
      *addr = value_to_write;
      // index = (index + 1) % threadArea;

      // index = i + threadId * threadArea;
      // addr = &((uint64_t*)largeMemRegion)[index];
      // onBeforeWrite(threadId, addr, readAddrAcc + threadId + 1);
      // *addr = readAddrAcc + threadId + 1;
    }
  // }

  inTxFn(arg);

  NV_HTM_END(threadId);

  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts3 = rdtscp();
    bench_tot_time += ts3 - ts0;
    bench_read_time_samples++;
  }
}

void* bench_stm_init(int sameMemPool, long size)
{
  stm_init();
  mod_mem_init(0);
  mod_stats_init();
  if (sameMemPool) {
    return pstm_nvmalloc(size);
  } else {
    return NULL;
  }
}

void* bench_palloc(int id, long size)
{
  return pstm_local_nvmalloc(id, size);
}

void bench_stm_exit()
{
  stm_exit();
}

void bench_stm_init_thread()
{
  stm_init_thread();
}

void bench_stm_exit_thread()
{
  stm_exit_thread();
}

void bench_stm_print(int nbThreads, double duration)
{
  unsigned long exec_commits, exec_aborts;
  stm_get_global_stats("global_nb_commits", &exec_commits);
  stm_get_global_stats("global_nb_aborts", &exec_aborts);
  printf("#"
    "THREADS\t"      
    "THROUGHPUT\t"         
    "TIME\t"         
    "COMMITS\t"      
    "ABORTS\n"       
  );
  printf("%i\t", nbThreads);
  printf("%f\t", (double)exec_commits / duration);
  printf("%f\t", duration);
  printf("%li\t", exec_commits);
  printf("%li\t", exec_aborts);
  printf("%li\t", 0L);
  printf("%li\n", 0L);
  printf("%li\t", 0L);
}

void bench_no_conflicts_with_reads_stm(
  void *largeMemRegion,
  long size_region,
  int threadId,
  long nbReads,
  long nbWrites,
  void(*inTxFn)(void* arg),
  void* arg)
{
  // each thread has 1MB

  // 2147483648
  // uint64_t threadArea = 16777216L / sizeof(uint64_t) / gs_appInfo->info.nbThreads;
  uint64_t threadArea = size_region / sizeof(uint64_t);
  uint64_t readAddrAcc = 0;
  int index;
  uint64_t *addr;
  volatile uint64_t ts0, ts1, ts2, ts3;

  if (!seedSet) {
    seed *= (threadId + 12345);
    seedSet = 1;
  }

  uint64_t save_seed = seed;
  // // prefetch
  // for (int i = 0; i < nbReads + nbWrites; ++i) {
  //   index = (RAND_R_FNC(seed) % threadArea);
  //   addr = &((uint64_t*)largeMemRegion)[index];
  //   __builtin_prefetch(addr, 1, 3);
  //   // readAddrAcc += *addr;
  //   // *addr = readAddrAcc;
  // }
  // // ---------

  sampleCount++;
  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts0 = rdtscp();
  }

  stm_tx_attr_t _a = {};
  sigjmp_buf *buf = stm_start(_a);
  sigsetjmp(buf, 0);

  seed = save_seed;

  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts1 = rdtscp();
  }
  // index = (RAND_R_FNC(seed) % threadArea);
  readAddrAcc = 0;
  int i;
  for (i = 0; i < nbReads; ++i) {
    index = (RAND_R_FNC(seed) % threadArea);
    addr = &((uint64_t*)largeMemRegion)[index];
    readAddrAcc += stm_load((volatile stm_word_t *)(void *)addr);
    index = (index + 1) % threadArea;
  }
  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts2 = rdtscp();
    bench_read_time += ts2 - ts1;
  }

  for (i = 0; i < nbWrites; ++i) {
    index = (RAND_R_FNC(seed) % threadArea);
    // index /= 1024; // TODO: makes transactions abort less
    addr = &((uint64_t*)largeMemRegion)[index];
    uint64_t value_to_write = readAddrAcc + 1;
    // index = (index + 1) % threadArea;

    stm_store((volatile stm_word_t *)(void *)addr, (stm_word_t)value_to_write);

    // index = i + threadId * threadArea;
    // addr = &((uint64_t*)largeMemRegion)[index];
    // onBeforeWrite(threadId, addr, readAddrAcc + threadId + 1);
    // *addr = readAddrAcc + threadId + 1;
  }

  inTxFn(arg);

  stm_commit();

  if ((sampleCount & (SAMPLE_COUNT-1)) == (SAMPLE_COUNT-1)) {
    ts3 = rdtscp();
    bench_tot_time += ts3 - ts0;
    bench_read_time_samples++;
  }
}

void bench_random_with_reads(void *largeMemRegion, int threadId, long nbReads, long nbWrites, void(*inTxFn)(void* arg), void* arg)
{
  // TODO:
  // static __thread int seed = 0x1234; // TODO: need to be different for each thread
}
