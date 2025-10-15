#ifndef __ESB_HANDLER_H
#define __ESB_HANDLER_H

#include <zephyr/kernel.h>

#include "../lib/adpcm_lib/dvi_adpcm.h"
#include "../lib/inv_esb_lib/radio.h"
// #include "opus.h"
// #include "opus_application_config.h"




extern int inverse_esb_start(void);

extern void inverse_esb_init(void);


#endif