
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "timer.h"


#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0xFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_NB_OPERATIONS           1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

#include "tm.h"

long alpha;
int running;
__thread __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t b_type;
__thread long num_threads;
__thread long counters_snapshot[80];
__thread long actions[81][81];
__thread long to_save[81];
__thread long kill_ignored;
__thread long kill_index;
__thread long kill_cansave;
__thread long kill_acc;
__thread long kill_index2;
__thread padded_scalar batching;
__thread int ro;
__thread padded_scalar start_time;

unsigned int allow_stms = 0;

static volatile int stop;

long nb_ops;

long empty_tx(TM_ARGDECL long val)
{
  ro = 0;
  for(int i=0; i < val; i++){
    TM_BEGIN_EXT(0, ro);
    TM_END();
  }
  return 0;
}

#include <sched.h>

int update;
unsigned long nb_add;
unsigned long nb_remove;
unsigned long nb_contains;
unsigned long nb_found;
unsigned long nb_aborts;
unsigned int nb_threads;
unsigned int seed;
long operations;

void *test(void *data)
{
  TM_THREAD_ENTER();
  empty_tx(nb_ops / nb_threads);
  TM_THREAD_EXIT();
  return NULL;
}

# define no_argument        0
# define required_argument  1
# define optional_argument  2

MAIN(argc, argv) {
    TIMER_T start;
    TIMER_T stop;


  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"seed",                      required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  int i, c;
  long val;
  operations = DEFAULT_DURATION;
  unsigned int initial = DEFAULT_INITIAL;
  nb_threads = DEFAULT_NB_THREADS;
  update = DEFAULT_UPDATE;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "hd:i:n:s:", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("intset -- STM stress test "
              "(hash map)\n"
              "\n"
              "Usage:\n"
              "  intset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"

         );
       exit(0);
     case 'd':
       operations = atoi(optarg);
       nb_ops = operations;
       break;
     case 'i':
       initial = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  if (seed == 0)
    srand((int)time(0));
  else
    srand(seed);

  SIM_GET_NUM_CPU(nb_threads);
  TM_STARTUP(nb_threads,42);
  P_MEMORY_STARTUP(nb_threads);
  thread_startup(nb_threads);

  seed = rand();
  TIMER_READ(start);
  GOTO_SIM();
//startEnergyIntel();

 thread_start(test, NULL);

  GOTO_REAL();
  TIMER_READ(stop);

//double energy = endEnergyIntel();
  puts("done.");
  printf("\nTime = %0.6lf\n", TIMER_DIFF_SECONDS(start, stop));
//printf("Energy = %0.6lf\n", energy);
  fflush(stdout);

  TM_SHUTDOWN();
  P_MEMORY_SHUTDOWN();
  GOTO_SIM();
  thread_shutdown();
  MAIN_RETURN(0);
}
