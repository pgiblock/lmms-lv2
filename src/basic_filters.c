/*
 * basic_filters.c - simple but powerful filters
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2012-2013 Paul Giblock    <p/at/pgiblock.net>
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

static void filter_clear_history (Filter *f);


Filter *
filter_create (float sample_rate)
{
	Filter *f = malloc(sizeof(Filter));
	filter_reset(f, sample_rate);
	if (!f) {
		fprintf(stderr, "Could not allocate Filter.\n");
	}

	return f;
}


void
filter_reset (Filter *f, float sample_rate)
{
	// These coeff assignments are probably unneccessary
/*
	f->b0a0 = 0.0f;
	f->b1a0 = 0.0f;
	f->b2a0 = 0.0f;
	f->a1a0 = 0.0f;
	f->a2a0 = 0.0f;
	f->rca  = 0.0f;
	f->rcb  = 1.0f;
	f->rcc  = 0.0f;
*/
	f->sample_rate = sample_rate;
	filter_clear_history(f);
}


static void
filter_clear_history (Filter *f)
{
/*
	int c, i;

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

		for (i=0; i<6; i++) {
			f->vflp[i][c] = f->vfbp[i][c] = f->vfhp[i][c] = f->vflast[i][c] = 0.0f;
		}
	}
*/
	memset(&f->c, 0, sizeof(f->c));
	memset(&f->st, 0, sizeof(f->st));
}


static inline sample_t
filter_get_sample_moog (struct filter_moog_state *st,
                        struct filter_moog_coeffs *c,
                        sample_t in, int chnl)
{
	sample_t x = in - c->r * st->y4[chnl];

	// four cascaded onepole filters
	// (bilinear transform)
	st->y1[chnl] = t_limit(
			( x + st->oldx[chnl] ) * c->p
			- c->k * st->y1[chnl],
			-10.0f, 10.0f );
	st->y2[chnl] = t_limit(
			( st->y1[chnl] + st->oldy1[chnl] ) * c->p
			- c->k * st->y2[chnl],
			-10.0f, 10.0f );
	st->y3[chnl] = t_limit(
			( st->y2[chnl] + st->oldy2[chnl] ) * c->p
			- c->k * st->y3[chnl],
			-10.0f, 10.0f );
	st->y4[chnl] = t_limit(
			( st->y3[chnl] + st->oldy3[chnl] ) * c->p
			- c->k * st->y4[chnl],
			-10.0f, 10.0f );

	st->oldx[chnl] = x;
	st->oldy1[chnl] = st->y1[chnl];
	st->oldy2[chnl] = st->y2[chnl];
	st->oldy3[chnl] = st->y3[chnl];
	return st->y4[chnl] - st->y4[chnl] * st->y4[chnl] *
			st->y4[chnl] * ( 1.0f / 6.0f );
}


