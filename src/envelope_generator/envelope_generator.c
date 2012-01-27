#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lmms_lv2.h"
#include "uris.h"
#include "envelope_generator.h"

// PORTS
enum {
	ENVGEN_DEL       = 0,
	ENVGEN_ATT       = 1,
	ENVGEN_HOLD      = 2,
	ENVGEN_DEC       = 3,
	ENVGEN_SUS       = 4,
	ENVGEN_REL       = 5,
	ENVGEN_MOD       = 6,
	ENVGEN_GATE_IN   = 7,
	ENVGEN_TRIGGER   = 8,
	ENVGEN_GATE_OUT  = 9,
	ENVGEN_ENV_OUT   = 10
};

// STATES
enum {
	ENV_OFF,
	ENV_DEL,
	ENV_ATT,
	ENV_HOLD,
	ENV_DEC,
	ENV_SUS,
	ENV_REL
};

typedef struct {
	int q;            // State
	uint32_t frame;   // Frame of current state
	uint32_t nframes; // Num frames to run current state
	float    rel_base;// Base value for releasing from
} EnvelopeState;


typedef struct {
	/* Features */
#ifdef USE_LV2_URID
	LV2_URID_Map*              map;
#else
	LV2_URI_Map_Feature*       map;
#endif

	/* Ports */
	float*  gate_in_port;
	float*  trigger_port;
	float*  gate_out_port;
	float*  env_out_port;

	float*  del_port;
	float*  att_port;
	float*  hold_port;
	float*  dec_port;
	float*  sus_port;
	float*  rel_port;
	float*  mod_port;

	/* URIs TODO: Global*/
	struct {
		URI_t midi_event;
		URI_t atom_message;
	} uris;

	/* Playback state */
	double    srate;
	float     lasto;

	EnvelopeState env;
} EnvelopeGenerator;


static void
envgen_connect_port(LV2_Handle instance,
                    uint32_t   port,
                    void*      data)
{
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;

	switch (port) {
		case ENVGEN_DEL:
			plugin->del_port = (float*)data;
			break;
		case ENVGEN_ATT:
			plugin->att_port = (float*)data;
			break;
		case ENVGEN_HOLD:
			plugin->hold_port = (float*)data;
			break;
		case ENVGEN_DEC:
			plugin->dec_port = (float*)data;
			break;
		case ENVGEN_SUS:
			plugin->sus_port = (float*)data;
			break;
		case ENVGEN_REL:
			plugin->rel_port = (float*)data;
			break;
		case ENVGEN_MOD:
			plugin->mod_port = (float*)data;
			break;
		case ENVGEN_GATE_IN:
			plugin->gate_in_port = (float*)data;
			break;
		case ENVGEN_TRIGGER:
			plugin->trigger_port = (float*)data;
			break;
		case ENVGEN_GATE_OUT:
			plugin->gate_out_port = (float*)data;
			break;
		case ENVGEN_ENV_OUT:
			plugin->env_out_port = (float*)data;
			break;
		default:
			break;
	}
}


static void
envgen_cleanup(LV2_Handle instance)
{
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;
	free(plugin);
}


static LV2_Handle
envgen_instantiate(const LV2_Descriptor*     descriptor,
                   double                    rate,
                   const char*               path,
                   const LV2_Feature* const* features)
{
	/* Malloc and initialize new Synth */
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)malloc(sizeof(EnvelopeGenerator));
	if (!plugin) {
		fprintf(stderr, "Could not allocate Envelope Generator.\n");
		return NULL;
	}
	plugin->srate = rate;
	plugin->lasto = 0.0f;

	/* TODO: Move EnvState init */ 
	plugin->env.q = ENV_OFF;
	plugin->env.frame = 0;

	memset(&plugin->uris, 0, sizeof(plugin->uris));

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


// FIXME: What good is this??
static inline float
expKnobVal( float _val )
{
	return ( ( _val < 0 ) ? -_val : _val ) * _val;
}



