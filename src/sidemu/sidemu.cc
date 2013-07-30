#include <iostream>
#include <sid.h>

#include "lmms_lv2.h"
#include "uris.h"
#include "sidemu_p.h"
#include "sidemu.h"

// Function implementations...
void
process ()
{
	// Just checking compilation and linking...
	SID sid;
	sid.clock();
}

// Fill in your call-back functions here
// Probably want static member functions that take an instance as
// the LV2_Handle
const LV2_Descriptor sidemu_descriptor = {
	SID_URI,
	NULL, // instantiate,
	NULL, // connect_port,
	NULL, // activate,
	NULL, // run,
	NULL, // deactivate,
	NULL, // cleanup,
	NULL, // extension_data
};

