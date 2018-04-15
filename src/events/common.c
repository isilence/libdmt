#include <dlm/event.h>
#include <stdlib.h>

#include "common.h"

static void wait_list_release(struct dlm_obj *obj)
{
	struct dlm_event_list *event = dlm_obj_to_event_list(obj);
	size_t i;

	for (i = 0; i < event->size; i += 1)
		if (event->events[i])
			dlm_event_release(event->events[i]);
	free(event);
}

static const struct dlm_obj_ops obj_ops = {
	.release = wait_list_release,
};


struct dlm_event_list *allocate_event_list(size_t n, struct dlm_obj *master)
{
	struct dlm_event_list *list;
	size_t size;

	size = sizeof(*list) + sizeof(list->events[0]) * n;
	list = (struct dlm_event_list *)calloc(1, size);
	if (!list)
		return NULL;

	dlm_obj_init(&list->obj, master);
	dlm_obj_set_ops(&list->obj, &obj_ops);
	list->obj.magic = DLM_MAGIC_EVENT_LIST;
	list->size = n;
	dlm_obj_retain(&list->obj);

	return list;
}

int dlm_event_list_wait(struct dlm_event_list *list)
{
	int ret;
	size_t i;
	struct dlm_event *e;

	for (i = 0; i < list->size; i += 1) {
		e = list->events[i];
		if (!e)
			continue;

		ret = dlm_event_wait(e, DLM_EVENT_INFINITY);
		if (ret)
			return ret;
	}
	return 0;
}