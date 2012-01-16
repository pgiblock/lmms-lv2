
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lmms_lv2.h"
#include "uris.h"
#include "oscillator.h"
#include "triple_oscillator.h"

#define NUM_VOICES 64

// PORTS
enum {
	TRIPOSC_CONTROL = 0,
	TRIPOSC_OUT_L = 1,
	TRIPOSC_OUT_R = 2,
	TRIPOSC_OSC1_VOL = 3,
	TRIPOSC_OSC1_PAN = 4,
	TRIPOSC_OSC1_DETUNE_COARSE = 5,
	TRIPOSC_OSC1_DETUNE_FINE_L = 6,
	TRIPOSC_OSC1_DETUNE_FINE_R = 7,
	TRIPOSC_OSC1_PHASE_OFFSET = 8,
	TRIPOSC_OSC1_PHASE_DETUNE = 9,
	TRIPOSC_OSC1_PHASE_RANDOM = 10,
	TRIPOSC_OSC1_WAVE_SHAPE = 11,
	TRIPOSC_OSC1_OSC2_MOD = 12,
	TRIPOSC_OSC2_VOL = 13,
	TRIPOSC_OSC2_PAN = 14,
	TRIPOSC_OSC2_DETUNE_COARSE = 15,
	TRIPOSC_OSC2_DETUNE_FINE_L = 16,
	TRIPOSC_OSC2_DETUNE_FINE_R = 17,
	TRIPOSC_OSC2_PHASE_OFFSET = 18,
	TRIPOSC_OSC2_PHASE_DETUNE = 19,
	TRIPOSC_OSC2_PHASE_RANDOM = 20,
	TRIPOSC_OSC2_WAVE_SHAPE = 21,
	TRIPOSC_OSC2_OSC3_MOD = 22,
	TRIPOSC_OSC3_VOL = 23,
	TRIPOSC_OSC3_PAN = 24,
	TRIPOSC_OSC3_DETUNE_COARSE = 25,
	TRIPOSC_OSC3_DETUNE_FINE_L = 26,
	TRIPOSC_OSC3_DETUNE_FINE_R = 27,
	TRIPOSC_OSC3_PHASE_OFFSET = 28,
	TRIPOSC_OSC3_PHASE_DETUNE = 29,
	TRIPOSC_OSC3_PHASE_RANDOM = 30,
	TRIPOSC_OSC3_WAVE_SHAPE = 31
};


typedef struct {
	// Ports
	float* vol_port;
	float* pan_port;
	float* detune_coarse_port;
	float* detune_fine_l_port;
	float* detune_fine_r_port;
	float* phase_offset_port;
	float* phase_detune_port;
	float* phase_random_port;
	float* wave_shape_port;
	float* modulation_port;

	// Calculated values
	float m_volumeLeft;
	float m_volumeRight;
	// normalized detuning -> x/sampleRate
	float m_detuningLeft;
	float m_detuningRight;
	// normalized offset -> x/360
	float m_phaseOffsetLeft;
	float m_phaseOffsetRight;
} OscillatorUnit;


typedef struct {
	// Standard voice state
	uint8_t    midi_note;
	uint32_t   frame;
	// Plugin voice state
	Oscillator osc_l[3];
	Oscillator osc_r[3];
} TripOscVoice;


typedef struct {
	/* Features */
#ifdef USE_LV2_URID
	LV2_URID_Map*              map;
#else
	LV2_URI_Map_Feature*       map;
#endif

	/* Ports */
	Event_Buffer_t*  event_port;
	float*           out_l_port;
	float*           out_r_port;

	OscillatorUnit   units[3];

	/* URIs TODO: Global*/
	struct {
		URI_t midi_event;
		URI_t atom_message;
	} uris;

	/* Playback state */
	uint32_t frame; // TODO: frame_t
	float    srate;

	TripOscVoice* voices;

} TripleOscillator;


void
voice_steal(TripleOscillator* triposc, uint8_t midi_note) {
	fprintf(stderr, "Stealing voice!!\n");
	for (int i=0; i<NUM_VOICES; ++i) {
		TripOscVoice* v = &triposc->voices[i];
		if (v->midi_note == 0xFF) {
			// Stealing
			v->midi_note = midi_note;
			// Init note
			float freq  = powf(2.0f, ((float)midi_note+3.0f) / 12.0f) * 55.0f;
			for (int i=2; i>=0; --i) {
				OscillatorUnit* u = &triposc->units[i];
				// FIXME: This check won't be needed if we fix oscillator to just hold float* members bound straight to ports
				float mod = (i==2)? 0 : *u->modulation_port;
				// FIXME: Detuning needs to happen occationally even after note-on
				float detune_l = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_l_port) / 1200.0f) / triposc->srate;
				float detune_r = powf( 2.0f, (*u->detune_coarse_port * 100.0f + *u->detune_fine_r_port) / 1200.0f) / triposc->srate;
				osc_reset(&(v->osc_l[i]), *u->wave_shape_port, mod,
				          freq, detune_l, *u->vol_port,
									i==2?NULL:&(v->osc_l[i+1]), *u->phase_offset_port,
									triposc->srate);
				osc_reset(&(v->osc_r[i]), *u->wave_shape_port, mod,
				          freq, detune_r, *u->vol_port,
									i==2?NULL:&(v->osc_r[i+1]), *u->phase_offset_port,
									triposc->srate);
			}
			break;
		}
	}
}
	

