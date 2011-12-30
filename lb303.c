/*
 * lb302.cpp - implementation of class lb302 which is a bass synth attempting 
 *             to emulate the Roland TB303 bass synth
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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom-buffer.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom-helpers.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "lb303.h"


static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	LB303Synth* plugin = (LB303Synth*)instance;

	switch (port) {
		case LB303_CONTROL:
			plugin->event_port = (LV2_Atom_Buffer*)data;
			break;
		case LB303_OUT:
			plugin->output_port = (float*)data;
			break;
		case LB303_VCF_CUT:
			plugin->vcf_cut_port = (float*)data;
			break;
		case LB303_VCF_RES:
			plugin->vcf_res_port = (float*)data;
			break;
		case LB303_VCF_MOD:
			plugin->vcf_mod_port = (float*)data;
			break;
		case LB303_VCF_DEC:
			plugin->vcf_dec_port = (float*)data;
			break;
		case LB303_SLIDE:
			plugin->slide_port = (float*)data;
			break;
		case LB303_SLIDE_DEC:
			plugin->slide_dec_port = (float*)data;
			break;
		case LB303_ACCENT:
			plugin->accent_port = (float*)data;
			break;
		case LB303_DEAD:
			plugin->dead_port = (float*)data;
			break;
		default:
			break;
	}
}


static void
cleanup(LV2_Handle instance)
{
	LB303Synth* plugin = (LB303Synth*)instance;
	free(instance);
}


static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               path,
            const LV2_Feature* const* features)
{
	/* Malloc and initialize new Synth */
	LB303Synth* plugin = (LB303Synth*)malloc(sizeof(LB303Synth));
	if (!plugin) {
		fprintf(stderr, "Could not allocate LB303Synth.\n");
		return NULL;
	}

	memset(&plugin->uris, 0, sizeof(plugin->uris));

	/* TODO: Move to constants or ControlPorts */
	plugin->vca_attack = 1.0 - 0.96406088; /* = 1.0 - 0.94406088; */
	plugin->vca_decay  = 0.99897516;

	plugin->srate = rate;

	// Start VCA on an attack.
	plugin->vca_mode = 3;
	plugin->vca_a    = 0;

	// Experimenting with a0 between original (0.5) and 1.0
	plugin->vca_a0   = 0.5;
	plugin->vca_a    = 9;
	plugin->vca_mode = 3;

	/* Scan host features for URID map and map everything */
	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
			plugin->uris.midi_event = plugin->map->map(
					plugin->map->handle, MIDI_EVENT_URI);
			plugin->uris.atom_message = plugin->map->map(
					plugin->map->handle, ATOM_MESSAGE_URI);
		}
	}

	if (!plugin->map) {
		fprintf(stderr, "Host does not support urid:map.\n");
		goto fail;
	}

	return (LV2_Handle)plugin;

fail:
	free(plugin);
	return 0;
}


