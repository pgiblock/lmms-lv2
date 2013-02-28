#ifndef ENVELOPE_GENERATOR_P_H__
#define ENVELOPE_GENERATOR_P_H__

#include "lmms_lv2.h"
#include "envelope.h"

#define SECS_PER_ENV_SEGMENT (5.0f)

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
	LV2_URID_Map *map;

	/* Ports */
	float *gate_in_port;
	float *trigger_port;
	float *gate_out_port;
	float *env_out_port;

	/* Direct parameter ports */
	EnvelopeParams params;

	/* URIs TODO: Global*/
	struct {
		LV2_URID midi_event;
		LV2_URID atom_message;
	} uris;

	/* Playback state */
	double    srate;
	float     lasto;

	Envelope *env;

} EnvelopeGenerator;


#endif // ENVELOPE_GENERATOR_P_H__
