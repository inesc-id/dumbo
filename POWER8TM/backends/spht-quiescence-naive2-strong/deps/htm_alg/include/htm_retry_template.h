#ifndef HTM_SGL_RETRY_TEMPLATE_H_GUARD
#define HTM_SGL_RETRY_TEMPLATE_H_GUARD

#include "htm_arch.h"

#ifdef __cplusplus
extern "C"
{
#endif



#ifndef HTM_SGL_INIT_BUDGET
#define HTM_SGL_INIT_BUDGET 10
#endif /* HTM_SGL_INIT_BUDGET */


extern long contar_tx;

typedef struct HTM_SGL_local_vars_ {
    int64_t budget;
    int64_t tid;
    HTM_STATUS_TYPE status;
    uint64_t padding[5];
} __attribute__((packed))  HTM_SGL_local_vars_s;

// there is some wasted space near the SGL that can be used as read-only storate
extern void * HTM_read_only_storage1;
extern int HTM_read_only_storage1_size;
extern void * HTM_read_only_storage2;
extern int HTM_read_only_storage2_size;

extern __thread int64_t * volatile HTM_SGL_var_addr; // points to the prev
extern __thread HTM_SGL_local_vars_s CL_ALIGN HTM_SGL_vars;
extern __thread int64_t HTM_SGL_errors[HTM_NB_ERRORS];

#define START_TRANSACTION(status) (HTM_begin(status) != HTM_CODE_SUCCESS)
#define BEFORE_TRANSACTION(tid, budget) /* empty */
#define AFTER_TRANSACTION(tid, budget)  /* empty */
#if defined(__powerpc__)
#define UPDATE_BUDGET(tid, budget, status) \
    HTM_inc_status_count(status); \
    HTM_INC(status); \
	budget = HTM_update_budget(budget, status)
#else
#define UPDATE_BUDGET(tid, budget, status) \
    HTM_inc_status_count(status); \
    HTM_INC(status); \
	budget = HTM_update_budget(budget, status)
#endif
/* The HTM_SGL_update_budget also handle statistics */

#define CHECK_SGL_NOTX() while (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1) { HTM_block(); }
#define CHECK_SGL_HTM()  if (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1) { HTM_abort(); }

#define AFTER_BEGIN(tid, budget, status)   /* empty */
#define BEFORE_COMMIT(tid, budget, status) /* empty */
#define COMMIT_TRANSACTION(tid, budget, status) \
    HTM_commit(); /* Commits and updates some statistics after */ \
    HTM_inc_status_count(status); \
    HTM_INC(status)

#define ENTER_SGL(tid) HTM_enter_fallback()
#define EXIT_SGL(tid)  HTM_exit_fallback()
#define AFTER_ABORT(tid, budget, status)  /* empty */

#define BEFORE_HTM_BEGIN(tid, budget)  
#define AFTER_HTM_BEGIN(tid, budget)   /* empty */
#define BEFORE_SGL_BEGIN(tid)          
#define AFTER_SGL_BEGIN(tid)           

#define BEFORE_HTM_COMMIT(tid, budget) 
#define AFTER_HTM_COMMIT(tid, budget)  
#define BEFORE_SGL_COMMIT(tid)   on_before_sgl_commit(tid)
#define AFTER_SGL_COMMIT(tid)          




#define BEFORE_CHECK_BUDGET(budget) /* empty */
// called within HTM_update_budget
#define HTM_UPDATE_BUDGET(budget, status) (budget - 1)

#define ENTER_HTM_COND(tid, budget) budget > 0
#define IN_TRANSACTION(tid, budget, status) \
	/*HTM_test()*/\
    (budget>0)\

// #################################
// Called within the API
#define HTM_INIT()       /* empty */
#define HTM_EXIT()       /* empty */
#define HTM_THR_INIT()   /* empty */
#define HTM_THR_EXIT()   /* empty */
#define HTM_INC(status)  /* Use this to construct side statistics */
// #################################

#define HTM_SGL_budget HTM_SGL_vars.budget
#define HTM_SGL_status &(HTM_SGL_vars.status)
#define HTM_SGL_tid    HTM_SGL_vars.tid

#define RO_begin() \
{ \
    loc_var.tid = local_thread_id;\
    q_args.tid = local_thread_id;\
    q_args.num_threads = global_numThread;\
    while ( 1 ) \
    { \
		UPDATE_TS_STATE(ACTIVE); \
        READ_TIMESTAMP(start_tx); \
		rmb(); \
		CONTINUE_LOOP_IF ( (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1), \
        { \
			UPDATE_STATE(INACTIVE); \
			rmb(); \
			CHECK_SGL_NOTX(); \
		}); \
		break; \
	} \
    /*printf("%d entrou RO tx\n", q_args.tid);*/\
}\

//     UPDATE_TS_STATE(ACTIVE); \
//     rmb(); \
//     while (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1) \
//     { \
//         UPDATE_STATE(INACTIVE); \
//         rwmb();\
//     }; \
//     READ_TIMESTAMP(start_tx); \
//     printf("%d entrou RO tx\n", q_args.tid);\
//     /*TODO: I believe we have to update(ACTIVE) after we found the SGL locked*/\
// } \


	



#define HTM_SGL_begin(threadid) \
{ \
loc_var.tid = local_thread_id;\
  q_args.tid = local_thread_id;\
  q_args.num_threads = global_numThread;\
  /*printf("tid arg %d q_args.tid %d, num threads %d, q_args.num_threads %d\n", threadid, q_args.tid, global_numThread, q_args.num_threads);*/\
    HTM_SGL_budget = HTM_SGL_INIT_BUDGET; /* HTM_get_budget(); */ \
    BEFORE_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget); \
    while (1) { \
        BEFORE_CHECK_BUDGET(HTM_SGL_budget); \
        if (ENTER_HTM_COND(HTM_SGL_tid, HTM_SGL_budget)) { \
            CHECK_SGL_NOTX(); \
            BEFORE_HTM_BEGIN(HTM_SGL_tid, HTM_SGL_budget); \
            UPDATE_TS_STATE(ACTIVE); /* JOAO: this is a key difference vs "naive" */ \
            /* SET_NON_DUR_STATE_RESTRICTED(NON_DURABLE); */\
            rwmb();\
            /*printf("%d: will try to HTM begin\n", HTM_SGL_tid);*/\
            if (START_TRANSACTION(HTM_SGL_status)) { \
                UPDATE_BUDGET(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); \
                UPDATE_STATE(INACTIVE); \ 
                rwmb();\
                /*printf("budget:%d\n",HTM_SGL_budget);*/\
                AFTER_ABORT(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); \
                continue; \
            } \
            CHECK_SGL_HTM(); \
            AFTER_HTM_BEGIN(HTM_SGL_tid, HTM_SGL_budget); \
        } \
        else { \
            /*printf("%d: will enter SGL\n", HTM_SGL_tid);*/\
            BEFORE_SGL_BEGIN(HTM_SGL_tid); \
            UPDATE_STATE(INACTIVE); /* Conservative (I believe it this is redundant) */\
            rmb(); \
            ENTER_SGL(HTM_SGL_tid); \
            /*printf("got SGL (budget %d)\n", HTM_SGL_budget);*/\
            QUIESCENCE_CALL_GL(tid);\
            /*printf("%d entrou SGL **********\n", q_args.tid);*/\
            AFTER_SGL_BEGIN(HTM_SGL_tid); \
        } \
        AFTER_BEGIN(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); \
        break; /* delete when using longjmp */ \
     } \
     READ_TIMESTAMP(start_tx); \
}


