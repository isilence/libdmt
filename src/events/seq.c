#include <dlm/memory.h>
#include <dlm/event.h>
#include <dlm/events/seq.h>
#include <stdlib.h>

#include "common.h"

#define dlm_obj_to_event_seq(dlm_obj) \
	dlm_event_to_seq(dlm_obj_to_event((dlm_obj)))

static void seq_release(struct dlm_obj *obj)
{
	struct dlm_event_seq *event = dlm_obj_to_event_seq(obj);

	free(event);
}

static inline bool obj_seq_ready(struct dlm_event_seq *event)
{
	return true;
}

static bool seq_ready(struct dlm_event *dlm_event)
{
	struct dlm_event_seq *event = dlm_event_to_seq(dlm_event);

	return obj_seq_ready(event);
}

static int seq_fire(struct dlm_event *dlm_event)
{
	return 0;
}

static int seq_wait(struct dlm_event *dlm_event, u32 ms)
{
	return 0;
}

int seq_reg_clb(struct dlm_event *e, event_callback clb, void *data)
{
	clb(e, data);

	return 0;
}

static const struct dlm_obj_ops seq_obj_ops = {
	.release = seq_release,
};

static const struct dlm_event_ops seq_event_ops = {
	.wait = seq_wait,
	.signal = seq_fire,
	.ready = seq_ready,
	.reg_clb = seq_reg_clb,
};

struct dlm_event_seq *dlm_event_seq_create(struct dlm_obj *master)
{
	struct dlm_event_seq *event;

	event = (struct dlm_event_seq *)malloc(sizeof(*event));
	if (!event)
		return NULL;

	dlm_event_init(&event->event, DLM_MAGIC_EVENT_SEQ,
		       &seq_event_ops, &seq_obj_ops, master);
	dlm_event_retain(&event->event);

	return event;
}