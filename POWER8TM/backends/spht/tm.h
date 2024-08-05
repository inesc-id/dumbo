#ifndef TM_H
#define TM_H 1

#  include <stdio.h>

#ifndef REDUCED_TM_API

#  define MAIN(argc, argv)              int main (int argc, char** argv)
#  define MAIN_RETURN(val)              return val

#  define GOTO_SIM()                    /* nothing */
#  define GOTO_REAL()                   /* nothing */
#  define IS_IN_SIM()                   (0)

#  define SIM_GET_NUM_CPU(var)          /* nothing */

#  define TM_PRINTF                     printf
#  define TM_PRINT0                     printf
#  define TM_PRINT1                     printf
#  define TM_PRINT2                     printf
#  define TM_PRINT3                     printf

#  define P_MEMORY_STARTUP(numThread)   /* nothing */
#  define P_MEMORY_SHUTDOWN()           /* nothing */

#  include <assert.h>
#  include "memory.h"
#  include "types.h"
#  include "thread.h"
#  include "input_handler.h"
#  include <math.h>

#include "spins.h"
#include "impl.h"
#include "htm_impl.h"
#include "threading.h"
#include "impl_pcwm.h"


#  define TM_ARG                         /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_CALLABLE                    /* nothing */


#  define TM_BEGIN_WAIVER()
#  define TM_END_WAIVER()

// outside the TX
//# define S_MALLOC                       nvmalloc
//# define S_FREE                         nvfree
//
//# define P_MALLOC(_size)                ({ void *_PTR = nvmalloc_local(HTM_SGL_tid, _size); /*onBeforeWrite(HTM_SGL_tid, _ptr, 0);*/ _PTR; })
//# define P_MALLOC_THR(_size, _thr)      ({ void *_PTR = nvmalloc_local(_thr, _size); /*onBeforeWrite(HTM_SGL_tid, _ptr, 0);*/ _PTR; })
//# define P_FREE(ptr)                    nvfree(ptr)
//
//// inside the TX
//# define TM_MALLOC(_size)               \
//({ \
//  void *_PTR; \
//  if (!isCraftySet) { \
//    _PTR = nvmalloc_local(HTM_SGL_tid, _size); \
//    onBeforeWrite(HTM_SGL_tid, _PTR, 0); \
//  } else { \
//    /*_PTR = craftyMalloc(HTM_SGL_tid, _size);*/ \
//  } \
//  _PTR; \
//})
//# define TM_FREE(ptr)                   ({ onBeforeWrite(HTM_SGL_tid, ptr, 0); nvfree(ptr); })

#  define P_MALLOC(size)                ({ void* _res = malloc(size); PRE_TOUCH_CACHELINES(_res, size); _res; })
#  define P_FREE(ptr)                   free(ptr)
#  define TM_MALLOC(size)               ({ void* _res = malloc(size); PRE_TOUCH_CACHELINES(_res, size); _res; })
#  define FAST_PATH_FREE(ptr)           free(ptr)
#  define SLOW_PATH_FREE(ptr)           free(ptr)

# define SETUP_NUMBER_TASKS(n)
# define SETUP_NUMBER_THREADS(n)
# define PRINT_STATS()
# define AL_LOCK(idx)

#endif

#ifdef REDUCED_TM_API
#  define SPECIAL_THREAD_ID()          get_tid()
#else
#  define SPECIAL_THREAD_ID()          thread_getId()
#endif

#include "extra_MACROS.h"

# define TM_STARTUP(numThread,dummy) \
  HTM_init(numThread); \
  for (int i = 0; i < 80; i++) memset(&(stats_array[i]), 0, sizeof(stats_array[i])); \
  input_parse_file("nvhtm_params.txt"); \
  learn_spin_nops(CPU_FREQ, FLUSH_LAT, /* FORCE_LEARN */1); \
  input_handler(); \
  global_structs_init( \
    numThread, \
    numReplayers, \
    2 /* NB_EPOCHS: use max of 1073741824 */, \
    1L<<24 /* LOG_SIZE: in nb of entries */, \
    1L<<20 /* LOCAL_MEM_REGION (1M) */, \
    1L<<26 /* SHARED_MEM_REGION (256M) */, \
    /* SPINS_FLUSH */FLUSH_LAT, \
    PINNING_MAT, \
    G_NUMA_PINNING, \
    NVRAM_REGIONS \
  ); \
  init_stats_pcwm(); \
  READ_TIMESTAMP(start_ts); \
  printf("flush lat %d\n",FLUSH_LAT) \
//

