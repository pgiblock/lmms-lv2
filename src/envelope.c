#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "envelope.h"
#include "lmms_math.h"

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

// Advance envelope state to next non-zero-length section
static void
advance_state(EnvelopeParams* p, EnvelopeState* st)
{
	switch (st->q) {
		case ENV_OFF:
			if (*p->del > 0.0f) {
				st->q       = ENV_DEL;
				st->nframes = p->time_base * expKnobVal(*p->del);
				st->frame   = 0;
				return;
			}
			// Fall-through
		case ENV_DEL:
			if (*p->att > 0.0f) {
				st->q       = ENV_ATT;
				st->nframes = p->time_base * expKnobVal(*p->att);
				st->frame   = 0;
				return;
			}
			// Fall-through
		case ENV_ATT:
			if (*p->hold > 0.0f) {
				st->q       = ENV_HOLD;
				st->nframes = p->time_base * expKnobVal(*p->hold);
				st->frame   = 0;
				return;
			}
			// Fall-through
		case ENV_HOLD:
			if (*p->dec > 0.0f && *p->sus > 0.0f) {
				st->q       = ENV_DEC;
				st->nframes = p->time_base * expKnobVal((*p->dec)*(*p->sus));
				st->frame   = 0;
				return;
			}
			// Fall-through
		case ENV_DEC:
			st->q       = ENV_SUS;
			st->nframes = 0;
			st->frame   = 0;
			return;
	}
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
	e->st.q = ENV_OFF;
	advance_state(e->p, &e->st);
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
			// Only do release if release has any length
			if (*e->p->rel > 0.0f) {
				e->st.q        = ENV_REL;
				e->st.nframes  = e->p->time_base * expKnobVal(*e->p->rel);
				e->st.frame    = 0;
				e->st.rel_base = e->st.last_sample;
			} else {
				e->st.q = ENV_OFF;
			}
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
					advance_state(e->p, &e->st);
				}
				break;

			case ENV_ATT:
				// Process
				o = ((float)e->st.frame) / (e->st.nframes);
				// State
				e->st.frame++;
				if (e->st.frame >= e->st.nframes) {
					advance_state(e->p, &e->st);
				}
				break;

			case ENV_HOLD:
				// Process
				o = 1.0f;
				// State
				e->st.frame++;
				if (e->st.frame > e->st.nframes) {
					advance_state(e->p, &e->st);
				}
				break;

			case ENV_DEC:
				// Process
				o = amsum + e->st.frame * ((1.0f / e->st.nframes)*((1.0f-(*e->p->sus))-1.0f)*amount);
				//o = 1.0f - (1.0f - (*eg->sus_port)) * ((float)e->st.frame / e->st.nframes);
				//State
				e->st.frame++;
				if (e->st.frame > e->st.nframes) {
					advance_state(e->p, &e->st);
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
