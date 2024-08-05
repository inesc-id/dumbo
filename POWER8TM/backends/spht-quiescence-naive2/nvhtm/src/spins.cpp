#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <stdint.h>

static const char SAVE_TO_FILE[] = "./learned_nop_loops";

/* extern */volatile __thread unsigned long spin_flush_ts, spin_flush_ts2;
/* extern */volatile __thread unsigned long spin_nb_flushes;

#define NOP_X10 asm volatile( \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop \n\t" \
  "nop" ::: "memory" ) \
//

#define SPIN_10NOPS(nb_spins) ({ \
  /*volatile */unsigned long long i; \
  for (i = 0; i < (unsigned long long) (nb_spins); ++i) { NOP_X10; } \
  i; \
})

/* extern */unsigned long learnedSpins = 0;
/* extern */spin_fn_t spin_fn = spin_nops;

void spin_nops(unsigned long nbSpins)
{
  SPIN_10NOPS(nbSpins);
  printf("spins %lu\n",nbSpins);
}

void spin_cycles(unsigned long nbCycles)
{
  volatile unsigned long ts = rdtsc();
  while (rdtsc() - ts < nbCycles);
}

void learn_spin_nops(
  double latencyInNanoSec,
  double cpuFreqInKiloHertz,
  int isForceTest
) {
	FILE *fp = fopen(SAVE_TO_FILE, "r+");

	if (fp == NULL || isForceTest) {
		// File does not exist
		unsigned long long ts1, ts2;
		double time;
		// in milliseconds (CPU_MAX_FREQ is in kilo)
		double latencyInSeconds = latencyInNanoSec * 1e-9; // ns -> seconds
		double clockInHertz     = cpuFreqInKiloHertz * 1e3; // kHz -> Hz
		double measured_cycles = 0;

		fp = fopen(SAVE_TO_FILE, "w");
		if (fp == NULL) {
			perror("open spin files");
			return;
		}

		ts1 = rdtscp();
		spin_fn(LEARN_SPINS_TEST_SIZE);
		ts2 = rdtscp();

		measured_cycles = ts2 - ts1;

		time = measured_cycles / (double) clockInHertz; // TODO:

		learnedSpins = (double) LEARN_SPINS_TEST_SIZE * (latencyInSeconds / time) + 1; // round up
		fprintf(fp, "%lu\n", learnedSpins);
		fclose(fp);
	} else {
    // Already learned
		if (fscanf(fp, "%lu\n", &learnedSpins) < 1)
      printf("error reading learned spins\n");
		fclose(fp);
	}
}
