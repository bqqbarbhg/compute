#pragma once

#include <assert.h>
#include <math.h>

#if F_DEBUG

#define F_ASSERT(x) assert((x))
#define F_ASSUME(cond, expr) (F_ASSERT(cond), (expr))

#else

#define F_ASSERT(x) (void)0
#define F_ASSUME(cond, expr) expr

#endif

#if SSE

#define F_MIN(a, b) (_mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b))))
#define F_MAX(a, b) (_mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b))))

#define F_MAX3(a, b, c) F_MAX(F_MAX(a, b), c)

uint32_t mmSignBit[4] = { 1U << 31, 1U << 31, 1U << 31, 1U << 31 };
#define F_OR_SIGN_IMPL(a, b) _mm_cvtps_f32(_mm_or_ps(_mm_set_ss(a), \
			_mm_and_ps(_mm_load_ps((float*)mmSignBit), _mm_set_ss(b))))
#define F_COPYSIGN_TO_POSITIVE(a, b) F_ASSUME(a >= 0.0f, F_OR_SIGN_IMPL(a, b))

#else

inline float FMinImpl(float a, float b) { return a < b ? a : b; }
inline float FMaxImpl(float a, float b) { return a > b ? a : b; }

#define F_MIN(a, b) (FMinImpl(a, b))
#define F_MAX(a, b) (FMaxImpl(a, b))

#define F_MAX3(a, b, c) F_MAX(F_MAX(a, b), c)
#define F_MIN3(a, b, c) F_MIN(F_MIN(a, b), c)

#define F_COPYSIGN_TO_POSITIVE(a, b) F_ASSUME(a >= 0.0f, copysign(a, b))

#define F_ABS(a) (fabsf(a))

#endif


