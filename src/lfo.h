#ifndef LFO_H__
#define LFO_H__

#include <stdint.h>

enum LfoWaveShapes {
	LFO_WAVE_SINE,
	LFO_WAVE_TRIANGLE,
	LFO_WAVE_SAW,
	LFO_WAVE_SQUARE
} ;

typedef struct {
	int q;                  // State
	uint32_t frame;         // Frame of current state
	uint32_t nframes;       // Num frames to run current state
	float    phase;         // LFO oscillator phase
} LfoState;

typedef struct {
	float time_base;
	float *del;     // predelay time
	float *att;     // attack time
	float *spd;     // speed
	float *shape;   // wave shape
	float *mod;     // modulation amount
	// TODO: This maybe belongs somewhere else (inside instrument)
	float *op;
	// TODO: Just make this a multiplier amount: {1, 100}
	//float *x100;    // frequency * 100
} LfoParams;

typedef struct {
	LfoParams *p;
	LfoState   st;
} Lfo;

Lfo *lfo_create (LfoParams *p);

void lfo_destroy (Lfo *lfo);

void lfo_trigger (Lfo *lfo);
int lfo_run (Lfo *lfo, float* samples, uint32_t nsamples);

#endif // LFO_H__
