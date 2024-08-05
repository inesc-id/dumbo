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

#  define P_MALLOC(size)                malloc(size)
#  define P_FREE(ptr)                   free(ptr)
#  define TM_MALLOC(size)               malloc(size)
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

#  define TM_THREAD_ENTER()        my_tm_thread_enter()
#  define TM_THREAD_EXIT()

# define TM_BEGIN(ro) \
  TM_BEGIN_EXT(0,ro)

# define READ_TIMESTAMP(dest) __asm__ volatile("0: \n\tmfspr   %0,268 \n": "=r"(dest));

// overrides the abortMarker() on "extra_MACROS.h"
#define abortMarker() SEQL_ABORT(order_ts[q_args.tid].value)

#include "POWER_common.h"
#include "extra_MACROS.h"
#include "seq_log.h"

//todo use qf of si
# define QUIESCENCE_CALL_ROT() { \
	q_args.num_threads = (long)global_numThread; \
	READ_TIMESTAMP(q_args.start_wait_time); \
	for ( q_args.index = 0; q_args.index < 80; q_args.index++ ) \
  { \
    if (q_args.index == q_args.num_threads) \
      break; \
		if(q_args.index == loc_var.tid) \
      continue; \
		q_args.temp = ts_state[q_args.index].value; \
		q_args.state = (q_args.temp & (3l<<62))>>62; \
		switch(q_args.state) { \
			case ACTIVE:\
				state_snapshot[q_args.index] = q_args.temp; \
				break;\
			case INACTIVE:\
			case NON_DURABLE:\
			default:\
				state_snapshot[q_args.index] = 0; \
				break;\
		} \
  } \
	for ( q_args.index=0; q_args.index < q_args.num_threads; q_args.index++ ) \
  { \
		if(q_args.index == loc_var.tid) \
      continue; \
		if(state_snapshot[q_args.index] != 0) \
    { \
			while(ts_state[q_args.index].value == state_snapshot[q_args.index]){ \
				cpu_relax(); \
			} \
		} \
	} \
  READ_TIMESTAMP(q_args.end_wait_time); \
  stats_array[local_thread_id].wait_time += q_args.end_wait_time - q_args.start_wait_time; \
	READ_TIMESTAMP(q_args.start_wait_time); \
  q_args.logptr = loc_var.mylogpointer_snapshot; \
  flush_log_entries( \
    loc_var.mylogpointer, \
    q_args.logptr, \
    loc_var.mylogstart, \
    loc_var.mylogend \
  );\
	READ_TIMESTAMP(q_args.end_wait_time); \
  stats_array[local_thread_id].flush_time += q_args.end_wait_time - q_args.start_wait_time; \
}


//todo if spinunlock already has an rmb remove line 491
# define RELEASE_WRITE_LOCK() { \
	if (loc_var.exec_mode == 1) \
  { \
	  __TM_suspend(); \
    READ_TIMESTAMP(q_args.ts1); \
    UPDATE_TS_STATE(NON_DURABLE); /* committing rot */ \
    order_ts[loc_var.tid].value = atomicInc(); \
    SEQL_START(order_ts[q_args.tid].value, q_args.tid, ((uint64_t)(loc_var.mylogpointer_snapshot - loc_var.mylogstart))); \
    QUIESCENCE_CALL_ROT(); \
    rmb(); \
    READ_TIMESTAMP(q_args.ts2); \
    stats_array[loc_var.tid].sus_time += q_args.ts2 - q_args.ts1;\
	  __TM_resume(); \
		__TM_end(); \
    READ_TIMESTAMP(end_tx); \
    stats_array[loc_var.tid].commit_time += end_tx - start_tx;\
	  READ_TIMESTAMP(loc_var.ts1); \
    loc_var.mylogpointer_snapshot = q_args.logptr /* cannot write within sus-res */; \
    SEQL_COMMIT(order_ts[q_args.tid].value, (loc_var.mylogpointer - loc_var.mylogstart)); \
    /* flush_log_commit_marker( \
      loc_var.mylogpointer, \
      order_ts[loc_var.tid].value, \
      loc_var.mylogstart, \
      loc_var.mylogend \
    ); */ \
    READ_TIMESTAMP(loc_var.ts2); \
    stats_array[loc_var.tid].flush_time += loc_var.ts2 - loc_var.ts1; \
    long state;\
	  READ_TIMESTAMP(loc_var.ts1); \
    for ( int index = 0; index < global_numThread; index++ ) \
    { \
      if ( index == loc_var.tid ) \
        continue; \
      state = (ts_state[index].value & (3l<<62))>>62;\
      while(state == NON_DURABLE && order_ts[index].value <= order_ts[loc_var.tid].value) \
      { cpu_relax(); } \
    } \
    READ_TIMESTAMP(loc_var.ts2); \
    stats_array[loc_var.tid].wait2_time += loc_var.ts2 - loc_var.ts1; \
    UPDATE_STATE(INACTIVE); /* inactive rot*/ \
    stats_array[loc_var.tid].rot_commits++; \
	} \
	else \
  { \
    order_ts[loc_var.tid].value = global_order_ts++;\
    rmb();\
		UNLOCK(single_global_lock); \
		stats_array[loc_var.tid].gl_commits++; \
	} \
};

# define RELEASE_READ_LOCK(){\
  rwmb();\
  UPDATE_STATE(INACTIVE);\
  stats_array[loc_var.tid].read_commits++;\
  long num_threads = global_numThread; \
  long index;\
  volatile long ts_snapshot = ts_state[loc_var.tid].value; \
  long state;\
  for ( index = 0; index < num_threads; index++ ) \
  { \
    if(index == loc_var.tid) \
      continue; \
    state = (ts_state[index].value & (3l<<62))>>62; \
    while(state == NON_DURABLE && ts_state[index].value < ts_state[loc_var.tid].value) \
    { cpu_relax(); } \
	} \
}\

//-------------------------------------------------------------------------------
#    define TM_BEGIN_RO()                      TM_BEGIN(1)
#    define TM_RESTART()                       __TM_abort();
#    define TM_EARLY_RELEASE(var)

# define FAST_PATH_RESTART()                   __TM_abort();

#define SLOW_PATH_SHARED_READ(var)             var;
#define SLOW_PATH_SHARED_READ_P(var)           var;
#define SLOW_PATH_SHARED_READ_D(var)           var;
#define SLOW_PATH_SHARED_READ_F(var)           var;

#define FAST_PATH_SHARED_READ(var)                 var
#define FAST_PATH_SHARED_READ_P(var)               var
#define FAST_PATH_SHARED_READ_D(var)               var
#define FAST_PATH_SHARED_READ_F(var)               var

#define SHARED_WRITE(var, val) ({var = val; write_in_log(loc_var.mylogpointer,&(var),val,loc_var.mylogstart,loc_var.mylogend); var;})
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