void
voice_release(TripleOscillator* triposc, uint8_t midi_note) {
	fprintf(stderr, "Releasing voice\n");
	for (int i=0; i<NUM_VOICES; ++i) {
		TripOscVoice* v = &triposc->voices[i];
		if (v->midi_note == midi_note) {
			v->midi_note = 0xFF;
		}
	}
}

static void
triposc_connect_port(LV2_Handle instance,
                     uint32_t   port,
                     void*      data)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	int oscidx, oscport;

	// Handle Plugin-global ports
	if (port < TRIPOSC_OSC1_VOL) {
		switch (port) {
			case TRIPOSC_CONTROL:
				plugin->event_port = (Event_Buffer_t*)data;
				break;
			case TRIPOSC_OUT_L:
				plugin->out_l_port = (float*)data;
				break;
			case TRIPOSC_OUT_R:
				plugin->out_r_port = (float*)data;
				break;
			default:
				break;
		}
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
	switch (oscport) {
		case TRIPOSC_OSC1_VOL:
			plugin->units[oscidx].vol_port = (float*)data;
			break;
		case TRIPOSC_OSC1_PAN:
			plugin->units[oscidx].pan_port = (float*)data;
			break;
		case TRIPOSC_OSC1_DETUNE_COARSE:
			plugin->units[oscidx].detune_coarse_port = (float*)data;
			break;
		case TRIPOSC_OSC1_DETUNE_FINE_L:
			plugin->units[oscidx].detune_fine_l_port = (float*)data;
			break;
		case TRIPOSC_OSC1_DETUNE_FINE_R:
			plugin->units[oscidx].detune_fine_r_port = (float*)data;
			break;
		case TRIPOSC_OSC1_PHASE_OFFSET:
			plugin->units[oscidx].phase_offset_port = (float*)data;
			break;
		case TRIPOSC_OSC1_PHASE_DETUNE:
			plugin->units[oscidx].phase_detune_port = (float*)data;
			break;
		case TRIPOSC_OSC1_PHASE_RANDOM:
			plugin->units[oscidx].phase_random_port = (float*)data;
			break;
		case TRIPOSC_OSC1_WAVE_SHAPE:
			plugin->units[oscidx].wave_shape_port = (float*)data;
			break;
		case TRIPOSC_OSC1_OSC2_MOD:
			plugin->units[oscidx].modulation_port = (float*)data;
			break;
		default:
			break;
	}
}


static void
triposc_cleanup(LV2_Handle instance)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	free(plugin->voices);
	free(instance);
}


