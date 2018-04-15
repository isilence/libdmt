#include <dlm/event.h>
#include <stdlib.h>

#include "common.h"

static int wait_list_release(struct dlm_obj *obj)
{
	int ret = 0;
	struct dlm_event_list *event = dlm_obj_to_event_list(obj);

	free(event);
	return ret;
}

static const struct dlm_obj_ops obj_ops = {
	.release = wait_list_release,
};


struct dlm_event_list *allocate_event_list(size_t n)
{
	struct dlm_event_list *list;
	size_t size;

	size = sizeof(*list) + sizeof(list->events[0]) * n;
	list = (struct dlm_event_list *)calloc(1, size);
	if (!list)
		return NULL;

	dlm_obj_init(&list->obj);
	dlm_obj_set_ops(&list->obj, &obj_ops);
	list->obj.magic = DLM_MAGIC_EVENT_LIST;

	return list;
}

void dlm_event_list_wait(struct dlm_event_list *list)
{
	size_t i;

	for (i = 0; i < list->size; i += 1)
		if (list->events[i])
			dlm_event_wait(list->events[i], DLM_EVENT_INFINITY);
}