static void
run(LV2_Handle instance,
    uint32_t   sample_count)
{
	LB303Synth* plugin      = (LB303Synth*)instance;
	float*      output      = plugin->output_port;

	uint32_t    pos;
	uint32_t    ev_frames;
	uint32_t    f = plugin->frame;

	LV2_Atom_Buffer_Iterator ev_i = lv2_atom_buffer_begin(plugin->event_port);
	LV2_Atom_Event const * ev = NULL;

	for (pos = 0; pos < sample_count;) {
		// Check for next event
		if (lv2_atom_buffer_is_valid(ev_i)) {
			ev        = lv2_atom_buffer_get(ev_i);
			ev_frames = ev->frames;
			ev_i      = lv2_atom_buffer_next(ev_i);
		} else {
			ev = NULL;
			ev_frames = sample_count;
		}

		// Run until next event
		for (; pos < ev_frames; ++pos, ++f) {
			// Process

			// update vco
			plugin->vco_c += plugin->vco_inc;

			if (plugin->vco_c > 0.5) {
				plugin->vco_c -= 1.0;
			}

			output[pos] = plugin->vco_c * plugin->vca_a;

			// Handle Envelope
			if (plugin->vca_mode==0) {
				plugin->vca_a += (plugin->vca_a0 - plugin->vca_a) * plugin->vca_attack;
				if (f >= 0.5 * plugin->srate) {
					plugin->vca_mode = 2;
				}
			}
			else if(plugin->vca_mode == 1) {
				plugin->vca_a *= plugin->vca_decay;
			}
			//printf("f: %d(%d)  INC: %f  Val: %f\n", f, pos, plugin->vco_inc, output[pos]);
		}

		// the following line actually speeds up processing
		if(plugin->vca_a < (1/65536.0)) {
			plugin->vca_a = 0;
			plugin->vca_mode = 3;
		}

		// Process event
		if (ev) {
			if (ev->body.type == plugin->uris.midi_event) {
				uint8_t* const data = (uint8_t* const)(ev + 1);
				uint8_t const  cmd  = data[0] & 0xF0;
				if (cmd == 0x90) {
					// Note On
					float freq        = powf(2.0f, (float)data[1] / 12.0f) * 110.0f;
					plugin->midi_note = data[1];
					plugin->vco_inc   = freq / plugin->srate;
					plugin->dead      = (*plugin->dead_port) > 0.0f;
					printf("MN: %d  INC: %f  FREQ: %f\n", data[1], plugin->vco_inc, freq);

					//TODO: catch_decay = 0;

					// Always reset vca on non-dead notes, and
					// Only reset vca on decaying(decayed) and never-played
					if (!plugin->dead || (plugin->vca_mode ==1 || plugin->vca_mode==3)) {
						plugin->vca_mode = 0;
						plugin->frame = 0;
						f = 0;

						// LB303:
						//vca_a = 0;
					}
					else {
						plugin->vca_mode = 2;
					}

					// Initiate Slide
					// TODO: Break out into function, should be called again on detuneChanged
					if (plugin->vco_slideinc) {
						//printf("    sliding\n");
						plugin->vco_slide     = plugin->vco_inc - plugin->vco_slideinc;	// Slide amount
						plugin->vco_slidebase = plugin->vco_inc;			                  // The REAL frequency
						plugin->vco_slideinc  = 0;					                            // reset from-note
					}
					else {
						plugin->vco_slide = 0;
					}
					// End break-out

					// Slide-from note, save inc for next note
					if (*plugin->slide_port > 0.0f) {
						plugin->vco_slideinc = plugin->vco_inc; // May need to equal vco_slidebase+vco_slide if last note slid
					}

					// TODO: recalcFilter(); ...

				} else if (cmd == 0x80) {
					// Note Off
					if (plugin->midi_note == data[1]) {
						plugin->vca_mode = 1;
					}
				}
			} else {
				fprintf(stderr, "Unknown event type %d\n", ev->body.type);
			}
		}

	}

	// TODO: Remove this stupid shadow?
	plugin->frame = f;
}


static uint32_t
map_uri(LB303Synth* plugin, const char* uri)
{
	return plugin->map->map(plugin->map->handle, uri);
}


static void
save(LV2_Handle                instance,
     LV2_State_Store_Function  store,
     void*                     callback_data,
     uint32_t                  flags,
     const LV2_Feature* const* features)
{
	LB303Synth* plugin = (LB303Synth*)instance;
	// TODO: store(...)
	printf("LB303 save stub.\n");
}


static void
restore(LV2_Handle                  instance,
        LV2_State_Retrieve_Function retrieve,
        void*                       callback_data,
        uint32_t                    flags,
        const LV2_Feature* const*   features)
{
	LB303Synth* plugin = (LB303Synth*)instance;
	// TODO: retrieve(...)
	printf("LB303 restore stub.\n");
}


const void*
extension_data(const char* uri)
{
	static const LV2_State_Interface state = { save, restore };
	if (!strcmp(uri, LV2_STATE_URI)) {
		return &state;
	}
	return NULL;
}


static const LV2_Descriptor descriptor = {
	LB303_SYNTH_URI,
	instantiate,
	connect_port,
	NULL, // activate,
	run,
	NULL, // deactivate,
	cleanup,
	extension_data
};


LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}


#if 0

//
// lb302Filter
//

lb302Filter::lb302Filter(lb302FilterKnobState* p_fs) :
	fs(p_fs),
	vcf_c0(0),
	vcf_e0(0),
	vcf_e1(0)
{
};


void lb302Filter::recalc()
{
	vcf_e1 = exp(6.109 + 1.5876*(fs->envmod) + 2.1553*(fs->cutoff) - 1.2*(1.0-(fs->reso)));
	vcf_e0 = exp(5.613 - 0.8*(fs->envmod) + 2.1553*(fs->cutoff) - 0.7696*(1.0-(fs->reso)));
	vcf_e0*=M_PI/engine::getMixer()->processingSampleRate();
	vcf_e1*=M_PI/engine::getMixer()->processingSampleRate();
	vcf_e1 -= vcf_e0;

	vcf_rescoeff = exp(-1.20 + 3.455*(fs->reso));
};


