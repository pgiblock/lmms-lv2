/*
 * lb302.h - declaration of class lb302 which is a bass synth attempting to
 *           emulate the Roland TB303 bass synth
 *
 * Copyright (c) 2006-2008 Paul Giblock <pgib/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * lb302FilterIIR2 is based on the gsyn filter code by Andy Sloane.
 * 
 * lb302Filter3Pole is based on the TB303 instrument written by 
 *   Josep M Comajuncosas for the CSounds library
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

#ifndef LB303_H__
#define LB303_H__

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

// Envelope Recalculation period
#define LB_ENVINC 64

//
// New config
//
#define LB_24_IGNORE_ENVELOPE   
#define LB_FILTERED 
//#define LB_DECAY
//#define LB_24_RES_TRICK         

#define LB_DIST_RATIO    4.0
#define LB_24_VOL_ADJUST 3.0
//#define LB_DECAY_NOTES


#endif // LB303_H__
#if 0

class lb302Filter
{
	public:
	lb302Filter(lb302FilterKnobState* p_fs);
	virtual ~lb302Filter() {};

	virtual void recalc();
	virtual void envRecalc();
	virtual float process(const float& samp)=0;
	virtual void playNote();

	protected:
	lb302FilterKnobState *fs;  

	// Filter Decay
	float vcf_c0;           // c0=e1 on retrigger; c0*=ed every sample; cutoff=e0+c0
	float vcf_e0,           // e0 and e1 for interpolation
	      vcf_e1;           
	float vcf_rescoeff;     // Resonance coefficient [0.30,9.54]
};

class lb302FilterIIR2 : public lb302Filter
{
	public:
	lb302FilterIIR2(lb302FilterKnobState* p_fs);
	virtual ~lb302FilterIIR2();

	virtual void recalc();
	virtual void envRecalc();
	virtual float process(const float& samp);

	protected:
	float vcf_d1,           //   d1 and d2 are added back into the sample with 
	      vcf_d2;           //   vcf_a and b as coefficients. IIR2 resonance
	                        //   loop.

	                        // IIR2 Coefficients for mixing dry and delay.
	float vcf_a,            //   Mixing coefficients for the final sound.  
	      vcf_b,            //  
	      vcf_c;

	effectLib::distortion * m_dist;
};


class lb302Filter3Pole : public lb302Filter
{
	public:
	lb302Filter3Pole(lb302FilterKnobState* p_fs);

	virtual void envRecalc();
	virtual void recalc();
	virtual float process(const float& samp);

	protected:
	float kfcn, 
	      kp, 
	      kp1, 
	      kp1h, 
	      kres;
	float ay1, 
	      ay2, 
	      aout, 
	      lastin, 
	      value;
};



class lb302Synth : public Instrument
{
	Q_OBJECT
public:
	lb302Synth( InstrumentTrack * _instrument_track );
	virtual ~lb302Synth();

	virtual void play( sampleFrame * _working_buffer );
	virtual void playNote( notePlayHandle * _n,
						sampleFrame * _working_buffer );
	virtual void deleteNotePluginData( notePlayHandle * _n );


	virtual void saveSettings( QDomDocument & _doc, QDomElement & _parent );
	virtual void loadSettings( const QDomElement & _this );

	virtual QString nodeName() const;

	virtual f_cnt_t desiredReleaseFrames() const
	{
		return 0; //4048;
	}

	virtual PluginView * instantiateView( QWidget * _parent );

private:

	void initNote(lb302Note *note);


private:
	FloatModel vco_fine_detune_knob;

	FloatModel dist_knob;
	IntModel wave_shape;
    
	BoolModel db24Toggle;


public slots:
	void filterChanged();
	void db24Toggled();

private:
	// Oscillator

	float vco_detune;

	enum  vco_shape_t { SAWTOOTH, SQUARE, TRIANGLE, MOOG, ROUND_SQUARE, SINE, EXPONENTIAL, WHITE_NOISE };
	vco_shape_t vco_shape;

	// User settings
	lb302FilterKnobState fs;
	lb302Filter *vcf;

	int release_frame;


	// More States
	int   vcf_envpos;       // Update counter. Updates when >= ENVINC

	// My hacks
	int   sample_cnt;

	int   last_offset;

	int catch_frame;
	int catch_decay;

	float new_freq;
	float current_freq;
	float delete_freq;
	float true_freq;

	void recalcFilter();

	int process(sampleFrame *outbuf, const Uint32 size);
} ;


#endif
