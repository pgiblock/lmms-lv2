/*
 * Oscillator.cpp - implementation of powerful oscillator-class
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include <math.h>
#include <stdio.h>

#include "oscillator.h"

float blep_table[BLEPSIZE];
float blamp_table[BLEPSIZE];
float sine_table[WAVE_LEN];

void sine_init (float *tbl, int len);


// FIXME: refactor
sample_t osc_get_aa_sample_sine (Oscillator *o, float increment, float sync_offset);
sample_t osc_get_aa_sample_triangle (Oscillator *o, float increment, float sync_offset);
sample_t osc_get_aa_sample_saw (Oscillator *o, float increment, float sync_offset);
sample_t osc_get_aa_sample_square (Oscillator *o, float increment, float sync_offset);
sample_t osc_get_aa_sample_moog_saw (Oscillator *o, float increment, float sync_offset);
sample_t osc_get_aa_sample_exp (Oscillator *o, float increment, float sync_offset);


Oscillator *
osc_create ()
{
	Oscillator *o = (Oscillator *)malloc(sizeof(Oscillator));
	osc_reset(o, 0.f, 0.f, 440.f, 1.f, NULL, 0.f, 0.f);
	if (!o) {
		fprintf(stderr, "Could not allocate Oscillator.\n");
		return NULL;
	}

	return o;
}


void
osc_reset (Oscillator *o, float wave_shape, float modulation_algo,
           float freq, float volume,
           Oscillator *sub_osc, float phase_offset,
           float sample_rate)
{
	int i;

	// FIXME: Ideally share some config across oscillators in the same instrument
	o->wave_shape = wave_shape;
	o->modulation_algo = modulation_algo;
	o->freq = freq / sample_rate;
	o->volume = volume;
	o->ext_phase_offset = phase_offset;
	o->sub_osc = sub_osc;
	o->phase_offset = phase_offset;
	o->phase = phase_offset;
	o->sample_rate = sample_rate;

	// Singleton construction of BLEP/BLAMP tables
	if (!blep_table[0] && !blamp_table[0]) {
		blep_init(blep_table, blamp_table, BLEPSIZE);
	}

	// Single construction of Sine table
	if (!sine_table[0]) {
		sine_init(sine_table, WAVE_LEN);
	}

	// Init bleps for this oscillator
	for (i = 0; i < NROFBLEPS; ++i) {
		blep_state_init(o->bleps + i);
	}
	// Blep "phase tracking"
	o->blep_idx   = 0;
	o->phase_mod  = 0;
	o->last_phase = 0;
}


void
osc_destroy (Oscillator *o)
{
	free(o);
}


void
osc_print (Oscillator *o)
{
	fprintf(stderr, "{wave_shape=%f\n modulation_algo=%f\n freq=%f\n "
	        "volume=%f\n ext_phase_offset=%f\n sub_osc=%p\n "
	        "phase_offset=%f\n phase=%f\n sample_rate=%f\n}\n",
	        o->wave_shape, o->modulation_algo, o->freq,
	        o->volume, o->ext_phase_offset, o->sub_osc,
	        o->phase_offset, o->phase, o->sample_rate);
}


// should be called every time phase-offset is changed...
static inline void
osc_recalc_phase (Oscillator *o)
{
	if (o->phase_offset != o->ext_phase_offset) {
		o->phase        -= o->phase_offset;
		o->phase_offset  = o->ext_phase_offset;
		o->phase        += o->phase_offset;
	}
	// make sure we're not running negative when doing PM
	o->phase = absFraction(o->phase) + 2;
}


//// Non-antialiased update functions


// if we have no sub-osc, we can't do any modulation... just get our samples
static void
osc_update_no_sub (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


// do PM by using sub-osc as modulator
static void
osc_update_pm (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_update(o->sub_osc, buff, bend, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] = osc_get_sample(o, o->phase + buff[frame]) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


// do fm by using sub-osc as modulator
static void
osc_update_fm (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	const float srate_correction = 44100.0f / o->sample_rate;

	osc_update(o->sub_osc, buff, bend, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		o->phase += buff[frame] * srate_correction;
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


// do AM by using sub-osc as modulator
static void
osc_update_am (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_update(o->sub_osc, buff, bend, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] *= osc_get_sample(o, o->phase) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


// do mixing by using sub-osc as mix-sample
static void
osc_update_mix (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_update(o->sub_osc, buff, bend, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] += osc_get_sample(o, o->phase) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


static inline bool
osc_sync_ok (Oscillator *o, float osc_coeff)
{
	const float v1 = o->phase;
	o->phase += osc_coeff;

	// check whether m_phase is in next period
	return floorf(o->phase) > floorf(v1);
}


static float
osc_sync_init (Oscillator *o, sample_t *buff, sample_t *bend, const fpp_t len)
{
	if (o->sub_osc != NULL) {
		osc_update(o->sub_osc, buff, bend, len);
	}
	osc_recalc_phase(o);

	// FIXME: Do we need to multiply by bend somehow?
	return o->freq;
}


// sync with sub-osc (every time sub-osc starts new period, we also start new
// period)
static void
osc_update_sync (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	// FIXME: sub_osc_coeff is not correct.  Fix for bend!
	const float sub_osc_coeff = osc_sync_init(o->sub_osc, buff, bend, len);

	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		if (osc_sync_ok(o->sub_osc, sub_osc_coeff)) {
			o->phase = o->phase_offset;
		}
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		o->phase += o->freq * bend[frame];
	}
}


void
osc_update (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	if (o->freq >= o->sample_rate / 2) {
		return;
	}
	if (o->sub_osc != NULL) {
		switch ((int)o->modulation_algo) {
		case OSC_MOD_PM:
			osc_update_pm(o, buff, bend, len);
			break;
		case OSC_MOD_AM:
			osc_update_am(o, buff, bend, len);
			break;
		case OSC_MOD_MIX:
			osc_update_mix(o, buff, bend, len);
			break;
		case OSC_MOD_SYNC:
			osc_update_sync(o, buff, bend, len);
			break;
		case OSC_MOD_FM:
			osc_update_fm(o, buff, bend, len);
			break;
		default:
			fprintf(stderr, "Oscillator: Invalid modulation algorithm\n");
		}
	} else {
		osc_update_no_sub(o, buff, bend, len);
	}
}


sample_t
osc_get_sample (Oscillator *o, float sample)
{
	switch ((int)(o->wave_shape)) {
	case OSC_WAVE_SINE:
		return osc_sample_sine(fraction(sample));
	case OSC_WAVE_TRIANGLE:
		return osc_sample_triangle(fraction(sample));
	case OSC_WAVE_SAW:
		return osc_sample_saw(fraction(sample));
	case OSC_WAVE_SQUARE:
		return osc_sample_square(fraction(sample));
	case OSC_WAVE_MOOG:
		return osc_sample_moog_saw(fraction(sample));
	case OSC_WAVE_EXPONENTIAL:
		return osc_sample_exp(fraction(sample));
	case OSC_WAVE_NOISE:
		return osc_sample_noise(sample);
	default:
		fprintf(stderr, "Oscillator: Invalid wave shape\n");
		return 0;
	}
}


//// Antialiased update functions


// if we have no sub-osc, we can't do any modulation... just get our samples
static void
osc_aa_update_no_sub (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	o->phase_mod = 0.0f;

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] = osc_get_aa_sample(o, o->freq * bend[frame], -1.0f) * o->volume;
	}
}


// do PM by using sub-osc as modulator
static void
osc_aa_update_pm (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	float osc_coeff;

	osc_aa_update(o->sub_osc, buff, bend, len);

	for (fpp_t frame = 0; frame < len; ++frame) {
		//TODO: Huh? what is the 2.0f?
		osc_coeff    = o->freq * bend[frame];
		o->phase_mod = buff[frame] * osc_coeff / 2.0f;
		buff[frame]  = osc_get_aa_sample(o, osc_coeff, -1.0f) * o->volume;
	}
}


// do FM by using sub-osc as modulator
static void
osc_aa_update_fm (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	float osc_coeff;

	osc_aa_update(o->sub_osc, buff, bend, len);

	for (fpp_t frame = 0; frame < len; ++frame) {
		//TODO: Huh? what is the 2.0f?
		osc_coeff    = o->freq * bend[frame];
		o->phase_mod = buff[frame] * osc_coeff * 2.0f;
		buff[frame]  = osc_get_aa_sample(o, osc_coeff, -1.0f) * o->volume;
	}
}


// do AM by using sub-osc as modulator
static void
osc_aa_update_am (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_aa_update(o->sub_osc, buff, bend, len);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] *= osc_get_aa_sample(o, o->freq * bend[frame], -1.0f) * o->volume;
	}
}


// do mix by using sub-osc as mix-sample
static void
osc_aa_update_mix (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	osc_aa_update(o->sub_osc, buff, bend, len);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] += osc_get_aa_sample(o, o->freq * bend[frame], -1.0f) * o->volume;
	}
}


// sync with sub-osc (every time sub-osc starts new period, we also start new
// period)
static void
osc_aa_update_sync (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	// FIXME: sub_osc_coeff is not correct.  Fix for bend!
	const float sub_osc_coeff = osc_sync_init(o->sub_osc, buff, bend, len);
	float osc_coeff;

	o->phase_mod = 0.0f;

	for (fpp_t frame = 0; frame < len; ++frame) {
		osc_coeff = o->freq * bend[frame];
		if (osc_sync_ok(o->sub_osc, sub_osc_coeff)) {
			buff[frame] = osc_get_aa_sample(o, osc_coeff, o->phase_offset) * o->volume;
		} else {
			buff[frame] = osc_get_aa_sample(o, osc_coeff, -1.0f) * o->volume;
		}
	}
}


void
osc_aa_update (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len)
{
	// FIXME: Seems like this check is basically repeated in the sample code
	// (inc_limit)
	if (o->freq >= o->sample_rate / 2) {
		return;
	}
	if (o->sub_osc != NULL) {
		switch ((int)o->modulation_algo) {
		case OSC_MOD_PM:
			osc_aa_update_pm(o, buff, bend, len);
			break;
		case OSC_MOD_AM:
			osc_aa_update_am(o, buff, bend, len);
			break;
		case OSC_MOD_MIX:
			osc_aa_update_mix(o, buff, bend, len);
			break;
		case OSC_MOD_SYNC:
			osc_aa_update_sync(o, buff, bend, len);
			break;
		case OSC_MOD_FM:
			osc_aa_update_fm(o, buff, bend, len);
			break;
		default:
			fprintf(stderr, "Oscillator: Invalid modulation algorithm\n");
		}
	} else {
		osc_aa_update_no_sub(o, buff, bend, len);
	}
}


sample_t
osc_get_aa_sample_sine (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	float inc_limit = 8372.018089619f / o->sample_rate;
	float inc = (increment > inc_limit) ? inc_limit : increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;

	int i;

	// update phase
	if (sync_offset < 0.0f) {
		// Normal case, just increment phase
		o->last_phase = o->phase;
		o->phase      = o->phase + inc + o->phase_mod;
		o->phase      = fraction(o->phase);

		value = osc_sample_sine(o->phase);
	} else {
		// Syncing,
		o->last_phase = o->phase + inc*(1.0f - sync_offset);
		o->phase      = sync_offset;

		last_value = osc_sample_sine(o->last_phase);
		value      = osc_sample_sine(o->phase);
		// Get new values for correction.
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
	}


	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		// Round-robin assignment of BLEP pipelines
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = 0;  // NOTE: This was missing in the original
		o->blep_idx = (o->blep_idx+1) % NROFBLEPS;
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		// FIXME: When is ptr < 0?
		// TODO: Could start at blep_idx and search until we find an expired one
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = ( o->bleps[i].ptr + o->bleps[i].phs ) * BLEPLEN;
			value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}


sample_t
osc_get_aa_sample_triangle (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	const float inc_limit = 8372.018089619f / o->sample_rate;
	const float inc = ( increment > inc_limit )? inc_limit:increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;
	int   corr_typ   = 1;

	int i;

	// update phase
	if (sync_offset < 0.0f) {
		o->last_phase = o->phase;
		o->phase      = safe_fmodf(o->phase + inc + o->phase_mod);

		// TODO: Figure out where 16 and -16 came from.
		//       I can only come up with 8 = 4 (ramps per period) * 2 (amplitude range)
		//       Also, why is it not divided by (inc * o->phase_mod)?
		//       Finally, do we need to handle last_phase <= 0.25 && phase > 0.75?
		if (o->last_phase <= 0.25f && o->phase > 0.25f) {
			// Discontinuity at the peak
			corr_phs = (o->phase - 0.25f)/inc;
			corr_amp = +16*inc;
		} else if (o->last_phase <= 0.75f && o->phase > 0.75f) {
			// Discontinuity at the trough
			corr_phs = (o->phase - 0.75f)/inc;
			corr_amp = -16*inc;
		}

		value = osc_sample_triangle(o->phase);
	} else {
		// sync
		o->last_phase = o->phase + inc*(1.0f - sync_offset);
		o->phase      = sync_offset;

		last_value = osc_sample_triangle(o->last_phase);
		value      = osc_sample_triangle(o->phase);
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
		corr_typ   = false; // this needs a blep
	}

	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		// Round-robin assignment of BLEP pipelines
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = corr_typ;
		o->blep_idx = (o->blep_idx+1) % NROFBLEPS;
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		// FIXME: When is ptr < 0?
		// TODO: Could start at blep_idx and search until we find an expired one
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = ( o->bleps[i].ptr + o->bleps[i].phs ) * BLEPLEN;
			// FIXME: EASY to remove this if
			if (o->bleps[i].typ) {
				value += o->bleps[i].vol * blamp_table[ offset % BLEPSIZE ];
			} else {
				value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			}
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}


// FIXME: increment can be deduced from Oscillator o.
sample_t
osc_get_aa_sample_saw (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	float inc_limit = 8372.018089619f / o->sample_rate;
	float inc = (increment > inc_limit) ? inc_limit : increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;

	int i;

	// update phase
	if (sync_offset < 0.0f) {
		// Normal case, just increment phase
		o->last_phase = o->phase;
		o->phase      = o->phase + inc + o->phase_mod;

		// TODO: Would be nice to remove this conditional somehow
		if (o->phase >= 1.0f) {
			// sawtooth just fell down cliff
			o->phase = safe_fmodf(o->phase);
			corr_phs = o->phase / (inc + o->phase_mod);
			corr_amp = 2.0f;
		} else if (o->phase <  0.0f) {
			// sawtooth just went up cliff 
			o->phase = safe_fmodf(o->phase);
			corr_phs = (1.0f - o->phase)/(inc - o->phase_mod);
			corr_amp = -2.0f;
		}

		value = osc_sample_saw(o->phase);
	} else {
		// Syncing,
		o->last_phase = o->phase + inc*(1.0f - sync_offset);
		o->phase      = sync_offset;

		last_value = osc_sample_saw(o->last_phase);
		value      = osc_sample_saw(o->phase);
		// Get new values for correction.
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
		// corr_typ   = false; (Default behavior)
	}


	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		// Round-robin assignment of BLEP pipelines
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = 0;  // NOTE: This was missing in the original
		o->blep_idx = (o->blep_idx+1) % NROFBLEPS;
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		// FIXME: When is ptr < 0?
		// TODO: Could start at blep_idx and search until we find an expired one
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = ( o->bleps[i].ptr + o->bleps[i].phs ) * BLEPLEN;
			value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}


// TODO: constify parameters
sample_t
osc_get_aa_sample_square (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	const float inc_limit = 8372.018089619f / o->sample_rate;
	const float inc = ( increment > inc_limit )? inc_limit:increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;

	int i;

	// update phase
	if (sync_offset < 0.0f) {
		o->last_phase = o->phase;
		o->phase      = o->phase + inc + o->phase_mod;

		if (o->phase >= 1.0f) {
			// Rising edge
			o->phase = safe_fmodf(o->phase);
			corr_phs = o->phase / (inc + o->phase_mod);
			corr_amp = -2.0f;
		} else if (o->phase < 0.0f) {
			// Falling edge
			o->phase = safe_fmodf(o->phase);
			corr_phs = (1.0f - o->phase)/(inc - o->phase_mod);
			corr_amp = 2.0f;
		}

		// Middle
		// TODO: what about phase < 0.5 && last_phase >= 0.5 (reverse direction)? 
		if (o->phase > 0.5f && o->last_phase <= 0.5f) {
			corr_phs = (o->phase - 0.5f) / inc;
			corr_amp = 2.0f;
		}

		value = osc_sample_square(o->phase);
	} else {
		o->last_phase = o->phase;
		o->phase      = sync_offset;

		last_value = osc_sample_square(o->last_phase);
		value      = osc_sample_square(o->phase);
		// New values for correction
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
	}

	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = 0;  // NOTE: This was missing in the original
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = ( o->bleps[i].ptr + o->bleps[i].phs ) * BLEPLEN;
			value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}


sample_t
osc_get_aa_sample_moog_saw (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	float inc_limit = 8372.018089619f / o->sample_rate;
	float inc = ( increment > inc_limit )? inc_limit:increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;

	int i;

	/* 
	 * NOTE: Stefan's original code shifted the wave by half a phase.  This
	 * places the discontinuity at 0.0 and 1.0.  This algorithm didn't work
	 * for me immediately, so I am using a standard phase offset, but
	 * detecting the discontinuity at 0.5.  This seems to work.
	 *
	 * However, Now I wonder if we need to check at 0.0 and 1.0 and do the
	 * same code from triangle wave...
	 */

	// update phase
	if (sync_offset < 0.0f) {
		// Normal case, just increment phase
		o->last_phase = o->phase;
		o->phase      = safe_fmodf(o->phase + inc + o->phase_mod);

		// Middle (Falling edge)
		// TODO: what about phase < 0.5 && last_phase >= 0.5 (reverse direction)? 
		if (o->phase > 0.5f && o->last_phase <= 0.5f) {
			corr_phs = (o->phase - 0.5f) / inc;
			corr_amp = 1.0f;
		}

		value = osc_sample_moog_saw(o->phase);
	} else {
		// Syncing, 
		o->last_phase = o->phase + inc*(1.0f - sync_offset);
		o->phase      = sync_offset;

		// Stefan Original:
		last_value = osc_sample_moog_saw(o->last_phase);
		value      = osc_sample_moog_saw(o->phase);
		
		// Get new values for correction.
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
		// corr_typ   = false; (Default behavior)
	}

	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		// Round-robin assignment of BLEP pipelines
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = 0;  // NOTE: This was missing in the original
		o->blep_idx = (o->blep_idx+1) % NROFBLEPS;
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		// FIXME: When is ptr < 0?
		// TODO: Could start at blep_idx and search until we find an expired one
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = ( o->bleps[i].ptr + o->bleps[i].phs ) * BLEPLEN;
			value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}


