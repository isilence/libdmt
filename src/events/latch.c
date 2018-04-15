#include <dlm/memory.h>
#include <dlm/event.h>
#include <dlm/events/latch.h>
#include <stdlib.h>

#include "common.h"

#define dlm_obj_to_event_latch(dlm_obj) \
	dlm_event_to_latch(dlm_obj_to_event((dlm_obj)))

struct callback_handler {
	struct dlm_obj obj;
	event_callback clb;
	void *data;
};

static void latch_release(struct dlm_obj *obj)
{
	struct dlm_event_latch *event = dlm_obj_to_event_latch(obj);

	(void)pthread_mutex_destroy(&event->mtx);
	free(event);
}

static inline bool obj_latch_ready(struct dlm_event_latch *event)
{
	return atomic_load(&event->ready);
}

static bool latch_ready(struct dlm_event *dlm_event)
{
	struct dlm_event_latch *event = dlm_event_to_latch(dlm_event);

	return obj_latch_ready(event);
}

static int latch_fire(struct dlm_event *dlm_event)
{
	int ret;
	struct dlm_obj *obj, *tmp;
	struct callback_handler *hd;
	struct dlm_event_latch *event = dlm_event_to_latch(dlm_event);

	ret = pthread_mutex_lock(&event->mtx);
	if (!ret)
		return ret;

	atomic_store(&event->ready, true);

	ret = pthread_mutex_unlock(&event->mtx);


	list_for_each_entry_safe(obj, tmp, &dlm_event->obj.deps, node) {
		if (obj->magic != DLM_MAGIC_EVENT_LATCH_CLB)
			continue;

		hd = container_of(obj, struct callback_handler, obj);
		hd->clb(dlm_event, hd->data);
		dlm_obj_release(&hd->obj);
	}

	return ret;
}

static int latch_wait(struct dlm_event *dlm_event, u32 ms)
{
	int ret = 0;
	struct dlm_event_latch *event = dlm_event_to_latch(dlm_event);

	if (ms != DLM_EVENT_INFINITY)
		return -ENOSYS;

	if (latch_ready(dlm_event)) /* double checking */
		return 0;

	ret = pthread_mutex_lock(&event->mtx);
	if (!ret)
		goto exit;

	ret = 0;
	while (!obj_latch_ready(event) && !ret)
		ret = pthread_cond_wait(&event->cond, &event->mtx);

	if (!ret)
		goto unlock;

	return pthread_mutex_unlock(&event->mtx);
unlock:
	pthread_mutex_unlock(&event->mtx);
exit:
	return ret;
}

static void clb_obj_release(struct dlm_obj *clb)
{
	struct callback_handler *hd;

	hd = container_of(clb, struct callback_handler, obj);
	free(hd);
}

struct dlm_obj_ops clb_obj_ops = {
	.release = clb_obj_release,
};

static int latch_reg_clb(struct dlm_event *dlm_event, event_callback clb, void *data)
{
	struct callback_handler *hd;
	struct dlm_event_latch *e = dlm_event_to_latch(dlm_event);

	if (dlm_event_ready(dlm_event)) {
		clb(dlm_event, data);
		return 0;
	}

	hd = (struct callback_handler *)malloc(sizeof(*hd));
	if (!hd)
		return -ENOMEM;

	dlm_obj_init(&hd->obj, &e->event.obj);
	hd->obj.magic = DLM_MAGIC_EVENT_LATCH_CLB;
	dlm_obj_set_ops(&hd->obj, &clb_obj_ops);
	hd->clb = clb;
	hd->data = data;

	return 0;
}

static const struct dlm_obj_ops latch_obj_ops = {
	.release = latch_release,
};

static const struct dlm_event_ops latch_ops = {
	.wait = latch_wait,
	.signal = latch_fire,
	.ready = latch_ready,
	.reg_clb = latch_reg_clb,
};

static int event_init_sync(struct dlm_event_latch *event)
{
	int ret;

	ret = pthread_mutex_init(&event->mtx, NULL);
	if (!ret)
		goto error;

	ret = pthread_cond_init(&event->cond, NULL);
	if (!ret)
		goto mtx_destroy;

	return 0;
mtx_destroy:
	pthread_mutex_destroy(&event->mtx);
error:
	return ret;
}

struct dlm_event_latch *dlm_event_latch_create(struct dlm_obj *master)
{
	int ret;
	struct dlm_event_latch *event;

	event = (struct dlm_event_latch *)malloc(sizeof(*event));
	if (!event)
		goto error;

	ret = event_init_sync(event);
	if (ret)
		goto dealloc;

	dlm_event_init(&event->event, DLM_MAGIC_EVENT_LATCH,
			&latch_ops, &latch_obj_ops, master);
	atomic_init(&event->ready, false);
	dlm_event_retain(&event->event);

	return event;
dealloc:
	free(event);
error:
	return NULL;
}