#ifndef ARCH_H_GUARD
#define ARCH_H_GUARD

#include "rdtsc.h"

#include <stdint.h>

// TODO: ARCH_CACHE_LINE_SIZE can be get using:
// > getconf LEVEL1_DCACHE_LINESIZE
// Number of processors:
// > getconf _NPROCESSORS_ONLN

#ifndef   NVM_LATENCY_NS
#define   NVM_LATENCY_NS 500
#endif /* NVM_LATENCY_NS */

// in kiloHz: use `cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq`
#ifndef   CPU_MAX_FREQ
#define   CPU_MAX_FREQ 1800000ULL
#endif /* CPU_MAX_FREQ */

// use `cat /proc/cpuinfo | grep processor | wc -l`
#ifndef   MAX_PHYS_THRS
#define   MAX_PHYS_THRS 56
#endif /* MAX_PHYS_THRS */

#if defined(__powerpc__)
	//#pragma message ( "USING_P8" )

	#include <htmxlintrin.h>
	#define ARCH_CACHE_LINE_SIZE 128
	// TODO: use __sync_synchronize() instead
	#define MEMFENCE asm volatile ("SYNC" : : : "memory")
	#define PAUSE() asm volatile ("or 31,31,31" : :)

  typedef enum {
		HTM_SUCCESS = 0,
		HTM_ABORT,
		HTM_EXPLICIT,
		HTM_ILLEGAL,
		HTM_CONFLICT,
		HTM_CAPACITY,
		HTM_TRANS,
		HTM_NON_TRANS,
		HTM_OTHER,
		HTM_FALLBACK,
		HTM_SELF,
		HTM_PERSISTENT
  } errors_e;

	#define HTM_NB_ERRORS 		11
	#define HTM_STATUS_TYPE		TM_buff_type
	#define HTM_CODE_SUCCESS	_HTM_TBEGIN_STARTED

	#define HTM_begin(var)			(__TM_begin_rot(var))
	#define HTM_abort()				__TM_abort()
	#define HTM_named_abort(code)	__TM_named_abort(code)
	#define HTM_is_named(status)	__TM_is_named_user_abort (status, NULL);
	#define HTM_test()				__builtin_tcheck()
	#define HTM_commit()			__TM_end()
	#define HTM_get_named(status)	({ \
		unsigned char code; \
		__TM_is_named_user_abort (status, &code); \
		code; \
	})


extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_self_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_SELF_INDUCED_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_trans_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_TRANSACTION_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_nontrans_conflict(void* const TM_buff)
{
  texasr_t texasr = __builtin_get_texasr ();
  return _TEXASR_NON_TRANSACTIONAL_CONFLICT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_persistent_abort(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_FAILURE_PERSISTENT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_conflict(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  /* Return TEXASR bits 11 (Self-Induced Conflict) through
     14 (Translation Invalidation Conflict).  */
  return (_TEXASR_EXTRACT_BITS (texasr, 14, 4)) ? 1 : 0;
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_user_abort (void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_ABORT (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_capacity_abort (void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
  return _TEXASR_FOOTPRINT_OVERFLOW (texasr);
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_begin_rot (void* const TM_buff)
{
  *_TEXASRL_PTR (TM_buff) = 0;
  if (__builtin_expect (__builtin_tbegin (1), 1)){
    return _HTM_TBEGIN_STARTED;
  }
#ifdef __powerpc64__
  *_TEXASR_PTR (TM_buff) = __builtin_get_texasr ();
#else
  *_TEXASRU_PTR (TM_buff) = __builtin_get_texasru ();
  *_TEXASRL_PTR (TM_buff) = __builtin_get_texasr ();
#endif
  *_TFIAR_PTR (TM_buff) = __builtin_get_tfiar ();
  return 0;
}

extern __inline long
__attribute__ ((__gnu_inline__, __always_inline__, __artificial__))
__TM_is_tfiar_exact(void* const TM_buff)
{
  texasr_t texasr = *_TEXASR_PTR (TM_buff);
 return _TEXASR_TFIAR_EXACT(texasr);
}

  // TODO: do also for TSX only with the common aborts
	#define P8_ERROR_TO_INDEX(tm_buffer) ({ \
	  errors_e code = HTM_OTHER; \
	  if(__TM_is_illegal  ((tm_buffer)))               code = HTM_ILLEGAL; \
	  else if(__TM_capacity_abort((tm_buffer)))  	     code = HTM_CAPACITY; \
	  else if(__TM_is_trans_conflict((tm_buffer)))     code = HTM_TRANS; \
	  else if(__TM_is_nontrans_conflict((tm_buffer)))  code = HTM_NON_TRANS; \
	  else if(__TM_user_abort((tm_buffer)))            code = HTM_EXPLICIT; \
	  else if(__TM_is_self_conflict((tm_buffer)))      code = HTM_SELF; \
	  else if(__TM_is_failure_persistent((tm_buffer))) code = HTM_PERSISTENT; \
		else if(__TM_conflict((tm_buffer)))           	 code = HTM_CONFLICT; \
	  else                                             code = HTM_SUCCESS; \
	  code; \
	})

	#define HTM_ERROR_INC(status, error_array) ({ \
	  errors_e code = P8_ERROR_TO_INDEX(status); \
	  if (code != HTM_SUCCESS) error_array[HTM_ABORT] += 1; \
	  error_array[code] += 1; \
	})

#else /* TSX code */
	//#pragma message ( "USING_TSX" )

	#include <immintrin.h> // includes avx512 now
	#define _IMMINTRIN_H_INCLUDED
	#include <xtestintrin.h>
	#include <rtmintrin.h>
	#define ARCH_CACHE_LINE_SIZE 64
	// TODO: use __sync_synchronize() instead
	#define MEMFENCE asm volatile ("MFENCE" : : : "memory")
	#define PAUSE()  _mm_pause()

  typedef enum {
    HTM_SUCCESS = 0,
		HTM_ABORT,
		HTM_EXPLICIT,
		HTM_RETRY,
		HTM_CONFLICT,
    HTM_CAPACITY,
    HTM_SELF,
    HTM_PERSISTENT,
		HTM_DEBUG,
		HTM_NESTED,
		HTM_OTHER,
		HTM_FALLBACK
  } HTM_errors_e;

	#define HTM_NB_ERRORS     10
	#define HTM_STATUS_TYPE   register int
	#define HTM_CODE_SUCCESS  _XBEGIN_STARTED

	#define HTM_begin(var)			  (printf("begin_rot\n"); var = __TM_begin_rot())
	#define HTM_abort()           _xabort(0)
	#define HTM_named_abort(code)	_xabort(code)
	#define HTM_test()            _xtest()
	#define HTM_commit()          _xend()
	#define HTM_get_named(status) (status >> 24)
	#define HTM_is_named(status)  (status & 1)

	#define HTM_ERROR_INC(status, error_array) ({ \
		printf("status:%d",status);\
	  if (status == _XBEGIN_STARTED) { \
			error_array[HTM_SUCCESS] += 1; \
	  } else { \
			error_array[HTM_ABORT] += 1; \
			int tsx_error = status & 0x1F; /* only catch known aborts */ \
			int nb_errors = __builtin_popcount(tsx_error); \
			do { \
			  int idx = tsx_error & _XABORT_EXPLICIT ? \
				({tsx_error = tsx_error & ~_XABORT_EXPLICIT; HTM_EXPLICIT;}) : \
			  tsx_error & _XABORT_RETRY ? \
				({tsx_error = tsx_error & ~_XABORT_RETRY; HTM_RETRY;}) : \
			  tsx_error & _XABORT_CONFLICT ? \
				({tsx_error = tsx_error & ~_XABORT_CONFLICT; HTM_CONFLICT;}) : \
			  tsx_error & _XABORT_CAPACITY ? \
				({tsx_error = tsx_error & ~_XABORT_CAPACITY; HTM_CAPACITY;}) : \
			  tsx_error & _XABORT_DEBUG ? \
				({tsx_error = tsx_error & ~_XABORT_DEBUG; HTM_DEBUG;}) : \
			  HTM_OTHER; \
			  error_array[idx] += 1; \
			  nb_errors--; \
			} while (nb_errors > 0); \
	  } \
	})

#endif /* __powerpc__ */

#ifdef DISABLE_HTM
	#undef HTM_begin
	#undef HTM_abort
	#undef HTM_commit

	#define HTM_begin(var) (var = HTM_CODE_SUCCESS)
	#define HTM_abort()
	#define HTM_commit()
#endif /* DISABLE_HTM */

#define CL_ALIGN __attribute__((aligned(ARCH_CACHE_LINE_SIZE)))
#define CL_DISTANCE(type) ARCH_CACHE_LINE_SIZE / sizeof(type)

#ifndef GRANULE_TYPE
#define GRANULE_TYPE intptr_t
#endif /* GRANULE_TYPE */

#ifndef GRANULE_P_TYPE
#define GRANULE_P_TYPE intptr_t*
#endif /* GRANULE_P_TYPE */

#ifndef GRANULE_D_TYPE
#define GRANULE_D_TYPE double
#endif /* GRANULE_D_TYPE */

// #############################
// ### GRANULE OPERATIONS ######
// #############################
typedef union convert_types_ {
	GRANULE_D_TYPE d;
	GRANULE_P_TYPE p;
	GRANULE_TYPE g;
} convert_types_u;

#define CONVERT_GRANULE_D(val) ({ \
	convert_types_u u; \
	u.d = (GRANULE_D_TYPE) val; \
	u.g; \
})

#define CONVERT_GRANULE_P(val) ({ \
	convert_types_u u; \
	u.p = (GRANULE_P_TYPE) val; \
	u.g; \
})
// #############################

typedef unsigned long long ts_s;

#endif /* ARCH_H_GUARD */
