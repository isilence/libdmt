#ifndef DLM_MEMORY_OPERATIONS_H__
#define DLM_MEMORY_OPERATIONS_H__

#include <pthread.h>
#include <stdatomic.h>

#include <dlm/memory.h>

int dlm_mem_generic_copy(struct dlm_mem * restrict src,
			 struct dlm_mem * restrict dst);

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic);

static inline void dlm_event_init(struct dlm_event *event)
{
	dlm_obj_init(&event->obj);
	event->ops = NULL;

	INIT_LIST_HEAD(&event->head);
	INIT_LIST_HEAD(&event->deps);
}

struct dlm_mtx_event {
	struct dlm_event event;

	pthread_mutex_t mtx;
	pthread_cond_t cond;
	atomic_bool ready;
};

struct dlm_mtx_event *dlm_event_mtx_create(void);

#define dlm_event_to_mtx(e) container_of((e), struct dlm_mtx_event, event)

#endif /* DLM_MEMORY_OPERATIONS_H__ */