sample_t
osc_get_aa_sample_exp (Oscillator *o, float increment, float sync_offset)
{
	// limit increment to C9 (higher does not make any musical sense and just
	// causes us a lot of trouble to correct this...)
	const float inc_limit = 8372.018089619f / o->sample_rate;
	const float inc = ( increment > inc_limit )? inc_limit:increment;

	// initialize current result with 0.0
	float value      = 0.0f;
	float last_value = 0.0f;
	float corr_amp   = 0.0f;
	float corr_phs   = 0.0f;
	int   corr_typ   = 1;

	int i;

	// update phase
	if (sync_offset < 0.0f) {
		o->last_phase = o->phase;
		o->phase      = safe_fmodf(o->phase + inc + o->phase_mod);

		// TODO: Figure out where 16 came from.
		if (o->phase > 0.5f && o->last_phase <= 0.5f) {
			corr_phs = (o->phase - 0.5f) / inc;
			corr_amp = 16.0f * inc;
		}

		value = osc_sample_exp(o->phase);
	} else {
		// sync
		o->last_phase = o->phase + inc*(1.0f - sync_offset);
		o->phase      = sync_offset;

		last_value = osc_sample_exp(o->last_phase);
		value      = osc_sample_exp(o->phase);
		corr_phs   = o->phase / inc;
		corr_amp   = last_value - value;
		corr_typ   = false; // this needs a blep
	}

	if (fabsf(corr_amp) > 0.00001f) { // need to correct this samples?
		// Round-robin assignment of BLEP pipelines
		o->bleps[o->blep_idx].ptr = 0;  // reset table-pointer to activate it
		o->bleps[o->blep_idx].vol = corr_amp;
		o->bleps[o->blep_idx].phs = corr_phs;
		o->bleps[o->blep_idx].typ = corr_typ;
		o->blep_idx = (o->blep_idx+1) % NROFBLEPS;
	}

	// now go through all pipelines, check if active and process if so...
	for (i = 0; i < NROFBLEPS; ++i) {
		// FIXME: When is ptr < 0?
		// TODO: Could start at blep_idx and search until we find an expired one
		if (o->bleps[i].ptr >= 0 && o->bleps[i].ptr < 8) {
			const int offset = (o->bleps[i].ptr + o->bleps[i].phs) * BLEPLEN;
			// FIXME: EASY to remove this if
			if (o->bleps[i].typ) {
				value += o->bleps[i].vol * blamp_table[ offset % BLEPSIZE ];
			} else {
				value += o->bleps[i].vol * blep_table[ offset % BLEPSIZE ];
			}
			o->bleps[i].ptr++;
		}
	}

	// puuhh... finished here... ;-)
	return value;
}



