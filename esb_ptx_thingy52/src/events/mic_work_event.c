#include "mic_work_event.h"


static const char *event_type_to_string(enum mic_status_type type)
{
	switch (type) {
	case MIC_STATUS_START:
		return "Micphone start to work. ";

    case MIC_STATUS_STOP:
		return "micphone stop to work. ";

	default:
		return "micphone is idle...";
	}
}

static void log_mic_work_event(const struct app_event_header *aeh)
{
	struct mic_work_event *event = cast_mic_work_event(aeh);

	APP_EVENT_MANAGER_LOG(aeh, "%s event", event_type_to_string(event->type));
}


APP_EVENT_TYPE_DEFINE(mic_work_event,
			      log_mic_work_event,
			      NULL,
			      APP_EVENT_FLAGS_CREATE());
