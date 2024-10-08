/* =============================================================================
 *
 * genome.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of ssca2, please see ssca2/COPYRIGHT
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */

//#include "rapl.h"
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gene.h"
#include "random.h"
#include "segments.h"
#include "sequencer.h"
#include "thread.h"
#include "timer.h"
#include "tm.h"
#include "vector.h"

unsigned int htm_rot_enabled = 1;
unsigned int allow_rots_ros = 1;
unsigned int allow_htms = 1;

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t exists_sw;

__thread unsigned long backoff = MIN_BACKOFF;
__thread unsigned long cm_seed = 123456789UL;

__attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[80];

__attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t single_global_lock = 0;
__attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t fallback_in_use = 0;

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t counters[80];

__attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t writers_lock = 0;

__thread unsigned int local_exec_mode = 0;

__thread unsigned int local_thread_id;

__thread void* rot_readset[1024];
__thread char crot_readset[8192];
__thread int irot_readset[2048];
__thread int16_t i2rot_readset[4096];

__thread long rs_mask_2 = 0xffffffffffff0000;
__thread long rs_mask_4 = 0xffffffff00000000;
__thread long offset = 0;
__thread char* p;
__thread int* ip;
__thread int16_t* i2p;
__thread long moffset = 0;
__thread long moffset_2 = 0;
__thread long moffset_6 = 0;

__thread unsigned long rs_counter = 0;


unsigned int allow_stms = 0;

pthread_spinlock_t global_lock = 0;

unsigned int ucb_levers = 4;
unsigned long ucb_trials[6];
unsigned long total_trials;
unsigned int selected_lever[50000];

enum param_types {
    PARAM_GENE    = (unsigned char)'g',
    PARAM_NUMBER  = (unsigned char)'n',
    PARAM_SEGMENT = (unsigned char)'s',
    PARAM_THREAD  = (unsigned char)'t',
    PARAM_REPEAT  = (unsigned char)'r',
};


#define PARAM_DEFAULT_GENE    (1L << 14)
#define PARAM_DEFAULT_NUMBER  (1L << 22)
#define PARAM_DEFAULT_SEGMENT (1L << 6)
#define PARAM_DEFAULT_THREAD  (1L)
#define PARAM_DEFAULT_ATT     (5L)
#define PARAM_DEFAULT_LOCK    (2L)
#define PARAM_DEFAULT_REPEAT  (1L)


long global_params[256]; /* 256 = ascii limit */


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                                (defaults)\n");
    printf("    g <UINT>   Length of [g]ene         (%li)\n", PARAM_DEFAULT_GENE);
    printf("    n <UINT>   Min [n]umber of segments (%li)\n", PARAM_DEFAULT_NUMBER);
    printf("    s <UINT>   Length of [s]egment      (%li)\n", PARAM_DEFAULT_SEGMENT);
    printf("    t <UINT>   Number of [t]hreads      (%li)\n", PARAM_DEFAULT_THREAD);
    puts("");
    puts("The actual number of segments created may be greater than -n");
    puts("in order to completely cover the gene.");
    exit(1);
}


/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams( void )
{
    global_params[PARAM_GENE]    = PARAM_DEFAULT_GENE;
    global_params[PARAM_NUMBER]  = PARAM_DEFAULT_NUMBER;
    global_params[PARAM_SEGMENT] = PARAM_DEFAULT_SEGMENT;
    global_params[PARAM_THREAD]  = PARAM_DEFAULT_THREAD;
    global_params[PARAM_REPEAT]  = PARAM_DEFAULT_REPEAT;
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    setDefaultParams();

    while ((opt = getopt(argc, argv, "g:n:s:t:f:p:r:")) != -1) {
        switch (opt) {
            case 'g':
            case 'n':
            case 's':
            case 't':
            case 'f':
            case 'p':
            case 'r':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}


/* =============================================================================
 * main
 * =============================================================================
 */
MAIN (argc,argv)
{
    TIMER_T start;
    TIMER_T stop;

    GOTO_REAL();

    /* Initialization */
    parseArgs(argc, (char** const)argv);
    SIM_GET_NUM_CPU(global_params[PARAM_THREAD]);

    //SETUP_NUMBER_TASKS(5);
    //SETUP_NUMBER_THREADS(global_params[PARAM_THREAD]);


    long geneLength = global_params[PARAM_GENE];
    long segmentLength = global_params[PARAM_SEGMENT];
    long minNumSegment = global_params[PARAM_NUMBER];
    long numThread = global_params[PARAM_THREAD];
    TM_STARTUP(numThread, GENOME_ID);
    P_MEMORY_STARTUP(numThread);
    thread_startup(numThread);

double time_total = 0.0;
double energy_total = 0.0;
int repeat = global_params[PARAM_REPEAT];
for (; repeat > 0; --repeat) {

    random_t* randomPtr = random_alloc();
    assert(randomPtr != NULL);
    random_seed(randomPtr, 0);

    gene_t* genePtr = gene_alloc(geneLength);
    assert( genePtr != NULL);
    gene_create(genePtr, randomPtr);
    char* gene = genePtr->contents;
    segments_t* segmentsPtr = segments_alloc(segmentLength, minNumSegment);
    assert(segmentsPtr != NULL);
    segments_create(segmentsPtr, genePtr, randomPtr);
    sequencer_t* sequencerPtr = sequencer_alloc(geneLength, segmentLength, segmentsPtr);
    assert(sequencerPtr != NULL);

/*    puts("done.");
    printf("Gene length     = %li\n", genePtr->length);
    printf("Segment length  = %li\n", segmentsPtr->length);
    printf("Number segments = %li\n", vector_getSize(segmentsPtr->contentsPtr));
    fflush(stdout);
    printf("Sequencing gene... ");
    fflush(stdout);*/
//startEnergyIntel();
    TIMER_READ(start);
    GOTO_SIM();
    thread_start(sequencer_run, (void*)sequencerPtr);
    GOTO_REAL();
    TIMER_READ(stop);
//    puts("done.");
//    printf("Time = %lf\n", TIMER_DIFF_SECONDS(start, stop));
    double tmp_time = TIMER_DIFF_SECONDS(start, stop);
    time_total += tmp_time;
//double tmp_energy = endEnergyIntel();
double tmp_energy = 0.0;
energy_total += tmp_energy;
printf("summary\t%lf\t%0.6lf\n", tmp_time, tmp_energy);
//PRINT_STATS();
//    fflush(stdout);

    /* Check result */
    {
        char* sequence = sequencerPtr->sequence;
        int result = strcmp(gene, sequence);
//        printf("Sequence matches gene: %s\n", (result ? "no" : "yes"));
        if (result) {
            printf("gene     = %s\n", gene);
            printf("sequence = %s\n", sequence);
        }
        fflush(stdout);
        assert(strlen(sequence) >= strlen(gene));
    }

    /* Clean up */
//    printf("Deallocating memory... ");
//    fflush(stdout);
    sequencer_free(sequencerPtr);
    segments_free(segmentsPtr);
    gene_free(genePtr);
    random_free(randomPtr);
//    puts("done.");
//    fflush(stdout);

}

    TM_SHUTDOWN();
    P_MEMORY_SHUTDOWN();

    GOTO_SIM();

    thread_shutdown();

printf("Time = %lf\n", time_total);
printf("Energy = %0.6lf\n", energy_total);

    MAIN_RETURN(0);
}



/* =============================================================================
 *
 * End of genome.c
 *
 * =============================================================================
 */