static LV2_Handle
triposc_instantiate(const LV2_Descriptor*     descriptor,
                    double                    rate,
                    const char*               path,
                    const LV2_Feature* const* features)
{
	/* Malloc and initialize new Synth */
	TripleOscillator* plugin = (TripleOscillator*)malloc(sizeof(TripleOscillator));
	if (!plugin) {
		fprintf(stderr, "Could not allocate TripleOscillator.\n");
		return NULL;
	}

	// Malloc voices
	plugin->voices = (TripOscVoice*)malloc(sizeof(TripOscVoice) * NUM_VOICES);
	if (!plugin->voices) {
		fprintf(stderr, "Could not allocate TripleOscillator voices.\n");
		return NULL;
	}
	for (int i=0; i<NUM_VOICES; ++i) {
		plugin->voices[i].midi_note = 0xFF;
	}

	memset(&plugin->uris, 0, sizeof(plugin->uris));

	// TODO: Initialize properly!!
	plugin->srate = rate;

	/*float wave_shape, float modulation_algo,
		float freq, float detuning, float volume,
		struct Oscillator_st* sub_osc, float phase_offset,
		float sample_rate*/
	//plugin->osc = osc_create(0.f, 0.f, 440.f, 1.f, 1.f, NULL, 0.f, plugin->srate);

	/* Scan host features for URID map and map everything */
	for (int i = 0; features[i]; ++i) {
#ifdef USE_LV2_URID
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
			plugin->uris.midi_event = plugin->map->map(
					plugin->map->handle, MIDI_EVENT_URI);
			plugin->uris.atom_message = plugin->map->map(
					plugin->map->handle, ATOM_MESSAGE_URI);
		}
#else
		if (!strcmp(features[i]->URI, LV2_URI_MAP_URI)) {
			fprintf(stderr, "Found URI-Map.");
			plugin->map = (LV2_URI_Map_Feature*)features[i]->data;
			plugin->uris.midi_event = plugin->map->uri_to_id(
					plugin->map->callback_data, NULL, MIDI_EVENT_URI);
			plugin->uris.atom_message = plugin->map->uri_to_id(
					plugin->map->callback_data, NULL, ATOM_MESSAGE_URI);
			fprintf(stderr, "%s -> %d\n", MIDI_EVENT_URI, plugin->uris.midi_event);
			fprintf(stderr, "%s -> %d\n", ATOM_MESSAGE_URI, plugin->uris.atom_message);
		}
#endif
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
triposc_run(LV2_Handle instance,
            uint32_t   sample_count)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;

	uint32_t    pos;
	uint32_t    ev_frames;
	uint32_t    f = plugin->frame;

#ifdef USE_LV2_ATOM
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
#else
	LV2_Event_Iterator ev_i;
	lv2_event_begin(&ev_i, plugin->event_port);
	LV2_Event const * ev = NULL;

	for (pos = 0; pos < sample_count;) {
		// Check for next event
		if (lv2_event_is_valid(&ev_i)) {
			ev        = lv2_event_get(&ev_i, NULL);
			ev_frames = ev->frames;
			lv2_event_increment(&ev_i);
		} else {
			ev = NULL;
			ev_frames = sample_count;
		}
#endif

		float outbuf[2048]; // FIXME: Ugh

		// Run until next event
		// FIXME: This extra arithmetic is stupid to have in this loop
		float *out_l = &plugin->out_l_port[pos];
		float *out_r = &plugin->out_r_port[pos];
		int    outlen = ev_frames-pos;
		for (int f=0; f<outlen; ++f) {
			out_l[f] = out_r[f] = 0.0f;
		}
		for (int i=0; i<NUM_VOICES; ++i) {
			TripOscVoice* v = &plugin->voices[i];
			if (v->midi_note != 0xFF) {
				osc_update(&v->osc_l[0], outbuf, outlen);
				for (int f=0; f<outlen; ++f) {
					out_l[f] += outbuf[f];
				}
				osc_update(&v->osc_r[0], outbuf, outlen);
				for (int f=0; f<outlen; ++f) {
					out_r[f] += outbuf[f];
				}
			}
		}
		pos = ev_frames;


		// Process event
		if (ev) {
#ifdef USE_LV2_ATOM
			if (ev->body.type == plugin->uris.midi_event) {
				uint8_t* const data = (uint8_t* const)(ev + 1);
#else
			if (ev->type == plugin->uris.midi_event || ev->type == 1) {
				uint8_t* const data = (uint8_t* const)(ev) + sizeof(LV2_Event);
#endif
				uint8_t const  cmd  = data[0] & 0xF0;
				//fprintf(stderr, "  cmd=%d data1=%d data2=%d\n", cmd, data[1], data[2]);
				if (cmd == 0x90) {
					// Note On
					voice_steal(plugin, data[1]);
					// TODO: steal voices etc...

				} else if (cmd == 0x80) {
					// Note Off
					// TODO need envelope
					voice_release(plugin, data[1]);
				}
			} else {
#ifdef USE_LV2_ATOM
				fprintf(stderr, "Unknown event type %d\n", ev->body.type);
#else
				fprintf(stderr, "Unknown event type %d (size %d)\n", ev->type, ev->size);
#endif
			}
		}

	}

	// TODO: Remove this stupid shadow?
	plugin->frame = f;
}


static uint32_t
triposc_map_uri(TripleOscillator* plugin, const char* uri)
{
#ifdef USE_LV2_ATOM
	return plugin->map->map(plugin->map->handle, uri);
#else
	return plugin->map->uri_to_id(plugin->map->callback_data, NULL, uri);
#endif
}


static void
triposc_save(LV2_Handle                instance,
             LV2_State_Store_Function  store,
             void*                     callback_data,
             uint32_t                  flags,
             const LV2_Feature* const* features)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	// TODO: store(...)
	printf("TripleOscillator save stub.\n");
}


static void
triposc_restore(LV2_Handle                  instance,
                LV2_State_Retrieve_Function retrieve,
                void*                       callback_data,
                uint32_t                    flags,
                const LV2_Feature* const*   features)
{
	TripleOscillator* plugin = (TripleOscillator*)instance;
	// TODO: store(...)
	printf("TripleOscillator restore stub.\n");
}


const void*
triposc_extension_data(const char* uri)
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
