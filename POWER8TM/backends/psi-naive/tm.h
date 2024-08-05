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
#  include "thread.h"
#  include "types.h"
#  include <math.h>

#  define TM_ARG                        /* nothing */
#  define TM_ARG_ALONE                  /* nothing */
#  define TM_ARGDECL                    /* nothing */
#  define TM_ARGDECL_ALONE              /* nothing */
#  define TM_CALLABLE                   /* nothing */

#  define TM_BEGIN_WAIVER()
#  define TM_END_WAIVER()

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
#    define SPECIAL_THREAD_ID()         get_tid()
#else
#    define SPECIAL_THREAD_ID()         thread_getId()
#endif

// overrides the abortMarker() on "extra_MACROS.h"
#define abortMarker() SEQL_ABORT(order_ts[q_args.tid].value)

#include "POWER_common.h"
#include "extra_MACROS.h"
#include "seq_log.h"

#  define TM_STARTUP(numThread, bId) \
  place_abort_marker = 1; \
  my_tm_startup(numThread); \
  seql_init(); \
  READ_TIMESTAMP(start_ts); \
// end TM_STARTUP
#  define TM_SHUTDOWN() \
{ \
  seql_destroy(); \
  FINAL_PRINT(start_ts, end_ts); \
/*printf("first time: %d, second time: %d\n",total_first_time,total_second_time);*/ \
} \
// end TM_SHUTDOWN

#  define TM_THREAD_ENTER()       my_tm_thread_enter()
#  define TM_THREAD_EXIT()

# define IS_LOCKED(lock)          *((volatile int*)(&lock)) != 0
# define IS_GLOBAL_LOCKED(lock)   *((volatile int*)(&lock)) == 2
# define TM_BEGIN(ro)             TM_BEGIN_EXT(0,ro)

// static __inline__ 
// __attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
// unsigned long long rdtsc(void) {
//     unsigned long long int result = 0;
//     unsigned long int upper, lower, tmp;
//     __asm__ volatile(
//             "0:               \n\t"
//             "mftbu   %0       \n\t"
//             "mftb    %1       \n\t"
//             "mftbu   %2       \n\t"
//             "cmpw    %2,%0    \n\t"
//             "bne     0b"
//             : "=r"(upper), "=r"(lower), "=r"(tmp)
//             );
//     result = upper;
//     result = result << 32;
//     result = result | lower;
//     return (result);
// }

# define READ_TIMESTAMP(dest) __asm__ volatile("0:                  \n\tmfspr   %0,268           \n": "=r"(dest));


# define atomicInc()   __atomic_add_fetch(&global_order_ts, 1, __ATOMIC_RELEASE) 


//-------------------------------TM_BEGIN------------------------------

# define QUIESCENCE_CALL_GL() { \
	int index; \
	int num_threads = global_numThread; \
  for ( index = 0; index < num_threads; index++ ) \
  { \
    while ( (get_state(ts_state[index].value)) != INACTIVE ) /*wait for active threads*/\
    { cpu_relax(); } \
  } \
};\

# define ACQUIRE_GLOBAL_LOCK(){ \
	UPDATE_STATE(INACTIVE); \
  rmb(); \
	while (! TRY_LOCK(single_global_lock) ) \
  { cpu_relax(); } \
	QUIESCENCE_CALL_GL(); \
};\

//-------------------------------TM_END------------------------------

//todo retirar slowdowns do cmmit log (emulate_pm_slowdown)
//cache line é calculada com um ++ em vez do emulate
# define QUIESCENCE_CALL_ROT(){ \
	for(q_args.index=0; q_args.index < q_args.num_threads; q_args.index++) \
  { \
		if(q_args.index == q_args.tid) \
      continue; \
		q_args.temp = ts_state[q_args.index].value; \
		q_args.state = get_state(q_args.temp); \
		if (q_args.state == ACTIVE) \
				state_snapshot[q_args.index] = q_args.temp; \
		else \
				state_snapshot[q_args.index] = 0; \
  } \
  max_cache_line[q_args.tid].value = 0; \
  q_args.logptr = loc_var.mylogpointer_snapshot;\
  flush_log_entries_no_wait( \
    loc_var.mylogpointer, \
    q_args.logptr, \
    loc_var.mylogstart, \
    loc_var.mylogend \
  );\
	for ( q_args.index = 0; q_args.index < q_args.num_threads; q_args.index++ ) \
  { \
		if ( q_args.index == q_args.tid ) \
      continue; \
		if ( state_snapshot[q_args.index] != 0 ) \
    { \
			while ( ts_state[q_args.index].value == state_snapshot[q_args.index]) \
      { cpu_relax(); } \
		} \
	} \
};


