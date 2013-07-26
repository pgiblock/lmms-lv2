#include <iostream>
#include "lmms_lv2.h"
#include "uris.h"
#include "sid_p.h"
#include "sid.h"

void SidInstrument::fun()
{
	std::cout << "Hello World!" << std::endl;
	*control_port1 = 1.0f;
}

// Fill in your call-back functions here
// Probably want static member functions that take an instance as
// the LV2_Handle
const LV2_Descriptor sid_descriptor = {
	SID_URI,
	NULL, // instantiate,
	NULL, // connect_port,
	NULL, // activate,
	NULL, // run,
	NULL, // deactivate,
	NULL, // cleanup,
	NULL, // extension_data
};