void lb302Filter::envRecalc()
{
	vcf_c0 *= fs->envdecay;       // Filter Decay. vcf_decay is adjusted for Hz and ENVINC
	// vcf_rescoeff = exp(-1.20 + 3.455*(fs->reso)); moved above
};


void lb302Filter::playNote()
{
	vcf_c0 = vcf_e1;
}


//
// lb302FilterIIR2
//

lb302FilterIIR2::lb302FilterIIR2(lb302FilterKnobState* p_fs) :
	lb302Filter(p_fs),
	vcf_d1(0),
	vcf_d2(0),
	vcf_a(0),
	vcf_b(0),
	vcf_c(1)
{

	m_dist = new effectLib::distortion( 1.0, 1.0f);
	
};


lb302FilterIIR2::~lb302FilterIIR2()
{
	delete m_dist;
}


void lb302FilterIIR2::recalc()
{
	lb302Filter::recalc();
	//m_dist->setThreshold(0.5+(fs->dist*2.0));
	m_dist->setThreshold(fs->dist*75.0);
};


void lb302FilterIIR2::envRecalc()
{
	float k, w;

	lb302Filter::envRecalc();

	w = vcf_e0 + vcf_c0;          // e0 is adjusted for Hz and doesn't need ENVINC
	k = exp(-w/vcf_rescoeff);     // Does this mean c0 is inheritantly?

	vcf_a = 2.0*cos(2.0*w) * k;
	vcf_b = -k*k;
	vcf_c = 1.0 - vcf_a - vcf_b;
}


float lb302FilterIIR2::process(const float& samp)
{
	float ret = vcf_a*vcf_d1 + vcf_b*vcf_d2 + vcf_c*samp;
	// Delayed samples for filter
	vcf_d2 = vcf_d1;
	vcf_d1 = ret;

	if(fs->dist > 0) 
		ret=m_dist->nextSample(ret);

	// output = IIR2 + dry
	return ret;
}


//
// lb302Filter3Pole
//

lb302Filter3Pole::lb302Filter3Pole(lb302FilterKnobState *p_fs) :
	lb302Filter(p_fs),
	ay1(0),
	ay2(0),
	aout(0),
	lastin(0) 
{
};


void lb302Filter3Pole::recalc()
{
	// DO NOT CALL BASE CLASS
	vcf_e0 = 0.000001;
	vcf_e1 = 1.0;
}


// TODO: Try using k instead of vcf_reso
void lb302Filter3Pole::envRecalc()
{
	float w,k;
	float kfco;

	lb302Filter::envRecalc();

	// e0 is adjusted for Hz and doesn't need ENVINC
	w = vcf_e0 + vcf_c0;
	k = (fs->cutoff > 0.975)?0.975:fs->cutoff;
	kfco = 50.f + (k)*((2300.f-1600.f*(fs->envmod))+(w) *
	                   (700.f+1500.f*(k)+(1500.f+(k)*(engine::getMixer()->processingSampleRate()/2.f-6000.f)) * 
	                   (fs->envmod)) );
	//+iacc*(.3+.7*kfco*kenvmod)*kaccent*kaccurve*2000


#ifdef LB_24_IGNORE_ENVELOPE
	// kfcn = fs->cutoff;
	kfcn = 2.0 * kfco / engine::getMixer()->processingSampleRate();
#else
	kfcn = w;
#endif
	kp   = ((-2.7528*kfcn + 3.0429)*kfcn + 1.718)*kfcn - 0.9984;
	kp1  = kp+1.0;
	kp1h = 0.5*kp1;
#ifdef LB_24_RES_TRICK
	k = exp(-w/vcf_rescoeff);
	kres = (((k))) * (((-2.7079*kp1 + 10.963)*kp1 - 14.934)*kp1 + 8.4974);
#else
	kres = (((fs->reso))) * (((-2.7079*kp1 + 10.963)*kp1 - 14.934)*kp1 + 8.4974);
#endif
	value = 1.0+( (fs->dist) *(1.5 + 2.0*kres*(1.0-kfcn))); // ENVMOD was DIST
}


