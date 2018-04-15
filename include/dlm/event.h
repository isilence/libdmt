#ifndef DLM_EVENT_H__
#define DLM_EVENT_H__

#include <dlm/object.h>

/* 	==================================================
 *		event magic
 */

#define DLM_EVENT_INFINITY (~((u32)0))

#define DLM_MAGIC_EVENT_BASE DLM_MAGIC_BASE_CREATE(0xCEE1)
#define DLM_MAGIC_IS_EVENT(magic) \
	(DLM_MAGIC_BASE_EXTRACT(magic) == DLM_MAGIC_EVENT_BASE)
#define DLM_MAGIC_EVENT_CREATE(num) \
	DLM_MAGIC_CREATE(DLM_MAGIC_EVENT_BASE, num)

#define DLM_MAGIC_EVENT_SEQ		DLM_MAGIC_MEM_CREATE(11)
#define DLM_MAGIC_EVENT_LATCH		DLM_MAGIC_MEM_CREATE(22)
#define DLM_MAGIC_EVENT_LATCH_CLB	DLM_MAGIC_MEM_CREATE(23)
#define DLM_MAGIC_EVENT_CL		DLM_MAGIC_MEM_CREATE(33)
#define DLM_MAGIC_EVENT_LIST		DLM_MAGIC_MEM_CREATE(666)

/* 	==================================================
 *		event
 */

typedef void (*event_callback)(struct dlm_event *, void *data);

struct dlm_event_ops {
	int	(*wait)(struct dlm_event *, u32 ms);
	int	(*signal)(struct dlm_event *);
	bool	(*ready)(struct dlm_event *);
	int	(*reg_clb)(struct dlm_event *, event_callback, void *data);
};

struct dlm_event {
	const struct dlm_event_ops *ops;
	struct dlm_obj obj;
};

static inline int dlm_event_wait(struct dlm_event *event, u32 ms)
{
	return event->ops->wait(event, ms);
}

static inline int dlm_event_signal(struct dlm_event *event)
{
	return event->ops->signal(event);
}

static inline bool dlm_event_ready(struct dlm_event *event)
{
	return event->ops->ready(event);
}

static inline int dlm_event_reg_clb(struct dlm_event *event,
				    event_callback clb, void *data)
{
	return event->ops->reg_clb(event, clb, data);
}

static inline void dlm_event_retain(struct dlm_event *event)
{
	dlm_obj_retain(&event->obj);
}

static inline void dlm_event_release(struct dlm_event *event)
{
	dlm_obj_release(&event->obj);
}

#define dlm_obj_to_event(dlm_obj) container_of((dlm_obj), struct dlm_event, obj)


/*	==================================================
 *		event list
 */

struct dlm_event_list {
	struct dlm_obj obj;

	size_t size;
	struct dlm_event *events[];
};

struct dlm_event_list *allocate_event_list(size_t size,
					   struct dlm_obj *master);
int dlm_event_list_wait(struct dlm_event_list *list);

static inline void dlm_event_list_set(struct dlm_event_list *list,
				      struct dlm_event *event,
				      size_t num)
{
	struct dlm_event *old;

	if (num >= list->size)
		return;

	old = list->events[num];
	if (old)
		dlm_event_release(old);
	if (event)
		dlm_event_retain(event);
	list->events[num] = event;
}

static inline bool dlm_event_list_valid(struct dlm_event_list *list)
{
	struct dlm_obj *obj;

	if (list == NULL)
		return false;

	obj = &list->obj;
	return DLM_MAGIC_IS_EVENT(obj->magic);
}

static inline void dlm_event_list_retain(struct dlm_event_list *list)
{
	if (dlm_event_list_valid(list))
		dlm_obj_retain(&list->obj);
}

static inline void dlm_event_list_release(struct dlm_event_list *list)
{
	if (dlm_event_list_valid(list))
		dlm_obj_release(&list->obj);
}

#define dlm_obj_to_event_list(dlm_obj) \
	container_of((dlm_obj), struct dlm_event_list, obj)

#endif /* DLM_EVENT_H__ */
