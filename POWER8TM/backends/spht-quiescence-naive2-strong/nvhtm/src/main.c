#ifdef __linux__
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE   600
#define _BSD_SOURCE     1
#define _GNU_SOURCE     1
#define _DEFAULT_SOURCE 1
#endif /* __linux__ */

#include "spins.h"
#include "impl.h"
#include "threading.h"
#include "rdtsc.h"
#include "input_handler.h"
#include "bench.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef __linux__
#include <unistd.h>
#else /* !__linux__ */
#error "TODO: implement a usleep function"
#endif /* __linux__ */

#include "htm_impl.h"

// DANGER: make sure log sizes are base 2
#define USE_BITWISE_FOR_MOD2

// how much to wait until we get impatient (in cycles)?

int NB_THREADS       = 12;
int NB_REPLAYERS     = 1;
unsigned int EXPERIMENT_TIME = 10e6; // in micro-seconds
int USE_MAIN_THREAD = 0;
int TID0_SLOWDOWN   = 0;
long EPOCH_TIMEOUT  = 50000;
int SAME_MEM_POOL   = 0;
long FIX_NUMBER_OF_TXS = -1;
long MALLOC_SIZE = 16777216L; // 1073741824
int FIX_NUMBER_OF_TXS_COMMITTED_THREADS = 0;

long SPINS_EXEC  = 1;
long NB_LOG_ENTRIES = 67108864L;
long NB_READS    = 1;
long NB_WRITES   = 1;
long SPINS_FLUSH = 500; // learned_spins
int numReplayers = 1;
// int FLUSH_LAT    = 500;
// int FORCE_LEARN  = 0;
char PROFILE_FILE[1024]  = "./profile.tsv";
char PROFILE_NVRAM_READS_FILE[1024]  = "./profile_nvram_pstm.tsv";
char ERROR_FILE[1024]  = "stderr"; // if not set prints to stderr
int PINNING      = 0; // 0 == default, 1 == Fill CPU/SMT/NUMA, 2 == SMT/CPU/NUMA
int *PINNING_MAT = NULL;
int usePSTM      = 0;

static __thread void *large_mem_region = NULL;
static void *shared_mem_region = NULL;

extern __thread long bench_read_time;
extern __thread long bench_tot_time;
extern __thread long bench_read_time_samples;
static long tot_bench_read_time = 0;
static long tot_bench_tot_time = 0;
static long tot_bench_read_time_samples = 0;

/* performance counters */
long nbExecStates = 0;
double timeBench = 0;
int startNow = 0;
__thread int isStarted = 0;

long nbSuccess = 0, nbFallback = 0, nbAbort = 0, nbConfl = 0, nbCapac = 0, nbExpli = 0, nbOther = 0;

unsigned long *count_txs;

static volatile int countNbThreadsReady = 0;

void(*state_profile)(int);
void(*state_print_profile)(char*);

// static void wait_fn()
// {
//   // struct timespec sleepTime = {.tv_sec = 0, .tv_nsec = 100000};
//   // nanosleep(&sleepTime, NULL); // sleeps for 100us
//   sched_yield();
// }

static void inTxFn(void *arg)
{
  intptr_t id = (intptr_t)arg;
  if (id == 0) {
    spin_fn(TID0_SLOWDOWN);
  }
  spin_fn(SPINS_EXEC);
}

