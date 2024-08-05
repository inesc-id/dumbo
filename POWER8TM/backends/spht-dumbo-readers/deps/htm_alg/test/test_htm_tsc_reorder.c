// #define _GNU_SOURCE // already defined below
#include "htm_retry_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>

#define TEST_SIZE   100000000

typedef struct padded_ts_s {
  uint64_t ts;
  int64_t padding[63];
} __attribute__((aligned(512))) padded_ts_s;

// lots of padding
padded_ts_s shared_ts[2];
uint64_t nbAborts = 0, nbCommits = 0;

// 2 transactions getting TSCs and snooping each others
uint64_t *TX_got_ts[2];
uint64_t *TX_observed_ts[2];
padded_ts_s TX_round[2]; // round of each TX --> they should advance sync

void *test_HTM(void *args)
{
  int id = (uint64_t)args;

  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(id, &cpu_set);
  sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);

  int status;
  volatile uint64_t *ptr_to_shared_ts = &shared_ts[id].ts;
  uint64_t local_test_round = 0;

  for (int i = 0; i < TEST_SIZE; ++i) {
    uint64_t first_ts, last_ts, tsc;

    // the 2 must start sync
    while(__atomic_load_n(&TX_round[(id+1)%2].ts, __ATOMIC_ACQUIRE) < local_test_round);

    if (HTM_begin(status) != HTM_CODE_SUCCESS) {
      // handle abort
      // printf("[TX] test_round = %li aborted (%i)!\n", local_test_round, status);
      TX_got_ts[id][local_test_round] = -1L;
      TX_observed_ts[id][local_test_round] = -1L;
      local_test_round++;
      __atomic_store_n(&TX_round[id].ts, local_test_round, __ATOMIC_RELEASE);
      nbAborts++;
      continue;
    }
    // TX

    __atomic_store_n(ptr_to_shared_ts, 0x1234, __ATOMIC_RELEASE); // makes sure we acquire the granule first
    __asm__ __volatile__("mfence" ::: "memory");
    *ptr_to_shared_ts = rdtscp(); // updates the granule again
    rdtscp(); // flushes the pipeline

    HTM_commit();

    __asm__ __volatile__("mfence" ::: "memory");
    // __atomic_thread_fence(__ATOMIC_ACQUIRE);
    rdtscp(); // flushes the pipeline

    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    TX_got_ts[id][local_test_round] = shared_ts[id].ts;
    TX_observed_ts[id][local_test_round] = shared_ts[(id+1)%2].ts;
    local_test_round++;
    __atomic_store_n(&TX_round[id].ts, local_test_round, __ATOMIC_RELEASE);
    nbCommits++;
  }

  local_test_round++;
  __atomic_store_n(&TX_round[id].ts, local_test_round, __ATOMIC_RELEASE);
}

int main()
{
  pthread_t htm_thread, no_tx_thread;

  for (int i = 0; i < 2; i++) {
    TX_got_ts[i] = (uint64_t*)malloc(sizeof(uint64_t)*TEST_SIZE);
    TX_observed_ts[i] = (uint64_t*)malloc(sizeof(uint64_t)*TEST_SIZE);
  }

  if (pthread_create(&no_tx_thread, NULL, test_HTM, (void*)0)) {
    perror("launching no_tx_thread");
  }

  if (pthread_create(&htm_thread, NULL, test_HTM, (void*)1)) {
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
    if (TX_observed_ts[0][i] == -1 || TX_observed_ts[1][i] == -1) continue; // one aborted
    
    // case1 we get a TS larger than the other thread, but we fail to see the other thread TS
    if ((TX_got_ts[0][i] > TX_got_ts[1][i] && TX_observed_ts[0][i] != TX_got_ts[1][i]) ||
        (TX_got_ts[1][i] > TX_got_ts[0][i] && TX_observed_ts[1][i] != TX_got_ts[0][i]) ) {
      // this is guaranteed to have messed up!!!
      printf("%i: \t%i\t[got:%lx - obs:%lx] \n\t%i\t[got:%lx - obs:%lx]\n", i, 0, TX_got_ts[0][i],
        TX_observed_ts[0][i], 1, TX_got_ts[1][i], TX_observed_ts[1][i]);
    }
    if (TX_got_ts[0][i] == TX_got_ts[1][i] && (TX_observed_ts[0][i] != TX_got_ts[1][i] || TX_observed_ts[1][i] != TX_got_ts[0][i])) {
      printf("%i equal TS: \t%i\t[got:%lx - obs:%lx] \n\t\t%i\t[got:%lx - obs:%lx]\n", i, 0, TX_got_ts[0][i],
        TX_observed_ts[0][i], 1, TX_got_ts[1][i], TX_observed_ts[1][i]);
    }
  }

  printf("nbCommits = %li nbAborts = %li\n", nbCommits, nbAborts);

  return EXIT_SUCCESS;
}
