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
#  include "thread.h"
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
#  define FAST_PATH_FREE(ptr)            free(ptr)
#  define SLOW_PATH_FREE(ptr)            free(ptr)

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


#include "POWER_common.h"
#include "extra_MACROS.h"

# define READ_TIMESTAMP(dest) __asm__ volatile("0:                  \n\tmfspr   %0,268           \n": "=r"(dest));
#  define TM_STARTUP(numThread, bId)
#  define TM_SHUTDOWN() \
{ \
  FINAL_PRINT(start_ts, end_ts); \
/*printf("first time: %d, second time: %d\n",total_first_time,total_second_time);*/ \
} \
//

#  define TM_THREAD_ENTER()
#  define TM_THREAD_EXIT()

# define IS_LOCKED(lock)        *((volatile long*)(&lock)) != 0
# define TM_BEGIN(b)            TM_BEGIN_EXT(b,0)
# define SPEND_BUDGET(b)	if(RETRY_POLICY == 0) (*b)=0; else if (RETRY_POLICY == 2) (*b)=(*b)/2; else (*b)=--(*b);


# define TM_BEGIN_EXT(b,ro) { \
        int tle_budget = HTM_RETRIES; \
	local_exec_mode = 0; \
	/*backoff = MIN_BACKOFF;*/ \
	loc_var.tid = SPECIAL_THREAD_ID();\
        while (1) { \
            while (IS_LOCKED(single_global_lock)) { \
		cpu_relax(); \
            } \
	    TM_buff_type TM_buff; \
            unsigned char status = __TM_begin(&TM_buff); \
            if (status == _HTM_TBEGIN_STARTED) { \
            	if (IS_LOCKED(single_global_lock)) { \
            		__TM_abort(); \
            	} \
            	break; \
            } \
	    else{ \
		if(__TM_is_persistent_abort(&TM_buff)){ \
			 SPEND_BUDGET(&tle_budget); \
			 stats_array[local_thread_id].htm_persistent_aborts++; \
		} \
		if(__TM_conflict(&TM_buff)){ \
                        stats_array[local_thread_id].htm_conflict_aborts++; \
                        if(__TM_is_self_conflict(&TM_buff)) {stats_array[local_thread_id].htm_self_conflicts++; }\
                        else if(__TM_is_trans_conflict(&TM_buff)) stats_array[local_thread_id].htm_trans_conflicts++; \
                        else if(__TM_is_nontrans_conflict(&TM_buff)) stats_array[local_thread_id].htm_nontrans_conflicts++; \
                        tle_budget--; \
                        /*unsigned long wait; \
                        volatile int j; \
                        cm_seed ^= (cm_seed << 17); \
                        cm_seed ^= (cm_seed >> 13); \
                        cm_seed ^= (cm_seed << 5); \
                        wait = cm_seed % backoff; \
                        for (j = 0; j < wait; j++); \
                        if (backoff < MAX_BACKOFF) \
                                backoff <<= 1;*/ \
                } \
                else if (__TM_user_abort(&TM_buff)) { \
                        stats_array[local_thread_id].htm_user_aborts++; \
                        tle_budget--; \
                } \
                else if(__TM_capacity_abort(&TM_buff)){ \
                        stats_array[local_thread_id].htm_capacity_aborts++; \
			SPEND_BUDGET(&tle_budget); \
                } \
                else{ \
                        stats_array[local_thread_id].htm_other_aborts++; \
                        tle_budget--; \
                } \
            } \
            if (tle_budget <= 0) {   \
		local_exec_mode = 2; \
        	while (! TRY_LOCK(single_global_lock)) { \
                    cpu_relax(); \
                } \
                break; \
	    } \
        } \
}


# define TM_END(){ \
    if (!local_exec_mode) { \
        __TM_suspend(); \
        __TM_resume(); \
	__TM_end(); \
	stats_array[SPECIAL_THREAD_ID()].htm_commits++; \
    } else {    \
    	UNLOCK(single_global_lock); \
	stats_array[SPECIAL_THREAD_ID()].gl_commits++; \
    } \
};

#    define TM_BEGIN_RO()                 TM_BEGIN(0)
#    define TM_RESTART()                  __TM_abort();
#    define TM_EARLY_RELEASE(var)


/*inline intptr_t TxLoad(intptr_t var);
inline intptr_t TxLoad(intptr_t var){
        return var;
}

inline intptr_t TxLoad_P(void* var);
inline intptr_t TxLoad_P(void* var){
        return var;
}*/

//#define FAST_PATH_SHARED_READ(var)               TxLoad((intptr_t)(var))  
//#define FAST_PATH_SHARED_READ_P(var)             TxLoad_P(var)
//#define FAST_PATH_SHARED_READ_D(var)             TxLoad((intptr_t)(var))



# define FAST_PATH_RESTART() __TM_abort();
# define FAST_PATH_SHARED_READ(var) (var)
# define FAST_PATH_SHARED_READ_P(var) (var)
# define FAST_PATH_SHARED_READ_D(var) (var)
# define FAST_PATH_SHARED_READ_F(var) (var)

# define FAST_PATH_SHARED_WRITE(var, val) ({var = val; var;})
# define FAST_PATH_SHARED_WRITE_P(var, val) ({var = val; var;})
# define FAST_PATH_SHARED_WRITE_D(var, val) ({var = val; var;})
# define FAST_PATH_SHARED_WRITE_F(var, val) ({var = val; var;})

# define SLOW_PATH_RESTART() FAST_PATH_RESTART()
# define SLOW_PATH_SHARED_READ(var)           FAST_PATH_SHARED_READ(var)
# define SLOW_PATH_SHARED_READ_P(var)         FAST_PATH_SHARED_READ_P(var)
# define SLOW_PATH_SHARED_READ_F(var)         FAST_PATH_SHARED_READ_F(var)
# define SLOW_PATH_SHARED_READ_D(var)         FAST_PATH_SHARED_READ_D(var)

# define SLOW_PATH_SHARED_WRITE(var, val)     FAST_PATH_SHARED_WRITE(var, val)
# define SLOW_PATH_SHARED_WRITE_P(var, val)   FAST_PATH_SHARED_WRITE_P(var, val)
# define SLOW_PATH_SHARED_WRITE_D(var, val)   FAST_PATH_SHARED_WRITE_D(var, val)
# define SLOW_PATH_SHARED_WRITE_F(var, val)   FAST_PATH_SHARED_WRITE_F(var, val)

#  define TM_LOCAL_WRITE(var, val)      ({var = val; var;})
#  define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#  define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})
#  define TM_LOCAL_WRITE_F(var, val)    ({var = val; var;})


#endif
