#include <dlm/memory.h>
#include <stdlib.h>

#include "generic.h"

#define dlm_obj_to_mtx(dlm_obj) dlm_event_to_mtx(dlm_obj_to_event((dlm_obj)))

static int dlm_event_mtx_release(struct dlm_obj *obj)
{
	int ret;
	struct dlm_mtx_event *event = dlm_obj_to_mtx(obj);

	ret = pthread_mutex_destroy(&event->mtx);
	free(event);

	return ret;
}

static inline bool event_mtx_ready(struct dlm_mtx_event *event)
{
	return atomic_load(&event->ready);
}

static bool dlm_event_mtx_ready(struct dlm_event *dlm_event)
{
	struct dlm_mtx_event *event = dlm_event_to_mtx(dlm_event);

	return event_mtx_ready(event);
}

static int dlm_event_mtx_signal(struct dlm_event *dlm_event)
{
	int ret;
	struct dlm_mtx_event *event = dlm_event_to_mtx(dlm_event);

	ret = pthread_mutex_lock(&event->mtx);
	if (!ret)
		return ret;

	atomic_store(&event->ready, true);

	return pthread_mutex_unlock(&event->mtx);
}

static int dlm_event_mtx_wait(struct dlm_event *dlm_event, u32 ms)
{
	int ret = 0;
	struct dlm_mtx_event *event = dlm_event_to_mtx(dlm_event);

	if (ms != DLM_EVENT_INFINITY)
		return -ENOSYS;

	if (dlm_event_mtx_ready(dlm_event)) /* double checking */
		return 0;

	ret = pthread_mutex_lock(&event->mtx);
	if (!ret)
		goto exit;

	ret = 0;
	while (!event_mtx_ready(event) && !ret)
		ret = pthread_cond_wait(&event->cond, &event->mtx);

	if (!ret)
		goto unlock;

	return pthread_mutex_unlock(&event->mtx);
unlock:
	pthread_mutex_unlock(&event->mtx);
exit:
	return ret;
}

static const struct dlm_obj_ops event_mtx_obj_ops = {
	.release = dlm_event_mtx_release,
};

static const struct dlm_event_ops event_mtx_event_ops = {
	.wait = dlm_event_mtx_wait,
	.signal = dlm_event_mtx_signal,
	.ready = dlm_event_mtx_ready,
};

static int event_init_sync(struct dlm_mtx_event *event)
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

struct dlm_mtx_event *dlm_event_mtx_create(void)
{
	int ret;
	struct dlm_mtx_event *event;

	event = (struct dlm_mtx_event *)malloc(sizeof(*event));
	if (!event)
		goto error;

	ret = event_init_sync(event);
	if (ret)
		goto dealloc;

	dlm_event_init(&event->event);
	event->event.ops = &event_mtx_event_ops;
	dlm_obj_set_ops(&event->event.obj, &event_mtx_obj_ops);
	dlm_event_retain(&event->event);
	atomic_init(&event->ready, false);

	return event;
dealloc:
	free(event);
error:
	return NULL;
}