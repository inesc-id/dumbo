#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE   600
#define _BSD_SOURCE     1
#define _GNU_SOURCE     1
#define _DEFAULT_SOURCE 1

#include "global_structs.h"
#include "impl.h" /* also includes global_structs */
#include "containers.h"
#include "htm_impl.h"

#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <stdlib.h>
#include <string.h>

// TODO: just added this for vcode to stop complaining (not needed)
// #include "htm_arch.h"
#ifndef ARCH_CACHE_LINE_SIZE
#define ARCH_CACHE_LINE_SIZE 64
#endif

#ifndef NVMALLOC_FILE
#define NVMALLOC_FILE "nvmalloc_file"
#endif /* NVMALLOC_FILE */

#ifndef NVMALLOC_THREAD_PRIV_FILE
#define NVMALLOC_THREAD_PRIV_FILE "nvmalloc_file_priv_t"
#endif /* NVMALLOC_THREAD_PRIV_FILE */

#ifndef NVMALLOC_THREAD_SHAR_FILE
#define NVMALLOC_THREAD_SHAR_FILE "nvmalloc_file_shar_t"
#endif /* NVMALLOC_THREAD_SHAR_FILE */

#ifndef NVMALLOC_SIZE
#define NVMALLOC_SIZE 16777216L /* 1048576L - 1MB */
#endif /* NVMALLOC_SIZE */

#ifndef NVMALLOC_THREAD_PRIV_SIZE
#define NVMALLOC_THREAD_PRIV_SIZE (1L<<29) /* 1GB */
#endif /* NVMALLOC_THREAD_PRIV_SIZE */

#ifndef NVMALLOC_THREAD_SHAR_SIZE
#define NVMALLOC_THREAD_SHAR_SIZE 1048576L /* 1MB + logs to be defined in init */
#endif /* NVMALLOC_THREAD_SHAR_SIZE */

/*extern*/__thread long nbTransactions = 0;

static uint64_t nvmalloc_count = 0;
static void *nvmalloc0_base_ptr;
static void *nvmalloc0_current_ptr;
static void *nvmalloc1_base_ptr;
static void *nvmalloc1_current_ptr;
static size_t nvmalloc_size = NVMALLOC_SIZE;

static void **nvmalloc_thr_priv_base_ptr;
static void **nvmalloc_thr_shar_base_ptr;
static __thread void *nvmalloc_thr_priv_base_ptr2 = NULL;
static __thread void *nvmalloc_thr_priv_current_ptr = NULL;
static __thread void *nvmalloc_thr_shar_base_ptr2 = NULL;
static __thread void *nvmalloc_thr_shar_current_ptr = NULL;
static long nvmalloc_thr_priv_size = NVMALLOC_THREAD_PRIV_SIZE;
static long nvmalloc_thr_shar_size = NVMALLOC_THREAD_SHAR_SIZE;

static int pidChildProc;

volatile __thread uint64_t timeTotalTS1 = 0;
volatile __thread uint64_t timeAfterTXTS1 = 0;
volatile __thread uint64_t timeTotalTS2 = 0;
volatile __thread uint64_t timeTotal = 0;

__thread uint64_t timeSGL_TS1 = 0;
__thread uint64_t timeSGL_TS2 = 0;
__thread uint64_t timeSGL = 0;
uint64_t timeSGL_global = 0;

__thread uint64_t timeAbortedTX_TS1 = 0;
__thread uint64_t timeAbortedTX_TS2 = 0;
__thread uint64_t timeAbortedUpdTX = 0;
__thread uint64_t timeAbortedROTX = 0;
uint64_t timeAbortedTX_global = 0;

volatile __thread uint64_t timeAfterTXSuc = 0;
volatile __thread uint64_t timeAfterTXFail = 0;

static void *nvramRanges[1024];
static int nbNvramRanges;

static const int EPOCH_TIMOUT = 32;

