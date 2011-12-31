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

#define NS_ATOM "http://lv2plug.in/ns/ext/atom#"
#define NS_RDF  "http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#define LB303_SYNTH_URI  "http://pgiblock.net/plugins/lb303-synth"
#define MIDI_EVENT_URI   "http://lv2plug.in/ns/ext/midi#MidiEvent"
#define ATOM_MESSAGE_URI "http://lv2plug.in/ns/ext/atom#Message"

// Envelope Recalculation period
#define ENVINC 64

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


#ifndef MPI
#define M_PI 3.14159265358979323846264338327
#endif

// PORTS
enum {
	LB303_CONTROL   = 0,
	LB303_OUT       = 1,
	LB303_VCF_CUT   = 2,
	LB303_VCF_RES   = 3,
	LB303_VCF_MOD   = 4,
	LB303_VCF_DEC   = 5,
	LB303_SLIDE     = 6,
	LB303_SLIDE_DEC = 7,
	LB303_ACCENT    = 8,
	LB303_DEAD      = 9,
	LB303_DIST			= 10,
	LB303_FILTER    = 11
};


enum {
	FILTER_IIR2  = 0,
	FILTER_3POLE = 1
};

typedef struct {
	int   envpos;       // Update counter. Updates when = 0

	float c0,           // c0=e1 on retrigger; c0*=ed every sample; cutoff=e0+c0
	      e0,           // e0 and e1 for interpolation
	      e1,           //
	      rescoeff;     // Resonance coefficient [0.30,9.54]

	// IIR2:
	float d1,           //   d1 and d2 are added back into the sample with 
	      d2;           //   vcf_a and b as coefficients. IIR2 resonance
	                    //   loop.

	float a,            // IIR2 Coefficients for mixing dry and delay.
	      b,            //   Mixing coefficients for the final sound.  
	      c;            //  

	// 3-Filter
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

} FilterState;


typedef struct {
	/* Features */
	LV2_URID_Map* map;

	/* Ports */
	LV2_Atom_Buffer* event_port;
	float*           output_port;
	float*           slide_port;
	float*           slide_dec_port;
	float*           accent_port;
	float*           dead_port;
	float*           dist_port;
	float*           filter_port;
	float*           vcf_cut_port;
	float*           vcf_res_port;
	float*           vcf_mod_port;
	float*           vcf_dec_port;

	/* URIs TODO: Global*/
	struct {
		LV2_URID midi_event;
		LV2_URID atom_message;
	} uris;

	/* Playback state */
	uint32_t frame; // TODO: frame_t
	float    srate;
	uint8_t  midi_note;

	bool  dead;

	float vco_inc,          // Sample increment for the frequency. Creates Sawtooth.
	      vco_c;            // Raw oscillator sample [-0.5,0.5]

	float vco_slide,        // Current value of slide exponential curve. Nonzero=sliding
	      vco_slideinc,     // Slide base to use in next node. Nonzero=slide next note
	      vco_slidebase;    // The base vco_inc while sliding.

	float vca_attack,       // Amp attack 
	      vca_decay,        // Amp decay
	      vca_a0,           // Initial amplifier coefficient 
	      vca_a;            // Amplifier coefficient.

	int   vca_mode;         // 0: attack, 1: decay, 2: idle, 3: never played

	FilterState vcf;			// State of Vcf

} LB303Synth;


void filter_recalc(LB303Synth*);
void filter_env_recalc(LB303Synth*);

void filter_iir2_run(LB303Synth*, float*);
void filter_iir2_recalc(LB303Synth*);
void filter_iir2_env_recalc(LB303Synth*);

void filter_3pole_run(LB303Synth*, float*);
void filter_3pole_recalc(LB303Synth*);
void filter_3pole_env_recalc(LB303Synth*);


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