float lb302Filter3Pole::process(const float& samp) 
{
	float ax1  = lastin;
	float ay11 = ay1;
	float ay31 = ay2;
	lastin  = (samp) - tanh(kres*aout);
	ay1     = kp1h * (lastin+ax1) - kp*ay1;
	ay2     = kp1h * (ay1 + ay11) - kp*ay2;
	aout    = kp1h * (ay2 + ay31) - kp*aout;

	return tanh(aout*value)*LB_24_VOL_ADJUST/(1.0+fs->dist);
}


//
// LBSynth
//

lb302Synth::lb302Synth( InstrumentTrack * _instrumentTrack ) :
	Instrument( _instrumentTrack, &lb302_plugin_descriptor ),
	dist_knob( 0.0f, 0.0f, 1.0f, 0.01f, this, tr( "Distortion" ) ),
	wave_shape( 0.0f, 0.0f, 7.0f, this, tr( "Waveform" ) ),
	db24Toggle( false, this, tr( "24dB/oct Filter" ) )	

{

	fs.cutoff = 0;
	fs.envmod = 0;
	fs.reso = 0;
	fs.envdecay = 0;
	fs.dist = 0;

	vcf_envpos = ENVINC;

	vco_shape = SAWTOOTH; 

	vcf = new lb302FilterIIR2(&fs);

	sample_cnt = 0;
	release_frame = 1<<24;
	catch_frame = 0;
	catch_decay = 0;

	recalcFilter();

	last_offset = 0;

	new_freq = -1;
	current_freq = -1;
	delete_freq = -1;

	InstrumentPlayHandle * iph = new InstrumentPlayHandle( this );
	engine::getMixer()->addPlayHandle( iph );

	filterChanged();
}


// TODO: Split into one function per knob.  envdecay doesn't require
// recalcFilter.
void lb302Synth::filterChanged()
{
	fs.cutoff = vcf_cut_knob.value();
	fs.reso   = vcf_res_knob.value();
	fs.envmod = vcf_mod_knob.value();
	fs.dist   = LB_DIST_RATIO*dist_knob.value();

	float d = 0.2 + (2.3*vcf_dec_knob.value());

	d *= engine::getMixer()->processingSampleRate();                                // d *= smpl rate
	fs.envdecay = pow(0.1, 1.0/d * ENVINC);    // decay is 0.1 to the 1/d * ENVINC
	                                           // vcf_envdecay is now adjusted for both
	                                           // sampling rate and ENVINC
	recalcFilter();
}


void lb302Synth::db24Toggled()
{
	delete vcf;
	if(db24Toggle.value()) {
		vcf = new lb302Filter3Pole(&fs);
	}
	else {
		vcf = new lb302FilterIIR2(&fs);
	}
	recalcFilter();
}



QString lb302Synth::nodeName() const
{
	return( lb302_plugin_descriptor.name );
}


// OBSOLETE. Break apart once we get Q_OBJECT to work. >:[
void lb302Synth::recalcFilter()
{
	vcf->recalc();

	// THIS IS OLD 3pole/24dB code, I may reintegrate it.  Don't need it
	// right now.   Should be toggled by LB_24_RES_TRICK at the moment.

	/*kfcn = 2.0 * (((vcf_cutoff*3000))) / engine::getMixer()->processingSampleRate();
	kp   = ((-2.7528*kfcn + 3.0429)*kfcn + 1.718)*kfcn - 0.9984;
	kp1  = kp+1.0;
	kp1h = 0.5*kp1;
	kres = (((vcf_reso))) * (((-2.7079*kp1 + 10.963)*kp1 - 14.934)*kp1 + 8.4974);
	value = 1.0+( (((0))) *(1.5 + 2.0*kres*(1.0-kfcn))); // ENVMOD was DIST*/

	vcf_envpos = ENVINC; // Trigger filter update in process()
}

inline int MIN(int a, int b) {
	return (a<b)?a:b;
}

inline float GET_INC(float freq) {
	return freq/engine::getMixer()->processingSampleRate();  // TODO: Use actual sampling rate.
}

