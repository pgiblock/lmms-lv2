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
#include "envelope_generator_p.h"

static uint32_t envgen_map_uri (EnvelopeGenerator *plugin, const char *uri);

static void
envgen_connect_port (LV2_Handle instance,
                     uint32_t   port,
                     void      *data)
{
	EnvelopeGenerator *plugin = (EnvelopeGenerator *)instance;

	BEGIN_CONNECT_PORTS(port);
	CONNECT_PORT(ENVGEN_DEL, params.del, float);
	CONNECT_PORT(ENVGEN_ATT, params.att, float);
	CONNECT_PORT(ENVGEN_HOLD, params.hold, float);
	CONNECT_PORT(ENVGEN_DEC, params.dec, float);
	CONNECT_PORT(ENVGEN_SUS, params.sus, float);
	CONNECT_PORT(ENVGEN_REL, params.rel, float);
	CONNECT_PORT(ENVGEN_MOD, params.mod, float);
	CONNECT_PORT(ENVGEN_GATE_IN, gate_in_port, float);
	CONNECT_PORT(ENVGEN_TRIGGER, trigger_port, float);
	CONNECT_PORT(ENVGEN_GATE_OUT, gate_out_port, float);
	CONNECT_PORT(ENVGEN_ENV_OUT, env_out_port, float);
	END_CONNECT_PORTS();
}


static void
envgen_cleanup (LV2_Handle instance)
{
	EnvelopeGenerator *plugin = (EnvelopeGenerator *)instance;
	envelope_destroy(plugin->env);
	free(plugin);
}


static LV2_Handle
envgen_instantiate(const LV2_Descriptor      *descriptor,
                   double                     rate,
                   const char                *path,
                   const LV2_Feature * const *features)
{
	/* Malloc and initialize new Synth */
	EnvelopeGenerator *plugin = malloc(sizeof(EnvelopeGenerator));
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
		if (!strcmp(features[i]->URI, LV2_URID_URI "#map")) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
			plugin->uris.midi_event   = envgen_map_uri(plugin, MIDI_EVENT_URI);
			plugin->uris.atom_message = envgen_map_uri(plugin, ATOM_MESSAGE_URI);
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
envgen_run (LV2_Handle instance,
            uint32_t   sample_count)
{
	EnvelopeGenerator *eg = (EnvelopeGenerator *)instance;

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
envgen_map_uri (EnvelopeGenerator *plugin, const char *uri)
{
	return plugin->map->map(plugin->map->handle, uri);
}


static LV2_State_Status
envgen_save (LV2_Handle                 instance,
             LV2_State_Store_Function   store,
	     LV2_State_Handle           handle,
	     uint32_t                   flags,
	     const LV2_Feature * const *features)
{
	printf("Envelope Generator save stub.\n");
	return LV2_STATE_SUCCESS;
}


static LV2_State_Status
envgen_restore (LV2_Handle       instance,
                LV2_State_Retrieve_Function retrieve,
                LV2_State_Handle            handle,
                uint32_t                    flags,
                const LV2_Feature * const  *features)
{
	printf("Envelope Generator restore stub.\n");
	return LV2_STATE_SUCCESS;
}


const void*
envgen_extension_data(const char *uri)
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

