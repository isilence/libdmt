#ifndef DLM_PROVIDERS_CL_EVENT_H__
#define DLM_PROVIDERS_CL_EVENT_H__

#include <dlm/event.h>
#include <dlm/providers/opencl.h>

static inline
struct dlm_event_cl *dlm_extract_event_cl(struct dlm_event *event)
{
	if (!event || event->obj.magic != DLM_MAGIC_EVENT_CL)
		return NULL;

	return dlm_event_to_cl(event);
}

int rc_cl2unix(cl_int err);

struct dlm_event_cl *allocate_event_cl(cl_event clevent);
struct dlm_event_cl *allocate_event_cl_user(struct dlm_mem_cl *mem);

#endif /* DLM_PROVIDERS_CL_EVENT_H__ */
