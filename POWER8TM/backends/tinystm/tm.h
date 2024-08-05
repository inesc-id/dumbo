#ifndef TM_H
#define TM_H 1

#  include <stdio.h>
#  include <unistd.h>


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
# define LOCKED(lock)        *((volatile long*)(&lock)) != 0

#define USE_HTM_HEAP 1

#  include <string.h>
#include <htmxlintrin.h>
#ifndef REDUCED_TM_API
#  include "thread.h"
#endif


#    define TM_ARG                        /* nothing */
#    define TM_ARG_ALONE                  /* nothing */
#    define TM_ARGDECL                    /* nothing */
#    define TM_ARGDECL_ALONE              /* nothing */
#    define TM_CALLABLE                   /* nothing */

#      include <mod_mem.h>
#      include <mod_stats.h>

#ifdef REDUCED_TM_API
#    define SPECIAL_THREAD_ID()         get_tid()
#else
#    define SPECIAL_THREAD_ID()         thread_getId()
#endif

#define DISJOINT_PARTITIONS 2000
#define MICROBENCHMARK 0
#define REVIVE_MODE_THRESHOLD 5
#define THRESHOLD_CAPACITY 0.8

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_nontrans_conflict(void* const TM_buff)
{
texasr_t texasr = __builtin_get_texasr ();
return _TEXASR_NON_TRANSACTIONAL_CONFLICT (texasr);
}

#define AL_LOCK(idx)


# define SETUP_NUMBER_TASKS(p);
# define SETUP_NUMBER_THREADS(p);
# define PRINT_STATS()


#define TM_STARTUP(numThread,useless)     \
                                            stm_init(); \
                                            mod_mem_init(0); \
                                            if (getenv("STM_STATS") != NULL) { \
                                                mod_stats_init(); \
                                            } \

#      define TM_SHUTDOWN()             \
                    /*Free memory*/ \
                                            if (getenv("STM_STATS") != NULL) { \
                                                unsigned long u; \
                                                if (stm_get_global_stats("global_nb_commits", &u) != 0) \
                                                    printf("#commits    : %lu\n", u); \
                                                if (stm_get_global_stats("global_nb_aborts", &u) != 0) \
                                                    printf("#aborts     : %lu\n", u); \
                                                if (stm_get_global_stats("global_max_retries", &u) != 0) \
                                                     printf("Max retries : %lu\n", u); \
                                            } \
                                        stm_exit(); \
    unsigned long htm_commits = 0; \
    unsigned long stm_commits = 0; \
    unsigned long stm_aborts = 0; \
    unsigned long conflicts = 0; \
    unsigned long capacity_aborts = 0; \
    unsigned long user_aborts = 0; \
    unsigned long nontrans = 0; \
    unsigned long other_aborts = 0; \
    unsigned long system_calls=0; \
    unsigned long htm_system_calls=0; \
    unsigned long fallback_to_STM=0; \
    unsigned long lookup_HTM = 0; \
    unsigned long lookup_STM = 0; \
    unsigned long lookup_STM_small = 0; \
    unsigned long insert_HTM = 0; \
    unsigned long insert_STM = 0; \
    unsigned long insert_STM_small = 0; \
    unsigned long sgl_commits = 0; \
    int ik = 0; \
    for (; ik < 80; ik++) { \
       /*if (!statistics_array[ik].htm_commits && !statistics_array[ik].stm_commits ) { break; } */\
            htm_commits += statistics_array[ik].htm_commits; \
            stm_commits += statistics_array[ik].stm_commits; \
            total_commits += statistics_array[ik].stm_commits+statistics_array[ik].htm_commits+statistics_array[ik].sgl_commits; \
            stm_aborts += statistics_array[ik].stm_aborts; \
            conflicts += statistics_array[ik].conflicts; \
            capacity_aborts += statistics_array[ik].capacity_aborts; \
            user_aborts += statistics_array[ik].user_aborts; \
            nontrans += statistics_array[ik].nontrans; \
            other_aborts += statistics_array[ik].other_aborts; \
            system_calls += statistics_array[ik].system_calls; \
            htm_system_calls += statistics_array[ik].htm_system_calls; \
            fallback_to_STM += statistics_array[ik].fallback_to_STM; \
            lookup_HTM += statistics_array[ik].lookup_HTM; \
            lookup_STM += statistics_array[ik].lookup_STM; \
            lookup_STM_small += statistics_array[ik].lookup_STM_small; \
            insert_STM += statistics_array[ik].insert_STM; \
            insert_HTM += statistics_array[ik].insert_HTM; \
            insert_STM_small += statistics_array[ik].insert_STM_small; \
            sgl_commits += statistics_array[ik].sgl_commits; \
    } \
    printf("Total lookup: %lu\n\tHTM lookup: %lu\n\tSTM lookup: %lu\n\tSTM small lookup %lu\nTotal inserts: %lu\n\tHTM insert: %lu\n\tSTM inser: %lu\n\tSTM small insert: %lu\nTotal commits: %ld \n\tHTM commits: %ld \n\tSGL Commits: %lu\n\tSTM commits: %ld \nTotal aborts: %ld\n\tHTM conficts: %lu\n\tCapacity aborts: %lu\n\tUser aborts: %lu\n\tNon-transactional aborts: %lu\n\tOther aborts: %lu\n\tSTM aborts: %lu\nSTM System Calls: %ld\nHTM System Calls: %ld\n", lookup_HTM+lookup_STM+lookup_STM_small,lookup_HTM,lookup_STM,lookup_STM_small,insert_HTM+insert_STM+insert_STM_small,insert_HTM,insert_STM,insert_STM_small,(htm_commits+sgl_commits+stm_commits),htm_commits,sgl_commits,stm_commits, stm_aborts+conflicts+capacity_aborts+user_aborts+other_aborts,conflicts,capacity_aborts,user_aborts,nontrans,other_aborts,stm_aborts,htm_system_calls,system_calls); \


