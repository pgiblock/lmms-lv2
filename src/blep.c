#include <math.h>

#include "lmms_math.h"
#include "blep.h"

static inline double
sinc (double x)
{
	double t;
	if (x == 0.0) {
		return 1.0;
	} else {
		t = M_PI * x;
		return sin(t) / t;
	}
}


void
blep_init (float *blep, float *blamp, int n)
{
	int i;
	double s, t, x;
	double sum, scale;
 
	sum = 0.0;
	for (i = 0; i < n; ++i) {
		// PG: FIXME: Hard coded 8.0
		x = ((double)i)/n * 8.0 * 0.90;

		// PG: Why default 1?
		if (x == 0) {
			s = 1.0;
		} else {
			t = x - sqrt(2.0);

			if (t < 0.0) {
				s  = sinc(t/sqrt(2.0));
				s *= s;
			} else {
				s  = sinc(t);
			}
		}

		sum += s;
		blep[i] = sum;
	}

	// normalize step
	scale = 1.0 / sum;
	for (i = 0; i < n; ++i) {
		blep[i] *= scale;
	}

	// integrate normalized step and store into Blamp-Table
	sum = 0.0;
	for (i = 0; i < n; ++i) {
		sum += blep[i];
		blamp[i] = sum;
	}

	// now substract the naive ramp
	scale = 1.0 / sum;
	for (i = 0; i < n; ++i) {
		// HUH?? should be i/n*8?? wtf with scale?
		blamp[i] = (blamp[i]*scale) - ((double)i)/n;
	}

	// substract the naive step so only the difference-signal remains
	for (i = 0; i < n; ++i) {
		blep[i] = 1.0 - blep[i];
	}
}


void
blep_state_init (BlepState* st) {
	st->ptr = 8; // deactivate this correction-pipeline
	st->vol = 0.0f;
	st->phs = 0.0f;
	st->typ = 0; // use a blep as default
}


