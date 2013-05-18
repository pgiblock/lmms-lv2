/*
 * lb303.cpp - implementation of class LB303 which is a bass synth attempting 
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lmms_lv2.h"
#include "lmms_math.h"
#include "uris.h"
#include "lb303.h"
#include "lb303_p.h"


static uint32_t lb303_map_uri (LB303Synth *plugin, const char *uri);


static void
lb303_connect_port (LV2_Handle  instance,
                    uint32_t    port,
                    void       *data)
{
	LB303Synth *plugin = (LB303Synth *)instance;

	BEGIN_CONNECT_PORTS(port);
	CONNECT_PORT(LB303_CONTROL, event_port, LV2_Atom_Sequence);
	CONNECT_PORT(LB303_OUT, output_port, float);
	CONNECT_PORT(LB303_VCF_CUT, vcf_cut_port, float);
	CONNECT_PORT(LB303_VCF_RES, vcf_res_port, float);
	CONNECT_PORT(LB303_VCF_MOD, vcf_mod_port, float);
	CONNECT_PORT(LB303_VCF_DEC, vcf_dec_port, float);
	CONNECT_PORT(LB303_SLIDE, slide_port, float);
	CONNECT_PORT(LB303_SLIDE_DEC, slide_dec_port, float);
	CONNECT_PORT(LB303_ACCENT, accent_port, float);
	CONNECT_PORT(LB303_DEAD, dead_port, float);
	CONNECT_PORT(LB303_DIST, dist_port, float);
	CONNECT_PORT(LB303_FILTER, filter_port, float);
	END_CONNECT_PORTS();
}


static void
lb303_cleanup (LV2_Handle instance)
{
	LB303Synth *plugin = (LB303Synth *)instance;
	free(plugin);
}


static LV2_Handle
lb303_instantiate (const LV2_Descriptor     *descriptor,
                   double                    rate,
                   const char               *path,
                   const LV2_Feature* const *features)
{
	/* Malloc and initialize new Synth */
	LB303Synth *plugin = (LB303Synth *)malloc(sizeof(LB303Synth));
	if (!plugin) {
		fprintf(stderr, "Could not allocate LB303Synth.\n");
		return NULL;
	}

	memset(&plugin->uris, 0, sizeof(plugin->uris));

	plugin->vco_c   = 0.0f;
	plugin->vco_slide =0.0f;
	plugin->vco_slideinc = 0.0f;
	plugin->vco_inc = 0.0f;

	/* TODO: Move to constants or ControlPorts */
	plugin->vca_attack = 1.0 - 0.96406088; /* = 1.0 - 0.94406088; */
	plugin->vca_decay  = 0.99897516;

	plugin->srate = rate;

	plugin->vcf.c0 = 0.0f;
	plugin->vcf.e0 = 0.0f;
	plugin->vcf.e1 = 0.0f;
	plugin->vcf.rescoeff = 0.0f;
	plugin->vcf.d1 = 0.0f;
	plugin->vcf.d2 = 0.0f;
	plugin->vcf.a = 0.0f;
	plugin->vcf.b = 0.0f;
	plugin->vcf.c = 1.0f;

	plugin->vcf.kfcn = 0.0f; 
	plugin->vcf.kp = 0.0f; 
	plugin->vcf.kp1 = 0.0f; 
	plugin->vcf.kp1h = 0.0f; 
	plugin->vcf.kres = 0.0f;
	plugin->vcf.ay1 = 0.0f; 
	plugin->vcf.ay2 = 0.0f; 
	plugin->vcf.aout = 0.0f; 
	plugin->vcf.lastin = 0.0f; 
	plugin->vcf.value = 0.0f;

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
			plugin->map = (LV2_URID_Map *)features[i]->data;
			plugin->uris.midi_event   = lb303_map_uri(plugin, MIDI_EVENT_URI);
			plugin->uris.atom_message = lb303_map_uri(plugin, ATOM_MESSAGE_URI);
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
lb303_run (LV2_Handle instance,
           uint32_t   sample_count)
{
	LB303Synth *plugin = (LB303Synth *)instance;
	float      *output = plugin->output_port;

	uint32_t    pos;
	uint32_t    ev_frames;
	uint32_t    f = plugin->frame;

	LV2_Atom_Event *ev = lv2_atom_sequence_begin(&plugin->event_port->body);

	for (pos = 0; pos < sample_count;) {
		// Check for next event
		if (!lv2_atom_sequence_is_end(&plugin->event_port->body, plugin->event_port->atom.size, ev)) {
			ev_frames = ev->time.frames;
		} else {
			ev = NULL;
			ev_frames = sample_count;
		}

		// Run until next event
		for (; pos < ev_frames; ++pos, ++f) {
			// Process
			
			if (plugin->vcf.envpos == 0) {
				
				lb303_filter_env_recalc(plugin);

				plugin->vcf.envpos = LB_ENVINC;

				if (plugin->vco_slide) {
					plugin->vco_inc = plugin->vco_slidebase - plugin->vco_slide;
					
					// Calculate coeff from dec_knob on knob change.
					plugin->vco_slide *= 0.9 + (*plugin->slide_dec_port * 0.0999); // TODO: Adjust for Hz and ENVINC
				}
			}
			else {
				plugin->vcf.envpos--;
			}

			// update vco
			plugin->vco_c += plugin->vco_inc;

			if (plugin->vco_c > 0.5) {
				plugin->vco_c -= 1.0;
			}

			//if (f < 100) printf("vco_c = %f,  vco_inc = %f,  vca_a = %f",
			//                    plugin->vco_c, plugin->vco_inc, plugin->vca_a);
			// TODO: Stupid hack to make filter printfs stop at the right time
			plugin->frame = f;

			// Apply envelope 
			output[pos] = plugin->vco_c * plugin->vca_a;

			// Filter
			switch ((int)*plugin->filter_port) {
			case LB303_FILTER_IIR2:
				lb303_filter_iir2_run(plugin, &(output[pos]));
				break;
			case LB303_FILTER_3POLE:
				lb303_filter_3pole_run(plugin, &(output[pos]));
				break;
			default:
				fprintf(stderr, "Invalid filter index.\n");
				break;
			}

			// Handle Envelope
			if (plugin->vca_mode==0) {
				plugin->vca_a += (plugin->vca_a0 - plugin->vca_a) * plugin->vca_attack;
				if (f >= 0.5 * plugin->srate) {
					plugin->vca_mode = 2;
				}
			}
			else if (plugin->vca_mode == 1) {
				plugin->vca_a *= plugin->vca_decay;
			}
		}

		// the following line actually speeds up processing
		if (plugin->vca_a < (1/65536.0)) {
			plugin->vca_a = 0;
			plugin->vca_mode = 3;
		}

		// Process event
		if (ev) {
			if (ev->body.type == plugin->uris.midi_event) {
				uint8_t * const data = (uint8_t * const)(ev + 1);
				uint8_t const  cmd  = data[0] & 0xF0;
				//fprintf(stderr, "  cmd=%d data1=%d data2=%d\n", cmd, data[1], data[2]);
				if (cmd == 0x90) {
					// Note On
					float freq        = powf(2.0f, ((float)data[1]+3.0f) / 12.0f) * 27.5f/2.0f;
					plugin->midi_note = data[1];
					plugin->vco_inc   = freq / plugin->srate;
					plugin->dead      = (*plugin->dead_port) > 0.0f;

					//TODO: catch_decay = 0;

					// Always reset vca on non-dead notes, and
					// Only reset vca on decaying(decayed) and never-played
					if (!plugin->dead || (plugin->vca_mode == 1 || plugin->vca_mode==3)) {
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
						plugin->vco_slide     = plugin->vco_inc - plugin->vco_slideinc; // Slide amount
						plugin->vco_slidebase = plugin->vco_inc;                        // The REAL frequency
						plugin->vco_slideinc  = 0;                                      // reset from-note
					}
					else {
						plugin->vco_slide = 0;
					}
					// End break-out

					// Slide-from note, save inc for next note
					if (true || /* FIXME: HACK!! */ *plugin->slide_port > 0.0f) {
						plugin->vco_slideinc = plugin->vco_inc; // May need to equal vco_slidebase+vco_slide if last note slid
					}

					/*
					// TODO:FIXME: SUPER HACK TO JUST SEE IF I CAN GET IIR2 WORKING AGAIN!!!
					plugin->vcf.a = 0;
					plugin->vcf.b = 0;
					plugin->vcf.c = 1;
					plugin->vcf.d1 = 0;
					plugin->vcf.d2 = 0;
					*/

					// TODO: recalcFilter(); ...
					lb303_filter_recalc(plugin);

					// TODO: is this the only place n->dead is used?
					// FIXME: How is this not being entered on first-note???  
					if (plugin->dead == 0){
						// Swap next two blocks??
						plugin->vcf.c0 = plugin->vcf.e1;
						// Ensure envelope is recalculated
						plugin->vcf.envpos = 0;

						// Double Check 
						//vca_mode = 0;
						//vca_a = 0.0;
					}

				} else if (cmd == 0x80) {
					// Note Off
					if (plugin->midi_note == data[1]) {
						plugin->vca_mode = 1;
					}
				}
			} else {
				fprintf(stderr, "lmms-lv2: Unknown event type: %d\n", ev->body.type);
			}
			ev = lv2_atom_sequence_next(ev);
		}

	}

	// TODO: Remove this stupid shadow?
	plugin->frame = f;
}


