
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lmms_lv2.h"
#include "cc_filters.h"
#include "uris.h"
#include "envelope.h"
#include "oscillator.h"
#include "voice.h"
#include "triple_oscillator.h"
#include "triple_oscillator_p.h"


static uint32_t
triposc_map_uri (TripleOscillator *plugin, const char *uri)
{
	return plugin->map->map(plugin->map->handle, uri);
}


static void
trip_osc_voice_steal(TripleOscillator* triposc, Voice* v, uint8_t velocity) {
	TripOscGenerator* g = (TripOscGenerator*)v->generator;

	// Init note
	float freq  = powf(2.0f, ((float)v->midi_note-69.0f) / 12.0f) * 440.0f;

	// Reset oscillators backwards, wee...
	for (int i=2; i>=0; --i) {
		OscillatorUnit* u = &triposc->units[i];
		// FIXME: This check won't be needed if we fix oscillator to just hold float* members bound straight to ports
		float mod = (i==2)? 0 : *u->modulation_port;
		// FIXME: Detuning needs to happen occationally even after note-on
		float detune_l = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_l_port) / 1200.0f);
		float detune_r = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_r_port) / 1200.0f);
		// FIXME: Volume/panning changes also need to happen occationally after note-on
		float vol_l, vol_r;
		
		// Combine volume and panning
		if (*u->pan_port >= 0.0f ) {
			vol_l = ( *u->vol_port * (PAN_MAX - *u->pan_port) ) /
			        ( PAN_MAX * VOL_MAX );
			vol_r = *u->vol_port / VOL_MAX;
		}
		else
		{
			vol_l = *u->vol_port / VOL_MAX;
			vol_r = ( *u->vol_port * (PAN_MAX + *u->pan_port) ) /
			        ( PAN_MAX * VOL_MAX);
		}
		// Apply velocity (TODO: Make sure velocity is supposed to be pre-filter)
		vol_l *= ((float)velocity) / 127.0;
		vol_r *= ((float)velocity) / 127.0;

		// FIXME: Yet another thing that *should* change during playback?
		float po_l = (*u->phase_offset_port + *u->phase_detune_port) / 360.0f;
		// FIXME: Double check this code.  Form is different than line above
		float po_r = *u->phase_offset_port / 360.0f;

		osc_reset(&(g->osc_l[i]), *u->wave_shape_port, mod,
		          freq * detune_l, vol_l,
		          i==2?NULL:&(g->osc_l[i+1]), po_l,
		          triposc->srate);
		osc_reset(&(g->osc_r[i]), *u->wave_shape_port, mod,
		          freq * detune_r, vol_r,
		          i==2?NULL:&(g->osc_r[i+1]), po_r,
		          triposc->srate);
	}
}


static void
trip_osc_voice_release(TripleOscillator* triposc, Voice* v) {
	// Noop
}


Voice*
voice_steal(TripleOscillator* triposc, uint8_t midi_note, uint8_t velocity) {
	// We are just going round-robin for now
	// TODO: Prioritize on vol-envelope state
	Voice* v = &triposc->voices[triposc->victim_idx];

	// Stealing
	v->midi_note = midi_note;
	// Would be func-pointer or voice_steal would be called by tovs()
	trip_osc_voice_steal(triposc, v, velocity);

	// Trigger envelopes
	envelope_trigger(v->env_vol);
	envelope_trigger(v->env_cut);
	envelope_trigger(v->env_res);

	// Trigger LFOs
	lfo_trigger(v->lfo_vol);
	// FIXME: Do LFOs _always_ retrigger?
	lfo_trigger(v->lfo_cut);
	lfo_trigger(v->lfo_res);

	// Pick next victim
	triposc->victim_idx = (triposc->victim_idx+1) % NUM_VOICES;
	return v;
}
	

