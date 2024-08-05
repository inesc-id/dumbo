#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>


#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define RO                              1
#define RW                              0

#include "tm.h"

#define DEFAULT_DURATION                10000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_SEED                    0
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define rmb()           asm volatile ("sync" ::: "memory")
#define rwmb()           asm volatile ("lwsync" ::: "memory")


// Variables for si-htm
__attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t single_global_lock;
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t counters[80];
__thread long counters_snapshot[80];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t tx_length[10];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t triggers[80];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[80];
__thread unsigned int local_exec_mode = 0;
__thread unsigned int local_thread_id;
//----------------------
static volatile int stop;

__attribute__((aligned(CACHE_LINE_SIZE)))  padded_scalar_t x;
__attribute__((aligned(CACHE_LINE_SIZE)))  padded_scalar_t y;
barrier_t barrier_xpto;
barrier_t barrier;
volatile int t=0;
volatile int rot_commit=0;
volatile int rot_abort=0;
extern __thread unsigned int thread_id;


static void print_error(TM_buff_type *TM_buff)
{
  if(__TM_conflict(TM_buff)){ 
		if(__TM_is_self_conflict(TM_buff)){ 
			printf("self_conflict at %p : %d\n",__TM_failure_address(TM_buff), __TM_is_tfiar_exact(TM_buff));
		}
		if(__TM_is_trans_conflict(TM_buff))  	printf("__TM_is_trans_conflict at %p : %d\n",__TM_failure_address(TM_buff), __TM_is_tfiar_exact(TM_buff)); 
    if (__TM_user_abort(&TM_buff)) {  
     	printf("__TM_user_abort at %p : %d\n",__TM_failure_address(TM_buff), __TM_is_tfiar_exact(TM_buff));
    }
    if(__TM_capacity_abort(&TM_buff)){ 
     	printf("__TM_capacity_abort at %p : %d\n",__TM_failure_address(TM_buff), __TM_is_tfiar_exact(TM_buff));
    }
  }
}


static int transaction()
{
  t=0;
  int ro=0;
  int x1;
  int y1;
  int total;
  TM_BEGIN_EXT(1, RW);
  x1 = FAST_PATH_SHARED_READ(x.value);
  x1-=1;
  FAST_PATH_SHARED_WRITE(x.value, x1);
  y1 = FAST_PATH_SHARED_READ(y.value);
  y1 += 1;
  FAST_PATH_SHARED_WRITE(y.value, y1);
  total=x.value+y.value;
  t=1;
  TM_END();
  printf("t1   x1=%d, y1=%d\n",x1,y1);
  assert(total==0 && "total diff de 0");
  return total;
}

static int transaction2()
{
  int ro=0;
  int x1;
  int y1;
  int total;
  volatile int i;

  TM_BEGIN_EXT(2, RW);
  x1 = FAST_PATH_SHARED_READ(x.value);
  while(t==0){
    for(i=0;i<20000;i++);
  }
  x1-=1;
  FAST_PATH_SHARED_WRITE(x.value, x1);
  y1 = FAST_PATH_SHARED_READ(y.value);
  y1 += 1;
  FAST_PATH_SHARED_WRITE(y.value, y1);
  total=x.value+y.value;
  TM_END();
  printf("t2   x1=%d, y1=%d\n",x1,y1);
  assert(total==0 && "total diff de 0");
  return total;
}

static int transaction_ROT()
{
  t=0;
  int x1;
  int y1;
  int total;
	TM_buff_type TM_buff;
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
    x1 = x.value;
    
    x1-=1;
    x.value=x1;
    y1 = y.value;
    y1 += 1;
    y.value=y1;
    total=x.value+y.value;
  } 
		else{ \
      printf("abort t1\n");
      rot_abort++;
    }

  __TM_end();
  rot_commit++;
  t=1;
  assert(total==0&& "total diff de 0");
  return total;
}

static int transaction_ROT2()
{
  int x1;
  int y1;
  int total;
  volatile int i;
  TM_buff_type TM_buff;
  
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
    x1 = x.value;
    while(t==0){
      for(i=0;i<20000;i++);
    }
    x1-=1;
    x.value=x1;
    y1 = y.value;
    y1 += 1;
    y.value=y1;
    total=x.value+y.value;
  }
   
		else{ \
      printf("abort t2\n");
      rot_abort++;
    }

  __TM_end(); 
  rot_commit++;
  assert(total==0&& "total diff de 0");
  return total;
}

