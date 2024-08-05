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

#include <asm/unistd.h>
#define rmb()           asm volatile ("sync" ::: "memory")
#define rwmb()           asm volatile ("lwsync" ::: "memory")
#define cpu_relax()     asm volatile ("" ::: "memory");
//#define cpu_relax() asm volatile ("or 31,31,31")
#ifdef REDUCED_TM_API
#    define SPECIAL_THREAD_ID()         get_tid()
#else
#    define SPECIAL_THREAD_ID()         thread_getId()
#endif


#include <htmxlintrin.h>

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_self_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_SELF_INDUCED_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_trans_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_TRANSACTION_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_nontrans_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_NON_TRANSACTIONAL_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_persistent_abort(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_FAILURE_PERSISTENT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_conflict(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  /* Return TEXASR bits 11 (Self-Induced Conflict) through
     14 (Translation Invalidation Conflict).  */
  return (_TEXASR_EXTRACT_BITS (texasr, 14, 4)) ? 1 : 0;
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_user_abort (void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_ABORT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_capacity_abort (void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_FOOTPRINT_OVERFLOW (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_begin_rot (void* const TM_buff)
{
  *_TEXASRL_PTR (TM_buff) = 0;
  if (__builtin_expect (__builtin_tbegin (1), 1)){
    return _HTM_TBEGIN_STARTED;
  }
#ifdef __powerpc64__
  *_TEXASR_PTR (TM_buff) = __builtin_get_texasr ();
#else
  *_TEXASRU_PTR (TM_buff) = __builtin_get_texasru ();
  *_TEXASRL_PTR (TM_buff) = __builtin_get_texasr ();
#endif
  *_TFIAR_PTR (TM_buff) = __builtin_get_tfiar ();
  return 0;
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_tfiar_exact(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
 return _TEXASR_TFIAR_EXACT(texasr);
}

#  define TM_STARTUP(numThread, bId) \
  my_tm_startup(numThread); \
  READ_TIMESTAMP(start_ts); \
//
#  define TM_SHUTDOWN() \
{ \
	  FINAL_PRINT(start_ts, end_ts); \
    /*printf("first time: %d, second time: %d\n",total_first_time,total_second_time);*/ \
} \

#  define TM_THREAD_ENTER() \
  my_tm_thread_enter() \
// end TM_THREAD_ENTER
#  define TM_THREAD_EXIT() \
// end TM_THREAD_EXIT


# define IS_LOCKED(lock) \
  *((volatile int*)(&lock)) != 0 \
// end IS_LOCKED

# define IS_GLOBAL_LOCKED(lock) \
  *((volatile int*)(&lock)) == 2 \
// end IS_GLOBAL_LOCKED

# define TM_BEGIN(ro) \
  TM_BEGIN_EXT(0,ro) \
// end TM_BEGIN

# define READ_TIMESTAMP(dest) __asm__ volatile("0:                  \n\tmfspr   %0,268           \n": "=r"(dest));
//-------------------------------------------------------------------------------

#include "extra_MACROS.h"

//todo use qf of si
# define QUIESCENCE_CALL_ROT(){ \
  __attribute__((aligned(CACHE_LINE_SIZE))) static __thread volatile QUIESCENCE_CALL_ARGS_t q_args;\
	q_args.num_threads = (long)global_numThread; \
	for(q_args.index=0; q_args.index < 80; q_args.index++){ \
    if (q_args.index == q_args.num_threads) break; \
		if(q_args.index == local_thread_id) continue; \
		q_args.temp = ts_state[q_args.index].value; \
		q_args.state = (q_args.temp & (3l<<62))>>62; \
		switch(q_args.state){ \
			case INACTIVE:\
        state_snapshot[q_args.index] = 0; \
        break;\
			case ACTIVE:\
				state_snapshot[q_args.index] = q_args.temp; \
				break;\
			case NON_DURABLE:\
				state_snapshot[q_args.index] = 0; \
				break;\
			default:\
				state_snapshot[q_args.index] = 0; \
				break;\
		} \
  } \
	READ_TIMESTAMP(q_args.start_wait_time); \
	for(q_args.index=0; q_args.index < q_args.num_threads; q_args.index++){ \
		if(q_args.index == local_thread_id) continue; \
		if(state_snapshot[q_args.index] != 0){ \
    /*commit_log(mylogpointer,order_ts[local_thread_id].value,mylogstart,mylogend);*/\
			while(ts_state[q_args.index].value==state_snapshot[q_args.index] || ts_state[q_args.index].value > state_snapshot[q_args.index]){ \
				cpu_relax(); \
			} \
		} \
	} \
  READ_TIMESTAMP(q_args.end_wait_time); \
  stats_array[local_thread_id].wait_time += q_args.end_wait_time - q_args.start_wait_time; \
};


//todo if spinunlock already has an rmb remove line 491
# define RELEASE_WRITE_LOCK(){ \
	if(local_exec_mode == 1){ \
    READ_TIMESTAMP(start_sus);\
	  __TM_suspend(); \
	    UPDATE_TS_STATE(NON_DURABLE); /* committing rot*/ \
      order_ts[local_thread_id].value=atomicInc();\
      rmb(); \
	  __TM_resume(); \
		QUIESCENCE_CALL_ROT(); \
    READ_TIMESTAMP(end_sus);\
    stats_array[local_thread_id].sus_time+=end_sus-start_sus;\
		__TM_end(); \
  \
  READ_TIMESTAMP(end_tx); \
  stats_array[local_thread_id].commit_time += end_tx - start_tx;\
  \
  \
  READ_TIMESTAMP(start_flush);\
  commit_log(mylogpointer,order_ts[local_thread_id].value,mylogstart,mylogend);\
	flush_log_commit_marker(mylogpointer,order_ts[local_thread_id].value,mylogstart,mylogend); \
  READ_TIMESTAMP(end_flush);\
  stats_array[local_thread_id].flush_time+=end_flush-start_flush;\
  long num_threads = global_numThread; \
  long index;\
  long state;\
  READ_TIMESTAMP(start_wait2);\
  for(index=0; index < num_threads; index++){ \
	if(index == local_thread_id) continue; \
  state= (ts_state[index].value & (3l<<62))>>62;\
	while(state == NON_DURABLE && order_ts[index].value <= order_ts[local_thread_id].value){ \
		cpu_relax(); \
	} \
	} \
  READ_TIMESTAMP(end_wait2);\
  stats_array[local_thread_id].wait2_time+=end_wait2-start_wait2;\
		UPDATE_STATE(INACTIVE); /* inactive rot*/ \
		stats_array[local_thread_id].rot_commits++; \
	} \
	else{ \
    order_ts[local_thread_id].value=global_order_ts++;\
    rmb();\
		pthread_spin_unlock(&single_global_lock); \
		stats_array[local_thread_id].gl_commits++; \
	} \
};

# define RELEASE_READ_LOCK(){\
  rwmb();\
  UPDATE_STATE(INACTIVE);\
  stats_array[local_thread_id].read_commits++;\
  \ 
  long num_threads = global_numThread; \
  long index;\
  volatile long ts_snapshot = ts_state[local_thread_id].value; \
  long state;\
  READ_TIMESTAMP(start_wait2);\
  for(index=0; index < num_threads; index++){ \
	if(index == local_thread_id) continue; \
  state= (ts_state[index].value & (3l<<62))>>62;\
	while(state == NON_DURABLE && ts_state[index].value < ts_state[local_thread_id].value){ \
		cpu_relax(); \
	} \
	} \
  READ_TIMESTAMP(end_wait2);\
  stats_array[local_thread_id].wait2_time+=end_wait2-start_wait2;\
}\ 

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

# define FAST_PATH_SHARED_WRITE(var, val)   ({var = val;write_in_log(mylogpointer,&(var),val,mylogstart,mylogend); var;})
# define FAST_PATH_SHARED_WRITE_P(var, val) ({var = val;write_in_log(mylogpointer,&(var),val,mylogstart,mylogend); var;})
# define FAST_PATH_SHARED_WRITE_D(var, val) ({var = val;write_in_log(mylogpointer,&(var),val,mylogstart,mylogend); var;})
# define FAST_PATH_SHARED_WRITE_F(var, val) ({var = val;write_in_log(mylogpointer,&(var),val,mylogstart,mylogend); var;})

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
