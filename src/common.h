#ifndef DLM_MEMORY_OPERATIONS_H__
#define DLM_MEMORY_OPERATIONS_H__

#include <dlm/memory.h>
#include <dlm/event.h>

int dlm_mem_generic_copy(struct dlm_mem * restrict src,
			 struct dlm_mem * restrict dst);

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic);

static inline void dlm_event_init(struct dlm_event *event,
				  magic_t magic,
				  const struct dlm_event_ops * ops,
				  const struct dlm_obj_ops *obj_ops)
{
	dlm_obj_init(&event->obj);
	dlm_obj_set_ops(&event->obj, obj_ops);
	event->obj.magic = magic;
	event->ops = ops;
}

#endif /* DLM_MEMORY_OPERATIONS_H__ */
