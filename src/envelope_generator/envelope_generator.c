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
	ENVGEN_TRIGGER   = 0,
	ENVGEN_OUT       = 1,
	ENVGEN_ENV_DEL   = 2,
	ENVGEN_ENV_ATT   = 3,
	ENVGEN_ENV_HOLD  = 4,
	ENVGEN_ENV_DEC   = 5,
	ENVGEN_ENV_SUS   = 6,
	ENVGEN_ENV_REL   = 7,
	ENVGEN_ENV_MOD   = 8,
};

typedef struct {
} EnvelopeState;


typedef struct {
	/* Features */
#ifdef USE_LV2_URID
	LV2_URID_Map*              map;
#else
	LV2_URI_Map_Feature*       map;
#endif

	/* Ports */
	float*           trigger_port;
	float*           output_port;

	/* URIs TODO: Global*/
	struct {
		URI_t midi_event;
		URI_t atom_message;
	} uris;

	/* Playback state */
	uint32_t frame; // TODO: frame_t
	float    srate;

	EnvelopeState env;
} EnvelopeGenerator;


static void
envgen_connect_port(LV2_Handle instance,
                    uint32_t   port,
                    void*      data)
{
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;

	switch (port) {
		case ENVGEN_TRIGGER:
			plugin->trigger_port = (float*)data;
			break;
		case ENVGEN_OUT:
			plugin->output_port = (float*)data;
			break;
		default:
			break;
	}
}


static void
envgen_cleanup(LV2_Handle instance)
{
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;
	free(instance);
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
		fprintf(stderr, "Could not allocate LB303Synth.\n");
		return NULL;
	}

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
	EnvelopeGenerator* plugin = (EnvelopeGenerator*)instance;
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