# define TM_SHUTDOWN() \
{ \
  FINAL_PRINT(start_ts, end_ts); \
  printf("stats_nbSuccess: %li (%f\%)\nstats_nbAbort: %li (%f\%)\n\tconfl: %li (%f\%)\n\tcapac: %li (%f\%)\n\texpli: %li (%f\%)\n\tother: %li (%f\%)\nstats_nbFallback: %li (%f\%)\n", \
    stats_nbSuccess, (float)(1+stats_nbSuccess) * 100.0f / (stats_nbSuccess+stats_nbAbort+stats_nbFallback+1), \
    stats_nbAbort, (float)(1+stats_nbAbort) * 100.0f / (stats_nbSuccess+stats_nbAbort+stats_nbFallback+1), \
    stats_nbConfl, (float)(1+stats_nbConfl) * 100.0f / (stats_nbAbort+1), \
    stats_nbCapac, (float)(1+stats_nbCapac) * 100.0f / (stats_nbAbort+1), \
    stats_nbExpli, (float)(1+stats_nbExpli) * 100.0f / (stats_nbAbort+1), \
    stats_nbOther, (float)(1+stats_nbOther) * 100.0f / (stats_nbAbort+1), \
    stats_nbFallback, (float)(1+stats_nbFallback) * 100.0f / (stats_nbSuccess+stats_nbAbort+stats_nbFallback+1)); \
  printf("THREADS\tTIME\tNB_HTM_SUCCESS\tNB_FALLBACK\tNB_ABORTS\tNB_CONFL\tNB_CAPAC\tNB_EXPLI\tNB_OTHER\n"); \
  printf("%i\t%lf\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", HTM_get_nb_threads(), stats_benchTime, stats_nbSuccess, stats_nbFallback, \
    stats_nbAbort, stats_nbConfl, stats_nbCapac, stats_nbExpli, stats_nbOther); \
  if (state_print_profile != NULL) { \
    state_print_profile(PROFILE_FILE); \
  } \
  nvmalloc_print_stats(malloc_stats_file); \
  global_structs_destroy(); \
  printf("%li \n",contar_tx);\
}

#ifndef NPROFILE /* do profile */
#  define TM_THREAD_EXIT_PROFILE() \
  if (state_profile != NULL) state_profile(thread_getId());
#else /* NPROFILE */
#  define TM_THREAD_EXIT_PROFILE() /* empty */
#endif /* end NPROFILE */

# define TM_THREAD_ENTER() \
  HTM_thr_init(thread_getId()); \
  stats_nbSuccess = 0; \
  stats_nbAbort = 0; \
  stats_nbConfl = 0; \
  stats_nbCapac = 0; \
  stats_nbExpli = 0; \
  stats_nbOther = 0; \
  stats_nbTrans = 0; \
  stats_nbNonTrans = 0; \
  stats_nbFallback = 0; \
  /* threading_pinThisThread(HTM_SGL_tid); */ /* Done in thread.h */ \
  HTM_set_is_record(1); \
  local_thread_id = SPECIAL_THREAD_ID();\
//

# define TM_THREAD_EXIT() \
  TM_THREAD_EXIT_PROFILE(); \
  int i = local_thread_id; \
  /* stats_array[i].htm_commits = HTM_get_status_count(HTM_SUCCESS, NULL); */\
  stats_array[i].htm_self_conflicts = HTM_get_status_count(HTM_SELF, NULL); \
  stats_array[i].htm_trans_conflicts = HTM_get_status_count(HTM_TRANS, NULL); \
  stats_array[i].htm_nontrans_conflicts = HTM_get_status_count(HTM_NON_TRANS, NULL); \
  stats_array[i].htm_conflict_aborts = stats_array[i].htm_self_conflicts + stats_array[i].htm_trans_conflicts + stats_array[i].htm_nontrans_conflicts; \
  stats_array[i].htm_persistent_aborts = HTM_get_status_count(HTM_PERSISTENT, NULL); \
  stats_array[i].htm_capacity_aborts = HTM_get_status_count(HTM_CAPACITY, NULL); \
  stats_array[i].htm_other_aborts = HTM_get_status_count(HTM_OTHER, NULL); \
  stats_array[i].htm_user_aborts = HTM_get_status_count(HTM_EXPLICIT, NULL); \
  stats_array[i].gl_commits = HTM_get_status_count(HTM_FALLBACK, NULL); \
  __atomic_fetch_add(&stats_nbSuccess,  HTM_get_status_count(HTM_SUCCESS, NULL),  __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbAbort,    HTM_get_status_count(HTM_ABORT, NULL),    __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbConfl,    HTM_get_status_count(HTM_CONFLICT, NULL), __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbCapac,    HTM_get_status_count(HTM_CAPACITY, NULL), __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbExpli,    HTM_get_status_count(HTM_EXPLICIT, NULL), __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbOther,    HTM_get_status_count(HTM_OTHER, NULL),    __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbTrans,    HTM_get_status_count(HTM_TRANS, NULL),    __ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbNonTrans, HTM_get_status_count(HTM_NON_TRANS, NULL),__ATOMIC_SEQ_CST); \
  __atomic_fetch_add(&stats_nbFallback, HTM_get_status_count(HTM_FALLBACK, NULL), __ATOMIC_SEQ_CST); \
  HTM_thr_exit(); \
