#ifndef ENVELOPE_H__
#define ENVELOPE_H__

#include <stdint.h>

typedef struct {
	int q;            // State
	uint32_t frame;   // Frame of current state
	uint32_t nframes; // Num frames to run current state
	float    rel_base;// Base value for releasing from
	float    last_sample;
} EnvelopeState;


typedef struct {
	float   time_base;	// [0,1.0] in params below are based by this
	float*  del;
	float*  att;
	float*  hold;
	float*  dec;
	float*  sus;
	float*  rel;
	float*  mod;
} EnvelopeParams;


typedef struct {
	EnvelopeParams *p;	// TODO: Consider copying the Params
	EnvelopeState st;
} Envelope;


Envelope* envelope_create(EnvelopeParams *p);

void envelope_destroy(Envelope* e);

void envelope_trigger(Envelope* e);
void envelope_release(Envelope* e);

void envelope_run(Envelope* e, float* sample, uint32_t nsamples);

#endif