static void thread_main(int id, int nb_thrs, void *arg)
{
  int core = id;
  if (!usePSTM) {
    HTM_thr_init(id);
    HTM_set_is_record(1); // collects aborts statistics
  } else {
    bench_stm_init_thread();
  }

  // -----------------------
  core = PINNING_MAT[id];
  // -----------------------

  threading_pinThisThread(core); // coreID == threadID (affinity only for 1 core)

  if (SAME_MEM_POOL) {
    large_mem_region = shared_mem_region;
  } else {
    if (!usePSTM) {
      large_mem_region = nvmalloc_local(id, MALLOC_SIZE / nb_thrs); // shared region by all threads
    } else {
      large_mem_region = bench_palloc(id, MALLOC_SIZE / nb_thrs); // shared region by all threads
    }
    // memset((void*)large_mem_region, 0, sharedMemRegionSize);
    ((int64_t*)large_mem_region)[(MALLOC_SIZE / nb_thrs) / sizeof(int64_t) - 1] = 0;
  }

  __atomic_fetch_add(&countNbThreadsReady, 1, __ATOMIC_RELAXED);
  while (!(countNbThreadsReady == nb_thrs)) sched_yield();

  // uint64_t ts0,ts1, tot = 0;
  while (!gs_appInfo->info.isExit) {

    // ts0 = rdtscp();

    if (!usePSTM && FIX_NUMBER_OF_TXS == -1 && startNow && !isStarted) {
      isStarted = 1;
      HTM_reset_status_count();
      count_txs[id] = 0;
    }

    // bench_no_conflicts(large_mem_region, id, SPINS_EXEC, inTxFn, (void*)(intptr_t)id);
    if (!usePSTM) {
      if (SAME_MEM_POOL) {
        bench_no_conflicts_with_reads(large_mem_region, MALLOC_SIZE,
          id, NB_READS, NB_WRITES, inTxFn, (void*)(intptr_t)id);
      } else {
        bench_no_conflicts_with_reads(large_mem_region, MALLOC_SIZE / nb_thrs,
          id, NB_READS, NB_WRITES, inTxFn, (void*)(intptr_t)id);
      }
    } else {
      if (SAME_MEM_POOL) {
        bench_no_conflicts_with_reads_stm(large_mem_region, MALLOC_SIZE,
          id, NB_READS, NB_WRITES, inTxFn, (void*)(intptr_t)id);
      } else {
        bench_no_conflicts_with_reads_stm(large_mem_region, MALLOC_SIZE / nb_thrs,
          id, NB_READS, NB_WRITES, inTxFn, (void*)(intptr_t)id);
      }
    }

    // // if we spin here the impatient does not proceed
    // state_commit(id); // post commit
    count_txs[id]++;
    // __atomic_thread_fence(__ATOMIC_ACQUIRE); // TODO: remove this fence (acquires isExit)

    if (FIX_NUMBER_OF_TXS != -1 && count_txs[id] > FIX_NUMBER_OF_TXS) {
      __atomic_fetch_add(&FIX_NUMBER_OF_TXS_COMMITTED_THREADS, 1, __ATOMIC_RELAXED);
      break;
    }

    // ts1 = rdtscp();
    // tot += ts1 - ts0;
  }

  // printf("Cycles per TX = %f\n", (float)tot / (float)count_txs[id]);
  // printf("Cycles in  TX = %f\n", (float)bench_tot_time / (float)bench_read_time_samples);

  __atomic_fetch_add(&tot_bench_read_time, bench_read_time, __ATOMIC_RELAXED);
  __atomic_fetch_add(&tot_bench_tot_time, bench_tot_time, __ATOMIC_RELAXED);
  __atomic_fetch_add(&tot_bench_read_time_samples, bench_read_time_samples, __ATOMIC_RELAXED);

  // __atomic_fetch_add(&countNbThreadsReady, 1, __ATOMIC_RELAXED);
  // while (!(countNbThreadsReady == 2 * nb_thrs)) sched_yield();

  if (!usePSTM) {
    __atomic_fetch_add(&nbSuccess,  HTM_get_status_count(HTM_SUCCESS, NULL),  __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbConfl,    HTM_get_status_count(HTM_CONFLICT, NULL), __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbCapac,    HTM_get_status_count(HTM_CAPACITY, NULL), __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbExpli,    HTM_get_status_count(HTM_EXPLICIT, NULL), __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbOther,    HTM_get_status_count(HTM_OTHER, NULL),    __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbFallback, HTM_get_status_count(HTM_FALLBACK, NULL), __ATOMIC_RELAXED);
    __atomic_fetch_add(&nbAbort,    HTM_get_status_count(HTM_ABORT, NULL),    __ATOMIC_RELAXED);

    HTM_thr_exit();
  } else {
    bench_stm_exit_thread();
  }
#ifndef NPROFILE
  if (state_profile != NULL) state_profile(id);
#endif
}

