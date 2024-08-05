// #define _GNU_SOURCE // already defined below
#include "htm_retry_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>

#define TEST_SIZE   50000000
#define SPIN_CYCLES 10000

// lots of padding
int64_t __attribute__((aligned(512))) shared_variable = 0;
volatile int64_t padding[64]; // needed, the previous var is aligned, but the space in front if it is used...
uint64_t test_round = 0, nbAborts = 0, nbCommits = 0;

uint64_t TX_first_ts[TEST_SIZE];
uint64_t TX_last_ts[TEST_SIZE];
uint64_t TX_val[TEST_SIZE];
uint64_t nonTX_first_ts[TEST_SIZE];
uint64_t nonTX_last_ts[TEST_SIZE];
uint64_t nonTX_val[TEST_SIZE];

void *test_HTM(void *args)
{
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(0, &cpu_set);
  sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);

  int status;
  volatile int64_t *ptr_to_shared_var = &shared_variable;
  uint64_t local_test_round = 0;

  for (int i = 0; i < TEST_SIZE; ++i) {
    uint64_t first_ts, last_ts, tsc;

    if (HTM_begin(status) != HTM_CODE_SUCCESS) {
      // handle abort
      // printf("[TX] test_round = %li aborted (%i)!\n", local_test_round, status);
      TX_first_ts[local_test_round] = -1;
      TX_last_ts[local_test_round] = -1;
      TX_val[local_test_round] = 0;
      local_test_round++;
      __atomic_store_n(&test_round, local_test_round, __ATOMIC_RELEASE);
      // this is waste of time
      // tsc = rdtsc();
      // while (rdtsc() - tsc < SPIN_CYCLES / 3);
      nbAborts++;
      continue;
    }
    // TX

    first_ts = rdtscp(); // first_ts
    
    *ptr_to_shared_var = local_test_round;

    // spin to make time to read the value
    while ((last_ts = rdtscp()) - first_ts < SPIN_CYCLES);

    HTM_commit();

    // printf("[TX] round=%li committed (TSC=%lx)!\n", local_test_round, tsc);
    TX_first_ts[local_test_round] = first_ts;
    TX_last_ts[local_test_round] = last_ts;
    TX_val[local_test_round] = local_test_round;
    local_test_round++;
    __atomic_store_n(&test_round, local_test_round, __ATOMIC_RELEASE);
    nbCommits++;
  }

  local_test_round++;
  __atomic_store_n(&test_round, local_test_round, __ATOMIC_RELEASE);
  *ptr_to_shared_var = local_test_round;
}

void *test_nonTX(void *args)
{
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(1, &cpu_set);
  sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);

  int status;
  volatile int64_t *ptr_to_shared_var = &shared_variable;
  int64_t value_of_shared_var;
  uint64_t local_test_round = 0;

  for (int i = 0; i < TEST_SIZE; ++i) {
    uint64_t first_ts, last_ts, tsc;
    
    // spin to make time to write the value
    tsc = rdtscp();
    while (rdtscp() - tsc < SPIN_CYCLES * 11.0f / 12.0f) {
      if (__atomic_load_n(&test_round, __ATOMIC_ACQUIRE) > local_test_round) {
        // HTM aborted
        break;
      }
    }

    first_ts = rdtscp();

    value_of_shared_var = *ptr_to_shared_var; // non-TX read

    last_ts = rdtscp();

    nonTX_first_ts[local_test_round] = first_ts;
    nonTX_last_ts[local_test_round] = last_ts;
    nonTX_val[local_test_round] = value_of_shared_var;

    local_test_round++;
    // printf("\t\t\t\t\t[nonTX]: sharedVar=%li (myCounter=%li, TSC=%lx)\n", value_of_shared_var, local_test_round, tsc);
    while(__atomic_load_n(&test_round, __ATOMIC_ACQUIRE) < local_test_round);
  }
}

int main()
{
  pthread_t htm_thread, no_tx_thread;

  if (pthread_create(&no_tx_thread, NULL, test_nonTX, NULL)) {
    perror("launching no_tx_thread");
  }

  if (pthread_create(&htm_thread, NULL, test_HTM, NULL)) {
    perror("launching htm_thread");
  }

  // experiment is done in other threads

  if (pthread_join(no_tx_thread, NULL)) {
    perror("joining no_tx_thread");
  }

  if (pthread_join(htm_thread, NULL)) {
    perror("joining htm_thread");
  }

  for (int i = 0; i < TEST_SIZE; ++i) {
    // look for committed TXs that occurred concurrently with nonTX reads
    if (TX_first_ts[i] != -1 && i == 0) printf("first TX committed\n");
    if (TX_first_ts[i] == -1) continue; // aborted
    
    if (TX_first_ts[i] < nonTX_first_ts[i] && TX_last_ts[i] > nonTX_last_ts[i]) {
      // this is guaranteed to have messed up!!!
      printf("%i --> \tTX\t[%lx:%lx] \n\t\tnon-TX\t[%lx:%lx] \nvalTX = %li valNonTX = %li\n", i,
        TX_first_ts[i], TX_last_ts[i], nonTX_first_ts[i], nonTX_last_ts[i], TX_val[i], nonTX_val[i]);
    }
  }

  printf("nbCommits = %li nbAborts = %li\n", nbCommits, nbAborts);

  return EXIT_SUCCESS;
}
