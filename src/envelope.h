#ifndef ENVELOPE_H__
#define ENVELOPE_H__

#include <stdint.h>

typedef struct {
	int q;                  // State
	uint32_t frame;         // Frame of current state
	uint32_t nframes;       // Num frames to run current state
	float    rel_base;      // Base value for releasing from
	float    last_sample;
} EnvelopeState;


typedef struct {
	float  time_base;       // [0,1.0] in params below are based by this
	float *del;             // predelay time
	float *att;             // attack time
	float *hold;            // hold time (before decaying)
	float *dec;             // decay time
	float *sus;             // sustain level
	float *rel;             // release time
	float *mod;             // modulation amount
} EnvelopeParams;


typedef struct {
	EnvelopeParams *p;      // TODO: Consider copying the Params
	EnvelopeState   st;     // Internal state of this envelope
} Envelope;


Envelope *envelope_create (EnvelopeParams *p);

void envelope_destroy (Envelope *e);

void envelope_trigger (Envelope *e);
void envelope_release (Envelope *e);

int envelope_run (Envelope *e, float *sample, uint32_t nsamples);

#endif // ENVELOPE_H__
