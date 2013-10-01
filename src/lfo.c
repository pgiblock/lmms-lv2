#include <stdlib.h>
#include <stdio.h>

#include "lfo.h"
#include "lmms_math.h"
#include "oscillator.h"

// STATES
enum {
	LFO_OFF,        // Off (Needed?)
	LFO_DEL,        // Delay
	LFO_ATT,        // Attack
	LFO_SUS         // Sustain
};


sample_t
lfo_get_osc_sample (float wave_shape, float sample)
{
	switch ((int)(wave_shape)) {
	case LFO_WAVE_SINE:
		return osc_sample_sine(sample);
	case LFO_WAVE_TRIANGLE:
		return osc_sample_triangle(fraction(sample));
	case LFO_WAVE_SAW:
		return osc_sample_saw(fraction(sample));
	case LFO_WAVE_SQUARE:
		return osc_sample_square(fraction(sample));
	default:
		fprintf(stderr, "Oscillator: Invalid wave shape\n");
		return 0;
	}
}


// Advance LFO state to next non-zero-length section
static void
advance_state (LfoParams *p, LfoState *st)
{
	// TODO: Bring back expKnobVal
	switch (st->q) {
	case LFO_OFF:
		if (*p->del > 0.0f) {
			st->q       = LFO_DEL;
			st->nframes = p->time_base * /*expKnobVal*/(*p->del);
			st->frame   = 0;
			return;
		}
		// Fall-through
	case LFO_DEL:
		if (*p->att > 0.0f) {
			st->q       = LFO_ATT;
			st->nframes = p->time_base * /*expKnobVal*/(*p->att);
			st->frame   = 0;
			return;
		}
		// Fall-through
	case LFO_ATT:
		st->q       = LFO_SUS;
		st->nframes = 0;
		st->frame   = 0;
		return;
	}
}


Lfo *
lfo_create (LfoParams *p)
{
	Lfo *l  = malloc(sizeof(Lfo));
	if (l) {
		// External parameters 
		l->p = p;

		// Internal state
		l->st.q           = LFO_OFF;
		l->st.frame       = 0;
		l->st.nframes     = 0;
		l->st.phase       = 0.0f;
		return l;
	}
	return NULL;
}


void
lfo_destroy (Lfo *lfo)
{
	free(lfo);
}


void
lfo_trigger (Lfo *lfo)
{
	lfo->st.q = LFO_OFF;
	lfo->st.phase = 0.0f;
	advance_state(lfo->p, &lfo->st);
}


int
lfo_run (Lfo *lfo, float *samples, uint32_t nsamples)
{
	int i;
	float o;

	for (i=0; i<nsamples; ++i) {
		// Stupid way
		switch (lfo->st.q) {
		case LFO_DEL:
			// Process
			o = 0.0f;
			// State change
			lfo->st.frame++;
			if (lfo->st.frame > lfo->st.nframes) {
				advance_state(lfo->p, &lfo->st);
			}
			break;

		case LFO_ATT:
			// Process
			o = ((float)lfo->st.frame) / (lfo->st.nframes);
			// State
			lfo->st.frame++;
			if (lfo->st.frame >= lfo->st.nframes) {
				advance_state(lfo->p, &lfo->st);
			}
			break;

		case LFO_SUS:
			o = 1.0f; // Sustain Level;
			break;

		case LFO_OFF:
		default:
			o = 0.0f;
			break;
		}

		// Modulate with LFO-env with LFO-osc
		o *= lfo_get_osc_sample (*lfo->p->shape, lfo->st.phase);
		// Total LFO amount
		o *= *lfo->p->mod * 0.5f;
		// Update phase
		// TODO: See if we can yank the divide out (into timebase?)
		lfo->st.phase += 1.0f/(lfo->p->time_base * (*lfo->p->spd));

		// Operation (modulate vs mix)
		if (*lfo->p->op > 0.5) {
			samples[i] *= (0.5f + o);
		} else {
			samples[i] += o;
		}
	}

	// Return 1 if envelope is still active
	return lfo->st.q != LFO_OFF;
}