static void t1()
{
  int x1;
  int y1;
  int total;
  volatile int i;
  TM_buff_type TM_buff;
  
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
    x.value=1;
    __TM_suspend(); 
    x.value=2;
    __TM_resume();
  }
		else{ 
      printf("abort t1\n");
    }

  __TM_end(); 

}

  __attribute__((aligned(CACHE_LINE_SIZE))) unsigned char tx_status;
static void t2()
{
  int x1;
  int y1;
  int total;
  volatile int i;
  unsigned char volatile padding[CACHE_LINE_SIZE];
  __attribute__((aligned(CACHE_LINE_SIZE))) TM_buff_type TM_buff;
  unsigned char volatile padding1[CACHE_LINE_SIZE];
  x.value=0;
  tx_status = __TM_begin_rot(&TM_buff);

  if (tx_status == _HTM_TBEGIN_STARTED) {
    x.value=1;
     __TM_suspend(); 
      x1=y.value;
      //int t=0;
      printf("%d \n",y.value);
    __TM_resume(); 
  }
	else{ 
      printf("abort t2 %p\n",&x.value);
      print_error(&TM_buff);
  }

  __TM_end(); 
}

static int t3()
{
  int x1;
  int y1;
  int total;
  volatile int i;
  TM_buff_type TM_buff;
  
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
    x.value=1;
     __TM_suspend(); 
     x1=x.value;
     printf("t3 %d \n",x1);
     printf("t3 x=%d \n",x.value);
     x1=2;
     printf("t3 %d \n",x1);
    __TM_resume();
  }
	else{ 
      printf("abort t3\n");
  }

  __TM_end(); 
  return total;
}

static int t4()
{
  int x1=0;
  int y1;
  int total;
  volatile int i;
  TM_buff_type TM_buff;
  
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
     __TM_suspend(); 
     x.value=x1;
     printf("%d\n",x.value);
    __TM_resume();
  }
		else{ \
      printf("abort t4\n");
    }

  __TM_end(); 
  return total;
}

static int t5()
{
  int x1=0;
  int y1;
  int total;
  volatile int i;
  TM_buff_type TM_buff;
  
  unsigned char tx_status = __TM_begin_rot(&TM_buff); 
  if (tx_status == _HTM_TBEGIN_STARTED) {
    x1=x.value;
     __TM_suspend(); 
     x.value=2;
     printf("%d\n",x);
    y1=x.value;
    __TM_resume();
  }
		else{ \
      printf("abort t5\n");
    }

  __TM_end(); 
  return total;
}

static int transaction_t1()
{
  int ro=0;

  TM_BEGIN_EXT(1, RW);

  FAST_PATH_SHARED_WRITE(x.value, 10);

  //FAST_PATH_SHARED_WRITE(y.value, 20);
  TM_END();
  printf("xaddr=%lx, xval=%lx\n",&x.value,x.value);
  return x.value;
}

static void *test(void *data)
{
  long id=(long)data;
  TM_THREAD_ENTER();
  int i;
  for(i=0;i<1000;i++){
    barrier_cross(&barrier_xpto);
    if(id==0){
      transaction_ROT();
    }
    else{
      transaction_ROT2();
    }
  }

  /* Free transaction */
  // TM_EXIT_THREAD;
  TM_THREAD_EXIT();
  

  return NULL;
}

static void *test2(void *data)
{

  long id=(long)data;
  TM_THREAD_ENTER();
  int i;
  for(i=0;i<1000;i++){
    barrier_cross(&barrier_xpto);
    if(thread_id==0){
      printf("thread_id %d \n",thread_id);
      transaction();
    }
    else{
      printf("t2 \n");
      transaction2();
    }
  }

  /* Free transaction */
  // TM_EXIT_THREAD;
  TM_THREAD_EXIT();
  printf("total %d,x=%d,y=%d \n",x.value+y.value,x.value,y.value);
  return NULL;
}

static void test3()
{
  t1();
  t2();
  t3();
  t4();
  t5();
  return NULL;
}

static void *test4(void *data)
{

  long id=(long)data;
  TM_THREAD_ENTER();
  int i;
  
  transaction_t1();

  /* Free transaction */
  // TM_EXIT_THREAD;
  TM_THREAD_EXIT();
  return NULL;
}


int main(int argc, char **argv)
{

  TM_STARTUP(2, 123);


  thread_startup(2);
  thread_start(test4,NULL);

  TM_SHUTDOWN();
  thread_shutdown();
//test3();
// printf("-----------------------------------");
// printf("rot_commit=%d \n",rot_commit);
// printf("rot_abort=%d \n",rot_abort);
// printf("-----------------------------------");

  return 0;
  }