static void input_handler()
{
  int usePhysicalClocks = 1;
  install_bindings_pc(); // may be overrided
  state_profile = state_gather_profiling_info_pc;
  state_print_profile = state_fprintf_profiling_info_pc;
  log_replay_flags = LOG_REPLAY_FORWARD;

  if (input_exists("LOG_REPLAY_BACKWARD") || input_exists("usePCWM3")) {
    log_replay_flags = LOG_REPLAY_BACKWARD;
    printf("LOG_REPLAY_BACKWARD is set\n");
  }

  if (input_exists("LOG_REPLAY_BUFFER_WBINVD")) {
    log_replay_flags |= LOG_REPLAY_BUFFER_WBINVD;
    printf("LOG_REPLAY_BUFFER_WBINVD is set\n");
  } else if (input_exists("LOG_REPLAY_BUFFER_FLUSHES")) {
    log_replay_flags |= LOG_REPLAY_BUFFER_FLUSHES;
    printf("LOG_REPLAY_BUFFER_FLUSHES is set\n");
  } else if (input_exists("LOG_REPLAY_RANGE_FLUSHES")) {
    log_replay_flags |= LOG_REPLAY_RANGE_FLUSHES;
    printf("LOG_REPLAY_RANGE_FLUSHES is set\n");
  }

  if (input_exists("LOG_REPLAY_ASYNC_SORTER")) {
    log_replay_flags |= LOG_REPLAY_ASYNC_SORTER;
    printf("LOG_REPLAY_ASYNC_SORTER is set\n");
  }

  if (input_exists("LOG_REPLAY_STATS_FILE")) {
    input_getString("LOG_REPLAY_STATS_FILE", log_replay_stats_file);
  }
  printf("LOG_REPLAY_STATS_FILE = %s\n", log_replay_stats_file);

  if (input_exists("PINNING")) {
    PINNING = input_getLong("PINNING");
  }
  printf("PINNING is set to %i /* 0 = CPU/NUMA/SMT | 1 = CPU/SMT/NUMA | 2 = SMT/CPU/NUMA */\n", PINNING);
  if (PINNING == 0) {
    PINNING_MAT = (int*)G_PINNING_0;
  } else if (PINNING == 1) {
    PINNING_MAT = (int*)G_PINNING_1;
  } else if (PINNING == 2) {
    PINNING_MAT = (int*)G_PINNING_2;
  }

  if (input_exists("EXPERIMENT_TIME")) {
    EXPERIMENT_TIME = input_getLong("EXPERIMENT_TIME");
  }
  printf("EXPERIMENT_TIME = %u us\n", EXPERIMENT_TIME);

  if (input_exists("SPINS_EXEC")) {
    SPINS_EXEC = input_getLong("SPINS_EXEC");
  }
  printf("SPINS_EXEC = %li\n", SPINS_EXEC);

  if (input_exists("NB_READS")) {
    NB_READS = input_getLong("NB_READS");
  }
  printf("NB_READS = %li\n", NB_READS);

  if (input_exists("NB_WRITES")) {
    NB_WRITES = input_getLong("NB_WRITES");
  }
  printf("NB_WRITES = %li\n", NB_WRITES);

  if (input_exists("NB_LOG_ENTRIES")) {
    NB_LOG_ENTRIES = input_getLong("NB_LOG_ENTRIES");
  }
  printf("NB_LOG_ENTRIES = %li\n", NB_LOG_ENTRIES);

  if (input_exists("PROFILE_NVRAM_READS_FILE")) {
    if (input_getString("PROFILE_NVRAM_READS_FILE", PROFILE_NVRAM_READS_FILE) >= 1024) {
      fprintf(stderr, "string copy exceeded the capacity of the buffer\n");
    }
  }
  printf("PROFILE_NVRAM_READS_FILE = %s\n", PROFILE_NVRAM_READS_FILE);

  if (input_exists("NB_THREADS")) {
    NB_THREADS = input_getLong("NB_THREADS");
  }
  printf("NB_THREADS = %i\n", NB_THREADS);

  if (input_exists("NB_REPLAYERS")) {
    NB_REPLAYERS = input_getLong("NB_REPLAYERS");
  }
  printf("NB_REPLAYERS = %i\n", NB_REPLAYERS);

  if (input_exists("MALLOC_STATS_FILE")) {
    input_getString("MALLOC_STATS_FILE", malloc_stats_file);
  }
  printf("MALLOC_STATS_FILE = %s\n", malloc_stats_file);

  // if (input_exists("FLUSH_LAT")) {
  //   FLUSH_LAT = input_getLong("FLUSH_LAT");
  //   FORCE_LEARN = 1;
  // }
  // printf("FLUSH_LAT = %i\n", FLUSH_LAT);

  // if (input_exists("FORCE_LEARN")) {
  //   FORCE_LEARN = input_getLong("FORCE_LEARN");
  // }
  // printf("FORCE_LEARN = %i\n", FORCE_LEARN);

  if (input_exists("PROFILE_FILE")) {
    if (input_getString("PROFILE_FILE", PROFILE_FILE) >= 1024) {
      fprintf(stderr, "string copy exceeded the capacity of the buffer\n");
    }
  }
  printf("PROFILE_FILE = \"%s\"\n", PROFILE_FILE);

  //extern FILE *error_fp;
  if (input_exists("ERROR_FILE")) {
    if (input_getString("ERROR_FILE", ERROR_FILE) >= 1024) {
      fprintf(stderr, "string copy exceeded the capacity of the buffer\n");
    }
    //error_fp = fopen(ERROR_FILE, "a+");
  } else {
    //error_fp = fopen("error-log-checker.log", "a+");
  }
  printf("ERROR_FILE = \"%s\"\n", ERROR_FILE);

  if (input_exists("spinInCycles")) {
    printf("spinInCycles is set /* use TSC instead of NOPs */\n");
    spin_fn = spin_cycles;
  } else {
    printf("spinInCycles not set /* use NOPs */\n");
  }

  if (input_exists("tid0Slowdown")) {
    TID0_SLOWDOWN = input_getLong("tid0Slowdown");
  }
  printf("tid0Slowdown is set to %i\n", TID0_SLOWDOWN);

  if (input_exists("MALLOC_SIZE")) {
    MALLOC_SIZE = input_getLong("MALLOC_SIZE");
  }
  printf("MALLOC_SIZE is set to %li\n", MALLOC_SIZE);

  if (input_exists("EPOCH_TIMEOUT")) {
    EPOCH_TIMEOUT = input_getLong("EPOCH_TIMEOUT");
  }
  printf("EPOCH_TIMEOUT is set to %li\n", EPOCH_TIMEOUT);

  if (input_exists("SAME_MEM_POOL")) {
    SAME_MEM_POOL = 1;
    printf("SAME_MEM_POOL is set\n");
  }

  if (input_exists("LOG_REPLAY_PARALLEL")) {
    log_replay_flags |= LOG_REPLAY_PARALLEL;
    printf("LOG_REPLAY_PARALLEL is set\n");
  }

  extern int PCWC_haltSnoopAfterAborts;
  extern int PCWC2_haltSnoopAfterAborts;
  if (input_exists("ABORTS_BEFORE_STOP_SNOOP")) {
    PCWC_haltSnoopAfterAborts = input_getLong("ABORTS_BEFORE_STOP_SNOOP");
    PCWC2_haltSnoopAfterAborts = input_getLong("ABORTS_BEFORE_STOP_SNOOP");
  }
  printf("ABORTS_BEFORE_STOP_SNOOP is set to %i\n", PCWC_haltSnoopAfterAborts);

  if (input_exists("FIX_NUMBER_OF_TXS")) {
    FIX_NUMBER_OF_TXS = input_getLong("FIX_NUMBER_OF_TXS") / NB_THREADS;
    printf("FIX_NUMBER_OF_TXS is set to %li\n", FIX_NUMBER_OF_TXS);
  }

  if (input_exists("useLogicalClocks")) {
    printf("useLogicalClocks is set\n");
    usePhysicalClocks = 0;
    install_bindings_lc();
    wait_commit_fn = wait_commit_lc;
    state_profile = state_gather_profiling_info_lc;
    state_print_profile = state_fprintf_profiling_info_lc;
    log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
  } else if (input_exists("usePCWM")) {
    printf("usePCWM is set\n");
    usePhysicalClocks = 0;
    install_bindings_pcwm();
    wait_commit_fn = wait_commit_pcwm;
    state_profile = state_gather_profiling_info_pcwm;
    state_print_profile = state_fprintf_profiling_info_pcwm;
    log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS;
  } else if (input_exists("usePCWM2")) {
    printf("usePCWM2 is set\n");
    usePhysicalClocks = 0;
    install_bindings_pcwm2();
    wait_commit_fn = wait_commit_pcwm2;
    state_profile = state_gather_profiling_info_pcwm2;
    state_print_profile = state_fprintf_profiling_info_pcwm2;
    log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS_SORTED;
  } else if (input_exists("usePCWM3")) {
    printf("usePCWM3 is set\n");
    usePhysicalClocks = 0;
    install_bindings_pcwm3();
    wait_commit_fn = wait_commit_pcwm3;
    state_profile = state_gather_profiling_info_pcwm3;
    state_print_profile = state_fprintf_profiling_info_pcwm3;
    log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS_SORTED_BACKWARD;
  } else if (input_exists("useHTM")) {
    printf("useHTM is set\n");
    usePhysicalClocks = 0;
    install_bindings_htmOnly();
    wait_commit_fn = wait_commit_htmOnly;
    state_profile = state_gather_profiling_info_htmOnly;
    state_print_profile = state_fprintf_profiling_info_htmOnly;
    log_replay_flags = 0;
  } else if (input_exists("useCrafty")) {
    printf("useCrafty is set\n");
    isCraftySet = 1;
    usePhysicalClocks = 0;
    install_bindings_crafty();
    wait_commit_fn = NULL;
    state_profile = state_gather_profiling_info_crafty;
    state_print_profile = state_fprintf_profiling_info_crafty;
    log_replay_flags = 0;
  } else if (input_exists("useCcHTM") || input_exists("useCcHTMbest")) {
    printf("useCcHTM is set");
    usePhysicalClocks = 0;
    log_replay_flags = LOG_REPLAY_CC_HTM;
    install_bindings_ccHTM();
    wait_commit_fn = wait_commit_ccHTM;
    state_profile = state_gather_profiling_info_ccHTM;
    state_print_profile = state_fprintf_profiling_info_ccHTM;
    if (input_exists("useCcHTMbest")) {
      printf(" --- no log re-write \n");
      log_replay_flags |= LOG_REPLAY_CC_HTM_NO_W;
    } else {
      printf("\n");
    }
  } else if (input_exists("usePHTM")) {
    printf("usePHTM is set\n");
    usePhysicalClocks = 0;
    log_replay_flags = 0;
    install_bindings_PHTM();
    wait_commit_fn = wait_commit_PHTM;
    state_profile = state_gather_profiling_info_PHTM;
    state_print_profile = state_fprintf_profiling_info_PHTM;
  } else if (input_exists("useUpperBound")) {
    printf("useUpperBound is set\n");
    usePhysicalClocks = 0;
    log_replay_flags = 0; // TODO
    install_bindings_ub();
    wait_commit_fn = wait_commit_ub;
    state_profile = state_gather_profiling_info_ub;
    state_print_profile = state_fprintf_profiling_info_ub;
  } else if (input_exists("useEpochCommit1")) {
    printf("useEpochCommit1 is set /* patient version */\n");
    usePhysicalClocks = 0;
    log_replay_flags = 0; // TODO
    install_bindings_epoch_sync();
    wait_commit_fn = wait_commit_epoch_sync;
    state_profile = state_gather_profiling_info_epoch_sync;
    state_print_profile = state_fprintf_profiling_info_epoch_sync;
  } else if (input_exists("useEpochCommit2")) {
    printf("useEpochCommit2 is set /* impatient version */\n");
    usePhysicalClocks = 0;
    log_replay_flags = 0; // TODO
    install_bindings_epoch_impa();
    wait_commit_fn = wait_commit_epoch_impa;
    state_profile = state_gather_profiling_info_epoch_impa;
    state_print_profile = state_fprintf_profiling_info_epoch_impa;
  } else if (input_exists("usePCWC-F") || input_exists("useFastPCWC")
      || input_exists("usePCWCnoCheck") || input_exists("useFastPCWCnoCheck")
      || input_exists("usePCWCnoCheckVect")) {
    printf("usePCWC-F is set");
    log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
    usePhysicalClocks = 0;
    install_bindings_pcwc();
    wait_commit_fn = wait_commit_pcwc;
    state_profile = state_gather_profiling_info_pcwc;
    state_print_profile = state_fprintf_profiling_info_pcwc;
    if (input_exists("useFastPCWC") || input_exists("useFastPCWCnoCheck")) {
      printf(" -- fast validation for upper bound\n");
      extern volatile int useFastPCWC; // hacked
      useFastPCWC = 1;
    } else if (input_exists("usePCWCnoCheckVect")) {
      printf(" -- using vectorized instructions for scanning\n");
      extern volatile int useFastPCWC; // hacked
      useFastPCWC = 2; // reusing the variable -> 2 means vect path
    } else {
      printf("\n");
    }
  } else if (input_exists("usePCWC-NF")) {
    printf("usePCWC-NF is set");
    log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
    usePhysicalClocks = 0;
    install_bindings_pcwc2();
    wait_commit_fn = wait_commit_pcwc2;
    state_profile = state_gather_profiling_info_pcwc2;
    state_print_profile = state_fprintf_profiling_info_pcwc2;
    printf("\n");
  } else if (input_exists("useEpochCommit3")) {
    printf("useEpochCommit3 is set /* deadline version */\n");
    usePhysicalClocks = 0;
    install_bindings_epoch_static_deadline();
    wait_commit_fn = wait_commit_epoch_static_deadline;
  } else if (input_exists("usePSTM")) {
    printf("usePSTM is set\n");
    usePhysicalClocks = 0;
    log_replay_flags = 0;
    usePSTM = 1;
    state_profile = NULL;
  }

  if (usePhysicalClocks) {
    log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS;
    printf("usePhysicalClocks is set\n"); // an optimized version, but does not seem that good anymore
    if (input_exists("waitCommitBitArray")) {
      printf("  waitCommitBitArray is set\n");
      wait_commit_fn = wait_commit_pc_bitmap;
    } else {
      printf("  waitCommitBitArray not set\n");
    }
  }

  if (input_exists("LOG_REPLAY_CONCURRENT") && !(input_exists("useCcHTM"))) {
    printf("LOG_REPLAY_CONCURRENT is set\n");
    log_replay_flags |= LOG_REPLAY_CONCURRENT;
  }

  if (input_exists("DISABLE_LOG_REPLAY") && !(input_exists("useCcHTM") || input_exists("useCcHTMbest"))) {
    printf("DISABLE_LOG_REPLAY is set\n");
    log_replay_flags = 0;
  }

  printf(" --- \n");
}

