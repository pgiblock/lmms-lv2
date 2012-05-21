#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lmms_lv2.h"
#include "uris.h"
#include "envelope.h"
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

	/* Direct parameter ports */
	EnvelopeParams params;

	/* URIs TODO: Global*/
	struct {
		URI_t midi_event;
		URI_t atom_message;
	} uris;

	/* Playback state */
	double    srate;
	float     lasto;

	Envelope *env;

} EnvelopeGenerator;


static void
envgen_connect_port(LV2_Handle instance,
                    uint32_t   port,
                    void*      data)
{
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;

	switch (port) {
		case ENVGEN_DEL:
			plugin->params.del = (float*)data;
			break;
		case ENVGEN_ATT:
			plugin->params.att = (float*)data;
			break;
		case ENVGEN_HOLD:
			plugin->params.hold = (float*)data;
			break;
		case ENVGEN_DEC:
			plugin->params.dec = (float*)data;
			break;
		case ENVGEN_SUS:
			plugin->params.sus = (float*)data;
			break;
		case ENVGEN_REL:
			plugin->params.rel = (float*)data;
			break;
		case ENVGEN_MOD:
			plugin->params.mod = (float*)data;
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
	envelope_destroy(plugin->env);
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

	// TODO: Setting this param like this is a hack
	plugin->params.time_base = rate * SECS_PER_ENV_SEGMENT;

	plugin->env = envelope_create(&plugin->params);

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



static void
envgen_run(LV2_Handle instance,
           uint32_t   sample_count)
{
	EnvelopeGenerator* eg = (EnvelopeGenerator*)instance;

	int i;
	for (i=0; i<sample_count; ++i) {
		// Reset envelope
		if (eg->trigger_port[i] > 0.5f) {
			envelope_trigger(eg->env);
		}

		// release
		if (eg->gate_in_port[i] < 0.5f) {
			envelope_release(eg->env);
		}

		// Stupid way FIXME: Run for the longest run before a trig/rel
		envelope_run(eg->env, &(eg->env_out_port[i]), 1);
		eg->gate_out_port[i] = eg->env_out_port[i] > 0.0f;
	}
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


static LV2_State_Status
envgen_save(LV2_Handle        instance,
		LV2_State_Store_Function  store,
		LV2_State_Handle          handle,
		uint32_t                  flags,
		const LV2_Feature* const* features)
{
	printf("Envelope Generator save stub.\n");
	return LV2_STATE_SUCCESS;
}


static LV2_State_Status
envgen_restore(LV2_Handle       instance,
		LV2_State_Retrieve_Function retrieve,
		LV2_State_Handle            handle,
		uint32_t                    flags,
		const LV2_Feature* const*   features)
{
	printf("Envelope Generator restore stub.\n");
	return LV2_STATE_SUCCESS;
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

