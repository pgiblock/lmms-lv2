#ifndef BLEP_H__
#define BLEP_H__

typedef struct BlepState_st {
	int   ptr;	// Increments from 0..8 (BLEPSIZE/BLEPLEN?) 8: Done!
  float vol;	
	float phs;
	int   typ;
} BlepState;

void blep_init (float *blep, float *blamp, int n);
void blep_state_init (BlepState* st);

#endif // BLEP_H__