void
voice_release(TripleOscillator* triposc, uint8_t midi_note) {
	for (int i=0; i<NUM_VOICES; ++i) {
		Voice* v = &triposc->voices[i];
		if (v->midi_note == midi_note) {
			envelope_release(v->env_vol);
			envelope_release(v->env_cut);
			envelope_release(v->env_res);
			trip_osc_voice_release(triposc, v);
		}
	}
}


static void
triposc_connect_port (LV2_Handle instance,
                      uint32_t   port,
                      void*      data)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	int oscidx, oscport;

	// Handle Plugin-global ports
	if (port < TRIPOSC_OSC1_VOL || port >= TRIPOSC_ENV_VOL_DEL) {
		BEGIN_CONNECT_PORTS(port);
		CONNECT_PORT(TRIPOSC_CONTROL, event_port, LV2_Atom_Sequence);
		CONNECT_PORT(TRIPOSC_OUT_L, out_l_port, float);
		CONNECT_PORT(TRIPOSC_OUT_R, out_r_port, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_DEL, env_vol_params.del, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_ATT, env_vol_params.att, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_HOLD, env_vol_params.hold, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_DEC, env_vol_params.dec, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_SUS, env_vol_params.sus, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_REL, env_vol_params.rel, float);
		CONNECT_PORT(TRIPOSC_ENV_VOL_MOD, env_vol_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_DEL, lfo_vol_params.del, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_ATT, lfo_vol_params.att, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_SPD, lfo_vol_params.spd, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_SHAPE, lfo_vol_params.shape, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_MOD, lfo_vol_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_VOL_OP, lfo_vol_params.op, float);
		CONNECT_PORT(TRIPOSC_FILTER_ENABLED, filter_enabled_port, float);
		CONNECT_PORT(TRIPOSC_FILTER_TYPE, filter_type_port, float);
		CONNECT_PORT(TRIPOSC_FILTER_CUT, filter_cut_port, float);
		CONNECT_PORT(TRIPOSC_FILTER_RES, filter_res_port, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_DEL, env_cut_params.del, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_ATT, env_cut_params.att, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_HOLD, env_cut_params.hold, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_DEC, env_cut_params.dec, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_SUS, env_cut_params.sus, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_REL, env_cut_params.rel, float);
		CONNECT_PORT(TRIPOSC_ENV_CUT_MOD, env_cut_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_DEL, lfo_cut_params.del, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_ATT, lfo_cut_params.att, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_SPD, lfo_cut_params.spd, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_SHAPE, lfo_cut_params.shape, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_MOD, lfo_cut_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_CUT_OP, lfo_cut_params.op, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_DEL, env_res_params.del, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_ATT, env_res_params.att, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_HOLD, env_res_params.hold, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_DEC, env_res_params.dec, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_SUS, env_res_params.sus, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_REL, env_res_params.rel, float);
		CONNECT_PORT(TRIPOSC_ENV_RES_MOD, env_res_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_DEL, lfo_res_params.del, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_ATT, lfo_res_params.att, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_SPD, lfo_res_params.spd, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_SHAPE, lfo_res_params.shape, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_MOD, lfo_res_params.mod, float);
		CONNECT_PORT(TRIPOSC_LFO_RES_OP, lfo_res_params.op, float);
		END_CONNECT_PORTS();
		return;
	// Calculate osc index of osc-specific ports
	} else if (port < TRIPOSC_OSC2_VOL) {
		oscidx = 0;
		oscport = port;
	} else if (port < TRIPOSC_OSC3_VOL) {
		oscidx = 1;
		oscport = port - (TRIPOSC_OSC2_VOL - TRIPOSC_OSC1_VOL);
	} else {
		oscidx = 2;
		oscport = port - (TRIPOSC_OSC3_VOL - TRIPOSC_OSC1_VOL);
	}

	// Now connect an osc-specific port
	BEGIN_CONNECT_PORTS(oscport);
	CONNECT_PORT(TRIPOSC_OSC1_VOL, units[oscidx].vol_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_PAN, units[oscidx].pan_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_DETUNE_COARSE, units[oscidx].detune_coarse_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_DETUNE_FINE_L, units[oscidx].detune_fine_l_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_DETUNE_FINE_R, units[oscidx].detune_fine_r_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_PHASE_OFFSET, units[oscidx].phase_offset_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_PHASE_DETUNE, units[oscidx].phase_detune_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_PHASE_RANDOM, units[oscidx].phase_random_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_WAVE_SHAPE, units[oscidx].wave_shape_port, float);
	CONNECT_PORT(TRIPOSC_OSC1_OSC2_MOD, units[oscidx].modulation_port, float);
	END_CONNECT_PORTS();
}

