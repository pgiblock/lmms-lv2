/* Declarations for math functions.
   Originally from the GNU C Library.
   Free Software Foundation, Inc.

   These are math constants and functions that are not
   part of the C99 standard.  Additionally, some functions
   from LMMS are thrown in.
*/

#include <math.h>

#ifndef PRG_MATH_H__
#define PRG_MATH_H__

#define M_2PI          6.28318530717958647692  /* 2pi */

#define M_El           2.7182818284590452353602874713526625L  /* e */
#define M_LOG2El       1.4426950408889634073599246810018921L  /* log_2 e */
#define M_LOG10El      0.4342944819032518276511289189166051L  /* log_10 e */
#define M_LN2l         0.6931471805599453094172321214581766L  /* log_e 2 */
#define M_LN10l        2.3025850929940456840179914546843642L  /* log_e 10 */
#define M_PIl          3.1415926535897932384626433832795029L  /* pi */
#define M_2PIl         6.2831853071795864769252867665590058L  /* 2pi */
#define M_PI_2l        1.5707963267948966192313216916397514L  /* pi/2 */
#define M_PI_4l        0.7853981633974483096156608458198757L  /* pi/4 */
#define M_1_PIl        0.3183098861837906715377675267450287L  /* 1/pi */
#define M_2_PIl        0.6366197723675813430755350534900574L  /* 2/pi */
#define M_2_SQRTPIl    1.1283791670955125738961589031215452L  /* 2/sqrt(pi) */
#define M_SQRT2l       1.4142135623730950488016887242096981L  /* sqrt(2) */
#define M_SQRT1_2l     0.7071067811865475244008443621048490L  /* 1/sqrt(2) */

static inline float
absFraction (const float x) { 
	return x - (x >= 0.0f ? floorf(x) : floorf(x) - 1); 
}


// TODO: Avoid this in triple-osc
static inline float
fraction (float x) {
	return x - floorf(x);
}

#define q_max(x,y) ((x)>(y)?(x):(y))
#define q_min(x,y) ((x)<(y)?(x):(y))
#define t_limit(x, x1, x2) ( q_max((x1), q_min((x), (x2))) )

static inline float exp_knob_val (float val) {
	return fabsf(val) * val;
}

#define FAST_RAND_MAX 32767
static inline int
fast_rand () {
	static unsigned long next = 1;
	next = next * 1103515245 + 12345;
	return( (unsigned)( next / 65536 ) % 32768 );
}

#define safe_fmodf(x) fmodf((x) + 4.0f, 1.0f)

#endif
