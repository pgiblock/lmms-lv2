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

Oscillator*
osc_create() {
	Oscillator* o = (Oscillator*)malloc(sizeof(Oscillator));
	osc_reset(o, 0.f, 0.f, 404.f, 1.f, 1.f, NULL, 0.f, 0.f);
	if (!o) {
		fprintf(stderr, "Could not allocate Oscillator.\n");
		return NULL;
	}

	return o;
}

void
osc_reset(Oscillator* o, float wave_shape, float modulation_algo,
           float freq, float detuning, float volume,
           Oscillator* sub_osc, float phase_offset,
           float sample_rate) {

	// FIXME: Ideally share some config across oscillators in the same instrument
	o->wave_shape = wave_shape;
	o->modulation_algo = modulation_algo;
	o->freq = freq;
	o->detuning = detuning;
	o->volume = volume;
	o->ext_phase_offset = phase_offset;
	o->sub_osc = sub_osc;
	o->phase_offset = phase_offset;
	o->phase = phase_offset;
	o->sample_rate = sample_rate;
}


void
osc_destroy(Oscillator* o) {
	free(o);
}

void
osc_print(Oscillator* o) {
	fprintf(stderr, "{wave_shape=%f\n modulation_algo=%f\n freq=%f\n "
	        "detuning=%f\n volume=%f\n ext_phase_offset=%f\n sub_osc=%lx\n "
	        "phase_offset=%f\n phase=%f\n sample_rate=%f\n}\n",
	        o->wave_shape, o->modulation_algo, o->freq,
	        o->detuning, o->volume, o->ext_phase_offset, o->sub_osc,
	        o->phase_offset, o->phase, o->sample_rate);
}


// should be called every time phase-offset is changed...
static inline void
osc_recalc_phase(Oscillator* o) {
	if (o->phase_offset != o->ext_phase_offset) {
		o->phase        -= o->phase_offset;
		o->phase_offset  = o->ext_phase_offset;
		o->phase        += o->phase_offset;
	}
	// make sure we're not running negative when doing PM
	o->phase = absFraction(o->phase) + 2;
}


static inline bool
osc_sync_ok(Oscillator* o, float _osc_coeff) {
	const float v1 = o->phase;
	o->phase += _osc_coeff;

	// check whether m_phase is in next period
	return( floorf( o->phase ) > floorf( v1 ) );
}


static float
osc_sync_init(Oscillator* o, sample_t* buff, const fpp_t len) {
	if (o->sub_osc != NULL) {
		osc_update(o->sub_osc, buff, len);
	}
	osc_recalc_phase(o);
	return( o->freq * o->detuning );
}



// if we have no sub-osc, we can't do any modulation... just get our samples
void osc_update_no_sub(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;

	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		//printf("b[f]=%f, ph=%f, vol=%f, coef=%f\n", buff[frame], o->phase, o->volume, osc_coeff);
		o->phase += osc_coeff;
	}
}


// do pm by using sub-osc as modulator
void osc_update_pm(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;

	osc_update(o->sub_osc, buff, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] = osc_get_sample(o, o->phase + buff[frame]) * o->volume;
		o->phase += osc_coeff;
	}
}


// do am by using sub-osc as modulator
void osc_update_am(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;

	osc_update(o->sub_osc, buff, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] *= osc_get_sample(o, o->phase) * o->volume;
		o->phase += osc_coeff;
	}
}


// do mix by using sub-osc as mix-sample
void osc_update_mix(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;

	osc_update(o->sub_osc, buff, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		buff[frame] += osc_get_sample(o, o->phase) * o->volume;
		o->phase += osc_coeff;
	}
}


// sync with sub-osc (every time sub-osc starts new period, we also start new
// period)
void osc_update_sync(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;
	const float sub_osc_coeff = osc_sync_init(o->sub_osc, buff, len);

	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		if (osc_sync_ok(o->sub_osc, sub_osc_coeff)) {
			o->phase = o->phase_offset;
		}
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		o->phase += osc_coeff;
	}
}


// do fm by using sub-osc as modulator
void osc_update_fm(Oscillator* o, sample_t* buff, fpp_t len) {
	const float osc_coeff = o->freq * o->detuning;
	const float srate_correction = 44100.0f / o->sample_rate;

	osc_update(o->sub_osc, buff, len);
	osc_recalc_phase(o);

	for (fpp_t frame = 0; frame < len; ++frame) {
		o->phase += buff[frame] * srate_correction;
		buff[frame] = osc_get_sample(o, o->phase) * o->volume;
		o->phase += osc_coeff;
	}
}


void
osc_update(Oscillator* o, sample_t* buff, fpp_t len) {
	if (o->freq >= o->sample_rate / 2) {
		return;
	}
	if (o->sub_osc != NULL) {
		switch ((int)o->modulation_algo) {
			case OSC_MOD_PM:
				osc_update_pm(o, buff, len);
				break;
			case OSC_MOD_AM:
				osc_update_am(o, buff, len);
				break;
			case OSC_MOD_MIX:
				osc_update_mix(o, buff, len);
				break;
			case OSC_MOD_SYNC:
				osc_update_sync(o, buff, len);
				break;
			case OSC_MOD_FM:
				osc_update_fm(o, buff, len);
				break;
			default:
				fprintf(stderr, "Oscillator: Invalid modulation algorithm\n");
		}
	} else {
		osc_update_no_sub(o, buff, len);
	}
}


sample_t
osc_get_sample(Oscillator* o, float sample) {
	switch ((int)(o->wave_shape)) {
		case OSC_WAVE_SINE:
			return osc_sample_sin(sample);
		case OSC_WAVE_TRIANGLE:
			return osc_sample_triangle(sample);
		case OSC_WAVE_SAW:
			return osc_sample_saw(sample);
		case OSC_WAVE_SQUARE:
			return osc_sample_square(sample);
		case OSC_WAVE_MOOG:
			return osc_sample_moog_saw(sample);
		case OSC_WAVE_EXPONENTIAL:
			return osc_sample_exp(sample);
		case OSC_WAVE_NOISE:
			return osc_sample_noise(sample);
		default:
			fprintf(stderr, "Oscillator: Invalid wave shape\n");
	}
}