static void
triposc_cleanup (LV2_Handle instance)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	free(plugin->voices);
	free(instance);
}


static LV2_Handle
triposc_instantiate (const LV2_Descriptor*     descriptor,
                     double                    rate,
                     const char*               path,
                     const LV2_Feature* const* features)
{
	/* Malloc and initialize new Synth */
	TripleOscillator *plugin = (TripleOscillator*)malloc(sizeof(TripleOscillator));
	if (!plugin) {
		fprintf(stderr, "Could not allocate TripleOscillator.\n");
		return NULL;
	}

	// FIXME: Hardcoding envelope for now. Yuck
	plugin->env_vol_params.time_base =
	plugin->env_cut_params.time_base =
	plugin->env_res_params.time_base = rate * SECS_PER_ENV_SEGMENT;

	// FIXME: What to use for time_base?? 
	plugin->lfo_vol_params.time_base =
	plugin->lfo_cut_params.time_base =
	plugin->lfo_res_params.time_base = rate * SECS_PER_LFO_PERIOD;

	plugin->pitch_bend = plugin->pitch_bend_lagged = 1.0f;

	// FIXME: Leak!
	TripOscGenerator *generators = malloc(sizeof(TripOscGenerator) * NUM_VOICES);

	// Malloc voices
	plugin->voices = malloc(sizeof(Voice) * NUM_VOICES);
	if (!plugin->voices || !generators) {
		fprintf(stderr, "Could not allocate TripleOscillator voices.\n");
		return NULL;
	}
	for (int i=0; i<NUM_VOICES; ++i) {
		plugin->voices[i].midi_note = 0xFF;
		plugin->voices[i].env_vol = envelope_create(&plugin->env_vol_params);
		plugin->voices[i].env_cut = envelope_create(&plugin->env_cut_params);
		plugin->voices[i].env_res = envelope_create(&plugin->env_res_params);
		plugin->voices[i].lfo_vol = lfo_create(&plugin->lfo_vol_params);
		plugin->voices[i].lfo_cut = lfo_create(&plugin->lfo_cut_params);
		plugin->voices[i].lfo_res = lfo_create(&plugin->lfo_res_params);
		plugin->voices[i].filter  = filter_create(rate);

		plugin->voices[i].generator = generators + i;
		// TODO: Another callback voice_alloc and voice_free??

	}
	plugin->victim_idx = 0;

	memset(&plugin->uris, 0, sizeof(plugin->uris));

	// TODO: Initialize properly!!
	plugin->frame      = 0;
	plugin->srate      = rate;

	/*float wave_shape, float modulation_algo,
		float freq, float detuning, float volume,
		struct Oscillator_st* sub_osc, float phase_offset,
		float sample_rate*/
	//plugin->osc = osc_create(0.f, 0.f, 440.f, 1.f, 1.f, NULL, 0.f, plugin->srate);

	/* Scan host features for URID map and map everything */
	for (int i = 0; features[i]; ++i) {
		if (!strcmp(features[i]->URI, LV2_URID__map)) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
		}
	}

	if (!plugin->map) {
		fprintf(stderr, "Host does not support urid:map.\n");
		goto fail;
	}

	plugin->uris.midi_event   = triposc_map_uri(plugin, MIDI_EVENT_URI);
	plugin->uris.atom_message = triposc_map_uri(plugin, ATOM_MESSAGE_URI);

	return (LV2_Handle)plugin;