//

# define IS_LOCKED(lock)        *((volatile int*)(&lock)) != 0

extern int isCraftySet; // need flag for crafty

extern __thread int PCWM_readonly_tx;

#define TM_BEGIN_EXT(id,ro) TM_BEGIN(ro)
# define TM_BEGIN(ro) \
  NV_HTM_BEGIN(HTM_SGL_tid, ro) \
//

# define TM_END() \
  NV_HTM_END(HTM_SGL_tid) \
  /* extern volatile __thread uint64_t timeScanning; \
  stats_array[local_thread_id].wait_time += timeScanning; */ \
//

# define TM_RESTART()                  HTM_abort();
# define TM_EARLY_RELEASE(var)

# define TM_LOCAL_WRITE(var, val)      ({ var = val; var; })
# define TM_LOCAL_WRITE_P(var, val)    ({ var = val; var; })
# define TM_LOCAL_WRITE_D(var, val)    ({ var = val; var; })

# define TM_SHARED_READ(var) (var)
# define TM_SHARED_READ_P(var) (var)
# define TM_SHARED_READ_D(var) (var)
# define TM_SHARED_READ_F(var) (var)

// # define TM_SHARED_WRITE(var, val)   ({ onBeforeWrite(HTM_SGL_tid, &var, val); var = val; var;})
// # define TM_SHARED_WRITE_P(var, val) ({ onBeforeWrite(HTM_SGL_tid, &var, val); var = val; var;})
// # define TM_SHARED_WRITE_D(var, val) ({ onBeforeWrite(HTM_SGL_tid, &var, val); var = val; var;})
// # define TM_SHARED_WRITE_F(var, val) ({ onBeforeWrite(HTM_SGL_tid, &var, val); var = val; var;})
# define TM_SHARED_WRITE(var, val)   ({ MACRO_PCWM_on_before_htm_write_8B_pcwm(HTM_SGL_tid, &var, val); var = val; var;})
# define TM_SHARED_WRITE_P(var, val) ({ MACRO_PCWM_on_before_htm_write_8B_pcwm(HTM_SGL_tid, &var, val); var = val; var;})
# define TM_SHARED_WRITE_D(var, val) ({ MACRO_PCWM_on_before_htm_write_8B_pcwm(HTM_SGL_tid, &var, val); var = val; var;})
# define TM_SHARED_WRITE_F(var, val) ({ MACRO_PCWM_on_before_htm_write_8B_pcwm(HTM_SGL_tid, &var, val); var = val; var;})

// ----------------------------------------------
#  define FAST_PATH_FREE(ptr)           free(ptr)
#  define SLOW_PATH_FREE(ptr)           free(ptr)

#define SLOW_PATH_SHARED_READ TM_SHARED_READ
#define FAST_PATH_SHARED_READ TM_SHARED_READ
#define SLOW_PATH_SHARED_READ_D TM_SHARED_READ_D
#define FAST_PATH_SHARED_READ_D TM_SHARED_READ_D
#define SLOW_PATH_SHARED_READ_P TM_SHARED_READ_P
#define FAST_PATH_SHARED_READ_P TM_SHARED_READ_P

#define SLOW_PATH_SHARED_WRITE TM_SHARED_WRITE
#define FAST_PATH_SHARED_WRITE TM_SHARED_WRITE
#define SLOW_PATH_SHARED_WRITE_D TM_SHARED_WRITE_D
#define FAST_PATH_SHARED_WRITE_D TM_SHARED_WRITE_D
#define SLOW_PATH_SHARED_WRITE_P TM_SHARED_WRITE_P
#define FAST_PATH_SHARED_WRITE_P TM_SHARED_WRITE_P
// ----------------------------------------------

