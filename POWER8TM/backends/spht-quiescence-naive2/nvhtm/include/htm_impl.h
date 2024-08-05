#ifndef HTM_IMPL_H_GUARD
#define HTM_IMPL_H_GUARD

// before include
//#define HTM_SGL_INIT_BUDGET /* default */20

#include "htm_retry_template.h"

#undef AFTER_ABORT
#define AFTER_ABORT(tid, budget, status) \
  if (!isCraftySet || (isCraftySet && !crafty_isValidate)) {\
    MEASURE_TS(timeAbortedTX_TS2); \
    if (readonly_tx) \
      INC_PERFORMANCE_COUNTER(timeAbortedTX_TS1, timeAbortedTX_TS2, timeAbortedROTX); \
    else \
      INC_PERFORMANCE_COUNTER(timeAbortedTX_TS1, timeAbortedTX_TS2, timeAbortedUpdTX); \
    timeAbortedTX_TS1 = timeAbortedTX_TS2; \
  } \
  on_htm_abort(tid)

#undef BEFORE_SGL_BEGIN
#define BEFORE_SGL_BEGIN(HTM_SGL_tid) MEASURE_TS(timeSGL_TS1);

#undef AFTER_SGL_COMMIT
#define AFTER_SGL_COMMIT(HTM_SGL_tid) \
  MEASURE_TS(timeSGL_TS2); \
  INC_PERFORMANCE_COUNTER(timeSGL_TS1, timeSGL_TS2, timeSGL);

#undef BEFORE_CHECK_BUDGET
#define BEFORE_CHECK_BUDGET(HTM_SGL_budget) MEASURE_TS(timeAbortedTX_TS1);

#endif /* HTM_IMPL_H_GUARD */