fail:
	free(plugin);
	return 0;
}


static void
triposc_run (LV2_Handle instance,
             uint32_t   sample_count)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;

	uint32_t    pos;
	uint32_t    ev_frames;
	uint32_t    f = plugin->frame;

	// TODO: Reuse buffers when possible (bendbuf+cut, vol+res)
	float outbuf[2][sample_count];
	float bendbuf[sample_count];
	float envbuf_vol[sample_count];
	float envbuf_cut[sample_count];
	float envbuf_res[sample_count];

	LV2_Atom_Event* ev = lv2_atom_sequence_begin(&plugin->event_port->body);

	for (pos = 0; pos < sample_count;) {
		// Check for next event
		if (!lv2_atom_sequence_is_end(&plugin->event_port->body, plugin->event_port->atom.size, ev)) {
			ev_frames = ev->time.frames;
		} else {
			ev = NULL;
			ev_frames = sample_count;
		}

		// Run until next event
		// FIXME: This extra arithmetic is stupid to have in this loop
		float *out_l = &plugin->out_l_port[pos];
		float *out_r = &plugin->out_r_port[pos];
		int    outlen = ev_frames-pos;

		// Zero the output buffers and fill pitch-bend
		for (int f=0; f<outlen; ++f) {
			out_l[f] = out_r[f] = 0.0f;

			bendbuf[f] = cc_lag(&plugin->pitch_bend_lagged,
			                    plugin->pitch_bend,
			                    PITCH_BEND_LAG);
		}

		// Accumulate voices
		for (int i=0; i<NUM_VOICES; ++i) {
			Voice* v = &plugin->voices[i];

			if (v->midi_note != 0xFF) {
				int active;

				// Calculate envelopes
				active = envelope_run(v->env_vol, envbuf_vol, outlen);
				envelope_run(v->env_cut, envbuf_cut, outlen);
				envelope_run(v->env_res, envbuf_res, outlen);

				// Calculate LFOs
				lfo_run(v->lfo_vol, envbuf_vol, outlen);
				lfo_run(v->lfo_cut, envbuf_cut, outlen);
				lfo_run(v->lfo_res, envbuf_res, outlen);

				TripOscGenerator *g = (TripOscGenerator *)v->generator;
				// TODO: Make a mixing version of the run function
				
				// Generate samples
				osc_aa_update(&g->osc_l[0], outbuf[0], bendbuf, outlen);
				osc_aa_update(&g->osc_r[0], outbuf[1], bendbuf, outlen);

				// Amount to add to value generated by envelope
				// For volume, the envelope is more of a "mix" than a pure mod
				float vol_amt_add = (*plugin->env_vol_params.mod >= 0.0f)
				                    ? 1.0f - *plugin->env_vol_params.mod
				                    : 1.0f;

				// Standard filter
				if (*plugin->filter_enabled_port > 0.5f) {
					// Filter enabled
					for (int f=0; f<outlen; ++f) {
						// TODO: only recalc when needed
						const float cut = expKnobVal(envbuf_cut[f]) * CUT_FREQ_MULTIPLIER
						                  + *plugin->filter_cut_port;
						const float res = envbuf_res[f] * RES_MULTIPLIER
						                  + *plugin->filter_res_port;

						// The actual volume for this sample (squared mix of envelope and 1.0f)
						float out_mod_amt = envbuf_vol[f] + vol_amt_add;
						out_mod_amt = out_mod_amt * out_mod_amt;

						//printf("%f\t%f\t%f\t%f\n", cut, res, outbuf[0][f], outbuf[1][f]);
						// FIXME: Is calc_coeffs supposed to run each frame? or is it based
						// off of one of those arbitrary "every 32 frame" rules or whatever?
						filter_calc_coeffs(v->filter, cut, res);
						out_l[f] +=  filter_get_sample(v->filter, outbuf[0][f], 0) * out_mod_amt;
						out_r[f] +=  filter_get_sample(v->filter, outbuf[1][f], 1) * out_mod_amt;
						//printf("%f %f %f %f %f\n", cut, envbuf_cut[f], *plugin->filter_cut_port, out_l[f], outbuf[0][f] * out_mod_amt);
					}
				} else {
					// No Filter
					for (int f=0; f<outlen; ++f) {
						// The actual volume for this sample (squared mix of envelope and 1.0f)
						float out_mod_amt = envbuf_vol[f] + vol_amt_add;
						out_mod_amt = out_mod_amt * out_mod_amt;

						out_l[f] +=  outbuf[0][f] * out_mod_amt;
						out_r[f] +=  outbuf[1][f] * out_mod_amt;
					}
				}

				// Kill finished voice
				if (!active) {
					v->midi_note = 0xFF;
				}

				/* TODO: Apply default release */
			}
		}
		pos = ev_frames;


		// Process event
		if (ev) {
			if (ev->body.type == plugin->uris.midi_event) {
				uint8_t* const data = (uint8_t* const)(ev + 1);
			  ev = lv2_atom_sequence_next(ev);
				uint8_t const  cmd  = data[0];
				//fprintf(stderr, "  cmd=%d data1=%d data2=%d\n", cmd, data[1], data[2]);
				if (cmd == 0x90) {
					// Note On (probably)
					if (data[2] == 0x00) {
						// Actually a Note Off due to zero velocity
						voice_release(plugin, data[1]);
					} else {
						// Yep, really Note On
						Voice *v = voice_steal(plugin, data[1], data[2]);
						v->filter->type = *plugin->filter_type_port;
					}
				} else if (cmd == 0x80) {
					// Note Off
					// TODO need envelope
					voice_release(plugin, data[1]);
				} else if (cmd == 0xe0) {
					// Pitch Bend
					plugin->pitch_bend = powf(2.0f, ((data[2]/64.0f) - 1.0f) * PITCH_BEND_RANGE);
					//printf("PB 0x%x 0x%x 0x%x 0x%x    %f\n",cmd,data[1],data[2],data[3],plugin->pitch_bend);
				} else {
					//printf("   0x%x 0x%x 0x%x\n",cmd,data[1],data[2]);
				}

			} else {
				fprintf(stderr, "Unknown event type %d\n", ev->body.type);
			}
		}

	}

	// TODO: Remove this stupid shadow?
	plugin->frame = f;
}


static LV2_State_Status
triposc_save (LV2_Handle                instance,
              LV2_State_Store_Function  store,
              LV2_State_Handle          handle,
              uint32_t                  flags,
              const LV2_Feature* const* features)
{
	fprintf(stderr, "TripleOscillator save stub.\n");
	return LV2_STATE_SUCCESS;
}


static LV2_State_Status
triposc_restore (LV2_Handle                  instance,
                 LV2_State_Retrieve_Function retrieve,
                 LV2_State_Handle            handle,
                 uint32_t                    flags,
                 const LV2_Feature* const*   features)
{
	fprintf(stderr, "TripleOscillator restore stub.\n");
	return LV2_STATE_SUCCESS;
}


const void *
triposc_extension_data (const char *uri)
{
	static const LV2_State_Interface state = { triposc_save, triposc_restore };
	if (!strcmp(uri, LV2_STATE_URI)) {
		return &state;
	}
	return NULL;
}


const LV2_Descriptor triple_oscillator_descriptor = {
	TRIPLE_OSCILLATOR_URI,
	triposc_instantiate,
	triposc_connect_port,
	NULL, // activate,
	triposc_run,
	NULL, // deactivate,
	triposc_cleanup,
	triposc_extension_data
};

