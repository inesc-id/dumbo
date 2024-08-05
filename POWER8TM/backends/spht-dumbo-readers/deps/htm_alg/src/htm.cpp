#include "htm_retry_template.h"

#include <cstdlib>
#include <thread>
#include <mutex>
#include <stdio.h>

#define LOCK(mtx) \
  while (!__sync_bool_compare_and_swap(&mtx, 0, 1)) PAUSE() \
//

#define UNLOCK(mtx) \
  mtx = 0; \
  __sync_synchronize() \
//

using namespace std;

#define SGL_SIZE 128
#define SGL_POS  16

long contar_tx=0;

static volatile int64_t CL_ALIGN HTM_SGL_var[SGL_SIZE] = { -1 };
/* extern */__thread int64_t * volatile HTM_SGL_var_addr = (int64_t * volatile)&(HTM_SGL_var[SGL_POS]);
/* extern */__thread CL_ALIGN HTM_SGL_local_vars_s HTM_SGL_vars;
/* extern */__thread int64_t HTM_SGL_errors[HTM_NB_ERRORS];

/* extern */void * HTM_read_only_storage1 = (void*)&(HTM_SGL_var[0]);
/* extern */int HTM_read_only_storage1_size = SGL_POS * sizeof(int64_t);
/* extern */void * HTM_read_only_storage2 = (void*)&(HTM_SGL_var[SGL_POS+1]);
/* extern */int HTM_read_only_storage2_size = (SGL_SIZE - SGL_POS - 1) * sizeof(int64_t);

static mutex mtx;
static int init_budget = HTM_SGL_INIT_BUDGET;
static int threads;
static int thr_counter;

static __thread int is_record=1;
static __thread int tid = -1;

void HTM_init_(int init_budget, int nb_threads)
{
  init_budget = HTM_SGL_INIT_BUDGET;
  threads = nb_threads;
  HTM_SGL_var[SGL_POS] = -1;
  HTM_SGL_var_addr = (int64_t * volatile)&(HTM_SGL_var[SGL_POS]);
  HTM_INIT();
}

void HTM_exit()
{
  HTM_EXIT();
}

int HTM_thr_init(int reqTID)
{
  if (tid != -1) return tid; // TODO
  mtx.lock();
  if (reqTID != -1) {
    tid = reqTID;
    HTM_SGL_tid = reqTID;
  } else {
    tid = thr_counter++;
    HTM_SGL_tid = tid;
  }
  HTM_SGL_var_addr = (int64_t * volatile)&(HTM_SGL_var[SGL_POS]);
  HTM_THR_INIT();
  mtx.unlock();
  return tid;
}

void HTM_thr_exit()
{
  mtx.lock();
  --thr_counter;
  HTM_THR_EXIT();
  mtx.unlock();
}

int HTM_get_budget() { return init_budget; }
void HTM_set_budget(int _budget) { init_budget = _budget; }

void HTM_enter_fallback()
{
  // mtx.lock();
  while (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != tid) {
    while (__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1) {
      // printf("%d: waiting before acquiring SGL\n", tid);
      // PAUSE(); /*this was hurting performance, so I removed it*/ 
    }
    __sync_val_compare_and_swap(HTM_SGL_var_addr, -1, tid);
  }
//printf("passei aqui %d\n", tid);
  // HTM_SGL_var = 1;
  // __sync_synchronize();
  HTM_SGL_errors[HTM_FALLBACK]++;
}

void HTM_exit_fallback()
{
  // __sync_val_compare_and_swap(&HTM_SGL_var, 1, 0);
  // __asm__ __volatile__("mfence" ::: "memory");
  __atomic_store_n(HTM_SGL_var_addr, -1, __ATOMIC_RELEASE);
  // mtx.unlock();
}

void HTM_block()
{
  //while(__atomic_load_n(HTM_SGL_var_addr, __ATOMIC_ACQUIRE) != -1) {
  //  PAUSE();
  //}

  // mtx.lock();
  // mtx.unlock();
}

void HTM_inc_status_count(HTM_STATUS_TYPE* status_code)
{
  if (is_record) {
    HTM_ERROR_INC(status_code, HTM_SGL_errors);
  }
}



// int HTM_update_budget(int budget, HTM_STATUS_TYPE status)
// {
//   int res = 0;
//   // HTM_inc_status_count(status);
//   res = HTM_UPDATE_BUDGET(budget, status);
//   return res;
// }

long HTM_get_status_count(int code, long **accum)
{
  long res = 0;
  //int code=P8_ERROR_TO_INDEX(status_code);
  res = HTM_SGL_errors[code];
  if (accum != NULL) {
    accum[tid][code] = HTM_SGL_errors[code];
  }
  return res;
}

void HTM_reset_status_count()
{
  int i, j;
  for (i = 0; i < HTM_NB_ERRORS; ++i) {
    HTM_SGL_errors[i] = 0;
  }
}

int HTM_get_nb_threads() { return threads; }
int HTM_get_tid() { return tid; }

void HTM_set_is_record(int is_rec) { is_record = is_rec; }
int HTM_get_is_record() { return is_record; }
