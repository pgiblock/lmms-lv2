/*
 * basic_filters.h - simple but powerful filters
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2012      Paul Giblock    <p/at/pgiblock.net>
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

#ifndef BASIC_FILTERS_H__
#define BASIC_FILTERS_H__

#include <stdlib.h>

#include "lmms_lv2.h"

// TODO: rework "Channels"
#define CHANNELS (2)
typedef sample_t frame[CHANNELS];

typedef enum filter_types {
	FILTER_LOWPASS,
	FILTER_HIPASS,
	FILTER_BANDPASS_CSG,
	FILTER_BANDPASS_CZPG,
	FILTER_NOTCH,
	FILTER_ALLPASS,
	FILTER_MOOG,
	FILTER_DOUBLELOWPASS,
	FILTER_LOWPASS_RC12,
	FILTER_BANDPASS_RC12,
	FILTER_HIGHPASS_RC12,
	FILTER_LOWPASS_RC24,
	FILTER_BANDPASS_RC24,
	FILTER_HIGHPASS_RC24,
	FILTER_FORMANTFILTER
} FilterTypes;

// Struct soup: define structs for each filter type to reduce memory usage

// coefficients for basic filters
struct filter_basic_coeffs {
	float b0a0, b1a0, b2a0, a1a0, a2a0;
};

// coefficients for moog-filter
struct filter_moog_coeffs {
	float r, p, k;
};

// coefficents for RC-type-filters
struct filter_rc_coeffs {
	float a, b, c, q;
};

// coefficients for formant-filters
struct filter_formant_coeffs {
	float a[4], b[4], c[4], q;
};

// in/out history
struct filter_basic_state {
	frame ou1, ou2, in1, in2;
	// extra state for double-mode
	frame din1, din2;
};

// in/out history for moog-filter
struct filter_moog_state {
	frame y1, y2, y3, y4;
    frame oldx, oldy1, oldy2, oldy3;
};

// in/out history for RC-type-filters
struct filter_rc_state {
	frame bp0, lp0, hp0, last0;
	frame bp1, lp1, hp1, last1;
};

// in/out history for Formant-filters
struct filter_formant_state {
	frame bp[6], lp[6], hp[6], last[6];
};

typedef struct filter {
	// All coefficients
	union {
		struct filter_basic_coeffs b;
		struct filter_moog_coeffs m;
		struct filter_rc_coeffs r;
		struct filter_formant_coeffs f;
	} c;

	// All state
	union {
		struct filter_basic_state b;
		struct filter_moog_state m;
		struct filter_rc_state r;
		struct filter_formant_state f;
	} st;
	
	// What type of filter are we?
	FilterTypes type;

	float sample_rate;
} Filter;


// Public interface

Filter  *filter_create (float sample_rate);
void     filter_reset (Filter *f, float sample_rate);
void     filter_calc_coeffs (Filter *f, float freq, float q);
sample_t filter_get_sample (Filter *f, sample_t in, int chnl);
void     filter_destroy (Filter *f);

#endif