// pinning matrix, G_PINNING[threadID] <-- coreID
// const int G_PINNING_0[] = { // intel14_v1
//    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
//   14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
//   28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
//   42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55
// };
// const int G_PINNING_0[] = { // ngstorage (NUMA not available)
//    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
//   20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39
// };
/* extern */const int G_PINNING_0[] = { // nvram (16C/32T +NUMA)
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

// const int G_PINNING_1[] = { // intel14_v1 (HT first, NUMA second)
//     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
//    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
//    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
//    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55
//  };
// const int G_PINNING_1[] = { // node02
//    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
//   36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
//   18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
//   54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71
// };
// const int G_PINNING_1[] = { // ngstorage (NUMA not available)
//   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
//  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39
// };
/* extern */const int G_PINNING_1[] = { // nvram (16C/32T +NUMA)
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

// const int G_PINNING_2[] = { // intel14_v1 (pair with HT)
//   0,  28, 1,  29, 2,  30, 3,  31, 4,  32, 5,  33, 6,  34,
//   7,  35, 8,  36, 9,  37, 10, 38, 11, 39, 12, 40, 13, 41,
//   14, 42, 15, 43, 16, 44, 17, 45, 18, 46, 19, 47, 20, 48,
//   21, 49, 22, 50, 23, 51, 24, 52, 25, 53, 26, 54, 27, 55
// };
// const int G_PINNING_2[] = { // ngstorage (NUMA not available)
//   0,  20, 1,  21, 2,  22, 3,  23, 4,  24, 5,  25, 6,  26,
//   7,  27, 8,  28, 9,  29, 10, 30, 11, 31, 12, 32, 13, 33,
//   14, 34, 15, 35, 16, 36, 17, 37, 18, 38, 19, 39
// };
/* extern */const int G_PINNING_2[] = { // nvram (16C/32T +NUMA)
  0, 32,  1, 33,  2, 34,  3, 35,  4, 36,  5, 37,  6, 38,  7, 39,
  8, 40,  9, 41, 10, 42, 11, 43, 12, 44, 13, 45, 14, 46, 15, 47,
  16, 48, 17, 49, 18, 50, 19, 51, 20, 52, 21, 53, 22, 54, 23, 55,
  24, 56, 25, 57, 26, 58, 27, 59, 28, 60, 29, 61, 30, 62, 31, 63,
};

// NUMA matrix G_NUMA_PINNING[coreID] <-- numa_node_id
/* extern */const int G_NUMA_PINNING[] = { // nvram
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1
};

#ifndef PM_DIR
// #define PM_DIR "/mnt/nvram"
 #define PM_DIR "./"
#endif /* PM_DIR */

// numa_node nvram regions
/* extern */const char* NVRAM_REGIONS[] = {PM_DIR "0/", PM_DIR "1/"};

/* extern */wait_commit_fn_t wait_commit_fn = wait_commit_pcwm;
/* extern */prune_log_fn_t try_prune_log_fn = try_prune_log_epoch_impa;
/* extern */prune_log_fn_t prune_log_fn = prune_log_forward_epoch_impa;

// one must call install_bindings_<solution> to set this
/* extern */void(*on_htm_abort)(int threadId);
/* extern */void(*on_before_htm_begin)(int threadId, int ro);
/* extern */void(*on_before_htm_write)(int threadId, void *addr, uint64_t val);
/* extern */void(*on_before_htm_commit)(int threadId);
/* extern */void(*on_after_htm_commit)(int threadId);
/* extern */void(*on_before_sgl_commit)(int threadId);
/*extern */replay_log_next_entry_s(*log_replay_next_tx_search)();
/*extern */uint64_t(*log_replay_next_tx_apply)(replay_log_next_entry_s curPtr);

// 1 cache line per thread to flag the current state
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  large_cache_line_s *gs_ts_array;

/*extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  large_cache_line_s *G_observed_ts;

/*extern */volatile pcwc_info_s **gs_pcwc_info;

/*extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  large_cache_line_s *P_last_safe_ts;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t **P_epoch_ts;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t *P_epoch_persistent; /* persistent */

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  cache_line_s *G_next;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  cache_line_s *gs_appInfo;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  cache_line_s gs_log_data;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t **P_write_log;

/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  int P_start_epoch = 0; /* persistent */

/* extern */volatile int *G_epoch_lock;

// TODO: these are also extern
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  int32_t *G_flag_checkpointer_exit;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  int32_t *G_flag_checkpointer_ready;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  int32_t *G_flag_checkpointer_done;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  cache_line_s **G_flag_checkpointer_G_next;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t ***G_flag_checkpointer_P_write_log;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t **G_flag_checkpointer_P_last_safe_ts;
/* extern */volatile __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
  uint64_t **G_flag_checkpointer_P_last_safe_ts;

/* extern */volatile __thread void(*onBeforeWrite)(int, void*, uint64_t); /* = on_before_htm_write*/
/* extern */volatile __thread void(*onBeforeHtmCommit)(int); /* = on_before_htm_commit */
/* extern */volatile __thread uint64_t *write_log_thread; /* = &(P_write_log[threadId][0]); */

static void *alocateInNVRAM(const char *memRegion, const char *file, size_t bytes, long mapFlag, void *addr)
{
  char fileNameBuffer[1024];
  void *res = NULL;
  int fd;
  
  sprintf(fileNameBuffer, "%s%s", memRegion, file);
  fd = open(fileNameBuffer, O_CREAT | O_TRUNC | O_RDWR, 0666);
  close(fd); // writes the permissions
  fd = open(fileNameBuffer, O_CREAT | O_RDWR, 0666);

  if (fd == -1) {
    fprintf(stderr, "Error open file %s: %s\n", fileNameBuffer, strerror(errno));
  }

  // TODO: ftruncate needed after munmap...
  if (ftruncate(fd, bytes)) { // if address != NULL there was a ftruncate before
    fprintf(stderr, "Error ftruncate file %s: %s\n", fileNameBuffer, strerror(errno));
  }

  if (addr != NULL) {
    res = mmap(addr, bytes, PROT_READ | PROT_WRITE, mapFlag | MAP_FIXED, fd, 0);
    if (res != addr) {
      fprintf(stderr, "Error getting the requested address %p (got %p): %s\n", addr, res, strerror(errno));
    }
  } else {
    res = mmap(NULL, bytes, PROT_READ | PROT_WRITE, mapFlag, fd, 0);
  }
  if (res == (void*)-1 || res == NULL) {
    fprintf(stderr, "Error mmapping file %s: %s\n", fileNameBuffer, strerror(errno));
  }
  return res;
}

void* internal_replay_log_apply_fn(void* replayerId); // implemented on impl_log_replayer.cpp

static void checkpointer()
{
  // TODO: pin this thread to some unused slot
  __atomic_store_n(G_flag_checkpointer_ready, 1, __ATOMIC_RELEASE);

  pthread_t threads[gs_appInfo->info.nbReplayers];
  if (log_replay_flags & LOG_REPLAY_PARALLEL) {
    int i;
    for (i = 0; i < gs_appInfo->info.nbReplayers-1; i++) {
      pthread_create(&(threads[i]), NULL, internal_replay_log_apply_fn, (void*)((uintptr_t)i+1));
    }
  }

  if (log_replay_flags == 0 || isCraftySet) {
    __atomic_store_n(G_flag_checkpointer_done, 1, __ATOMIC_RELEASE);
    exit(EXIT_SUCCESS);
  }
  if (!(log_replay_flags & (LOG_REPLAY_CC_HTM|LOG_REPLAY_CONCURRENT))) {
    while(!__atomic_load_n(G_flag_checkpointer_exit, __ATOMIC_ACQUIRE)) {
      sched_yield();
      usleep(100);
    }
  }
  // after exit replay the log (assumes very long logs)

  //   /* ----------------------------- */
  // // fork again for perf
  // if (fork() == 0) {
  //   char buf[1024];
  //   sprintf(buf, "perf stat -p %d -e L1-dcache-load-misses,L1-dcache-loads,LLC-load-misses,LLC-loads sleep 5 ", pidChildProc);
  //   printf("[PERF] attaching to proc %d\n!", pidChildProc);
  //   execl("/bin/sh", "sh", "-c", buf, NULL);
  //   // done with this code
  //   exit(0);
  // }
  // /* ----------------------------- */

  // TODO: strange bug: the log is not visible on replay_log_init
  while (!(__atomic_load_n(G_flag_checkpointer_P_write_log, __ATOMIC_ACQUIRE))); // must not be NULL!

  replay_log_init(gs_appInfo->info.nbThreads, gs_appInfo->info.nbReplayers, gs_appInfo->info.allocLogSize,
    *G_flag_checkpointer_G_next, *G_flag_checkpointer_P_write_log, *G_flag_checkpointer_P_last_safe_ts,
    ccHTM_Q, log_replay_flags, nvramRanges, nbNvramRanges);

  replay_log_apply();

  if (log_replay_flags & LOG_REPLAY_PARALLEL) {
    int i;
    for (i = 0; i < gs_appInfo->info.nbReplayers-1; i++) {
      pthread_join(threads[i], NULL);
    }
  }

  replay_log_print_stats(log_replay_stats_file);

  __atomic_store_n(G_flag_checkpointer_done, 1, __ATOMIC_RELEASE);
  exit(EXIT_SUCCESS);
}

void *nvmalloc_init(
  int nbThreads,
  uint64_t logBytesPerThread,
  uint64_t sharedBytes,
  int *pinning,
  int *numa_nodes,
  char *nvram_regions[]
) {
  nvmalloc_thr_priv_size = NVMALLOC_THREAD_PRIV_SIZE;
  nvmalloc_thr_shar_size = NVMALLOC_THREAD_SHAR_SIZE;

  // TODO: fork the process here, map shared in the child process (checkpointer), and private in the father
  volatile intptr_t readSharedMem = 0;

  // put this in a non-DAX fs to make use of shared DRAM mappings
  G_flag_checkpointer_exit = (int*)alocateInNVRAM("./", "flag_checkpointer",
    4096 /* var space */ + sizeof(cc_htm_queue_s), MAP_SHARED, NULL);
  char *beginFlags = (char*)G_flag_checkpointer_exit;
  int sizeOfExitFlag = sizeof(__typeof__(G_flag_checkpointer_exit));
  G_flag_checkpointer_ready = (__typeof__(G_flag_checkpointer_ready))(beginFlags + sizeOfExitFlag);
  G_flag_checkpointer_done = (__typeof__(G_flag_checkpointer_done))(beginFlags + 2*sizeOfExitFlag);
  G_flag_checkpointer_P_write_log = (__typeof__(G_flag_checkpointer_P_write_log)) (beginFlags + 4*sizeOfExitFlag);
  G_flag_checkpointer_P_last_safe_ts = (__typeof__(G_flag_checkpointer_P_last_safe_ts)) (beginFlags + 6*sizeOfExitFlag);
  G_flag_checkpointer_G_next = (__typeof__(G_flag_checkpointer_G_next)) (beginFlags + 8*sizeOfExitFlag); // sizeof(int32_t) != sizeof(void*)
  ccHTM_Q = (__typeof__(ccHTM_Q)) (beginFlags + 4096);

  ccHTM_Q->redHeadIdx = CC_HTM_RED_Q_HEAD;
  ccHTM_Q->txCounter = 0;
  ccHTM_Q->txMin = 1;

  *G_flag_checkpointer_exit = 0;
  *G_flag_checkpointer_ready = 0;
  *G_flag_checkpointer_done = 0;

  nvmalloc_thr_priv_base_ptr = malloc(nbThreads * sizeof(void*));
  nvmalloc_thr_shar_base_ptr = malloc(nbThreads * sizeof(void*));
  nvmalloc_thr_shar_size += logBytesPerThread;
  nvmalloc_size += sharedBytes;

  char localMallocFile0[1024];
  char localMallocFile1[1024];
  sprintf(localMallocFile0, "%s%i", NVMALLOC_THREAD_PRIV_FILE, 0);
  sprintf(localMallocFile1, "%s%i", NVMALLOC_THREAD_PRIV_FILE, 1);

  long poolAreas = nbThreads <= 1 ? 1 : 32; // TODO
  // long poolAreasNUMA0 = nbThreads <= 32 ? nbThreads : 32;
  // long poolAreasNUMA1 = nbThreads > 32 ? 1 : nbThreads - 32;

  long mapFlag = MAP_PRIVATE;
  if (isCraftySet) mapFlag = /*MAP_SHARED_VALIDATE|MAP_SYNC*/MAP_SHARED;
  //void *addrNUMA0 = alocateInNVRAM(nvram_regions[0], localMallocFile0, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2, mapFlag, NULL);
  //void *addrNUMA1 = alocateInNVRAM(nvram_regions[1], localMallocFile1, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2, mapFlag, NULL);

  void *addrNUMA0= malloc(nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2);
  void *addrNUMA1= malloc(nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2);

  // TODO: test if using HTM on MAP_SHARED is slower
  nvmalloc0_base_ptr = ((char*)addrNUMA0) + nvmalloc_thr_priv_size * poolAreas;
  nvmalloc1_base_ptr = ((char*)addrNUMA1) + nvmalloc_thr_priv_size * poolAreas;
  // for (long i = 0; i < nvmalloc_size / sizeof(intptr_t); i += 4096 / sizeof(intptr_t)) {
  //   readSharedMem = ((intptr_t*)nvmalloc_base_ptr)[i];
  //   __atomic_store_n(&((intptr_t*)nvmalloc_base_ptr)[i], readSharedMem, __ATOMIC_RELEASE);
  //   if (i > 1048576) break;
  // }
  nvmalloc0_current_ptr = nvmalloc0_base_ptr;
  nvmalloc1_current_ptr = nvmalloc1_base_ptr;

  char logMallocFile0[1024];
  char logMallocFile1[1024];
  sprintf(logMallocFile0, "%s%i", NVMALLOC_THREAD_SHAR_FILE, 0);
  sprintf(logMallocFile1, "%s%i", NVMALLOC_THREAD_SHAR_FILE, 1);

  //void *sharNUMA0 = alocateInNVRAM(nvram_regions[0], logMallocFile0,
  //  nvmalloc_thr_shar_size * poolAreas, /*MAP_SHARED_VALIDATE|MAP_SYNC*/MAP_SHARED, NULL);
  //void *sharNUMA1 = alocateInNVRAM(nvram_regions[1], logMallocFile1,
  //  nvmalloc_thr_shar_size * poolAreas, /*MAP_SHARED_VALIDATE|MAP_SYNC*/MAP_SHARED, NULL);

  void *sharNUMA0 =  malloc(nvmalloc_thr_shar_size * poolAreas);
  void *sharNUMA1 =  malloc(nvmalloc_thr_shar_size * poolAreas);

  if (!sharNUMA0 || !sharNUMA1)
  {
    fprintf(stderr, "Not enough memory! Requires %zu (nvmalloc_thr_shar_size=%zu, logBytesPerThread=%zu)\n",
      2 * nvmalloc_thr_shar_size * poolAreas, nvmalloc_thr_shar_size, logBytesPerThread);
    exit(-1);
  }

  int countNUMAThr0 = 0;
  int countNUMAThr1 = 0;
  int i;
  for (i = 0; i < nbThreads ; ++i) {
      int coreId = pinning[i];
      // int nodeId = numa_nodes[coreId];
      int nodeId = i % 2; // TODO: this one is round-robin

      if (nodeId == 0) {
        nvmalloc_thr_priv_base_ptr[i] = ((char*)addrNUMA0) + nvmalloc_thr_priv_size * countNUMAThr0;
        nvmalloc_thr_shar_base_ptr[i] = ((char*)sharNUMA0) + nvmalloc_thr_shar_size * countNUMAThr0;
        // for (long j = 0; j < nvmalloc_thr_priv_size / sizeof(intptr_t) - 1; j += 4096 / sizeof(intptr_t)) {
        //   readSharedMem = ((intptr_t*)nvmalloc_thr_priv_base_ptr[countNUMAThr0])[j];
        //   __atomic_store_n(&((intptr_t*)nvmalloc_thr_priv_base_ptr[countNUMAThr0])[j], readSharedMem, __ATOMIC_RELEASE);
        //   // if (j > 1048576) break;
        // }
        countNUMAThr0++;
      } else {
        nvmalloc_thr_priv_base_ptr[i] = ((char*)addrNUMA1) + nvmalloc_thr_priv_size * countNUMAThr1;
        nvmalloc_thr_shar_base_ptr[i] = ((char*)sharNUMA1) + nvmalloc_thr_shar_size * countNUMAThr1;
        // for (long j = 0; j < nvmalloc_thr_priv_size / sizeof(intptr_t) - 1; j += 4096 / sizeof(intptr_t)) {
        //   readSharedMem = ((intptr_t*)nvmalloc_thr_priv_base_ptr[countNUMAThr1])[j];
        //   __atomic_store_n(&((intptr_t*)nvmalloc_thr_priv_base_ptr[countNUMAThr1])[j], readSharedMem, __ATOMIC_RELEASE);
        //   // if (j > 1048576) break;
        // }
        countNUMAThr1++;
      }

  }

  if (fork() == 0) { // child process
    // VERY IMPORTANT!!!
    // munmap the previous addresses and mmap them again in MAP_SHARED | MAP_FIXED mode
    exit(0); //todo replayer
    pidChildProc = getpid();

    void *prevAddr0 = addrNUMA0;
    void *prevAddr1 = addrNUMA1;

    if (munmap(addrNUMA0, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2)) {
      fprintf(stderr, "Error munmap %s: %s\n", NVMALLOC_FILE, strerror(errno));
    }
    if (munmap(addrNUMA1, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2)) {
      fprintf(stderr, "Error munmap %s: %s\n", NVMALLOC_FILE, strerror(errno));
    }

    addrNUMA0 = alocateInNVRAM(nvram_regions[0], localMallocFile0, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2,
      /*MAP_SHARED_VALIDATE|MAP_SYNC*/MAP_SHARED, prevAddr0);
    addrNUMA1 = alocateInNVRAM(nvram_regions[1], localMallocFile1, nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2,
      /*MAP_SHARED_VALIDATE|MAP_SYNC*/MAP_SHARED, prevAddr1);

    nvmalloc0_base_ptr = ((char*)addrNUMA0) + nvmalloc_thr_priv_size * poolAreas;
    nvmalloc1_base_ptr = ((char*)addrNUMA1) + nvmalloc_thr_priv_size * poolAreas;
    nvmalloc0_current_ptr = nvmalloc0_base_ptr;
    nvmalloc1_current_ptr = nvmalloc1_base_ptr;

    nvramRanges[2*nbNvramRanges] = addrNUMA0;
    nvramRanges[2*nbNvramRanges+1] = (void*)(((uintptr_t)addrNUMA0) + nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2);
    nbNvramRanges++;

    nvramRanges[2*nbNvramRanges] = addrNUMA1;
    nvramRanges[2*nbNvramRanges+1] = (void*)(((uintptr_t)addrNUMA1) + nvmalloc_thr_priv_size * poolAreas + nvmalloc_size / 2);
    nbNvramRanges++;

    checkpointer();
  }

  // TODO: if the replayer is not active this must be commented
  //while(!__atomic_load_n(G_flag_checkpointer_ready, __ATOMIC_ACQUIRE)) {
  //  sched_yield(); // wait the checkpointer
  //  usleep(100);
  //}

  return (void*)(readSharedMem ^ readSharedMem); /* make sure the child reads the mem! */
}

static void *nvmalloc_local_impl(int threadId, size_t bytes, int useThreadLocalVars, int isShared)
{
  intptr_t curPtr, basePtr;
  volatile void *addr;
  // static __thread uint64_t accumulatedBytes = 0;

  if (isShared) {
    // shared
    if (useThreadLocalVars) {
      if (nvmalloc_thr_shar_current_ptr == NULL) {
        nvmalloc_thr_shar_current_ptr = nvmalloc_thr_shar_base_ptr[threadId];
        nvmalloc_thr_shar_base_ptr2 = nvmalloc_thr_shar_base_ptr[threadId];
      }
      addr = nvmalloc_thr_shar_current_ptr;
      nvmalloc_thr_shar_current_ptr = (void*)((intptr_t)nvmalloc_thr_shar_current_ptr + bytes);
      curPtr = (intptr_t)nvmalloc_thr_shar_current_ptr;
      basePtr = (intptr_t)nvmalloc_thr_shar_base_ptr2;
    } else {
      addr = nvmalloc_thr_shar_base_ptr[threadId];
      nvmalloc_thr_shar_base_ptr[threadId] = (void*)((intptr_t)nvmalloc_thr_shar_base_ptr[threadId] + bytes);
      curPtr = (intptr_t)nvmalloc_thr_shar_base_ptr[threadId];
      basePtr = (intptr_t)nvmalloc_thr_shar_base_ptr[threadId];
    }

    if (curPtr > basePtr + nvmalloc_thr_shar_size) {
      addr = (void*)-1;
      fprintf(stderr, "[nvmalloc_local]: shared alloc out of space\n");
    }
  } else {
    // private
    if (useThreadLocalVars) {
      if (nvmalloc_thr_priv_current_ptr == NULL) {
        nvmalloc_thr_priv_current_ptr = nvmalloc_thr_priv_base_ptr[threadId];
        nvmalloc_thr_priv_base_ptr2 = nvmalloc_thr_priv_base_ptr[threadId];
      }
      addr = nvmalloc_thr_priv_current_ptr;
      nvmalloc_thr_priv_current_ptr = (void*)((intptr_t)nvmalloc_thr_priv_current_ptr + bytes);
      curPtr = (intptr_t)nvmalloc_thr_priv_current_ptr;
      basePtr = (intptr_t)nvmalloc_thr_priv_base_ptr2;
    } else {
      addr = nvmalloc_thr_priv_base_ptr[threadId];
      nvmalloc_thr_priv_base_ptr[threadId] = (void*)((intptr_t)nvmalloc_thr_priv_base_ptr[threadId] + bytes);
      curPtr = (intptr_t)nvmalloc_thr_priv_base_ptr[threadId];
      basePtr = (intptr_t)nvmalloc_thr_priv_base_ptr[threadId];
    }

    if (curPtr > basePtr + nvmalloc_thr_priv_size) {
      addr = (void*)-1;
      fprintf(stderr, "[nvmalloc_local]: private alloc out of space (MAX = %lu)\n", nvmalloc_thr_priv_size);
    }
  }
  // accumulatedBytes += bytes;
  // printf("[%i] nvmalloc_local = %p (%zu B, acc = %zu)\n", threadId, addr, bytes, accumulatedBytes);
  return (void*)addr;
}

void *nvmalloc_local(int threadId, size_t bytes)
{
  return nvmalloc_local_impl(threadId, bytes, 1/* useThreadLocalVars */, 0/* !isShared */);
}

void *nvmalloc(size_t bytes)
{
  //intptr_t addr;
  //if (__sync_fetch_and_add(&nvmalloc_count, 1) % 2) {
  //  addr = __sync_fetch_and_add((intptr_t*)&nvmalloc0_current_ptr, bytes);
  //  if (addr + bytes > (intptr_t)nvmalloc0_base_ptr + (nvmalloc_size / 2)) {
  //    addr = -1;
  //    fprintf(stderr, "[nvmalloc]: out of space (total space = %zu, alloc = %zu)\n",
  //      nvmalloc_size / 2, bytes);
  //  }
  //} else {
  //  addr = __sync_fetch_and_add((intptr_t*)&nvmalloc1_current_ptr, bytes);
  //  if (addr + bytes > (intptr_t)nvmalloc1_base_ptr + (nvmalloc_size / 2)) {
  //    addr = -1;
  //    fprintf(stderr, "[nvmalloc]: out of space (total space = %zu, alloc = %zu)\n",
  //      nvmalloc_size / 2, bytes);
  //  }
  //}
  //return (void*)addr;
  return malloc(bytes);
}

void nvfree(void *ptr)
{
  free(ptr);
}

void global_structs_init(
  int nbThreads,
  int nbReplayers,
  uint64_t allocEpochs,
  uint64_t allocLogSize,
  uint64_t localMallocSize,
  uint64_t sharedMallocSize,
  int spinsFlush,
  int *pinning,
  int *numa_nodes,
  char *nvram_regions[]
) {

  // TODO: check whether the log pointers are persistent or not

  nvmalloc_thr_priv_size = localMallocSize;

  if (HTM_read_only_storage1_size > sizeof(cache_line_s)) {
    gs_appInfo = (cache_line_s*)HTM_read_only_storage1;
  } else {
    EASY_MALLOC(gs_appInfo, 1);
  }

  gs_appInfo->info.isExit          = 0;
  gs_appInfo->info.nbThreads       = nbThreads;
  gs_appInfo->info.nbReplayers     = nbReplayers;
  gs_appInfo->info.allocEpochs     = allocEpochs;
  gs_appInfo->info.allocLogSize    = allocLogSize;
  gs_appInfo->info.localMallocSize = localMallocSize;
  gs_appInfo->info.epochTimeout    = EPOCH_TIMOUT;
  gs_appInfo->info.spinsFlush      = spinsFlush;

  gs_log_data.log.epoch_end        = allocEpochs - 1;
  gs_log_data.log.who_is_pruning   = -1;
 
  nvmalloc_init(nbThreads, allocLogSize*sizeof(uint64_t) + allocEpochs*sizeof(uint64_t), sharedMallocSize,
    pinning, numa_nodes, nvram_regions);

  EASY_MALLOC(gs_ts_array, nbThreads);
  memset((void*)gs_ts_array, 0, sizeof(large_cache_line_s)*nbThreads);

  EASY_MALLOC(G_observed_ts, nbThreads);
  memset((void*)G_observed_ts, 0, sizeof(large_cache_line_s)*nbThreads);

  EASY_MALLOC(gs_pcwc_info, nbThreads);

  // EASY_MALLOC(P_epoch_ts, nbThreads);
  // EASY_MALLOC(P_epoch_persistent, allocEpochs);
  // EASY_MALLOC(P_write_log, nbThreads);

  G_next = nvmalloc_local_impl(0, sizeof(cache_line_s) * nbThreads, 0, 1);
  memset((void*)G_next, 0, sizeof(cache_line_s)*nbThreads);
  *G_flag_checkpointer_G_next = G_next;

  P_epoch_ts = nvmalloc(sizeof(uint64_t*) * nbThreads);
  P_epoch_persistent = nvmalloc(sizeof(uint64_t) * allocEpochs);

  P_write_log =  nvmalloc_local_impl(0, sizeof(uint64_t*) * nbThreads, 0, 1);
  *G_flag_checkpointer_P_write_log = P_write_log;

  P_last_safe_ts = nvmalloc_local_impl(0, sizeof(large_cache_line_s), 0, 1);
  *G_flag_checkpointer_P_last_safe_ts = &(P_last_safe_ts->ts);

  memset((void*)P_last_safe_ts, 0, sizeof(large_cache_line_s));

  EASY_MALLOC(phtm_logs, nbThreads);

  EASY_MALLOC(G_epoch_lock, allocEpochs);
  memset((void*)G_epoch_lock, 0, sizeof(int)*allocEpochs);
  int i;
  for (i = 0; i < nbThreads; ++i) {
    
    // TODO: this is local memory, so make sure this does not hit the same cache-line
    EASY_MALLOC(gs_pcwc_info[i], nbThreads + 64); // each thread has the state of each other
    gs_pcwc_info[i] = &(gs_pcwc_info[i][16]);

    //P_epoch_ts[i] = nvmalloc_local_impl(i, sizeof(uint64_t) * allocEpochs, 0, 1);
    P_epoch_ts[i] = malloc(sizeof(uint64_t) * allocEpochs);
    memset((void*)P_epoch_ts[i], 0, sizeof(uint64_t)*allocEpochs);

    uint64_t *startLogMarker = nvmalloc_local_impl(i, sizeof(uint64_t) * 3, 0, 1);
    startLogMarker[0] = (uint64_t)-1;
    startLogMarker[1] = (uint64_t)-1;
    startLogMarker[2] = (uint64_t)-1;
    P_write_log[i] = nvmalloc_local_impl(i, sizeof(uint64_t) * allocLogSize, 0, 1);

    phtm_logs[i] = (PHTM_log_s*)&(P_write_log[i][0]);
    phtm_logs[i]->size = 0;
    phtm_logs[i]->is_persistent = 0;

    // P_write_log does not need initialization
    G_observed_ts[i].ts = (uint64_t)-1;
    G_next[i].log_ptrs.flush_epoch = allocEpochs;
  }

  // --------------------------------
  // PHTM markers
	int nb_cache_lines = PHTM_NB_MARKERS;

  // The markers can be on volatile memory
  // phtm_markers = (PHTM_marker_pool_s*) ((char*)P_write_log[0] + sizeof (PHTM_log_s) + 128);
  // phtm_markers->markers = (PHTM_marker_s*) ((char*)P_write_log[0] + sizeof (PHTM_log_s) + sizeof (PHTM_marker_pool_s) + 128);
  phtm_markers = (PHTM_marker_pool_s*) malloc(sizeof(PHTM_marker_pool_s));
  phtm_markers->markers = (PHTM_marker_s*) malloc(sizeof(PHTM_marker_s) * PHTM_NB_MARKERS);
	phtm_markers->nb_markers = nb_cache_lines;

	for (i = 0; i < nb_cache_lines; ++i) {
		phtm_markers->markers[i].tid = -1;
	}
  // --------------------------------
}

void nvmalloc_print_stats(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\n",
              "NB_MALLOCS",
              "NB_BYTES_NUMA0",
              "NB_BYTES_NUMA1");
  }
  fprintf(fp, "%lu\t%lu\t%lu\n", nvmalloc_count,
    (uintptr_t)nvmalloc0_current_ptr - (uintptr_t)nvmalloc0_base_ptr,
    (uintptr_t)nvmalloc1_current_ptr - (uintptr_t)nvmalloc1_base_ptr);
	fclose(fp);
}

void global_structs_destroy()
{
  free((void*)gs_ts_array);
  //nvfree((void*)G_next); // TODO
  //int i;
  //for (i = 0; i < gs_appInfo->info.nbThreads; ++i) {
  //  nvfree((void*)P_epoch_ts[i]);
  //  nvfree((void*)P_write_log[i]);
  //}
  //nvfree((void*)P_epoch_ts);
  //nvfree((void*)P_write_log);
  if (gs_appInfo != (cache_line_s*)HTM_read_only_storage1) {
    free((void*)gs_appInfo);
  }
  __atomic_store_n(G_flag_checkpointer_exit, 1, __ATOMIC_RELEASE);
  //while(!__atomic_load_n(G_flag_checkpointer_done, __ATOMIC_ACQUIRE)) {
  //  sched_yield(); // wait the checkpointer
  //  usleep(100);
  //}

  // TODO: erase the file
}
