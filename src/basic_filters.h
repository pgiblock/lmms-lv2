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

enum FilterTypes {
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
};


struct Filter_st {
	// TODO: Collapse all these coeffs into a union or something
	
	// filter coeffs
	float b0a0, b1a0, b2a0, a1a0, a2a0;

	// coeffs for moog-filter
	float r, p, k;

	// coeffs for RC-type-filters
	float rca, rcb, rcc, rcq;

	// coeffs for formant-filters
	float vfa[4], vfb[4], vfc[4], vfq;
	
	// in/out history
	frame ou1, ou2, in1, in2;

	// in/out history for moog-filter
	frame y1, y2, y3, y4, oldx, oldy1, oldy2, oldy3;
	
	// in/out history for RC-type-filters
	frame rcbp0, rclp0, rchp0, rclast0;
	frame rcbp1, rclp1, rchp1, rclast1;

	// in/out history for Formant-filters
	frame vfbp[6], vflp[6], vfhp[6], vflast[6];
	
	enum FilterTypes type;

	float sample_rate;
};
typedef struct Filter_st Filter;


// Public interface

Filter  *filter_create (float sample_rate);
void     filter_reset (Filter *f, float sample_rate);
void     filter_calc_coeffs (Filter *f, float freq, float q);
sample_t filter_get_sample (Filter *f, sample_t in, int chnl);
void     filter_destroy (Filter *f);

#endif
