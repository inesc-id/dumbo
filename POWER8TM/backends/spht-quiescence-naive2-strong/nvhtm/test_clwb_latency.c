#define _GNU_SOURCE

#include "rdtsc.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <sched.h>

#define ASM_TO_REPEAT \
  valueToFlush[0] = 1; \
//

#define ASM_TO_REPEAT_1a \
  valueToFlush[0] = 1; \
  asm volatile("clwb (%0)\n\t" \
               /* "sfence   \n\t" */ \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//
#define ASM_TO_REPEAT_1b \
  valueToFlush[0] = 1; \
  asm volatile("clwb (%0)\n\t" \
               "sfence   \n\t" \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//
#define ASM_TO_REPEAT_2a \
  asm volatile("clwb (%0)\n\t" \
               /* "sfence   \n\t" */ \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//
#define ASM_TO_REPEAT_2b \
  asm volatile("clwb (%0)\n\t" \
               "sfence   \n\t" \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//
#define ASM_TO_REPEAT_3a \
  valueToFlush[0] = 1; \
  asm volatile("clflushopt (%0)\n\t" \
               /* "sfence   \n\t" */ \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//
#define ASM_TO_REPEAT_3b \
  valueToFlush[0] = 1; \
  asm volatile("clflushopt (%0)\n\t" \
               "sfence   \n\t" \
               : \
               : "r"(&(valueToFlush[0])) \
               : "memory"); \
//

#define TEN(a)     a;a;a;a;a;a;a;a;a;a;
#define HUNDRED(a) TEN(TEN(a))

int main()
{
  const int NB_SAMPLES = 1000;
  const int NB_SAMPLES_2 = 100*100;
  double sumLat = 0.0;

  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(40, &cpu_set); // pinned to a core
  sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);

  volatile int __attribute__((aligned(64))) valueToFlush[64];
  valueToFlush[0] = 1;
  valueToFlush[1] = 2;
  valueToFlush[2] = 3;
  valueToFlush[3] = 4;
  valueToFlush[4] = 5;

  // printf("w+clwb;w+clwb+fence;clwb;clwb+fence;w+clflushopt;w+clflushopt+fence;\n");

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_1a));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_1b));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_2a));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_2b));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_3a));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);

  sumLat = 0.0;
  for (int i = 0; i < NB_SAMPLES; ++i)
  {
    uint64_t ts1, ts2, cycles;

    ts1 = rdtscp();
    HUNDRED(HUNDRED(ASM_TO_REPEAT_3b));
    ts2 = rdtscp();

    cycles = ts2 - ts1;

    sumLat += cycles;
  }
  printf("%0.9f;", sumLat / NB_SAMPLES / NB_SAMPLES_2);
  sumLat = 0.0;

  printf("\n");

  return 0;
}
