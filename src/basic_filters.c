/*
 * basic_filters.c - simple but powerful filters
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "basic_filters.h"
#include "lmms_math.h"

#define MIN_FREQ (0.01f)
#define MIN_Q    (0.01f)

static void filter_clear_history(Filter* f);


Filter*
filter_create(float sample_rate) {
	Filter *f = malloc(sizeof(Filter));
	filter_reset(f, sample_rate);
	if (!f) {
		fprintf(stderr, "Could not allocate Filter.\n");
	}

	return f;
}


void
filter_reset(Filter *f, float sample_rate) {
	f->sample_rate = sample_rate;
	filter_clear_history(f);
}


static void
filter_clear_history(Filter* f) {
	int c;

	// reset in/out history
	for (c = 0; c < CHANNELS; ++c) {
		// reset in/out history for simple filters
		f->ou1[c] = f->ou2[c] = f->in1[c] =
			f->in2[c] = 0.0f;

		// reset in/out history for moog-filter
		f->y1[c] = f->y2[c] = f->y3[c] = f->y4[c] =
			f->oldx[c] = f->oldy1[c] =
			f->oldy2[c] = f->oldy3[c] = 0.0f;

		// reset in/out history for RC-filters
		f->rclp0[c] = f->rcbp0[c] = f->rchp0[c] = f->rclast0[c] = 0.0f;
		f->rclp1[c] = f->rcbp1[c] = f->rchp1[c] = f->rclast1[c] = 0.0f;

		for(int i=0; i<6; i++) {
			f->vflp[i][c] = f->vfbp[i][c] = f->vfhp[i][c] = f->vflast[i][c] = 0.0f;
		}
	}
}


static inline sample_t
filter_get_sample_moog(Filter* f , sample_t in, int chnl) {
	sample_t x = in - f->r * f->y4[chnl];

	// four cascaded onepole filters
	// (bilinear transform)
	f->y1[chnl] = tLimit(
			( x + f->oldx[chnl] ) * f->p
			- f->k * f->y1[chnl],
			-10.0f, 10.0f );
	f->y2[chnl] = tLimit(
			( f->y1[chnl] + f->oldy1[chnl] ) * f->p
			- f->k * f->y2[chnl],
			-10.0f, 10.0f );
	f->y3[chnl] = tLimit(
			( f->y2[chnl] + f->oldy2[chnl] ) * f->p
			- f->k * f->y3[chnl],
			-10.0f, 10.0f );
	f->y4[chnl] = tLimit(
			( f->y3[chnl] + f->oldy3[chnl] ) * f->p
			- f->k * f->y4[chnl],
			-10.0f, 10.0f );

	f->oldx[chnl] = x;
	f->oldy1[chnl] = f->y1[chnl];
	f->oldy2[chnl] = f->y2[chnl];
	f->oldy3[chnl] = f->y3[chnl];
	return f->y4[chnl] - f->y4[chnl] * f->y4[chnl] *
			f->y4[chnl] * ( 1.0f / 6.0f );
}


// 4-times oversampled simulation of an active RC-Bandpass,-Lowpass,-Highpass-
// Filter-Network as it was used in nearly all modern analog synthesizers. This
// can be driven up to self-oscillation (BTW: do not remove the limits!!!).
// (C) 1998 ... 2009 S.Fendt. Released under the GPL v2.0  or any later version.
static inline void
filter_get_sample_rc12(Filter* f , sample_t in0, int chnl) {
	sample_t lp, hp, bp;

	sample_t in;

	// 4-times oversampled... (even the moog-filter would benefit from this)
	for (int n = 4; n != 0; --n) {
		in = in0 + f->rcbp0[chnl] * f->rcq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->rcb + f->rclp0[chnl] * f->rca;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->rcc * ( f->rchp0[chnl] + in - f->rclast0[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->rcb + f->rcbp0[chnl] * f->rca;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->rclast0[chnl] = in;
		f->rclp0[chnl] = lp;
		f->rchp0[chnl] = hp;
		f->rcbp0[chnl] = bp;
	}
}


static inline void
filter_get_sample_rc24(Filter* f, sample_t in0, int chnl) {
	sample_t lp, hp, bp;

	sample_t in;

	for(int n = 4; n != 0; --n)
	{
		// first stage is as for the 12dB case...
		in = in0 + f->rcbp0[chnl] * f->rcq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->rcb + f->rclp0[chnl] * f->rca;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->rcc * ( f->rchp0[chnl] + in - f->rclast0[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->rcb + f->rcbp0[chnl] * f->rca;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->rclast0[chnl] = in;
		f->rclp0[chnl] = lp;
		f->rchp0[chnl] = hp;
		f->rcbp0[chnl] = bp;

		// second stage gets the output of the first stage as input...
		if( f->type == FILTER_LOWPASS_RC24 ) {
			in = lp + f->rcbp1[chnl] * f->rcq;
		} else if( f->type == FILTER_BANDPASS_RC24 ) {
			in = bp + f->rcbp1[chnl] * f->rcq;
		} else {
			in = hp + f->rcbp1[chnl] * f->rcq;
		}

		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->rcb + f->rclp1[chnl] * f->rca;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->rcc * ( f->rchp1[chnl] + in - f->rclast1[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->rcb + f->rcbp1[chnl] * f->rca;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->rclast1[chnl] = in;
		f->rclp1[chnl] = lp;
		f->rchp1[chnl] = hp;
		f->rcbp1[chnl] = bp;
	}
}

static inline sample_t
filter_get_sample_formant(Filter* f, sample_t in0, int chnl) {
	sample_t lp, hp, bp, in;
	int out = 0, o;

	for(o=0; o<4; o++) {
		// first formant
		in = in0 + f->vfbp[0][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[0] + f->vflp[0][chnl] * f->vfa[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[0] * ( f->vfhp[0][chnl] + in - f->vflast[0][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[0] + f->vfbp[0][chnl] * f->vfa[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[0][chnl] = in;
		f->vflp[0][chnl] = lp;
		f->vfhp[0][chnl] = hp;
		f->vfbp[0][chnl] = bp;

		in = bp + f->vfbp[2][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[0] + f->vflp[2][chnl] * f->vfa[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[0] * ( f->vfhp[2][chnl] + in - f->vflast[2][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[0] + f->vfbp[2][chnl] * f->vfa[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[2][chnl] = in;
		f->vflp[2][chnl] = lp;
		f->vfhp[2][chnl] = hp;
		f->vfbp[2][chnl] = bp;  

		in = bp + f->vfbp[4][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[0] + f->vflp[4][chnl] * f->vfa[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[0] * ( f->vfhp[4][chnl] + in - f->vflast[4][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[0] + f->vfbp[4][chnl] * f->vfa[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[4][chnl] = in;
		f->vflp[4][chnl] = lp;
		f->vfhp[4][chnl] = hp;
		f->vfbp[4][chnl] = bp;  

		out += bp;

		// second formant
		in = in0 + f->vfbp[0][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[1] + f->vflp[1][chnl] * f->vfa[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[1] * ( f->vfhp[1][chnl] + in - f->vflast[1][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[1] + f->vfbp[1][chnl] * f->vfa[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[1][chnl] = in;
		f->vflp[1][chnl] = lp;
		f->vfhp[1][chnl] = hp;
		f->vfbp[1][chnl] = bp;

		in = bp + f->vfbp[3][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[1] + f->vflp[3][chnl] * f->vfa[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[1] * ( f->vfhp[3][chnl] + in - f->vflast[3][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[1] + f->vfbp[3][chnl] * f->vfa[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[3][chnl] = in;
		f->vflp[3][chnl] = lp;
		f->vfhp[3][chnl] = hp;
		f->vfbp[3][chnl] = bp;  

		in = bp + f->vfbp[5][chnl] * f->vfq;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * f->vfb[1] + f->vflp[5][chnl] * f->vfa[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = f->vfc[1] * ( f->vfhp[5][chnl] + in - f->vflast[5][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * f->vfb[1] + f->vfbp[5][chnl] * f->vfa[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		f->vflast[5][chnl] = in;
		f->vflp[5][chnl] = lp;
		f->vfhp[5][chnl] = hp;
		f->vfbp[5][chnl] = bp;  

		out += bp;
	}

	return out/2.0f;
}


sample_t
filter_get_sample(Filter* f, sample_t in, int chnl) {
	sample_t out;
	switch( f->type ) {
		case FILTER_MOOG:
			out = filter_get_sample_moog(f, in, chnl);
			break;

		case FILTER_LOWPASS_RC12:
			filter_get_sample_rc12(f, in, chnl);
			return f->rclp0[chnl];

		case FILTER_BANDPASS_RC12:
			filter_get_sample_rc12(f, in, chnl);
			return f->rcbp0[chnl];

		case FILTER_HIGHPASS_RC12:
			filter_get_sample_rc12(f, in, chnl);
			return f->rchp0[chnl];

		case FILTER_LOWPASS_RC24:
			filter_get_sample_rc24(f, in, chnl);
			return f->rclp1[chnl];

		case FILTER_BANDPASS_RC24:
			filter_get_sample_rc24(f, in, chnl);
			return f->rcbp1[chnl];

		case FILTER_HIGHPASS_RC24:
			filter_get_sample_rc24(f, in, chnl);
			return f->rchp1[chnl];

		case FILTER_FORMANTFILTER:
			return filter_get_sample_formant(f, in, chnl);

		default:
			// filter
			out = f->b0a0*in +
				f->b1a0*f->in1[chnl] +
				f->b2a0*f->in2[chnl] -
				f->a1a0*f->ou1[chnl] -
				f->a2a0*f->ou2[chnl];

			// push in/out buffers
			f->in2[chnl] = f->in1[chnl];
			f->in1[chnl] = in;
			f->ou2[chnl] = f->ou1[chnl];

			f->ou1[chnl] = out;
			break;
	}

	/* TODO:
	if( f->doubleFilter ) {
		return f->subFilter->update( out, chnl );
	} */

	// Clipper band limited sigmoid
	return out;
}


