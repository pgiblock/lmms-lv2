#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "oscillator.h"

#define NPERIODS 4
#define NSAMPLES 64

int
main (int argc, char **argv)
{
	int i;

	Oscillator osc_l[3];
	Oscillator osc_r[3];

	float bendbuf[NSAMPLES];
	float outbuf[2][NSAMPLES];
	
	// Init buffers
	for (i=0; i<NSAMPLES; ++i) {
		bendbuf[i] = 0.0f;
		outbuf[0][i] = 0.0f;
		outbuf[1][i] = 0.0f;
	}	

	// Reset Oscillators
	for (i=2; i>=0; --i) {
		osc_reset(&osc_l[i],
				OSC_WAVE_SAW, OSC_MOD_MIX,
				440.0f, 1.0f,
				i==2?NULL:&(osc_l[i+1]),
				0.0f, 48000.0f);

		osc_reset(&osc_r[i],
				OSC_WAVE_SAW, OSC_MOD_MIX,
				440.0f, 1.0f,
				i==2?NULL:&(osc_r[i+1]),
				0.0f, 48000.0f);
	}

	// RUN
	for (i=NPERIODS; i; i--) {
		printf("----------------------------------------\n");
		osc_aa_update(&osc_l[0], outbuf[0], bendbuf, NSAMPLES);
		osc_aa_update(&osc_r[0], outbuf[1], bendbuf, NSAMPLES);
	}


	return EXIT_SUCCESS;
}