static uint32_t
lb303_map_uri (LB303Synth *plugin, const char *uri)
{
	return plugin->map->map(plugin->map->handle, uri);
}


static LV2_State_Status
lb303_save (LV2_Handle                 instance,
            LV2_State_Store_Function   store,
            LV2_State_Handle           handle,
            uint32_t                   flags,
            const LV2_Feature * const *features)
{
	// TODO: store(...)
	printf("LB303 save stub.\n");
	return LV2_STATE_SUCCESS;
}


static LV2_State_Status
lb303_restore (LV2_Handle                  instance,
               LV2_State_Retrieve_Function retrieve,
               LV2_State_Handle            handle,
               uint32_t                    flags,
               const LV2_Feature * const  *features)
{
	// TODO: retrieve(...)
	printf("LB303 restore stub.\n");
	return LV2_STATE_SUCCESS;
}


const void *
lb303_extension_data (const char *uri)
{
	static const LV2_State_Interface state = { lb303_save, lb303_restore };
	if (!strcmp(uri, LV2_STATE_URI)) {
		return &state;
	}
	return NULL;
}


const LV2_Descriptor lb303_descriptor = {
	LB303_SYNTH_URI,
	lb303_instantiate,
	lb303_connect_port,
	NULL, // activate,
	lb303_run,
	NULL, // deactivate,
	lb303_cleanup,
	lb303_extension_data
};