void
filter_calc_coeffs(Filter* f, float freq, float q) {
	// temp coef vars
	// limit freq and q for not getting bad noise out of the filter...
	freq = qMax(freq, MIN_FREQ);
	q    = qMax(q, MIN_Q);

	if( f->type == FILTER_LOWPASS_RC12  ||
			f->type == FILTER_BANDPASS_RC12 ||
			f->type == FILTER_HIGHPASS_RC12 ||
			f->type == FILTER_LOWPASS_RC24  ||
			f->type == FILTER_BANDPASS_RC24 ||
			f->type == FILTER_HIGHPASS_RC24 ) {

		if( freq < 50.f ) {
			freq = 50.f;
		}

		f->rca = 1.0f - (1.0f/(f->sample_rate*4)) / ( (1.0f/(freq*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );
		f->rcb = 1.0f - f->rca;
		f->rcc = (1.0f/(freq*2.0f*M_PI)) / ( (1.0f/(freq*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );

		// Stretch Q/resonance, as self-oscillation reliably starts at a q of ~2.5 - ~2.6
		f->rcq = q/4.f;
	}
	else if( f->type == FILTER_FORMANTFILTER ) {
		// formats for a, e, i, o, u, a
		const float formants[5][2] = {
			{ 1000, 1400 },
			{ 500, 2300 },
			{ 320, 3200 },
			{ 500, 1000 },
			{ 320, 800 } };

		// Stretch Q/resonance
		f->vfq = q/4.f;

		// frequency in lmms ranges from 1Hz to 14000Hz
		const int   vowel = (int)( floor( freq/14000.f * 4.f ) );
		const float fract = ( freq/14000.f * 4.f ) - (float)vowel;

		// interpolate between formant frequencies         
		const float f0 = formants[vowel+0][0] * (1.0f - fract) + 
		                 formants[vowel+1][0] * (fract);

		const float f1 = formants[vowel+0][1] * (1.0f - fract) +
		                 formants[vowel+1][1] * (fract);

		f->vfa[0] = 1.0f - (1.0f/(f->sample_rate*4)) /
			( (1.0f/(f0*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );
		f->vfb[0] = 1.0f - f->vfa[0];
		f->vfc[0] = (1.0f/(f0*2.0f*M_PI)) /
			( (1.0f/(f0*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );

		f->vfa[1] = 1.0f - (1.0f/(f->sample_rate*4)) /
			( (1.0f/(f1*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );
		f->vfb[1] = 1.0f - f->vfa[1];
		f->vfc[1] = (1.0f/(f1*2.0f*M_PI)) /
			( (1.0f/(f1*2.0f*M_PI)) + (1.0f/(f->sample_rate*4)) );
	}

	if( f->type == FILTER_MOOG )
	{
		// [ 0 - 0.5 ]
		const float fr = freq / f->sample_rate;
		// (Empirical tuning)
		f->p = (3.6f - 3.2f * fr) * fr;
		f->k = 2.0f * f->p - 1;
		f->r = q * powf( M_E, (1 - f->p) * 1.386249f );

		/* TODO
		if( f->doubleFilter ) {
			f->subFilter->f->r = f->r;
			f->subFilter->f->p = f->p;
			f->subFilter->f->k = f->k;
		} */
		return;
	}

	// other filters
	const float omega = M_2_PI * freq / f->sample_rate;
	const float tsin = sinf(omega);
	const float tcos = cosf(omega);
	//float alpha;

	//if (q_is_bandwidth)
	//alpha = tsin*sinhf(logf(2.0f)/2.0f*q*omega/
	//					tsin);
	//else
	const float alpha = 0.5f * tsin / q;

	const float a0 = 1.0f / (1.0f + alpha);

	f->a1a0 = -2.0f * tcos * a0;
	f->a2a0 = ( 1.0f - alpha ) * a0;

	switch( f->type )
	{
		case FILTER_LOWPASS:
			f->b1a0 = ( 1.0f - tcos ) * a0;
			f->b0a0 = f->b1a0 * 0.5f;
			f->b2a0 = f->b0a0;//((1.0f-tcos)/2.0f)*a0;
			break;
		case FILTER_HIPASS:
			f->b1a0 = ( -1.0f - tcos ) * a0;
			f->b0a0 = f->b1a0 * -0.5f;
			f->b2a0 = f->b0a0;//((1.0f+tcos)/2.0f)*a0;
			break;
		case FILTER_BANDPASS_CSG:
			f->b1a0 = 0.0f;
			f->b0a0 = tsin * 0.5f * a0;
			f->b2a0 = -f->b0a0;
			break;
		case FILTER_BANDPASS_CZPG:
			f->b1a0 = 0.0f;
			f->b0a0 = alpha * a0;
			f->b2a0 = -f->b0a0;
			break;
		case FILTER_NOTCH:
			f->b1a0 = f->a1a0;
			f->b0a0 = a0;
			f->b2a0 = a0;
			break;
		case FILTER_ALLPASS:
			f->b1a0 = f->a1a0;
			f->b0a0 = f->a2a0;
			f->b2a0 = 1.0f;//(1.0f+alpha)*a0;
			break;
		default:
			break;
	}

	/* TODO:
	if( m_doubleFilter ) {
		m_subFilter->m_b0a0 = m_b0a0;
		m_subFilter->m_b1a0 = m_b1a0;
		m_subFilter->m_b2a0 = m_b2a0;
		m_subFilter->m_a1a0 = m_a1a0;
		m_subFilter->m_a2a0 = m_a2a0;
	} */
}