//
//
# define RELEASE_WRITE_LOCK(){ \
  if(loc_var.exec_mode == 1) \
  { \
    if (0) {\
      __TM_end(); \
      RELEASE_READ_LOCK(); \
      /* TODO: avoid this ugly work around to have correct commit stats*/\
      stats_array[q_args.tid].rot_commits++; \
      stats_array[q_args.tid].nontx_commits--; \
    }\
    else { \
      __TM_suspend(); \
      READ_TIMESTAMP(start_sus);\
      stats_array[q_args.tid].tx_time_upd_txs += start_sus - start_tx;\
      __thread long myOldActiveState = ts_state[q_args.tid].value; \
      UPDATE_STATE(INACTIVE); /*JOAO: perf bug fix 25jun*/ \
      /*order_ts[q_args.tid].value = atomicInc(); naive */ \
      QUIESCENCE_CALL_ROT();  \
      rmb(); \
      READ_TIMESTAMP(end_sus);\
      stats_array[q_args.tid].wait_time += end_sus - start_sus;\
      UPDATE_TS_STATE(NON_DURABLE); /*JOAO: optimization 8jul*/ \
      __TM_resume(); \
      __TM_end(); \
      READ_TIMESTAMP(end_tx); \
      stats_array[q_args.tid].commit_time += end_tx - start_tx;\
      /* flush_log_commit_marker( \
        loc_var.mylogpointer, \
        order_ts[q_args.tid].value, \
        loc_var.mylogstart, \
        loc_var.mylogend \
      ); */ \
      if (((start_flush - q_args.start_wait_time)/delay_per_cache_line) < max_cache_line[q_args.tid].value) \
      {\
        READ_TIMESTAMP(start_flush);\
        emulate_pm_slowdown_foreach_line(  /* 0); */ \
        max_cache_line[q_args.tid].value /* computed number of cache-lines to flush*/ \
        - ((q_args.end_wait_time - q_args.start_wait_time)/delay_per_cache_line) /* discount of the wait phase */ );\
        READ_TIMESTAMP(end_flush);\
        stats_array[q_args.tid].flush_time += end_flush-start_flush;\
      }\
      long num_threads = global_numThread; \
      READ_TIMESTAMP(loc_var.start_wait2);\
      /*naive: substituir com logica commit do spht*/\
      for(int index=0; index < num_threads; index++) \
      { \
        if(index == q_args.tid) \
          continue; \
        while(get_state(dur_state[index].value) == NON_DURABLE && get_ts_from_state(ts_state[index].value) < get_ts_from_state(myOldActiveState)) \
        { cpu_relax(); } \
      } \
      /*SEQL_START(order_ts[q_args.tid].value, q_args.tid, ((uint64_t)(loc_var.mylogpointer_snapshot - loc_var.mylogstart))); */\
      /*SEQL_COMMIT(order_ts[q_args.tid].value, (loc_var.mylogpointer - loc_var.mylogstart));*/ \
      READ_TIMESTAMP(loc_var.end_wait2); \
      stats_array[q_args.tid].dur_commit_time += loc_var.end_wait2 - loc_var.start_wait2; \
      UPDATE_STATE(INACTIVE); /* inactive rot*/ \
      stats_array[q_args.tid].rot_commits++; \
      /*printf("release_write_lock numWrites=%d (ou %d)\n", numLoggedWrites, max_cache_line[q_args.tid].value);*/\
    } \
  }\
  else \
  { \
    /* flush redo log entries and durmarker (this is mostly copied from above)*/\
    max_cache_line[q_args.tid].value = 0; \
    q_args.logptr = loc_var.mylogpointer_snapshot;\
    flush_log_entries( \
      loc_var.mylogpointer, \
      q_args.logptr, \
      loc_var.mylogstart, \
      loc_var.mylogend \
    );\
    SEQL_START(order_ts[q_args.tid].value, q_args.tid, ((uint64_t)(loc_var.mylogpointer_snapshot - loc_var.mylogstart))); \
    SEQL_COMMIT(order_ts[q_args.tid].value, (loc_var.mylogpointer - loc_var.mylogstart)); \
    order_ts[q_args.tid].value = global_order_ts++; \
    rmb(); \
    UNLOCK(single_global_lock); \
    stats_array[q_args.tid].gl_commits++; \
  } \
  assert(single_global_lock!=q_args.tid+1);\
};

