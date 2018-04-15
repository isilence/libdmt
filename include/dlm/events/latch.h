#ifndef DLM_EVENT_LATCH_H__
#define DLM_EVENT_LATCH_H__

#include <dlm/event.h>
#include <pthread.h>
#include <stdatomic.h>

struct dlm_event_latch {
	struct dlm_event event;

	pthread_mutex_t mtx;
	pthread_cond_t cond;
	atomic_bool ready;
};

struct dlm_event_latch *dlm_event_latch_create(void);

#define dlm_event_to_latch(e) container_of((e), struct dlm_event_latch, event)

#endif /* DLM_EVENT_LATCH_H__ */
