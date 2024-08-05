#include "replayer.hpp"

#include <cstdio>
#include <cstdlib>
#include <time.h>

#define TIMER_T                         struct timespec
#define TIMER_READ(time)                clock_gettime(CLOCK_MONOTONIC, &(time))
#define TIMER_DIFF_SECONDS(start, stop) \
    (((double)(stop.tv_sec)  + (double)(stop.tv_nsec / 1.0e9)) - \
     ((double)(start.tv_sec) + (double)(start.tv_nsec / 1.0e9)))
#define TIMER_DIFF_NSEC(start, stop) \
    (((double)(stop.tv_sec * 1.0e9)  + (double)(stop.tv_nsec)) - \
     ((double)(start.tv_sec * 1.0e9) + (double)(start.tv_nsec)))

static void test(const char *s)
{
  int nbGen = 0;
  int nbRep = 0;
  TIMER_T t1, t2;
  double t;

  TIMER_READ(t1);
  nbGen = rep::generate_log();
  TIMER_READ(t2);
  t = TIMER_DIFF_SECONDS(t1, t2);
  printf("[%s] Generated %i TXs in %fs (%fns/TX)\n", s, nbGen, t, t / nbGen * 1.0e9);

  TIMER_READ(t1);
  nbRep = rep::replay();
  TIMER_READ(t2);
  t = TIMER_DIFF_SECONDS(t1, t2);
  printf("[%s] Replayed %i TXs in %fs (%fns/TX)\n", s, nbRep, t, t / nbRep * 1.0e9);
}

static void parse_argv(int argc, char** argv, rep::args_t& a)
{
  for (int i = 0; i < argc; ++i)
  {
    char *p = argv[i];

    if (*p == '-')
    {
      p++;
      switch (*p)
      {
        case 'n': // NB_THREADS
          i++; a.nb_threads = atol(argv[i]);
          break;
        case 'l': // SIZE_THREAD_LOG
          i++; a.size_thread_log = atol(argv[i]);
          break;
        case 'm': // SIZE_METADATA
          i++; a.size_metadata = atol(argv[i]);
          break;
        case 'h': // SIZE_HEAP
          i++; a.heap_size = atol(argv[i]);
          break;
        case 'w': // MIN_WRITES
          i++; a.min_writes = atoi(argv[i]);
          break;
        case 'W': // MAX_WRITES
          i++; a.max_writes = atoi(argv[i]);
          break;
        case 's': // SEED
          i++; a.seed = atol(argv[i]);
          break;
        case 'd': // PM_DELAY
          i++; a.pm_delay = atoi(argv[i]);
          break;
        default:
          printf("Unknown option %s\n", argv[i]);
          break;
      }
    }
  }

  printf("nb_threads = %li\n", a.nb_threads);
  printf("size_thread_log = %li\n", a.size_thread_log);
  printf("size_metadata = %li\n", a.size_metadata);
  printf("heap_size = %li\n", a.heap_size);
  printf("min_writes = %i\n", a.min_writes);
  printf("max_writes = %i\n", a.max_writes);
  printf("pm_delay = %i\n", a.pm_delay);
  printf("seed = %li\n", a.seed);
}


int main(int argc, char** argv)
{
  rep::args_t a = { // Default values
    .nb_threads = 8,
    .size_thread_log = 134217728ul, // 128MB
    .size_metadata = 16ul * 8388608ul / 20ul,
    .heap_size = 134217728ul,
    .min_writes = 1,
    .max_writes = 20,
    .pm_delay = 46,
    .seed = 1234
  };

  parse_argv(argc, argv, a);
  a.size_thread_log /= a.nb_threads;

#ifdef NAIVE
  printf(
    "---------------------------------\n" 
    "Naive log replayer implementation\n"
    "---------------------------------\n"
  );
  rep::setup_naive(a);
  test("naive");
  rep::destroy();
#endif

#ifdef LOG_LINK
  printf(
    "-------------------------------------------\n" 
    "Forward linking log replayer implementation\n"
    "-------------------------------------------\n"
  );
  rep::setup_forward_link(a);
  test("forward_link");
  rep::destroy();
#endif

#ifdef SEQ_LOG
  printf(
    "--------------------------------------\n" 
    "Sequential log replayer implementation\n"
    "--------------------------------------\n"
  ); 
  rep::setup_seq_log(a);
  test("seq_log");
  rep::destroy();
#endif

  return EXIT_SUCCESS;
}