int lb302Synth::process(sampleFrame *outbuf, const Uint32 size)
{
	unsigned int i;
	float w;
	float samp;

	if( delete_freq == current_freq ) {
		// Normal release
		delete_freq = -1;
		vca_mode = 1;
	}

	if( new_freq > 0.0f ) {
		//printf("  playing new note..\n");
		lb302Note note;
		note.vco_inc = GET_INC( true_freq );
		//printf("GET_INC %f %f %d\n", note.vco_inc, new_freq, vca_mode );
		///**vco_detune*//engine::getMixer()->processingSampleRate();  // TODO: Use actual sampling rate.
		//printf("VCO_INC = %f\n", note.vco_inc);
		note.dead = deadToggle.value();
		initNote(&note);
		//printf("%f %f,  ", vco_inc, vco_c);
		
		current_freq = new_freq; 

		new_freq = -1.0f;
		//printf("GOT_INC %f %f %d\n\n", note.vco_inc, new_freq, vca_mode );
	} 

	

	// TODO: NORMAL RELEASE
	// vca_mode = 1;

	for(i=0;i<size;i++) {

		// update vcf
		if(vcf_envpos >= ENVINC) {
			vcf->envRecalc();

			vcf_envpos = 0;

			if (vco_slide) {
					vco_inc=vco_slidebase-vco_slide;
					// Calculate coeff from dec_knob on knob change.
					vco_slide*= 0.9+(slide_dec_knob.value()*0.0999); // TODO: Adjust for Hz and ENVINC

			}
		}


		sample_cnt++;
		vcf_envpos++;

		//int  decay_frames = 128;

		// update vco
		vco_c += vco_inc;

		if(vco_c > 0.5)
			vco_c -= 1.0;

		/*LB303
		if (catch_decay > 0) {
			if (catch_decay < decay_frames) {
				catch_decay++;
			}
		}*/

		switch(int(rint(wave_shape.value()))) {
			case 0: vco_shape = SAWTOOTH; break;
			case 1: vco_shape = TRIANGLE; break;
			case 2: vco_shape = SQUARE; break;
			case 3: vco_shape = ROUND_SQUARE; break;
			case 4: vco_shape = MOOG; break;
			case 5: vco_shape = SINE; break;
			case 6: vco_shape = EXPONENTIAL; break;
			case 7: vco_shape = WHITE_NOISE; break;
			default:  vco_shape = SAWTOOTH; break;
		}

		// add vco_shape_param the changes the shape of each curve.
		// merge sawtooths with triangle and square with round square?
		switch (vco_shape) {
			case SAWTOOTH: // p0: curviness of line
				vco_k = vco_c;  // Is this sawtooth backwards?
				break;

			case TRIANGLE:  // p0: duty rev.saw<->triangle<->saw p1: curviness
				vco_k = (vco_c*2.0)+0.5;
				if (vco_k>0.5)
					vco_k = 1.0- vco_k;
				break;

			case SQUARE: // p0: slope of top
				vco_k = (vco_c<0)?0.5:-0.5;
				break;

			case ROUND_SQUARE: // p0: width of round
				vco_k = (vco_c<0)?(sqrtf(1-(vco_c*vco_c*4))-0.5):-0.5;
				break;

			case MOOG: // Maybe the fall should be exponential/sinsoidal instead of quadric.
				// [-0.5, 0]: Rise, [0,0.25]: Slope down, [0.25,0.5]: Low 
				vco_k = (vco_c*2.0)+0.5;
				if (vco_k>1.0) {
					vco_k = -0.5 ;
				}
				else if (vco_k>0.5) {
					w = 2.0*(vco_k-0.5)-1.0;
					vco_k = 0.5 - sqrtf(1.0-(w*w));
				}
				vco_k *= 2.0;  // MOOG wave gets filtered away 
				break;

			case SINE:
				// [-0.5, 0.5]  : [-pi, pi]
				vco_k = 0.5f * Oscillator::sinSample( vco_c );
				break;

			case EXPONENTIAL:
				vco_k = 0.5 * Oscillator::expSample( vco_c );
				break;

			case WHITE_NOISE:
				vco_k = 0.5 * Oscillator::noiseSample( vco_c );
				break;
		}

		//vca_a = 0.5;
		// Write out samples.
#ifdef LB_FILTERED
		//samp = vcf->process(vco_k)*2.0*vca_a;
		//samp = vcf->process(vco_k)*2.0;
		samp = vcf->process(vco_k) * vca_a;
		//printf("%f %d\n", vco_c, sample_cnt);	
		

		//samp = vco_k * vca_a;

		if( sample_cnt <= 4 )
		{
	//			vca_a = 0;
		}
		
#else
		//samp = vco_k*vca_a;
#endif
		/*
		float releaseFrames = desiredReleaseFrames();
		samp *= (releaseFrames - catch_decay)/releaseFrames;
		*/
		//LB302 samp *= (float)(decay_frames - catch_decay)/(float)decay_frames;

		for(int c=0; c<DEFAULT_CHANNELS; c++) {
			outbuf[i][c]=samp;
		}


		/*LB303
		if((int)i>=release_frame) {
			vca_mode=1;
		}
		*/

		// Handle Envelope
		if(vca_mode==0) {
			vca_a+=(vca_a0-vca_a)*vca_attack;
			if(sample_cnt>=0.5*engine::getMixer()->processingSampleRate()) 
				vca_mode = 2;
		}
		else if(vca_mode == 1) {
			vca_a *= vca_decay;

			// the following line actually speeds up processing
			if(vca_a < (1/65536.0)) {
				vca_a = 0;
				vca_mode = 3;
			}
		}

	}
	return 1;
}


