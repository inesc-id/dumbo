#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define RO                              1
#define RW                              0

#include "tm.h"

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static __attribute__((aligned(CACHE_LINE_SIZE))) volatile int stop;

/* ################################################################### *
 * ARRAY
 * ################################################################### */

#define ARRAY_SIZE 0xFFFF
static __attribute__((aligned(CACHE_LINE_SIZE))) uintptr_t *array;
static int nb_items;

typedef struct {
  int p;
  int k;
  int nb_items;
  uintptr_t *array;
  uintptr_t i;
  uintptr_t value;
} aux_t;

static int tx(int p, uintptr_t value)
{
  volatile __attribute__((aligned(CACHE_LINE_SIZE))) aux_t aux;
  volatile __attribute__((aligned(CACHE_LINE_SIZE))) int ro = 0;
  aux.array = array;
  aux.p = p;
  aux.value = value;
  aux.nb_items = nb_items;

  TM_BEGIN_EXT(RW, ro);
  for (aux.k = 0; aux.k < aux.nb_items; aux.k++)
  {
    aux.i = FAST_PATH_SHARED_READ(aux.array[aux.p + aux.k]);
    FAST_PATH_SHARED_WRITE(aux.array[aux.p + aux.k], aux.value+aux.i);
  }
  TM_END();

  return aux.i;
}

// TODO: remove these from here
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t counters[80];
// extern void *bank_heap_ptr;
__thread long counters_snapshot[80];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t tx_length[10];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t triggers[80];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[80];
__thread unsigned int local_exec_mode = 0;
__thread unsigned int local_thread_id;
extern __thread unsigned int thread_id;

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
  uintptr_t *array;
  barrier_t *barrier;
  unsigned int seed;
  int nb_threads;
  int id;
  char padding[64];
} thread_data_t;

static thread_data_t *data;

static void *test(void *arg)
{
  int src, dst, nb;
  int rand_max, rand_min;
  thread_data_t *d = (thread_data_t *)arg;
  unsigned short seed[3];
  uintptr_t i = 0;

  d += thread_id;

  /* Initialize seed (use rand48 as rand is poor) */
  seed[0] = (unsigned short)rand_r(&d->seed);
  seed[1] = (unsigned short)rand_r(&d->seed);
  seed[2] = (unsigned short)rand_r(&d->seed);

  /* Create transaction */
  // TM_INIT_THREAD;
  TM_THREAD_ENTER();
  /* Wait on barrier */
  printf("Start thread %i\n", d->id);
  barrier_cross(d->barrier);

  // uintptr_t *a = d->array + d->id * 512;
  while (__atomic_load_n(&stop, __ATOMIC_ACQUIRE) == 0)
    tx(d->id * 512, i++);

  TM_THREAD_EXIT();
  barrier_cross(d->barrier);
  return NULL;
}

static void catcher(int sig)
{
  __atomic_store_n(&stop, 1, __ATOMIC_RELEASE);
  static int nb = 0;
  printf("CAUGHT SIGNAL %d\n", sig);
  if (++nb >= 3)
    exit(1);
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"items-written",             required_argument, NULL, 'm'},
    {"duration",                  required_argument, NULL, 'd'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"seed",                      required_argument, NULL, 's'},
    {"disjoint",                  no_argument,       NULL, 'j'},
    {NULL, 0, NULL, 0}
  };

  int i, c, ret;
  unsigned long reads, writes, updates;
  // TODO: @Miguel collect statistics
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
  sigset_t block_set;
  int nb_threads = 1;
  long duration = 10000;
  long seed = 0;

  array = (uintptr_t*) calloc(ARRAY_SIZE/*  + 512 */, sizeof(uintptr_t));
  // array += 512; // some padding

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "hm:d:n:s:j", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("bank -- STM stress test\n"
              "\n"
              "Usage:\n"
              "  array [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -m, --items-written\n"
              "        Number of items written in the array\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
         );
       exit(0);
     case 'd':
       duration = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'm':
       nb_items = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(nb_threads > 0);

  printf("Duration       : %d\n", duration);
  printf("Nb threads     : %d\n", nb_threads);
  printf("Type sizes     : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(size_t));

  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

  if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  thread_startup(nb_threads);


  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);

  // bank_heap_ptr = bank->accounts;
  stop = 0;

  /* Init STM */
  printf("Initializing STM\n");
  TM_STARTUP(nb_threads, 123);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  printf("STARTING...\n");
  alarm(duration/1000);
  printf("%d\n",duration/1000);
  // pthread_attr_destroy(&attr);

  /* Catch some signals */
  if (signal(SIGHUP, catcher) == SIG_ERR ||
      signal(SIGINT, catcher) == SIG_ERR ||
      signal(SIGALRM, catcher) == SIG_ERR ||
      signal(SIGTERM, catcher) == SIG_ERR) {
    perror("signal");
    exit(1);
  }

  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].array = array;
    data[i].id = i;
    data[i].nb_threads = nb_threads;
    data[i].seed = rand();
    data[i].barrier = &barrier;
    // if (i > 0 && pthread_create(&threads[i], &attr, test, (void *)(data+i)) != 0) {
    //   fprintf(stderr, "Error creating thread\n");
    //   exit(1);
    // }
  }
  gettimeofday(&start, NULL);
  thread_start(test, (void *)(data));
  gettimeofday(&end, NULL);

  printf("STOPPING...\n");

  /* Wait for thread completion */
  // for (i = 1; i < nb_threads; i++) {
  //   if (pthread_join(threads[i], NULL) != 0) {
  //     fprintf(stderr, "Error waiting for thread completion\n");
  //     exit(1);
  //   }
  // }

  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
  printf("Time = %f\n", (float)duration / 1000.0f);
  

  /* Cleanup STM */
  TM_SHUTDOWN();
  thread_shutdown();

  free(threads);
  free(data);
  free(array);

  return ret;
}
