#ifndef TRIPLE_OSCILLATOR_P_H__
#define TRIPLE_OSCILLATOR_P_H__

#include "lmms_lv2.h"
#include "envelope.h"
#include "lfo.h"
#include "oscillator.h"

// max length of each envelope-segment (e.g. attack)
#define SECS_PER_ENV_SEGMENT 5.0f
// max length of LFO period
#define SECS_PER_LFO_PERIOD 20.0f
// Scale Cutoff frequency
#define CUT_FREQ_MULTIPLIER 6000.0f
// Scale Resonance value
#define RES_MULTIPLIER 2.0f

#define PAN_MAX 100.0f
#define VOL_MAX 100.0f

#define PITCH_BEND_LAG   (0.5)
#define PITCH_BEND_RANGE (1.0) // One octave


// Per-oscillator ports and calculated coefficients
typedef struct oscillator_unit {
	// Ports
	float *vol_port;
	float *pan_port;
	float *detune_coarse_port;
	float *detune_fine_l_port;
	float *detune_fine_r_port;
	float *phase_offset_port;
	float *phase_detune_port;
	float *phase_random_port;
	float *wave_shape_port;
	float *modulation_port;

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


// Plugin voice state
typedef struct triposc_generator {
	Oscillator osc_l[3];
	Oscillator osc_r[3];
} TripOscGenerator;


// The entire instrument
typedef struct triple_oscillator {
	/* Features */
	LV2_URID_Map *map;

	/* Instrument Ports */
	LV2_Atom_Sequence *event_port;
	float *out_l_port;
	float *out_r_port;

	float *filter_enabled_port;
	float *filter_type_port;
	float *filter_cut_port;
	float *filter_res_port;

	EnvelopeParams env_vol_params;
	EnvelopeParams env_cut_params;
	EnvelopeParams env_res_params;

	LfoParams lfo_vol_params;
	LfoParams lfo_cut_params;
	LfoParams lfo_res_params;

	/* Generic instrument stuff */
	Voice *voices;

	float  pitch_bend;        // Pitchbend in-value
	float  pitch_bend_lagged; // Pitchbend out-value

	/* URIs TODO: Global*/
	struct {
		LV2_URID midi_event;
		LV2_URID atom_message;
	} uris;

	/* Generator Ports */
	OscillatorUnit units[3];

	/* Playback state */
	uint32_t frame; // TODO: frame_t
	float    srate;
} TripleOscillator;


#endif // TRIPLE_OSCILLATOR_P_H__
