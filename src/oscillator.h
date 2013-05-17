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

// Wave-table lookup configuration
#define OSC_WAVE_BITS  10
#define OSC_WAVE_LEN   ((1 << OSC_WAVE_BITS) + 1)
#define OSC_FRAC_BITS  (32 - OSC_WAVE_BITS)
#define OSC_FRAC_MASK  ((1 << OSC_FRAC_BITS) - 1)
#define OSC_FRAC_SCALE (1.0 / (1 << OSC_FRAC_BITS))

// BLEP-table lookup configuration
#define OSC_BLEP_SIZE 8192
#define OSC_BLEP_LEN  (OSC_BLEP_SIZE/8)
#define OSC_NBLEPS 8

typedef enum wave_shapes {
	OSC_WAVE_SINE,
	OSC_WAVE_TRIANGLE,
	OSC_WAVE_SAW,
	OSC_WAVE_SQUARE,
	OSC_WAVE_MOOG,
	OSC_WAVE_EXPONENTIAL,
	OSC_WAVE_NOISE
} WaveShapes;


typedef enum modulation_algos {
	OSC_MOD_PM,
	OSC_MOD_AM,
	OSC_MOD_MIX,
	OSC_MOD_SYNC,
	OSC_MOD_FM
} ModulationAlgos;


typedef struct oscillator {
	//// PARAMS:

	// Shared by all oscillators of the group
	float wave_shape;
	float modulation_algo;
	// This oscillator's config
	float freq;
	float volume;
	float ext_phase_offset;

	struct oscillator *sub_osc;

	//// STATE

	float phase_offset;
	float phase;

	// Experimental MINBLEP stuff
	BlepState bleps[OSC_NBLEPS];
	int   blep_idx;
	float phase_mod;
	float last_phase;

	//// HMM????

	// TODO: const sampleBuffer * m_userWave;
	float sample_rate;
} Oscillator;


// Public interface

Oscillator *osc_create();

void osc_reset (Oscillator *osc,
                float wave_shape, float modulation_algo,
                float freq, float volume,
                Oscillator *sub_osc, float phase_offset,
                float sample_rate);

void osc_destroy (Oscillator *o);

// Original synthesis functions
void osc_update (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len);
sample_t osc_get_sample (Oscillator *o, float sample);

// Antialiased synthesis functions
void osc_aa_update (Oscillator *o, sample_t *buff, sample_t *bend, fpp_t len);
sample_t osc_get_aa_sample (Oscillator *o, float increment, float sync_offset);

void osc_print (Oscillator *o);

// Waveform sample routines

static inline sample_t
osc_sample_sine (const float ph)
{
	extern float sine_table[OSC_WAVE_LEN];
	uint32_t phase = ph * 4294967296.0;
	int idx = phase >> OSC_FRAC_BITS;

	// Linearly interpolate the two nearest samples
	float samp0 = sine_table[idx];
	float samp1 = sine_table[idx+1];
	return samp0 + (phase & OSC_FRAC_MASK) * OSC_FRAC_SCALE * (samp1-samp0); 
}

static inline sample_t
osc_sample_triangle (const float ph)
{
	if (ph <= 0.25f) {
		return ph * 4.0f;
	} else if (ph <= 0.75f) {
		return 2.0f - ph * 4.0f;
	} else {
		return ph * 4.0f - 4.0f;
	}
}

static inline sample_t
osc_sample_saw (const float ph)
{
	return -1.0f + ph * 2.0f;
}

static inline sample_t
osc_sample_square (const float ph)
{
	return (ph > 0.5f) ? -1.0f : 1.0f;
}

static inline sample_t
osc_sample_moog_saw (const float ph)
{
	if (ph < 0.5f) {
		return -1.0f + ph * 4.0f;
	} else {
		return 1.0f - 2.0f * ph;
	}
}

static inline sample_t
osc_sample_exp (const float ph)
{
	if (ph > 0.5f) {
		return -1.0f + 8.0f * (1.0f - ph) * (1.0f - ph);
	} else {
		return -1.0f + 8.0f * ph * ph;
	}
}

static inline sample_t
osc_sample_noise (const float sample)
{
	// Precise implementation
	// return 1.0f - rand() * 2.0f / RAND_MAX;

	// Fast implementation
	return 1.0f - fast_rand() * 2.0f / FAST_RAND_MAX;
}

#endif
