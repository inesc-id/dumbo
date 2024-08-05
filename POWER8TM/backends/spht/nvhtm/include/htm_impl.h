#ifndef HTM_IMPL_H_GUARD
#define HTM_IMPL_H_GUARD

// before include
//#define HTM_SGL_INIT_BUDGET /* default */20

#define HTM_SGL_INIT_BUDGET 10
#include "htm_retry_template.h"
#include "impl_pcwm.h"

#undef AFTER_ABORT
#define AFTER_ABORT(tid, budget, status) \
  if (!isCraftySet || (isCraftySet && !crafty_isValidate)) {\
    MEASURE_TS(timeAbortedTX_TS2); \
    if (PCWM_readonly_tx) \
      INC_PERFORMANCE_COUNTER(timeAbortedTX_TS1, timeAbortedTX_TS2, timeAbortedROTX); \
    else \
      INC_PERFORMANCE_COUNTER(timeAbortedTX_TS1, timeAbortedTX_TS2, timeAbortedUpdTX); \
    timeAbortedTX_TS1 = timeAbortedTX_TS2; \
  } \
  MACRO_PCWM_on_htm_abort_pcwm(tid) /* on_htm_abort(tid) */

#undef BEFORE_SGL_BEGIN
#define BEFORE_SGL_BEGIN(_tid) MEASURE_TS(timeSGL_TS1);

#undef AFTER_SGL_COMMIT
#define AFTER_SGL_COMMIT(_tid) \
  MEASURE_TS(timeSGL_TS2); \
  INC_PERFORMANCE_COUNTER(timeSGL_TS1, timeSGL_TS2, timeSGL);

#undef BEFORE_CHECK_BUDGET
#define BEFORE_CHECK_BUDGET(_budget) \
	MEASURE_TS(timeAbortedTX_TS1); \
//

#undef BEFORE_HTM_COMMIT
#define BEFORE_HTM_COMMIT(_tid, _budget) \
	MACRO_PCWM_on_before_htm_commit_pcwm(_tid) /* onBeforeHtmCommit(HTM_SGL_tid); */ \
//

#undef AFTER_HTM_COMMIT
#define AFTER_HTM_COMMIT(_tid, _budget) \
	on_after_htm_commit_pcwm(_tid) /* on_after_htm_commit(HTM_SGL_tid); */ \
//

#undef BEFORE_SGL_COMMIT
#define BEFORE_SGL_COMMIT(_tid) \
	MACRO_PCWM_on_before_sgl_commit_pcwm(_tid) \
//

#endif /* HTM_IMPL_H_GUARD */