/*  Prepares the active LB302 note.  I separated this into a function because it
 *  needs to be called onplayNote() when a new note is started.  It also needs
 *  to be called from process() when a prior edge-to-edge note is done releasing.
 */

void lb302Synth::initNote( lb302Note *n)
{
	catch_decay = 0;

	vco_inc = n->vco_inc;
    
	// Always reset vca on non-dead notes, and
	// Only reset vca on decaying(decayed) and never-played
	if(n->dead == 0 || (vca_mode==1 || vca_mode==3)) {
		//printf("    good\n");
		sample_cnt = 0;
		vca_mode = 0;
		// LB303:
		//vca_a = 0;
	}
	else {
		vca_mode = 2;
	}

	// Initiate Slide
	// TODO: Break out into function, should be called again on detuneChanged
	if (vco_slideinc) {
		//printf("    sliding\n");
		vco_slide = vco_inc-vco_slideinc;	// Slide amount
		vco_slidebase = vco_inc;			// The REAL frequency
		vco_slideinc = 0;					// reset from-note
	}
	else {
		vco_slide = 0;
	}
	// End break-out

	// Slide-from note, save inc for next note
	if (slideToggle.value()) {
		vco_slideinc = vco_inc; // May need to equal vco_slidebase+vco_slide if last note slid
	}


	recalcFilter();
	
	if(n->dead ==0){
		// Swap next two blocks??
		vcf->playNote();
		// Ensure envelope is recalculated
		vcf_envpos = ENVINC;

		// Double Check 
		//vca_mode = 0;
		//vca_a = 0.0;
	}
}


void lb302Synth::playNote( notePlayHandle * _n, sampleFrame * _working_buffer )
{
	//fpp_t framesPerPeriod = engine::getMixer()->framesPerPeriod();

	if( _n->isArpeggioBaseNote() )
	{
		return;
	}

	// Currently have release/decay disabled
	// Start the release decay if this is the first release period.
	//if (_n->released() && catch_decay == 0)
	//        catch_decay = 1;

	bool decay_note = false;

	release_frame = _n->framesLeft() - desiredReleaseFrames();


	//LB303 if ( _n->totalFramesPlayed() <= 0 ) {
		// This code is obsolete, hence the "if false"

		// Existing note. Allow it to decay. 
		if(deadToggle.value() == 0 && decay_note) {

	/*		lb302Note note;
			note.vco_inc = _n->frequency()*vco_detune/engine::getMixer()->processingSampleRate();  // TODO: Use actual sampling rate.
			note.dead = deadToggle.value();
			initNote(&note);
			vca_mode=0;
	*/

		}
		/// Start a new note.
		else if( _n->totalFramesPlayed() == 0 ) {
			new_freq = _n->unpitchedFrequency();
			true_freq = _n->frequency();
			_n->m_pluginData = this;
		}

		// Check for slide
		if( _n->unpitchedFrequency() == current_freq ) {
			true_freq = _n->frequency();

			if( slideToggle.value() ) {
				vco_slidebase = GET_INC( true_freq );			// The REAL frequency
			}
			else {
				vco_inc = GET_INC( true_freq );
			}
		}

	//LB303 }


}



void lb302Synth::play( sampleFrame * _working_buffer )
{
	//printf(".");
	const fpp_t frames = engine::getMixer()->framesPerPeriod();

	process( _working_buffer, frames); 
	instrumentTrack()->processAudioBuffer( _working_buffer, frames,
									NULL );
}



void lb302Synth::deleteNotePluginData( notePlayHandle * _n )
{
	//printf("GONE\n");
	if( _n->unpitchedFrequency() == current_freq ) 
	{
		delete_freq = current_freq;
	}
}


PluginView * lb302Synth::instantiateView( QWidget * _parent )
{
	return( new lb302SynthView( this, _parent ) );
}


