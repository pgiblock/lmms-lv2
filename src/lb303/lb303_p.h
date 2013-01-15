#ifndef LB303_P_H__
#define LB303_P_H__

#include "lmms_lv2.h"

// Envelope Recalculation period
#define LB_ENVINC 64

//
// New config
//
#define LB_24_IGNORE_ENVELOPE   
#define LB_FILTERED 
//#define LB_DECAY
//#define LB_24_RES_TRICK         

#define LB_DIST_RATIO    4.0
#define LB_24_VOL_ADJUST 3.0
//#define LB_DECAY_NOTES


// PORTS
enum {
	LB303_CONTROL   = 0,
	LB303_OUT       = 1,
	LB303_VCF_CUT   = 2,
	LB303_VCF_RES   = 3,
	LB303_VCF_MOD   = 4,
	LB303_VCF_DEC   = 5,
	LB303_SLIDE     = 6,
	LB303_SLIDE_DEC = 7,
	LB303_ACCENT    = 8,
	LB303_DEAD      = 9,
	LB303_DIST      = 10,
	LB303_FILTER    = 11
};

enum {
	LB303_FILTER_IIR2  = 0,
	LB303_FILTER_3POLE = 1
};

typedef struct {
	int   envpos;       // Update counter. Updates when = 0

	float c0,           // c0=e1 on retrigger; c0*=ed every sample; cutoff=e0+c0
	      e0,           // e0 and e1 for interpolation
	      e1,           //
	      rescoeff;     // Resonance coefficient [0.30,9.54]

	// IIR2:
	float d1,           //   d1 and d2 are added back into the sample with 
	      d2;           //   vcf_a and b as coefficients. IIR2 resonance
	                    //   loop.

	float a,            // IIR2 Coefficients for mixing dry and delay.
	      b,            //   Mixing coefficients for the final sound.  
	      c;            //  

	// 3-Filter
	float kfcn, 
	      kp, 
	      kp1, 
	      kp1h, 
	      kres;

	float ay1, 
	      ay2, 
	      aout, 
	      lastin, 
	      value;

} LB303FilterState;


typedef struct {
	/* Features */
	LV2_URID_Map  *map;

	/* Ports */
	LV2_Atom_Sequence *event_port;
	float *output_port;
	float *slide_port;
	float *slide_dec_port;
	float *accent_port;
	float *dead_port;
	float *dist_port;
	float *filter_port;
	float *vcf_cut_port;
	float *vcf_res_port;
	float *vcf_mod_port;
	float *vcf_dec_port;

	/* URIs TODO: Global*/
	struct {
		LV2_URID midi_event;
		LV2_URID atom_message;
	} uris;

	/* Playback state */
	uint32_t frame; // TODO: frame_t
	float    srate;
	uint8_t  midi_note;

	bool  dead;

	float vco_inc,          // Sample increment for the frequency. Creates Sawtooth.
	      vco_c;            // Raw oscillator sample [-0.5,0.5]

	float vco_slide,        // Current value of slide exponential curve. Nonzero=sliding
	      vco_slideinc,     // Slide base to use in next node. Nonzero=slide next note
	      vco_slidebase;    // The base vco_inc while sliding.

	float vca_attack,       // Amp attack 
	      vca_decay,        // Amp decay
	      vca_a0,           // Initial amplifier coefficient 
	      vca_a;            // Amplifier coefficient.

	int   vca_mode;         // 0: attack, 1: decay, 2: idle, 3: never played

	LB303FilterState vcf;		// State of Vcf

} LB303Synth;


void lb303_filter_recalc (LB303Synth *plugin);
void lb303_filter_env_recalc (LB303Synth *plugin);
void lb303_filter_3pole_run (LB303Synth *plugin, float *buf);
void lb303_filter_iir2_run (LB303Synth *plugin, float *buf);



#endif // LB303_H_P__
