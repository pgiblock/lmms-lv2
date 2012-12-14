/*
 * Oscillator.h - declaration of class Oscillator
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

#ifndef OSCILLATOR_H__
#define OSCILLATOR_H__

#include <stdlib.h>

#include "blep.h"
#include "lmms_lv2.h"
#include "lmms_math.h"

#define BLEPSIZE 8192
#define BLEPLEN  (BLEPSIZE/8)
#define NROFBLEPS 8

enum WaveShapes {
	OSC_WAVE_SINE,
	OSC_WAVE_TRIANGLE,
	OSC_WAVE_SAW,
	OSC_WAVE_SQUARE,
	OSC_WAVE_MOOG,
	OSC_WAVE_EXPONENTIAL,
	OSC_WAVE_NOISE
} ;


enum ModulationAlgos {
	OSC_MOD_PM,
	OSC_MOD_AM,
	OSC_MOD_MIX,
	OSC_MOD_SYNC,
	OSC_MOD_FM
};


struct Oscillator_st {
	// Shared by all oscillators of the group
	float wave_shape;
	float modulation_algo;
	// This oscillator's config
	float freq;
	float detuning;
	float volume;
	float ext_phase_offset;
	// State
	struct Oscillator_st* sub_osc;
	float phase_offset;
	float phase;
	
	// Experimental MINBLEP stuff
	BlepState bleps[NROFBLEPS];
	int   blep_idx;
	float phase_mod;
	float last_phase;

	// TODO: const sampleBuffer * m_userWave;
	float sample_rate;
};
typedef struct Oscillator_st Oscillator;


// Public interface

Oscillator* osc_create();

void osc_reset(Oscillator* osc,
		float wave_shape, float modulation_algo,
		float freq, float detuning, float volume,
		Oscillator* sub_osc, float phase_offset,
		float sample_rate);

void osc_destroy(Oscillator* o);

void osc_update(Oscillator* o, sample_t* buff, fpp_t len);
sample_t osc_get_sample(Oscillator* o, float sample);

void osc_print(Oscillator* o);

// Waveform sample routines

static inline sample_t
osc_sample_sin(const float _sample) {
	return sinf( _sample * M_PI * 2.0f );
}

static inline sample_t
osc_sample_triangle(const float _sample) {
	const float ph = fraction( _sample );
	if( ph <= 0.25f )
	{
		return ph * 4.0f;
	}
	else if( ph <= 0.75f )
	{
		return 2.0f - ph * 4.0f;
	}
	return ph * 4.0f - 4.0f;
}

static inline sample_t
osc_sample_saw(const float _sample) {
	return -1.0f + fraction( _sample ) * 2.0f;
}

static inline sample_t
osc_sample_square(const float _sample) {
	return ( fraction( _sample ) > 0.5f ) ? -1.0f : 1.0f;
}

static inline sample_t
osc_sample_moog_saw(const float _sample) {
	const float ph = fraction( _sample );
	if( ph < 0.5f )
	{
		return -1.0f + ph * 4.0f;
	}
	return 1.0f - 2.0f * ph;
}

static inline sample_t
osc_sample_exp(const float _sample) {
	float ph = fraction( _sample );
	if( ph > 0.5f )
	{
		ph = 1.0f - ph;
	}
	return -1.0f + 8.0f * ph * ph;
}

static inline sample_t
osc_sample_noise(const float _sample) {
	// Precise implementation
	// return 1.0f - rand() * 2.0f / RAND_MAX;

	// Fast implementation
	return 1.0f - fast_rand() * 2.0f / FAST_RAND_MAX;
}

#endif
