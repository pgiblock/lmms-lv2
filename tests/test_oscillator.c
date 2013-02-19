#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "oscillator.h"

#define NPERIODS 1
#define NSAMPLES 1024

int
main (int argc, char **argv)
{
	Oscillator osc;
	float bendbuf[NSAMPLES];
	float outbuf[NSAMPLES];
	
	int i, j;

	// MOOG WAVE LOOKS BROKEN!
	osc_reset(&osc,
		  OSC_WAVE_MOOG, OSC_MOD_MIX,
		  440.0f, 1.0f, NULL,
		  0.0f, 44100.0f);

	// Init buffers
	for (j=0; j<NSAMPLES; ++j) {
		bendbuf[j] = 1.0f + ((float)j)/NSAMPLES;
		outbuf[j]  = 0.0f;
	}	


	// RUN
	for (i=0; i<NPERIODS*NSAMPLES;) {
		osc_aa_update(&osc, outbuf, bendbuf, NSAMPLES);
		for (j=0; j<NSAMPLES; j++, i++) {
			printf("%d %f\n", i, outbuf[j]);
		}
	}

	return EXIT_SUCCESS;
}
