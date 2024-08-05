#include "replayer.hpp"
#include "replayer_internal.hpp"

#include <random>

static std::mt19937 *gen;
static rep::args_t g_args;

/*extern*/ int (*rep::init)(rep::args_t&);
/*extern*/ int (*rep::destroy)();
/*extern*/ int (*rep::generate_log)();
/*extern*/ int (*rep::replay)();
/*extern*/ int pm_delay = 46;
/*extern*/ thread_local int nb_cache_lines;

static void setup(rep::args_t& a)
{
  if (gen)
  {
    delete gen;
    gen = nullptr;
  }
  gen = new std::mt19937(a.seed);
  g_args = a;
  pm_delay = a.pm_delay;
  rep::init(a);
}

void rep::setup_naive(rep::args_t& a)
{
  rep::init = rep::naive::init;
  rep::generate_log = rep::naive::generate_log;
  rep::replay = rep::naive::replay;
  rep::destroy = rep::naive::destroy;
  setup(a);
}

void rep::setup_seq_log(rep::args_t& a)
{
  rep::init = rep::seq_log::init;
  rep::generate_log = rep::seq_log::generate_log;
  rep::replay = rep::seq_log::replay;
  rep::destroy = rep::seq_log::destroy;
  setup(a);
}

void rep::setup_forward_link(rep::args_t& a)
{
  rep::init = rep::forward_link::init;
  rep::generate_log = rep::forward_link::generate_log;
  rep::replay = rep::forward_link::replay;
  rep::destroy = rep::forward_link::destroy;
  setup(a);
}

long rep::random(long a, long b)
{
  std::uniform_int_distribution<long> distrib(a, b);
  return distrib(*gen);
}

long rep::random_access()
{
  std::uniform_int_distribution<long> distrib(0, g_args.heap_size - 1);
  return distrib(*gen);
}
