#include "CuTest.h"
#include "spins.h"
#include "TestSpins.h"
#include "impl.h"
#include "global_structs.h"

void TestSpinTimeIs500ns(CuTest* tc)
{
  double timeSpins = 0;
  unsigned long long ts1 = 0, ts2 = 0;
  TEST_MSG_INIT();

  learn_spin_nops(500.0, CPU_FREQ, 1);

  ts1 = rdtscp();
  spin_nops(learnedSpins); // an interrupt here screws the experiment
  ts2 = rdtscp();

  timeSpins = (ts2 - ts1) / CPU_FREQ;

  TEST_MSG_WRITE("Time taken is %f, not 500ns (learnedSpins=%i)", timeSpins, learnedSpins);
  CuAssertDblEquals_Msg(tc, TEST_MSG_GET(), 0.0005, timeSpins, 0.0001);

  TEST_MSG_EXIT();
}

void TestEpochImpaMacros(CuTest* tc)
{
  global_structs_init(
    4              /* NB_THREADS */,
    1,
    4096           /* NB_EPOCHS */,
    4096*512       /* LOG_SIZE */,
    5*1024*1024    /* MEM_REGION (5M) */,
    100            /* EPOCH_TIMEOUT */,
    500            /* SPIN_FLUSHES */
  );

  G_next[0].log_ptrs.epoch_next = 4095;
  G_next[1].log_ptrs.epoch_next = 4095;
  G_next[2].log_ptrs.epoch_next = 4095;
  G_next[3].log_ptrs.epoch_next = 4095;

  CuAssert(tc, "0 comes before 1", IS_EPOCH_AFTER(0, 1));
  CuAssert(tc, "4094 comes after 0", IS_EPOCH_AFTER(0, 4094));
  CuAssert(tc, "4000 comes after 100", IS_EPOCH_AFTER(100, 4000));
  CuAssert(tc, "4094 comes after 4093", IS_EPOCH_AFTER(4093, 4094));

  gs_log_data.log.epoch_end = 2000;

  CuAssert(tc, "2001 comes before 1999 (wrap)", IS_EPOCH_AFTER(2001, 1999));

  gs_log_data.log.epoch_end = 4095;

  // ------------------------

  P_epoch_ts[0][0] = 0;
  P_epoch_ts[1][0] = 1;
  P_epoch_ts[2][0] = 2;
  P_epoch_ts[3][0] = 3;

  // ------------------------

  P_epoch_ts[0][4095] = 10000;
  P_epoch_ts[1][4095] = 10001;
  P_epoch_ts[2][4095] = 10002;
  P_epoch_ts[3][4095] = 10003;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "cannot advance", G_next[0].log_ptrs.epoch_next == 4095);
  CuAssert(tc, "cannot advance", G_next[1].log_ptrs.epoch_next == 4095);
  CuAssert(tc, "cannot advance", G_next[2].log_ptrs.epoch_next == 4095);
  CuAssert(tc, "cannot advance", G_next[3].log_ptrs.epoch_next == 4095);

  CuAssert(tc, "FIND_LAST_SAFE_EPOCH() == 4094", FIND_LAST_SAFE_EPOCH() == 4094);

  G_next[3].log_ptrs.epoch_next = 2000;

  CuAssert(tc, "FIND_LAST_SAFE_EPOCH() == 1999", FIND_LAST_SAFE_EPOCH() == 1999);

  G_next[0].log_ptrs.epoch_next = 1988;
  G_next[1].log_ptrs.epoch_next = 4093;
  G_next[2].log_ptrs.epoch_next = 1;
  G_next[3].log_ptrs.epoch_next = 2;

  gs_log_data.log.epoch_end = 1984;

  CuAssert(tc, "IS_EPOCH_AFTER(1988, 4093) == 1", IS_EPOCH_AFTER(1988, 4093) == 1);
  CuAssert(tc, "IS_EPOCH_AFTER(4093,    1) == 1", IS_EPOCH_AFTER(4093,    1) == 1);
  CuAssert(tc, "IS_EPOCH_AFTER(   1,    2) == 1", IS_EPOCH_AFTER(   1,    2) == 1);
  CuAssert(tc,  "FIND_LAST_SAFE_EPOCH() == 1987", FIND_LAST_SAFE_EPOCH() == 1987);

  G_next[0].log_ptrs.epoch_next = 3723;
  G_next[1].log_ptrs.epoch_next = 3723;
  G_next[2].log_ptrs.epoch_next = 3725;
  G_next[3].log_ptrs.epoch_next = 3723;

  gs_log_data.log.epoch_end = 3723;

  CuAssert(tc, "IS_EPOCH_AFTER(3723, 3723)", IS_EPOCH_AFTER(3723, 3723) == 0);
  CuAssert(tc, "IS_EPOCH_AFTER(3723, 3725)", IS_EPOCH_AFTER(3723, 3725) == 1);
  CuAssert(tc, "IS_EPOCH_AFTER(3722, 3725)", IS_EPOCH_AFTER(3722, 3725) == 0);
  CuAssert(tc, "wrong safe epoch", FIND_LAST_SAFE_EPOCH() == 3722);

  G_next[0].log_ptrs.epoch_next = 485;
  G_next[1].log_ptrs.epoch_next = 485;
  G_next[2].log_ptrs.epoch_next = 485;
  G_next[3].log_ptrs.epoch_next = 483;

  gs_log_data.log.epoch_end = 494;
  P_start_epoch = 495;

  CuAssert(tc, "IS_EPOCH_AFTER(485, 485)", IS_EPOCH_AFTER(485, 485) == 0);
  CuAssert(tc, "IS_EPOCH_AFTER(485, 483)", IS_EPOCH_AFTER(485, 483) == 0);
  CuAssert(tc, "FIND_LAST_SAFE_EPOCH() == 482", FIND_LAST_SAFE_EPOCH() == 482);

  CuAssert(tc, "IS_CLOSE_TO_END(494, 493)", IS_CLOSE_TO_END(494, 493) == 0);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 494)", IS_CLOSE_TO_END(494, 494) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 495)", IS_CLOSE_TO_END(494, 495) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 496)", IS_CLOSE_TO_END(494, 496) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 497)", IS_CLOSE_TO_END(494, 497) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 498)", IS_CLOSE_TO_END(494, 498) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 499)", IS_CLOSE_TO_END(494, 499) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(494, 500)", IS_CLOSE_TO_END(494, 500) == 0);

  CuAssert(tc, "IS_CLOSE_TO_END(4094, 4093)", IS_CLOSE_TO_END(4094, 4093) == 0);
  CuAssert(tc, "IS_CLOSE_TO_END(4094, 4094)", IS_CLOSE_TO_END(4094, 4094) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094, 4094)", IS_CLOSE_TO_END(4094, 4095) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094,    0)", IS_CLOSE_TO_END(4094,    0) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094,    1)", IS_CLOSE_TO_END(4094,    1) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094,    2)", IS_CLOSE_TO_END(4094,    2) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094,    3)", IS_CLOSE_TO_END(4094,    3) == 1);
  CuAssert(tc, "IS_CLOSE_TO_END(4094,    4)", IS_CLOSE_TO_END(4094,    4) == 0);

  G_next[0].log_ptrs.epoch_next = 1902;
  G_next[1].log_ptrs.epoch_next = 1908;
  G_next[2].log_ptrs.epoch_next = 2070;
  G_next[3].log_ptrs.epoch_next = 533;

  gs_log_data.log.epoch_end = 1913;
  P_start_epoch = 1914;

  CuAssert(tc, "FIND_LAST_SAFE_EPOCH() == 2069", FIND_LAST_SAFE_EPOCH() == 2069);

  G_next[0].log_ptrs.epoch_next = 1286;
  G_next[1].log_ptrs.epoch_next = 1285;
  G_next[2].log_ptrs.epoch_next = 1286;
  G_next[3].log_ptrs.epoch_next = 1286;

  gs_log_data.log.epoch_end = 1279;
  P_start_epoch = 1280;

  CuAssert(tc, "FIND_LAST_SAFE_EPOCH() == 2069", FIND_LAST_SAFE_EPOCH() == 1284);

  gs_log_data.log.epoch_end = 1999;
  P_start_epoch = 2000;

  G_next[0].log_ptrs.epoch_next = 1991;
  G_next[1].log_ptrs.epoch_next = 1991;
  G_next[2].log_ptrs.epoch_next = 1991;
  G_next[3].log_ptrs.epoch_next = 1991;

  P_epoch_ts[0][1990] = 2000;
  P_epoch_ts[1][1990] = 2001;
  P_epoch_ts[2][1990] = 2002;
  P_epoch_ts[3][1990] = 2003;

  P_epoch_ts[0][1991] = 2010;
  P_epoch_ts[1][1991] = 2011;
  P_epoch_ts[2][1991] = 2012;
  P_epoch_ts[3][1991] = 2013;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "cannot advance", G_next[0].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[1].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[2].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[3].log_ptrs.epoch_next == 1992);

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  // again the same
  CuAssert(tc, "cannot advance", G_next[0].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[1].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[2].log_ptrs.epoch_next == 1992);
  CuAssert(tc, "cannot advance", G_next[3].log_ptrs.epoch_next == 1992);

  P_epoch_ts[0][1992] = 2020;
  P_epoch_ts[1][1992] = 2021;
  P_epoch_ts[2][1992] = 2022;
  P_epoch_ts[3][1992] = 2023;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "cannot advance", G_next[0].log_ptrs.epoch_next == 1993);
  CuAssert(tc, "cannot advance", G_next[1].log_ptrs.epoch_next == 1993);
  CuAssert(tc, "cannot advance", G_next[2].log_ptrs.epoch_next == 1993);
  CuAssert(tc, "cannot advance", G_next[3].log_ptrs.epoch_next == 1993);

  P_epoch_ts[0][1993] = 2030;
  P_epoch_ts[1][1993] = 2031;
  P_epoch_ts[2][1993] = 2032;
  P_epoch_ts[3][1993] = 2033;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "cannot advance", G_next[0].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "cannot advance", G_next[1].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "cannot advance", G_next[2].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "cannot advance", G_next[3].log_ptrs.epoch_next == 1994);

  P_epoch_ts[0][1994] = 2030;
  P_epoch_ts[1][1994] = 2031;
  P_epoch_ts[2][1994] = 2032;
  P_epoch_ts[3][1994] = 2033;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "out of space", G_next[0].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "out of space", G_next[1].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "out of space", G_next[2].log_ptrs.epoch_next == 1994);
  CuAssert(tc, "out of space", G_next[3].log_ptrs.epoch_next == 1994);

  G_next[0].log_ptrs.epoch_next = 1995;
  G_next[1].log_ptrs.epoch_next = 1995;
  G_next[2].log_ptrs.epoch_next = 1995;
  G_next[3].log_ptrs.epoch_next = 1995;

  P_epoch_ts[0][1995] = 2040;
  P_epoch_ts[1][1995] = 2041;
  P_epoch_ts[2][1995] = 2042;
  P_epoch_ts[3][1995] = 2043;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "out of space", G_next[0].log_ptrs.epoch_next == 1995);
  CuAssert(tc, "out of space", G_next[1].log_ptrs.epoch_next == 1995);
  CuAssert(tc, "out of space", G_next[2].log_ptrs.epoch_next == 1995);
  CuAssert(tc, "out of space", G_next[3].log_ptrs.epoch_next == 1995);

  G_next[0].log_ptrs.epoch_next = 2000; // beginning
  G_next[1].log_ptrs.epoch_next = 2000;
  G_next[2].log_ptrs.epoch_next = 2000;
  G_next[3].log_ptrs.epoch_next = 2000;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "empty", G_next[0].log_ptrs.epoch_next == 2000);
  CuAssert(tc, "empty", G_next[1].log_ptrs.epoch_next == 2000);
  CuAssert(tc, "empty", G_next[2].log_ptrs.epoch_next == 2000);
  CuAssert(tc, "empty", G_next[3].log_ptrs.epoch_next == 2000);

  P_epoch_ts[0][2000] = 2050;
  P_epoch_ts[1][2000] = 2051;
  P_epoch_ts[2][2000] = 2052;
  P_epoch_ts[3][2000] = 2053;

  LOOK_UP_FREE_SLOT(0);
  LOOK_UP_FREE_SLOT(1);
  LOOK_UP_FREE_SLOT(2);
  LOOK_UP_FREE_SLOT(3);

  CuAssert(tc, "non-empty", G_next[0].log_ptrs.epoch_next == 2001);
  CuAssert(tc, "non-empty", G_next[1].log_ptrs.epoch_next == 2001);
  CuAssert(tc, "non-empty", G_next[2].log_ptrs.epoch_next == 2001);
  CuAssert(tc, "non-empty", G_next[3].log_ptrs.epoch_next == 2001);

  global_structs_destroy();
}