static void
envgen_run(LV2_Handle instance,
           uint32_t   sample_count)
{
	EnvelopeGenerator* eg = (EnvelopeGenerator*)instance;

	const double frames_per_env_seg = SECS_PER_ENV_SEGMENT * eg->srate;

	float amsum  = 1.0f;
	float amount = 1.0f;

	int i;
	float o = eg->lasto;
	for (i=0; i<sample_count; ++i) {
		// Reset envelope
		if (eg->trigger_port[i] > 0.5f) {
			eg->env.q       = ENV_DEL;
			eg->env.nframes = frames_per_env_seg * expKnobVal(*eg->del_port);
			eg->env.frame   = 0;
		}

		// Trigger release
		if (eg->gate_in_port[i] < 0.5f) {
			switch (eg->env.q) {
				case ENV_OFF:
				case ENV_REL:
					break;
				case ENV_DEL:
					eg->env.q = ENV_OFF;
					break;
				default:
					eg->env.q        = ENV_REL;
					eg->env.nframes  = frames_per_env_seg * expKnobVal(*eg->rel_port);
					eg->env.frame    = 0;
					eg->env.rel_base = o; 
					break;
			}
		}

		// Stupid way
		switch (eg->env.q) {
			case ENV_OFF:
				o = 0.0f;
				break;

			case ENV_DEL:
				// Process
				o = 0.0f;
				// State change
				eg->env.frame++;
				if (eg->env.frame > eg->env.nframes) {
					eg->env.q       = ENV_ATT;
					eg->env.nframes = frames_per_env_seg * expKnobVal(*eg->att_port);
					eg->env.frame   = 0;
				}
				break;

			case ENV_ATT:
				// Process
				o = ((float)eg->env.frame) / (eg->env.nframes);
				// State
				eg->env.frame++;
				if (eg->env.frame >= eg->env.nframes) {
					eg->env.q       = ENV_HOLD;
					eg->env.nframes = frames_per_env_seg * expKnobVal(*eg->hold_port);
					eg->env.frame   = 0;
				}
				break;

			case ENV_HOLD:
				// Process
				o = 1.0f;
				// State
				eg->env.frame++;
				if (eg->env.frame > eg->env.nframes) {
					eg->env.q       = ENV_DEC;
					eg->env.nframes = frames_per_env_seg * expKnobVal((*eg->dec_port)*(*eg->sus_port));
					eg->env.frame   = 0;
				}
				break;

			case ENV_DEC:
				// Process
				o = amsum + eg->env.frame * ((1.0f / eg->env.nframes)*((1.0f-(*eg->sus_port))-1.0f)*amount);
				//o = 1.0f - (1.0f - (*eg->sus_port)) * ((float)eg->env.frame / eg->env.nframes);
				//State
				eg->env.frame++;
				if (eg->env.frame > eg->env.nframes) {
					eg->env.q       = ENV_SUS;
					eg->env.nframes = 0;
					eg->env.frame   = 0;
				}
				break;

			case ENV_SUS:
				o = 1.0f - (*eg->sus_port); // Sustain Level;
				break;

			case ENV_REL:
				// Process
				o = eg->env.rel_base * (1.0f - ((float)eg->env.frame / eg->env.nframes));
				// State
				eg->env.frame++;
				if (eg->env.frame >= eg->env.nframes) {
					eg->env.q = ENV_OFF;
				}
				break;

			default:
				break;
		}

		float mod = *eg->mod_port;
		if (mod >= 0.0f) {
			eg->env_out_port[i] = o * mod;
		}
		else {
			// TODO: Negative mod
		}
		eg->gate_out_port[i] = eg->env_out_port[i] > 0.0f;
	}
	eg->lasto = o;
}


static uint32_t
envgen_map_uri(EnvelopeGenerator* plugin, const char* uri)
{
#ifdef USE_LV2_ATOM
	return plugin->map->map(plugin->map->handle, uri);
#else
	return plugin->map->uri_to_id(plugin->map->callback_data, NULL, uri);
#endif
}


static void
envgen_save(LV2_Handle                instance,
           LV2_State_Store_Function  store,
           void*                     callback_data,
           uint32_t                  flags,
           const LV2_Feature* const* features)
{
	printf("Envelope Generator save stub.\n");
}


static void
envgen_restore(LV2_Handle                  instance,
              LV2_State_Retrieve_Function retrieve,
              void*                       callback_data,
              uint32_t                    flags,
              const LV2_Feature* const*   features)
{
	printf("Envelope Generator restore stub.\n");
}


const void*
envgen_extension_data(const char* uri)
{
	static const LV2_State_Interface state = { envgen_save, envgen_restore };
	if (!strcmp(uri, LV2_STATE_URI)) {
		return &state;
	}
	return NULL;
}


const LV2_Descriptor envelope_generator_descriptor = {
	ENVELOPE_GENERATOR_URI,
	envgen_instantiate,
	envgen_connect_port,
	NULL, // activate,
	envgen_run,
	NULL, // deactivate,
	envgen_cleanup,
	envgen_extension_data
};