extern void(*state_profile)(int);
extern void(*state_print_profile)(char*);
extern char PROFILE_FILE[1024];
extern char ERROR_FILE[1024];
extern int FLUSH_LAT;
extern int numReplayers;
extern int PINNING; // 0 == default, 1 == Fill CPU/SMT/NUMA, 2 == SMT/CPU/NUMA
extern int *PINNING_MAT;

extern long stats_nbSuccess;
extern long stats_nbFallback;
extern long stats_nbAbort;
extern long stats_nbConfl;
extern long stats_nbCapac;
extern long stats_nbExpli;
extern long stats_nbOther;
extern long stats_nbTrans;
extern long stats_nbNonTrans;
extern double stats_benchTime;

// extern __thread long stats_nbTXwrites;

static void input_handler()
{
  int usePhysicalClocks = 1;
  //install_bindings_pcwm(); // may be overrided
  install_bindings_htmOnly();
  //state_profile = state_gather_profiling_info_pc;
  //state_print_profile = state_fprintf_profiling_info_pc;
  spin_fn = spin_cycles;
  log_replay_flags = LOG_REPLAY_FORWARD;

  if (input_exists("LOG_REPLAY_BACKWARD") || input_exists("usePCWM3")) {
    log_replay_flags = LOG_REPLAY_BACKWARD;
    printf("LOG_REPLAY_BACKWARD is set\n");
  }

  if (input_exists("FLUSH_LAT")) {
    FLUSH_LAT = input_getLong("FLUSH_LAT");
  }
  printf("\nFLUSH_LAT = %i\n", FLUSH_LAT);

  if (input_exists("PROFILE_FILE")) {
    if (input_getString("PROFILE_FILE", PROFILE_FILE) >= 1024) {
      fprintf(stderr, "string copy exceeded the capacity of the buffer\n");
    }
  }
  printf("PROFILE_FILE = \"%s\"\n", PROFILE_FILE);

  if (input_exists("ERROR_FILE")) {
    //extern FILE *error_fp;
    if (input_getString("ERROR_FILE", ERROR_FILE) >= 1024) {
      fprintf(stderr, "string copy exceeded the capacity of the buffer\n");
    }
    //error_fp = fopen(ERROR_FILE, "a+");
  }
  printf("ERROR_FILE = \"%s\"\n", ERROR_FILE);

  //extern int PCWC_haltSnoopAfterAborts;
  //extern int PCWC2_haltSnoopAfterAborts;
  //if (input_exists("ABORTS_BEFORE_STOP_SNOOP")) {
  //  PCWC_haltSnoopAfterAborts = input_getLong("ABORTS_BEFORE_STOP_SNOOP");
  //  PCWC2_haltSnoopAfterAborts = input_getLong("ABORTS_BEFORE_STOP_SNOOP");
  //}
  //printf("ABORTS_BEFORE_STOP_SNOOP is set to %i\n", PCWC_haltSnoopAfterAborts);
  
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

  if (input_exists("LOG_REPLAY_STATS_FILE")) {
    input_getString("LOG_REPLAY_STATS_FILE", log_replay_stats_file);
  }
  printf("LOG_REPLAY_STATS_FILE = %s\n", log_replay_stats_file);

  if (input_exists("MALLOC_STATS_FILE")) {
    input_getString("MALLOC_STATS_FILE", malloc_stats_file);
  }
  printf("MALLOC_STATS_FILE = %s\n", malloc_stats_file);

  if (input_exists("LOG_REPLAY_PARALLEL")) {
    log_replay_flags |= LOG_REPLAY_PARALLEL;
    printf("LOG_REPLAY_PARALLEL is set\n");
  }

  if (input_exists("NB_REPLAYERS")) {
    numReplayers = input_getLong("NB_REPLAYERS");
  }
  printf("NB_REPLAYERS = %i\n", numReplayers);

  if (input_exists("useLogicalClocks")) {
    printf("useLogicalClocks is set\n");
    //usePhysicalClocks = 0;
    //install_bindings_lc();
    //wait_commit_fn = wait_commit_lc;
    //state_profile = state_gather_profiling_info_lc;
    //state_print_profile = state_fprintf_profiling_info_lc;
    //log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
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
    //usePhysicalClocks = 0;
    //install_bindings_pcwm3();
    //wait_commit_fn = wait_commit_pcwm3;
    //state_profile = state_gather_profiling_info_pcwm3;
    //state_print_profile = state_fprintf_profiling_info_pcwm3;
    //log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS_SORTED_BACKWARD;
  } else if (input_exists("usePHTM")) {
    printf("usePHTM is set\n");
    //usePhysicalClocks = 0;
    //log_replay_flags = 0;
    //install_bindings_PHTM();
    //wait_commit_fn = wait_commit_PHTM;
    //state_profile = state_gather_profiling_info_PHTM;
    //state_print_profile = state_fprintf_profiling_info_PHTM;
  } else if (input_exists("useCrafty")) {
    printf("useCrafty is set\n");
    //isCraftySet = 1;
    //usePhysicalClocks = 0;
    //install_bindings_crafty();
    //wait_commit_fn = NULL;
    //state_profile = state_gather_profiling_info_crafty;
    //state_print_profile = state_fprintf_profiling_info_crafty;
    //log_replay_flags = 0;
  } else if (input_exists("useUpperBound")) {
    printf("useUpperBound is set\n");
    //usePhysicalClocks = 0;
    //install_bindings_ub();
    //wait_commit_fn = wait_commit_ub;
    //state_profile = state_gather_profiling_info_ub;
    //state_print_profile = state_fprintf_profiling_info_ub;
    //log_replay_flags = 0;
  } else if (input_exists("useEpochCommit1")) {
    printf("useEpochCommit1 is set /* patient version */\n");
    //usePhysicalClocks = 0;
    //install_bindings_epoch_sync();
    //wait_commit_fn = wait_commit_epoch_sync;
    //state_profile = state_gather_profiling_info_epoch_sync;
    //state_print_profile = state_fprintf_profiling_info_epoch_sync;
    //log_replay_flags = 0;
  } else if (input_exists("useEpochCommit2")) {
    printf("useEpochCommit2 is set /* impatient version */\n");
    //usePhysicalClocks = 0;
    //install_bindings_epoch_impa();
    //wait_commit_fn = wait_commit_epoch_impa;
    //state_profile = state_gather_profiling_info_epoch_impa;
    //state_print_profile = state_fprintf_profiling_info_epoch_impa;
    //log_replay_flags = 0;
  } else if (input_exists("usePCWC-F")) {
    printf("usePCWC-F is set");
    //usePhysicalClocks = 0;
    //install_bindings_pcwc();
    //wait_commit_fn = wait_commit_pcwc;
    //state_profile = state_gather_profiling_info_pcwc;
    //state_print_profile = state_fprintf_profiling_info_pcwc;
    //log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
  } else if (input_exists("usePCWC-NF")) {
    printf("usePCWC-NF is set");
    //usePhysicalClocks = 0;
    //install_bindings_pcwc2();
    //wait_commit_fn = wait_commit_pcwc2;
    //state_profile = state_gather_profiling_info_pcwc2;
    //state_print_profile = state_fprintf_profiling_info_pcwc2;
    //log_replay_flags |= LOG_REPLAY_LOGICAL_CLOCKS;
    printf("\n");
  } else if (input_exists("useHTM")) {
    printf("useHTM is set\n");
    //usePhysicalClocks = 0;
    //install_bindings_htmOnly();
    //wait_commit_fn = wait_commit_htmOnly;
    //state_profile = state_gather_profiling_info_htmOnly;
    //state_print_profile = state_fprintf_profiling_info_htmOnly;
    //log_replay_flags = 0;
  } else if (input_exists("useCcHTM") || input_exists("useCcHTMbest")) {
    printf("useCcHTM is set");
    //usePhysicalClocks = 0;
    //log_replay_flags = LOG_REPLAY_CC_HTM;
    //install_bindings_ccHTM();
    //wait_commit_fn = wait_commit_ccHTM;
    //state_profile = state_gather_profiling_info_ccHTM;
    //state_print_profile = state_fprintf_profiling_info_ccHTM;
    if (input_exists("useCcHTMbest")) {
      printf(" --- no log re-write \n");
      //log_replay_flags |= LOG_REPLAY_CC_HTM_NO_W;
    } else {
      printf("\n");
    }
  } else if (input_exists("useEpochCommit3")) {
    printf("useEpochCommit3 is set /* deadline version */\n");
    //usePhysicalClocks = 0;
    //install_bindings_epoch_static_deadline();
    //wait_commit_fn = wait_commit_epoch_static_deadline;
  }

  if (usePhysicalClocks) {
    log_replay_flags |= LOG_REPLAY_PHYSICAL_CLOCKS;
  }

  if (input_exists("DISABLE_LOG_REPLAY") && !input_exists("useCcHTM") && !input_exists("useCcHTMbest")) {
    printf("DISABLE_LOG_REPLAY is set\n");
    log_replay_flags = 0;
  }
  printf(" --- \n");
}

#endif