#define DECAY_FROM_PORT(val, srate) (pow(0.1, 1.0/((0.2 + (2.3*(val))) * (srate)) * LB_ENVINC))
/*  float d = 0.2 + (2.3*vcf_dec_knob.value());
    d *= engine::getMixer()->processingSampleRate();
    fs.envdecay = pow(0.1, 1.0/d * ENVINC);  // decay is 0.1 to the 1/d * ENVINC
	                                           // vcf_envdecay is now adjusted for both
	                                           // sampling rate and ENVINC
*/


void
lb303_filter_iir2_recalc (LB303Synth *p)
{
	p->vcf.e1 = exp(6.109 + 1.5876*(*p->vcf_mod_port) + 2.1553*(*p->vcf_cut_port) - 1.2*(1.0-(*p->vcf_res_port)));
	p->vcf.e0 = exp(5.613 - 0.8*(*p->vcf_mod_port) + 2.1553*(*p->vcf_cut_port) - 0.7696*(1.0-(*p->vcf_res_port)));
	p->vcf.e0*= M_PI / p->srate;
	p->vcf.e1*= M_PI / p->srate;
	p->vcf.e1-= p->vcf.e0;

	// TODO: Double check to see if this is IIR2 only..
	p->vcf.rescoeff = exp(-1.20 + 3.455*(*p->vcf_res_port));
}


void
lb303_filter_iir2_env_recalc (LB303Synth *p)
{
	float k, w;

	p->vcf.c0 *= DECAY_FROM_PORT(*p->vcf_dec_port, p->srate);  // Filter Decay. vcf_decay is adjusted for Hz and ENVINC
	// vcf_rescoeff = exp(-1.20 + 3.455*(fs->reso)); moved above

	w = p->vcf.e0 + p->vcf.c0;          // e0 is adjusted for Hz and doesn't need ENVINC
	k = exp(-w / p->vcf.rescoeff);     // Does this mean c0 is inheritantly?

	p->vcf.a = 2.0*cos(2.0*w) * k;
	p->vcf.b = -k*k;
	p->vcf.c = 1.0 - p->vcf.a - p->vcf.b;
}


