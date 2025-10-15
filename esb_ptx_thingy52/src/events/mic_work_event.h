#ifndef __MIC_WORK_EVENT_H__
#define __MIC_WORK_EVENT_H__

#include <zephyr/kernel.h>
#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>



enum mic_status_type {
	MIC_STATUS_IDLE=0,
	MIC_STATUS_START,
	MIC_STATUS_STOP
};


struct mic_work_event {
	struct app_event_header header;
	enum mic_status_type type;
};

APP_EVENT_TYPE_DECLARE(mic_work_event);



#endif