# define RO_commit(){\
  READ_TIMESTAMP(end_tx); \
  onBeforeHtmCommit(HTM_SGL_tid); \
  stats_array[q_args.tid].tx_time_ro_txs += end_tx - start_tx;\
  /*printf("%d saiu RO tx\n", q_args.tid);*/\
  UPDATE_STATE(INACTIVE);\
  rmb(); \
  on_after_htm_commit(HTM_SGL_tid); \
} \


//
#define HTM_SGL_commit() \
{ \
    BEFORE_COMMIT(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); \
    if (IN_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status)) { \
        BEFORE_HTM_COMMIT(HTM_SGL_tid, HTM_SGL_budget); \
        QUIESCENCE_CALL_HTM(); \
        onBeforeHtmCommit(HTM_SGL_tid); \
        COMMIT_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget, HTM_SGL_status); \
        /*printf("%d: HTM-committed\n", q_args.tid);*/\
        stats_array[q_args.tid].wait_time += q_args.end_wait_time - q_args.start_wait_time; \
        stats_array[q_args.tid].sus_time += (q_args.after_sus_ts - q_args.before_sus_ts) - (q_args.end_wait_time - q_args.start_wait_time); \
        stats_array[q_args.tid].tx_time_upd_txs += q_args.start_wait_time - start_tx; \
        on_after_htm_commit(HTM_SGL_tid); \
        /*UPDATE_STATE(INACTIVE); */\
        rmb(); \
        /*printf("%d: Now durable\n", q_args.tid);*/\
        AFTER_HTM_COMMIT(HTM_SGL_tid, HTM_SGL_budget); \
    } \
    else { \
        BEFORE_SGL_COMMIT(HTM_SGL_tid); \
        /*printf("%d vai sair do SGL **********\n", q_args.tid);*/\
        EXIT_SGL(HTM_SGL_tid); \
        /*UPDATE_STATE(INACTIVE);*/\
        AFTER_SGL_COMMIT(HTM_SGL_tid); \
        /*printf("%d: SGL-committed\n", q_args.tid);*/\
    } \
    AFTER_TRANSACTION(HTM_SGL_tid, HTM_SGL_budget); \
    assert(get_state(ts_state[q_args.tid].value)==INACTIVE);\
} \