lb302SynthView::lb302SynthView( Instrument * _instrument, QWidget * _parent ) :
	InstrumentView( _instrument, _parent )
{
	// GUI
	m_vcfCutKnob = new knob( knobBright_26, this );
	m_vcfCutKnob->move( 75, 130 );
	m_vcfCutKnob->setHintText( tr( "Cutoff Freq:" ) + " ", "" );
	m_vcfCutKnob->setLabel( tr("CUT") );

	m_vcfResKnob = new knob( knobBright_26, this );
	m_vcfResKnob->move( 120, 130 );
	m_vcfResKnob->setHintText( tr( "Resonance:" ) + " ", "" );
	m_vcfResKnob->setLabel( tr("RES") );

	m_vcfModKnob = new knob( knobBright_26, this );
	m_vcfModKnob->move( 165, 130 );
	m_vcfModKnob->setHintText( tr( "Env Mod:" ) + " ", "" );
	m_vcfModKnob->setLabel( tr("ENV MOD") );

	m_vcfDecKnob = new knob( knobBright_26, this );
	m_vcfDecKnob->move( 210, 130 );
	m_vcfDecKnob->setHintText( tr( "Decay:" ) + " ", "" );
	m_vcfDecKnob->setLabel( tr("DEC") );

	m_slideToggle = new ledCheckBox( "Slide", this );
	m_slideToggle->move( 10, 180 );

	m_accentToggle = new ledCheckBox( "Accent", this );
	m_accentToggle->move( 10, 200 );
	m_accentToggle->setDisabled(true);

	m_deadToggle = new ledCheckBox( "Dead", this );
	m_deadToggle->move( 10, 220 );

	m_db24Toggle = new ledCheckBox( "24dB/oct", this );
	m_db24Toggle->setWhatsThis( 
			tr( "303-es-que, 24dB/octave, 3 pole filter" ) );
	m_db24Toggle->move( 10, 150);


	m_slideDecKnob = new knob( knobBright_26, this );
	m_slideDecKnob->move( 210, 75 );
	m_slideDecKnob->setHintText( tr( "Slide Decay:" ) + " ", "" );
	m_slideDecKnob->setLabel( tr( "SLIDE"));

	m_distKnob = new knob( knobBright_26, this );
	m_distKnob->move( 210, 190 );
	m_distKnob->setHintText( tr( "DIST:" ) + " ", "" );
	m_distKnob->setLabel( tr( "DIST"));


	// Shapes
	// move to 120,75
	const int waveBtnX = 10;
	const int waveBtnY = 96;
	pixmapButton * sawWaveBtn = new pixmapButton( this, tr( "Saw wave" ) );
	sawWaveBtn->move( waveBtnX, waveBtnY );
	sawWaveBtn->setActiveGraphic( embed::getIconPixmap(
						"saw_wave_active" ) );
	sawWaveBtn->setInactiveGraphic( embed::getIconPixmap(
						"saw_wave_inactive" ) );
	toolTip::add( sawWaveBtn,
			tr( "Click here for a saw-wave." ) );

	pixmapButton * triangleWaveBtn =
		new pixmapButton( this, tr( "Triangle wave" ) );
	triangleWaveBtn->move( waveBtnX+(16*1), waveBtnY );
	triangleWaveBtn->setActiveGraphic(
		embed::getIconPixmap( "triangle_wave_active" ) );
	triangleWaveBtn->setInactiveGraphic(
		embed::getIconPixmap( "triangle_wave_inactive" ) );
	toolTip::add( triangleWaveBtn,
			tr( "Click here for a triangle-wave." ) );

	pixmapButton * sqrWaveBtn = new pixmapButton( this, tr( "Square wave" ) );
	sqrWaveBtn->move( waveBtnX+(16*2), waveBtnY );
	sqrWaveBtn->setActiveGraphic( embed::getIconPixmap(
					"square_wave_active" ) );
	sqrWaveBtn->setInactiveGraphic( embed::getIconPixmap(
					"square_wave_inactive" ) );
	toolTip::add( sqrWaveBtn,
			tr( "Click here for a square-wave." ) );

	pixmapButton * roundSqrWaveBtn =
		new pixmapButton( this, tr( "Rounded square wave" ) );
	roundSqrWaveBtn->move( waveBtnX+(16*3), waveBtnY );
	roundSqrWaveBtn->setActiveGraphic( embed::getIconPixmap(
					"round_square_wave_active" ) );
	roundSqrWaveBtn->setInactiveGraphic( embed::getIconPixmap(
					"round_square_wave_inactive" ) );
	toolTip::add( roundSqrWaveBtn,
			tr( "Click here for a square-wave with a rounded end." ) );
	
	pixmapButton * moogWaveBtn =	
		new pixmapButton( this, tr( "Moog wave" ) );
	moogWaveBtn->move( waveBtnX+(16*4), waveBtnY );
	moogWaveBtn->setActiveGraphic(
		embed::getIconPixmap( "moog_saw_wave_active" ) );
	moogWaveBtn->setInactiveGraphic(
		embed::getIconPixmap( "moog_saw_wave_inactive" ) );
	toolTip::add( moogWaveBtn,
			tr( "Click here for a moog-like wave." ) );

	pixmapButton * sinWaveBtn = new pixmapButton( this, tr( "Sine wave" ) );
	sinWaveBtn->move( waveBtnX+(16*5), waveBtnY );
	sinWaveBtn->setActiveGraphic( embed::getIconPixmap(
						"sin_wave_active" ) );
	sinWaveBtn->setInactiveGraphic( embed::getIconPixmap(
						"sin_wave_inactive" ) );
	toolTip::add( sinWaveBtn,
			tr( "Click for a sine-wave." ) );

	pixmapButton * exponentialWaveBtn =
		new pixmapButton( this, tr( "White noise wave" ) );
	exponentialWaveBtn->move( waveBtnX+(16*6), waveBtnY );
	exponentialWaveBtn->setActiveGraphic(
		embed::getIconPixmap( "exp_wave_active" ) );
	exponentialWaveBtn->setInactiveGraphic(
		embed::getIconPixmap( "exp_wave_inactive" ) );
	toolTip::add( exponentialWaveBtn,
			tr( "Click here for an exponential wave." ) );


	pixmapButton * whiteNoiseWaveBtn =
		new pixmapButton( this, tr( "White noise wave" ) );
	whiteNoiseWaveBtn->move( waveBtnX+(16*7), waveBtnY );
	whiteNoiseWaveBtn->setActiveGraphic(
		embed::getIconPixmap( "white_noise_wave_active" ) );
	whiteNoiseWaveBtn->setInactiveGraphic(
		embed::getIconPixmap( "white_noise_wave_inactive" ) );
	toolTip::add( whiteNoiseWaveBtn,
			tr( "Click here for white-noise." ) );

	m_waveBtnGrp = new automatableButtonGroup( this );
	m_waveBtnGrp->addButton( sawWaveBtn );
	m_waveBtnGrp->addButton( triangleWaveBtn );
	m_waveBtnGrp->addButton( sqrWaveBtn );
	m_waveBtnGrp->addButton( roundSqrWaveBtn );
	m_waveBtnGrp->addButton( moogWaveBtn );
	m_waveBtnGrp->addButton( sinWaveBtn );
	m_waveBtnGrp->addButton( exponentialWaveBtn );
	m_waveBtnGrp->addButton( whiteNoiseWaveBtn );

	setAutoFillBackground( true );
	QPalette pal;
	pal.setBrush( backgroundRole(), PLUGIN_NAME::getIconPixmap(
			"artwork" ) );
	setPalette( pal );
}


lb302SynthView::~lb302SynthView()
{
}


void lb302SynthView::modelChanged()
{
	lb302Synth * syn = castModel<lb302Synth>();
	
	m_vcfCutKnob->setModel( &syn->vcf_cut_knob );
	m_vcfResKnob->setModel( &syn->vcf_res_knob );
	m_vcfDecKnob->setModel( &syn->vcf_dec_knob );
	m_vcfModKnob->setModel( &syn->vcf_mod_knob );
	m_slideDecKnob->setModel( &syn->slide_dec_knob );

	m_distKnob->setModel( &syn->dist_knob );
	m_waveBtnGrp->setModel( &syn->wave_shape );
    
	m_slideToggle->setModel( &syn->slideToggle );
	m_accentToggle->setModel( &syn->accentToggle );
	m_deadToggle->setModel( &syn->deadToggle );
	m_db24Toggle->setModel( &syn->db24Toggle );
}



extern "C"
{

// necessary for getting instance out of shared lib
Plugin * PLUGIN_EXPORT lmms_plugin_main( Model *, void * _data )
{

	return( new lb302Synth(
	        static_cast<InstrumentTrack *>( _data ) ) );
}


}

#endif