void
lb303_filter_iir2_run (LB303Synth *p, float *sampl)
{
	float tmp;
	tmp    = p->vcf.a * p->vcf.d1 +
	         p->vcf.b * p->vcf.d2 +
	         p->vcf.c * *sampl;

	
	/*
	if (p->frame < 100)
	printf("%f = a[%f] * d1[%f] + "
			       " b[%f] * d2[%f] + "
				     " c[%f] * sa[%f]\n",
				 tmp, p->vcf.a, p->vcf.d1,
				      p->vcf.b, p->vcf.d2,
							p->vcf.c, *sampl);
	*/

	// Delayed samples for filter
	p->vcf.d2 = p->vcf.d1;
	p->vcf.d1 = tmp;

	*sampl = tmp;
}


void
lb303_filter_3pole_recalc (LB303Synth *p)
{
	p->vcf.e0 = 0.000001;
	p->vcf.e1 = 1.0;
}


// TODO: Try using k instead of vcf_reso
void
lb303_filter_3pole_env_recalc (LB303Synth *p)
{
	float w, k;
	float kfco;

	p->vcf.c0 *= DECAY_FROM_PORT(*p->vcf_dec_port, p->srate);  // Filter Decay. vcf_decay is adjusted for Hz and ENVINC
	// vcf_rescoeff = exp(-1.20 + 3.455*(fs->reso)); moved above

	// e0 is adjusted for Hz and doesn't need ENVINC
	w = p->vcf.e0 + p->vcf.c0;
	k = (*p->vcf_cut_port > 0.975)?0.975:*p->vcf_cut_port;
	kfco = 50.f + (k)*((2300.f-1600.f*(*p->vcf_mod_port))+(w) *
	                   (700.f+1500.f*(k)+(1500.f+(k)*(p->srate/2.f-6000.f)) * 
	                   (*p->vcf_mod_port)) );
	//+iacc*(.3+.7*kfco*kenvmod)*kaccent*kaccurve*2000

#ifdef LB_24_IGNORE_ENVELOPE
	// kfcn = fs->cutoff;
	p->vcf.kfcn = 2.0 * kfco / p->srate;
#else
	p->vcf.kfcn = w;
#endif
	p->vcf.kp   = ((-2.7528*p->vcf.kfcn + 3.0429)*p->vcf.kfcn + 1.718)*p->vcf.kfcn - 0.9984;
	p->vcf.kp1  = p->vcf.kp+1.0;
	p->vcf.kp1h = 0.5*p->vcf.kp1;
#ifdef LB_24_RES_TRICK
	k = exp(-w/p->vcf.rescoeff);
	p->vcf.kres = (((k))) * (((-2.7079*p->vcf.kp1 + 10.963)*p->vcf.kp1 - 14.934)*p->vcf.kp1 + 8.4974);
#else
	p->vcf.kres = (((*p->vcf_res_port))) * (((-2.7079*p->vcf.kp1 + 10.963)*p->vcf.kp1 - 14.934)*p->vcf.kp1 + 8.4974);
#endif
	p->vcf.value = 1.0+( (*p->dist_port) *(1.5 + 2.0*p->vcf.kres*(1.0-p->vcf.kfcn))); // ENVMOD was DIST
}


void
lb303_filter_3pole_run (LB303Synth *p, float *sampl)
{
	float ax1  = p->vcf.lastin;
	float ay11 = p->vcf.ay1;
	float ay31 = p->vcf.ay2;

	p->vcf.lastin  = (*sampl) - tanh(p->vcf.kres * p->vcf.aout);
	p->vcf.ay1     = p->vcf.kp1h * (p->vcf.lastin+ax1) - (p->vcf.kp * p->vcf.ay1);
	p->vcf.ay2     = p->vcf.kp1h * (p->vcf.ay1 + ay11) - (p->vcf.kp * p->vcf.ay2);
	p->vcf.aout    = p->vcf.kp1h * (p->vcf.ay2 + ay31) - (p->vcf.kp * p->vcf.aout);

	*sampl = tanh(p->vcf.aout * p->vcf.value) * LB_24_VOL_ADJUST / (1.0 + (*p->dist_port));
}


void
lb303_filter_recalc (LB303Synth *plugin)
{
	switch ((int)(*plugin->filter_port)) {
	case LB303_FILTER_IIR2:
		lb303_filter_iir2_recalc(plugin);
		break;
	case LB303_FILTER_3POLE:
		lb303_filter_3pole_recalc(plugin);
		break;
	}
}


void
lb303_filter_env_recalc (LB303Synth *plugin)
{
	switch ((int)(*plugin->filter_port)) {
	case LB303_FILTER_IIR2:
		lb303_filter_iir2_env_recalc(plugin);
		break;
	case LB303_FILTER_3POLE:
		lb303_filter_3pole_env_recalc(plugin);
		break;
	}
}
