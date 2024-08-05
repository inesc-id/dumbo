#pragma once

namespace rep
{

struct args_t
{
  long nb_threads;
  long size_thread_log; // sizes are multiplied internally by size of struct
  long size_metadata;
  long heap_size; 
  int min_writes;
  int max_writes;
  int pm_delay;
  long seed;
};

// setups the callbacks below (select one in the begining of the program)
// it also calls init(args_t)
void setup_naive(args_t&);
void setup_forward_link(args_t&);
void setup_seq_log(args_t&);

// to be setup by the calls above
extern int (*init)(args_t&);
extern int (*destroy)();
extern int (*generate_log)();
extern int (*replay)();

long random(long a, long b);
long random_access();

};
