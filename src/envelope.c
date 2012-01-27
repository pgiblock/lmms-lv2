#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "envelope.h"

// STATES
enum {
	ENV_OFF,
	ENV_DEL,
	ENV_ATT,
	ENV_HOLD,
	ENV_DEC,
	ENV_SUS,
	ENV_REL
};


static inline float
envelope_exp_val( float val )
{
	return ( ( val < 0 ) ? -val : val ) * val;
}


Envelope*
envelope_create(EnvelopeParams* p)
{
	Envelope* e = (Envelope*)malloc(sizeof(Envelope));
	if (e) {
		// Parameters made to external floats
		e->p = p;

		// State
		e->st.q        = ENV_OFF;
		e->st.frame    = 0;
		e->st.nframes  = 0;	
		e->st.rel_base = 0;
		return e;
	}
	return NULL;
}


void
envelope_destroy(Envelope* e)
{
	free(e);
}


void
envelope_trigger(Envelope* e)
{
	printf("TRIGGER\n");
	e->st.q       = ENV_DEL;
	e->st.nframes = e->p->time_base * envelope_exp_val(*e->p->del);
	e->st.frame   = 0;
}


void
envelope_release(Envelope* e)
{
	switch (e->st.q) {
		case ENV_OFF:
		case ENV_REL:
			break;
		case ENV_DEL:
			e->st.q = ENV_OFF;
			break;
		default:
			printf("RELEASE\n");
			e->st.q        = ENV_REL;
			e->st.nframes  = e->p->time_base * envelope_exp_val(*e->p->rel);
			e->st.frame    = 0;
			e->st.rel_base = e->st.last_sample; 
			break;
	}
}


void
envelope_run(Envelope* e, float* samples, uint32_t nsamples)
{
	// FIXME: Crap
	float amsum  = 1.0f;
	float amount = 1.0f;

	int i;
	float o = e->st.last_sample;
	for (i=0; i<nsamples; ++i) {
		// Stupid way
		switch (e->st.q) {
			case ENV_OFF:
				o = 0.0f;
				break;

			case ENV_DEL:
				// Process
				o = 0.0f;
				// State change
				e->st.frame++;
				if (e->st.frame > e->st.nframes) {
					printf("ATT\n");
					e->st.q       = ENV_ATT;
					e->st.nframes = e->p->time_base * envelope_exp_val(*e->p->att);
					e->st.frame   = 0;
				}
				break;

			case ENV_ATT:
				// Process
				o = ((float)e->st.frame) / (e->st.nframes);
				// State
				e->st.frame++;
				if (e->st.frame >= e->st.nframes) {
					printf("HOLD\n");
					e->st.q       = ENV_HOLD;
					e->st.nframes = e->p->time_base * envelope_exp_val(*e->p->hold);
					e->st.frame   = 0;
				}
				break;

			case ENV_HOLD:
				// Process
				o = 1.0f;
				// State
				e->st.frame++;
				if (e->st.frame > e->st.nframes) {
					printf("DEC\n");
					e->st.q       = ENV_DEC;
					e->st.nframes = e->p->time_base * envelope_exp_val((*e->p->dec)*(*e->p->sus));
					e->st.frame   = 0;
				}
				break;

			case ENV_DEC:
				// Process
				o = amsum + e->st.frame * ((1.0f / e->st.nframes)*((1.0f-(*e->p->sus))-1.0f)*amount);
				//o = 1.0f - (1.0f - (*eg->sus_port)) * ((float)e->st.frame / e->st.nframes);
				//State
				e->st.frame++;
				if (e->st.frame > e->st.nframes) {
					printf("SUS\n");
					e->st.q       = ENV_SUS;
					e->st.nframes = 0;
					e->st.frame   = 0;
				}
				break;

			case ENV_SUS:
				o = 1.0f - (*e->p->sus); // Sustain Level;
				break;

			case ENV_REL:
				// Process
				o = e->st.rel_base * (1.0f - ((float)e->st.frame / e->st.nframes));
				// State
				e->st.frame++;
				if (e->st.frame >= e->st.nframes) {
					printf("OFF\n");
					e->st.q = ENV_OFF;
				}
				break;

			default:
				break;
		}

		float mod = *e->p->mod;
		if (mod >= 0.0f) {
			samples[i] = o * mod;
		}
		else {
			// TODO: Negative mod
		}
	}
	e->st.last_sample = o;
}