# define RELEASE_READ_LOCK(){\
  READ_TIMESTAMP(end_tx); \
  stats_array[q_args.tid].tx_time_ro_txs += end_tx - start_tx;\
  rwmb();\
  long myOldActiveState = ts_state[q_args.tid].value; \
  UPDATE_STATE(INACTIVE);\
  stats_array[q_args.tid].nontx_commits++;\
  \ 
  long num_threads = global_numThread; \
  long index;\
  READ_TIMESTAMP(loc_var.start_wait2);\
  /*int spins = 0; */\
  for(index=0; index < num_threads; index++) \
  { \
    if(index == q_args.tid) \
      continue; \
    while(get_state(dur_state[index].value) == NON_DURABLE && get_ts_from_state(dur_state[index].value) < get_ts_from_state(myOldActiveState)) \
    { cpu_relax(); /*spins ++;*/} \
	} \
  /*printf("RO spins = %d\n", spins); */\
  READ_TIMESTAMP(loc_var.end_wait2);\
  stats_array[q_args.tid].readonly_durability_wait_time += loc_var.end_wait2-loc_var.start_wait2;\
} \

//-------------------------------------------------------------------------------
#    define TM_BEGIN_RO()                 TM_BEGIN(1)
#    define TM_RESTART()                  __TM_abort();
#    define TM_EARLY_RELEASE(var)

# define FAST_PATH_RESTART() __TM_abort();

#define SLOW_PATH_SHARED_READ(var)             var;
#define SLOW_PATH_SHARED_READ_P(var)           var;
#define SLOW_PATH_SHARED_READ_D(var)           var;
#define SLOW_PATH_SHARED_READ_F(var)           var;

#define FAST_PATH_SHARED_READ(var)                 var
#define FAST_PATH_SHARED_READ_P(var)               var
#define FAST_PATH_SHARED_READ_D(var)               var
#define FAST_PATH_SHARED_READ_F(var)               var

#define SHARED_WRITE(var, val) ({var = val; loc_var.numLoggedWrites++; write_in_log(loc_var.mylogpointer,&(var),val,loc_var.mylogstart,loc_var.mylogend); var;})
# define FAST_PATH_SHARED_WRITE(var, val)   SHARED_WRITE(var, val)
# define FAST_PATH_SHARED_WRITE_P(var, val) SHARED_WRITE(var, val)
# define FAST_PATH_SHARED_WRITE_D(var, val) SHARED_WRITE(var, val)
# define FAST_PATH_SHARED_WRITE_F(var, val) SHARED_WRITE(var, val)

# define SLOW_PATH_RESTART() FAST_PATH_RESTART()
# define SLOW_PATH_SHARED_WRITE(var, val)     FAST_PATH_SHARED_WRITE(var, val)
# define SLOW_PATH_SHARED_WRITE_P(var, val)   FAST_PATH_SHARED_WRITE_P(var, val)
# define SLOW_PATH_SHARED_WRITE_D(var, val)   FAST_PATH_SHARED_WRITE_D(var, val)
# define SLOW_PATH_SHARED_WRITE_F(var, val)   FAST_PATH_SHARED_WRITE_F(var, val)


#  define TM_LOCAL_WRITE(var, val)      ({var = val; var;})
#  define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#  define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})
#  define TM_LOCAL_WRITE_F(var, val)    ({var = val; var;})


#endif