# define QUIESCENCE_CALL_GL(tid) \
{ \
    assert(get_state(ts_state[q_args.tid].value)==INACTIVE);\
    assert(get_state(dur_state[q_args.tid].value)==INACTIVE);\
	int index;\
	int num_threads = global_numThread; \
  for ( index=0; index < num_threads; index++ ) \
  { \
   /*wait for active threads*/  \
    while (get_state(ts_state[index].value) != INACTIVE ) {/*printf("QUIESCENCE_CALL_GL(%d): %d, %d\n", q_args.tid, get_state(ts_state[0].value), get_state(ts_state[1].value));*/} \
  } \
};\
// end QUIESCENCE_CALL_GL

# define QUIESCENCE_CALL_HTM() { \
    READ_TIMESTAMP(q_args.before_sus_ts); \
	__TM_suspend(); \
    UPDATE_STATE(INACTIVE);\
    READ_TIMESTAMP(q_args.start_wait_time); \
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
  READ_TIMESTAMP(q_args.end_wait_time); \
  __TM_resume(); \
  READ_TIMESTAMP(q_args.after_sus_ts); \
}


#define HTM_SGL_before_write(addr, val) /* empty */
#define HTM_SGL_after_write(addr, val)  /* empty */

#define HTM_SGL_write(addr, val) ({ \
	HTM_SGL_before_write(addr, val); \
	*((GRANULE_TYPE*)addr) = val; \
	HTM_SGL_after_write(addr, val); \
	val; \
})

#define HTM_SGL_write_D(addr, val) ({ \
	GRANULE_TYPE g = CONVERT_GRANULE_D(val); \
	HTM_SGL_write((GRANULE_TYPE*)addr, g); \
	val; \
})

#define HTM_SGL_write_P(addr, val) ({ \
	GRANULE_TYPE g = (GRANULE_TYPE) val; /* works for pointers only */ \
	HTM_SGL_write((GRANULE_TYPE*)addr, g); \
	val; \
})

#define HTM_SGL_before_read(addr) /* empty */

#define HTM_SGL_read(addr) ({ \
	HTM_SGL_before_read(addr); \
	*((GRANULE_TYPE*)addr); \
})

#define HTM_SGL_read_P(addr) ({ \
	HTM_SGL_before_read(addr); \
	*((GRANULE_P_TYPE*)addr); \
})

#define HTM_SGL_read_D(addr) ({ \
	HTM_SGL_before_read(addr); \
	*((GRANULE_D_TYPE*)addr); \
})

/* TODO: persistency assumes an identifier */
#define HTM_SGL_alloc(size) malloc(size)
#define HTM_SGL_free(pool) free(pool)

// Exposed API
#define HTM_init(nb_threads) HTM_init_(HTM_SGL_INIT_BUDGET, nb_threads)
void HTM_init_(int init_budget, int nb_threads);
void HTM_exit();
int HTM_thr_init(int); // pass -1 to get an id
void HTM_thr_exit();
void HTM_block();

// int HTM_update_budget(int budget, HTM_STATUS_TYPE status);
#define HTM_update_budget(budget, status) HTM_UPDATE_BUDGET(budget, status)
void HTM_enter_fallback();
void HTM_exit_fallback();

void HTM_inc_status_count(HTM_STATUS_TYPE* status_code);
int HTM_get_nb_threads();
int HTM_get_tid();

// Getter and Setter for the initial budget
int HTM_get_budget();
void HTM_set_budget(int budget);

void HTM_set_is_record(int is_rec);
int HTM_get_is_record();
/**
 * @accum : int[nb_threads][HTM_NB_ERRORS]
 */
long HTM_get_status_count(int status_code, long **accum);
void HTM_reset_status_count();

#ifdef __cplusplus
}
#endif

#endif /* HTM_SGL_RETRY_TEMPLATE_H_GUARD */