// 4-times oversampled simulation of an active RC-Bandpass,-Lowpass,-Highpass-
// Filter-Network as it was used in nearly all modern analog synthesizers. This
// can be driven up to self-oscillation (BTW: do not remove the limits!!!).
// (C) 1998 ... 2009 S.Fendt. Released under the GPL v2.0  or any later version.
static inline void
filter_get_sample_rc12 (struct filter_rc_state *st,
                        struct filter_rc_coeffs *c,
                        sample_t in0, int chnl)
{
	sample_t lp, hp, bp, in;
	int n;

	// 4-times oversampled... (even the moog-filter would benefit from this)
	for (n = 4; n != 0; --n) {
		in = in0 + st->bp0[chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b + st->lp0[chnl] * c->a;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c * ( st->hp0[chnl] + in - st->last0[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b + st->bp0[chnl] * c->a;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last0[chnl] = in;
		st->lp0[chnl] = lp;
		st->hp0[chnl] = hp;
		st->bp0[chnl] = bp;
	}
}


static inline void
filter_get_sample_rc24 (struct filter_rc_state *st,
                        struct filter_rc_coeffs *c,
                        sample_t in0, int chnl, int type)
{
	sample_t lp, hp, bp, in;
	int n;

	for (n = 4; n != 0; --n)
	{
		// first stage is as for the 12dB case...
		in = in0 + st->bp0[chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b + st->lp0[chnl] * c->a;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c * ( st->hp0[chnl] + in - st->last0[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b + st->bp0[chnl] * c->a;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last0[chnl] = in;
		st->lp0[chnl] = lp;
		st->hp0[chnl] = hp;
		st->bp0[chnl] = bp;

		// second stage gets the output of the first stage as input...
		if( type == FILTER_LOWPASS_RC24 ) {
			in = lp + st->bp1[chnl] * c->q;
		} else if( type == FILTER_BANDPASS_RC24 ) {
			in = bp + st->bp1[chnl] * c->q;
		} else {
			in = hp + st->bp1[chnl] * c->q;
		}

		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b + st->lp1[chnl] * c->a;
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c * ( st->hp1[chnl] + in - st->last1[chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b + st->bp1[chnl] * c->a;
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last1[chnl] = in;
		st->lp1[chnl] = lp;
		st->hp1[chnl] = hp;
		st->bp1[chnl] = bp;
	}
}

static inline sample_t
filter_get_sample_formant (struct filter_formant_state *st,
                           struct filter_formant_coeffs *c,
                           sample_t in0, int chnl)
{
	sample_t lp, hp, bp, in, out;
	int o;

	out = 0.f;
	for (o=0; o<4; o++) {
		// first formant
		in = in0 + st->bp[0][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[0] + st->lp[0][chnl] * c->a[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[0] * ( st->hp[0][chnl] + in - st->last[0][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[0] + st->bp[0][chnl] * c->a[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[0][chnl] = in;
		st->lp[0][chnl] = lp;
		st->hp[0][chnl] = hp;
		st->bp[0][chnl] = bp;

		in = bp + st->bp[2][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[0] + st->lp[2][chnl] * c->a[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[0] * ( st->hp[2][chnl] + in - st->last[2][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[0] + st->bp[2][chnl] * c->a[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[2][chnl] = in;
		st->lp[2][chnl] = lp;
		st->hp[2][chnl] = hp;
		st->bp[2][chnl] = bp;  

		in = bp + st->bp[4][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[0] + st->lp[4][chnl] * c->a[0];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[0] * ( st->hp[4][chnl] + in - st->last[4][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[0] + st->bp[4][chnl] * c->a[0];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[4][chnl] = in;
		st->lp[4][chnl] = lp;
		st->hp[4][chnl] = hp;
		st->bp[4][chnl] = bp;  

		out += bp;

		// second formant
		in = in0 + st->bp[0][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[1] + st->lp[1][chnl] * c->a[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[1] * ( st->hp[1][chnl] + in - st->last[1][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[1] + st->bp[1][chnl] * c->a[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[1][chnl] = in;
		st->lp[1][chnl] = lp;
		st->hp[1][chnl] = hp;
		st->bp[1][chnl] = bp;

		in = bp + st->bp[3][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[1] + st->lp[3][chnl] * c->a[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[1] * ( st->hp[3][chnl] + in - st->last[3][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[1] + st->bp[3][chnl] * c->a[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[3][chnl] = in;
		st->lp[3][chnl] = lp;
		st->hp[3][chnl] = hp;
		st->bp[3][chnl] = bp;  

		in = bp + st->bp[5][chnl] * c->q;
		in = (in > +1.f) ? +1.f : in;
		in = (in < -1.f) ? -1.f : in;

		lp = in * c->b[1] + st->lp[5][chnl] * c->a[1];
		lp = (lp > +1.f) ? +1.f : lp;
		lp = (lp < -1.f) ? -1.f : lp;

		hp = c->c[1] * ( st->hp[5][chnl] + in - st->last[5][chnl] );
		hp = (hp > +1.f) ? +1.f : hp;
		hp = (hp < -1.f) ? -1.f : hp;

		bp = hp * c->b[1] + st->bp[5][chnl] * c->a[1];
		bp = (bp > +1.f) ? +1.f : bp;
		bp = (bp < -1.f) ? -1.f : bp;

		st->last[5][chnl] = in;
		st->lp[5][chnl] = lp;
		st->hp[5][chnl] = hp;
		st->bp[5][chnl] = bp;  

		out += bp;
	}

	return out/2.0f;
}


sample_t
filter_get_sample (Filter *f, sample_t in, int chnl)
{
	sample_t out;
	switch (f->type) {
	case FILTER_MOOG:
		out = filter_get_sample_moog(&f->st.m, &f->c.m, in, chnl);
		break;

	case FILTER_LOWPASS_RC12:
		filter_get_sample_rc12(&f->st.r, &f->c.r, in, chnl);
		return f->st.r.lp0[chnl];

	case FILTER_BANDPASS_RC12:
		filter_get_sample_rc12(&f->st.r, &f->c.r, in, chnl);
		return f->st.r.bp0[chnl];

	case FILTER_HIGHPASS_RC12:
		filter_get_sample_rc12(&f->st.r, &f->c.r, in, chnl);
		return f->st.r.hp0[chnl];

	case FILTER_LOWPASS_RC24:
		filter_get_sample_rc24(&f->st.r, &f->c.r, in, chnl, f->type);
		return f->st.r.lp1[chnl];

	case FILTER_BANDPASS_RC24:
		filter_get_sample_rc24(&f->st.r, &f->c.r, in, chnl, f->type);
		return f->st.r.bp1[chnl];

	case FILTER_HIGHPASS_RC24:
		filter_get_sample_rc24(&f->st.r, &f->c.r, in, chnl, f->type);
		return f->st.r.hp1[chnl];

	case FILTER_FORMANTFILTER:
		return filter_get_sample_formant(&f->st.f, &f->c.f, in, chnl);

	case FILTER_DOUBLELOWPASS:
		out = f->c.b.b0a0 * in +
		      f->c.b.b1a0 * f->st.b.din1[chnl] +
		      f->c.b.b2a0 * f->st.b.din2[chnl] -
		      f->c.b.a1a0 * f->st.b.in1[chnl] -
		      f->c.b.a2a0 * f->st.b.in2[chnl];

		// push in/out buffers
		f->st.b.din2[chnl] = f->st.b.din1[chnl];
		f->st.b.din1[chnl] = in;

		// Fall-thru with new input value
		in = out;
		
	default:
		// filter
		out = f->c.b.b0a0 * in +
		      f->c.b.b1a0 * f->st.b.in1[chnl] +
		      f->c.b.b2a0 * f->st.b.in2[chnl] -
		      f->c.b.a1a0 * f->st.b.ou1[chnl] -
		      f->c.b.a2a0 * f->st.b.ou2[chnl];

		// push in/out buffers
		f->st.b.in2[chnl] = f->st.b.in1[chnl];
		f->st.b.in1[chnl] = in;
		f->st.b.ou2[chnl] = f->st.b.ou1[chnl];

		f->st.b.ou1[chnl] = out;
		break;
	}


	// Clipper band limited sigmoid
	return out;
}


static inline void
filter_calc_basic_coeffs (struct filter_basic_coeffs *c, int type,
                          float freq, float q, float srate)
{

	// other filters
	const float omega = M_2PI * freq / srate;
	const float tsin = sinf(omega);
	const float tcos = cosf(omega);
	//float alpha;

	//if (q_is_bandwidth)
	//alpha = tsin*sinhf(logf(2.0f)/2.0f*q*omega/
	//					tsin);
	//else
	const float alpha = 0.5f * tsin / q;

	const float a0 = 1.0f / (1.0f + alpha);
	
	//printf("CALC: %f %f %f %f %f %f %f\n", freq, c->sample_rate,
	//       omega, tsin, tcos, alpha, a0);

	c->a1a0 = -2.0f * tcos * a0;
	c->a2a0 = (1.0f - alpha) * a0;

	switch (type) {
	case FILTER_LOWPASS:
	case FILTER_DOUBLELOWPASS:
		c->b1a0 = ( 1.0f - tcos ) * a0;
		c->b0a0 = c->b1a0 * 0.5f;
		c->b2a0 = c->b0a0;//((1.0f-tcos)/2.0f)*a0;
		break;
	case FILTER_HIPASS:
		c->b1a0 = ( -1.0f - tcos ) * a0;
		c->b0a0 = c->b1a0 * -0.5f;
		c->b2a0 = c->b0a0;//((1.0f+tcos)/2.0f)*a0;
		break;
	case FILTER_BANDPASS_CSG:
		c->b1a0 = 0.0f;
		c->b0a0 = tsin * 0.5f * a0;
		c->b2a0 = -c->b0a0;
		break;
	case FILTER_BANDPASS_CZPG:
		c->b1a0 = 0.0f;
		c->b0a0 = alpha * a0;
		c->b2a0 = -c->b0a0;
		break;
	case FILTER_NOTCH:
		c->b1a0 = c->a1a0;
		c->b0a0 = a0;
		c->b2a0 = a0;
		break;
	case FILTER_ALLPASS:
		c->b1a0 = c->a1a0;
		c->b0a0 = c->a2a0;
		c->b2a0 = 1.0f;//(1.0f+alpha)*a0;
		break;
	default:
		break;
	}
}


static inline void
filter_calc_moog_coeffs (struct filter_moog_coeffs *c,
                         float freq, float q, float srate)
{
	// [ 0 - 0.5 ]
	const float fr = freq / srate;
	// (Empirical tuning)
	c->p = (3.6f - 3.2f * fr) * fr;
	c->k = 2.0f * c->p - 1;
	c->r = q * powf( M_E, (1 - c->p) * 1.386249f );
}


static inline void
filter_calc_rc_coeffs (struct filter_rc_coeffs *c,
                       float freq, float q, float srate)
{
	if (freq < 50.f) {
		freq = 50.f;
	}

	c->a = 1.0f - (1.0f/(srate*4)) / ( (1.0f/(freq*M_2PI)) + (1.0f/(srate*4)) );
	c->b = 1.0f - c->a;
	c->c = (1.0f/(freq*M_2PI)) / ( (1.0f/(freq*M_2PI)) + (1.0f/(srate*4)) );

	// Stretch Q/resonance, as self-oscillation reliably starts at a q of ~2.5 - ~2.6
	c->q = q/4.f;
	return;
}


static inline void
filter_calc_formant_coeffs (struct filter_formant_coeffs *c,
                            float freq, float q, float srate)
{
	// formats for a, e, i, o, u, a
	const float formants[5][2] = {
		{ 1000, 1400 },
		{ 500, 2300 },
		{ 320, 3200 },
		{ 500, 1000 },
		{ 320, 800 }
	};

	// Stretch Q/resonance
	c->q = q/4.f;
    srate = 1.0f/(srate/4.0f);

	// frequency in lmms ranges from 1Hz to 14000Hz
	const int   vowel = (int)( floor( freq/14000.f * 4.f ) );
	const float fract = (freq/14000.f * 4.f) - (float)vowel;

	// interpolate between formant frequencies
	const float f0 = formants[vowel+0][0] * (1.0f - fract) + 
			 formants[vowel+1][0] * (fract);

	const float f1 = formants[vowel+0][1] * (1.0f - fract) +
			 formants[vowel+1][1] * (fract);

	c->a[0] = 1.0f - srate /
			( (1.0f/(f0*M_2PI)) + srate );
	c->b[0] = 1.0f - c->a[0];
	c->c[0] = (1.0f/(f0*M_2PI)) /
			( (1.0f/(f0*M_2PI)) + srate );

	c->a[1] = 1.0f - srate /
			( (1.0f/(f1*M_2PI)) + srate );
	c->b[1] = 1.0f - c->a[1];
	c->c[1] = (1.0f/(f1*M_2PI)) /
			( (1.0f/(f1*M_2PI)) + srate );
}


void
filter_calc_coeffs (Filter *f, float freq, float q)
{
	// temp coef vars
	// limit freq and q for not getting bad noise out of the filter...
	freq = q_max(freq, MIN_FREQ);
	q    = q_max(q, MIN_Q);

	switch (f->type) {
	case FILTER_LOWPASS_RC12:
	case FILTER_BANDPASS_RC12:
	case FILTER_HIGHPASS_RC12:
	case FILTER_LOWPASS_RC24:
	case FILTER_BANDPASS_RC24:
	case FILTER_HIGHPASS_RC24:
		filter_calc_rc_coeffs(&f->c.r, freq, q, f->sample_rate);
		break;
	case FILTER_FORMANTFILTER:
		filter_calc_formant_coeffs(&f->c.f, freq, q, f->sample_rate);
		break;
	case FILTER_MOOG:
		filter_calc_moog_coeffs(&f->c.m, freq, q, f->sample_rate);
		break;
	default:
		filter_calc_basic_coeffs(&f->c.b, f->type, freq, q, f->sample_rate);
		break;
	}
}