// static void test_latency()
// {
//   unsigned long ts1, ts2;
//
//   ts1 = rdtscp();
//   spin_fn(learnedSpins);
//   ts2 = rdtscp();
//   printf("test 1 spin --> %f ns\n", (ts2 - ts1) / CPU_FREQ * 1e6);
//
//   ts1 = rdtscp();
//   for (int i = 0; i < 9999; ++i) {
//     spin_fn(learnedSpins);
//   }
//   ts2 = rdtscp();
//   printf("test 9999 spins --> %f ns (per spin)\n",
//     (ts2 - ts1) / CPU_FREQ * 1e6 / 9999);
// }

int main(int argc, char **argv)
{
  input_parse(argc, argv);
  input_handler();

  unsigned long ts1, ts2;
  uint64_t memRegionSize = MALLOC_SIZE / NB_THREADS /* local MEM_REGION (1G) */;
  uint64_t sharedMemRegionSize = 0 /* shared MEM_REGION (1G) */;

  if (SAME_MEM_POOL) {
    sharedMemRegionSize = MALLOC_SIZE;
  }

  // TODO: removed emulation
  // learn_spin_nops(CPU_FREQ, FLUSH_LAT, FORCE_LEARN);
  // printf("learned 500ns --> %i spins\n", learnedSpins);
  // SPINS_FLUSH = learnedSpins;
  // test_latency();

  global_structs_init(
    NB_THREADS,
    NB_REPLAYERS,
    1 /* NB_EPOCHS: use max of 1073741824 */,
    // 268435456L
    NB_LOG_ENTRIES /* LOG_SIZE: in nb of entries (134217728) */,
    memRegionSize,
    sharedMemRegionSize,
    SPINS_FLUSH,
    (int*)PINNING_MAT,
    (int*)G_NUMA_PINNING,
    (char**)NVRAM_REGIONS
  );
  
  if (usePSTM) {
    shared_mem_region = bench_stm_init(SAME_MEM_POOL, MALLOC_SIZE);
  } else {

    if (SAME_MEM_POOL) {
      shared_mem_region = nvmalloc(MALLOC_SIZE);
    }
  }

  EASY_MALLOC(count_txs, NB_THREADS);
  memset((void*)count_txs, 0, NB_THREADS*sizeof(unsigned long));
  HTM_init(NB_THREADS);

  threading_start(NB_THREADS, USE_MAIN_THREAD, thread_main, NULL);

  while (!(countNbThreadsReady == NB_THREADS));

  if (FIX_NUMBER_OF_TXS != -1) {
    startNow = 1;
    __atomic_thread_fence(__ATOMIC_RELAXED);
    ts1 = rdtscp();
    while (__atomic_load_n(&FIX_NUMBER_OF_TXS_COMMITTED_THREADS, __ATOMIC_ACQUIRE) < NB_THREADS) {
      usleep(10);
    }
    gs_appInfo->info.isExit = 1;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ts2 = rdtscp();
  } else {
    usleep(500000); // waits 0.5s for warm-up
    // resets statistics
    __atomic_thread_fence(__ATOMIC_RELAXED);
    startNow = 1;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ts1 = rdtscp();
    usleep(EXPERIMENT_TIME);
    gs_appInfo->info.isExit = 1;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ts2 = rdtscp();
  }
	
	int i;
  for (i = 0; i < NB_THREADS; ++i) {
    nbExecStates += count_txs[i];
  }

  timeBench = (ts2 - ts1) / CPU_FREQ;
  threading_join();
  
  
  // printf("threads joined! Checking log ...\n");
  // if (input_exists("usePCWC-NF") || input_exists("usePCWC-F")) {
  //   // TODO: check blocks
  //   gs_appInfo->info.isExit = 0;
  //   state_log_checker_pcwc(wait_fn, 1);
  //   gs_appInfo->info.isExit = 1;
  // }
  // printf("   ... log checked!\n");


  // printf("Duration: %f ms, execStates: %li (%f /s)\n",
  //   timeBench, nbExecStates, nbExecStates / timeBench * 1000.0f);

  FILE *fp = fopen(PROFILE_NVRAM_READS_FILE, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", PROFILE_NVRAM_READS_FILE);
  }

  if (usePSTM) {
    bench_stm_exit();
    fseek(fp, 0L, SEEK_END);
    if ( ftell(fp) < 8 ) {
        fprintf(fp, "#%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                "THREADS",
                "NB_READS",
                "SAMPLES",
                "TOT_TIME",
                "READ_INSTRUMENTATION",
                "FLUSH_REDO_LOG",
                "FLUSH_DATA",
                "DESTROY_LOG");
    }
    extern unsigned long tot_pstm_time_flush_redo_log;
    extern unsigned long tot_pstm_time_flush_data;
    extern unsigned long tot_pstm_time_destroy_log;
    extern unsigned long tot_pstm_nb_samples;
    fprintf(fp, "%i\t%li\t%li\t%f\t%f\t%f\t%f\t%f\n", NB_THREADS, NB_READS, tot_bench_read_time_samples,
      (float)tot_bench_tot_time / (float)tot_bench_read_time_samples,
      (float)tot_bench_read_time / (float)tot_bench_read_time_samples,
      (float)tot_pstm_time_flush_redo_log / (float)tot_pstm_nb_samples, 
      (float)tot_pstm_time_flush_data / (float)tot_pstm_nb_samples,
      (float)tot_pstm_time_destroy_log / (float)tot_pstm_nb_samples
    );
    bench_stm_print(NB_THREADS, timeBench / 1000.0);
  } else {
    fseek(fp, 0L, SEEK_END);
    if ( ftell(fp) < 8 ) {
        fprintf(fp, "#%s\t%s\t%s\t%s\t%s\n",
                "THREADS",
                "NB_READS",
                "SAMPLES",
                "TOT_TIME",
                "READ_INSTRUMENTATION");
    }
    fprintf(fp, "%i\t%li\t%li\t%f\t%f\n", NB_THREADS, NB_READS, tot_bench_read_time_samples,
      (float)tot_bench_tot_time / (float)tot_bench_read_time_samples,
      (float)tot_bench_read_time / (float)tot_bench_read_time_samples
    );

    printf("%li commits (%li fallbacks), %li aborts (%li confl, %li capac, prob. %f) \n",
      nbSuccess, nbFallback, nbAbort, nbConfl, nbCapac, (float)nbAbort/(nbAbort+nbSuccess));
    
    // NB_THREADS THROUGHPUT HTM_COMMITS SGL_COMMITS HTM_ABORTS HTM_CONFLICTS HTM_CAPACITY
    printf("%i\t%.0f\t%li\t%li\t%li\t%li\t%li\t%li\t%li\n", NB_THREADS, (double)nbExecStates / timeBench * 1000.0f,
      nbSuccess, nbFallback, nbAbort, nbConfl, nbCapac, nbExpli, nbOther);
    // printf("%i\t%.0f\n", TID0_SLOWDOWN, (double)nbExecStates / timeBench * 1000.0f);
    if (state_print_profile != NULL) {
      state_print_profile(PROFILE_FILE);
    }
    nvmalloc_print_stats(malloc_stats_file);
    global_structs_destroy();
  }

  // printf(" --- latency after bench ---\n");
  // test_latency();
  return EXIT_SUCCESS;
}