sample_t
osc_get_aa_sample (Oscillator *o, float increment, float sync_offset)
{
	switch ((int)(o->wave_shape)) {
	case OSC_WAVE_SINE:
		return osc_get_aa_sample_sine(o, increment, sync_offset);
	case OSC_WAVE_TRIANGLE:
		return osc_get_aa_sample_triangle(o, increment, sync_offset);
	case OSC_WAVE_SAW:
		return osc_get_aa_sample_saw(o, increment, sync_offset);
	case OSC_WAVE_SQUARE:
		return osc_get_aa_sample_square(o, increment, sync_offset);
	case OSC_WAVE_MOOG:
		return osc_get_aa_sample_moog_saw(o, increment, sync_offset);
	case OSC_WAVE_EXPONENTIAL:
		return osc_get_aa_sample_exp(o, increment, sync_offset);
	case OSC_WAVE_NOISE:
		return 0.0f;
	default:
		fprintf(stderr, "Oscillator: Invalid wave shape\n");
		return 0;
	}
}



void
sine_init (float *tbl, int len)
{
	int i;
	// We want an extra sample at the end for interpolation
	float scale = 1.0f / (len - 1);
	for (i=0; i<len; ++i) {
		tbl[i] = sinf(M_2PI * scale * i);
	}
}