#      define TM_THREAD_ENTER()         ({  threadID=SPECIAL_THREAD_ID(); \
                                            stm_init_thread();})

#      define TM_THREAD_EXIT()          ({  stm_exit_thread(); \
                                        })
# define RETRY_POLICY 1
# define SPEND_BUDGET(b)    if(RETRY_POLICY == 0) (*b)=0; else if (RETRY_POLICY == 2) (*b)=(*b)/2; else (*b)=--(*b);

//#      define P_MALLOC(size)            malloc(size)
//#      define P_FREE(ptr)               free(ptr)
//#      define TM_MALLOC(size)           stm_malloc(size)
//#      define FAST_PATH_FREE(ptr)       free(ptr)
//#      define SLOW_PATH_FREE(ptr)       stm_free(ptr, sizeof(stm_word_t))
//#       define TM_FREE(ptr)             free(ptr)
//#      define P_MALLOC(size)            malloc(size)

#      define P_FREE(ptr)               free(ptr)
#      define TM_MALLOC(size)           stm_malloc(size)
#      define P_MALLOC(size)            TM_MALLOC(size)
#       define TM_FREE                  /* Do nothing, our allocator does the freeing in the end */
#      define FAST_PATH_FREE(ptr)       /*free(ptr)*/
#      define SLOW_PATH_FREE(ptr)       /*stm_free(ptr, sizeof(stm_word_t)) */



#    define TM_BEGIN_EXT(b,ro)		local_exec_mode = 3;TM_BEGIN(ro)
# define TM_BEGIN(ro) FAST_PATH_BEGIN(1,ro)

#define FAST_PATH_BEGIN(b,ro) SLOW_PATH_BEGIN(b,ro) 

# define FAST_PATH_END() SLOW_PATH_END()  


#define SLOW_PATH_BEGIN(b,ro) { \
						local_exec_mode = 1; \
                        sigjmp_buf buf; \
                        sigsetjmp(buf, 0); \
                        stm_tx_attr_t _a = {}; \
                        _a.read_only = ro; \
                        stm_start(_a, &buf); \
                        sigsetjmp(buf, 0); \
                        statistics_array[SPECIAL_THREAD_ID()].stm_aborts++; \
};	

#define TM_END() SLOW_PATH_END()
#define SLOW_PATH_END(){ \ 
        stm_commit(); \
        /*printf("End STM\n"); */\
        statistics_array[SPECIAL_THREAD_ID()].stm_aborts--; \
        statistics_array[SPECIAL_THREAD_ID()].stm_commits++; \
};


#    define FAST_PATH_RESTART()                SLOW_PATH_RESTART()
/*#    define SLOW_PATH_RESTART()                ({ \
                                                int loop_counter_abort; \
                                                int pageID; \
                                                for(loop_counter_abort=0;loop_counter_abort<counter_pages_written;loop_counter_abort++){ \
                                                    pageID=pages_written_heap[SPECIAL_THREAD_ID()][loop_counter_abort]; \
                                                    __sync_sub_and_fetch(&metadata_array[pageID].writer_count,1); \
                                                 } \
                                                 free(transition_count_pages_read); \
                                                 free(pages_written); \
                                                 free(pages_read); \
                                                counter_pages_read=0; \
                                                counter_pages_written=0; \
                                                stm_abort(0);})*/

#	define SLOW_PATH_RESTART()		stm_abort(0);



#    define TM_EARLY_RELEASE(var)       /* nothing */

#  include <wrappers.h>

#  define FAST_PATH_SHARED_READ(var)     stm_load((volatile stm_word_t *)(void *)&(var))
#  define FAST_PATH_SHARED_READ_P(var)   stm_load_ptr((volatile void **)(void *)&(var))
#  define FAST_PATH_SHARED_READ_D(var)   stm_load_double((volatile double *)(void *)&(var))
#  define FAST_PATH_SHARED_READ_F(var)   stm_load_float((volatile float *)(void *)&(var))
#  define FAST_PATH_SHARED_WRITE(var, val)     stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
#  define FAST_PATH_SHARED_WRITE_P(var, val)   stm_store_ptr((volatile void **)(void *)&(var), val)
#  define FAST_PATH_SHARED_WRITE_D(var, val)   stm_store_double((volatile double *)(void *)&(var), val)
#  define FAST_PATH_SHARED_WRITE_F(var, val)   stm_store_float((volatile float *)(void *)&(var), val)


#  define SLOW_PATH_SHARED_READ(var)     stm_load((volatile stm_word_t *)(void *)&(var))
#  define SLOW_PATH_SHARED_READ_P(var)   stm_load_ptr((volatile void **)(void *)&(var))
#  define SLOW_PATH_SHARED_READ_D(var)   stm_load_double((volatile double *)(void *)&(var))
#  define SLOW_PATH_SHARED_READ_F(var)   stm_load_float((volatile float *)(void *)&(var))
#  define SLOW_PATH_SHARED_WRITE(var, val)     stm_store((volatile stm_word_t *)(void *)&(var), (stm_word_t)val)
#  define SLOW_PATH_SHARED_WRITE_P(var, val)   stm_store_ptr((volatile void **)(void *)&(var), val)
#  define SLOW_PATH_SHARED_WRITE_D(var, val)   stm_store_double((volatile double *)(void *)&(var), val)
#  define SLOW_PATH_SHARED_WRITE_F(var, val)   stm_store_float((volatile float *)(void *)&(var), val)


#  define TM_LOCAL_WRITE(var, val)      ({var = val; var;})
#  define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#  define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})

#endif /* TM_H */
