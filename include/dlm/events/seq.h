#ifndef DLM_EVENT_SEQ_H__
#define DLM_EVENT_SEQ_H__

#include <dlm/event.h>
#include <pthread.h>
#include <stdatomic.h>

struct dlm_event_seq {
	struct dlm_event event;
};

struct dlm_event_seq *dlm_event_seq_create(struct dlm_obj *master);

#define dlm_event_to_seq(e) container_of((e), struct dlm_event_seq, event)

#endif /* DLM_EVENT_SEQ_H__ */
