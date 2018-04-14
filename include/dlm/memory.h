#ifndef DLM_MEMORY_H__
#define DLM_MEMORY_H__

#include <stddef.h>
#include <errno.h>
#include <dlm/list.h>
#include <dlm/compiler.h>
#include <dlm/object.h>
typedef unsigned int u32;

struct dlm_mem_ops;
struct dlm_mem;
struct dlm_event_ops;
struct dlm_event;

enum DLM_MEM_LINK_TYPE {
	DLM_MEM_LINK_TYPE_FALLBACK = 0,
	DLN_MEM_LINK_TYPE_SYS,
	DLM_MEM_LINK_TYPE_P2P,
};

enum DLM_MEM_MAP_FLAGS {
	DLM_MAP_READ	= 0x1,
	DLM_MAP_WRITE	= 0x2,
};

struct dlm_mem_ops {
	void * (*map) (struct dlm_mem *, enum DLM_MEM_MAP_FLAGS flags);
	int (*unmap) (struct dlm_mem *, void *va);
	int (*copy) (	struct dlm_mem * restrict src,
			struct dlm_mem * restrict dst);
};

struct dlm_mem {
	size_t size;

	struct dlm_obj obj;
	const struct dlm_mem_ops *ops;
};

static inline magic_t dlm_mem_get_magic(struct dlm_mem *mem)
{
	magic_t magic = mem->obj.magic;

	return magic;
}

static inline bool dlm_obj_is_mem(struct dlm_obj *obj)
{
	magic_t magic = obj->magic;
	bool res = DLM_MAGIC_IS_MEM(magic);

	return res;
};

static inline int dlm_mem_retain(struct dlm_mem *mem)
{
	if (!DLM_MAGIC_IS_MEM(dlm_mem_get_magic(mem)))
		return -EINVAL;

	return dlm_obj_retain(&mem->obj);
}

static inline int dlm_mem_release(struct dlm_mem *mem)
{
	if (!DLM_MAGIC_IS_MEM(dlm_mem_get_magic(mem)))
		return -EINVAL;

	return dlm_obj_release(&mem->obj);
}

static inline void *dlm_mem_map(struct dlm_mem *mem, enum DLM_MEM_MAP_FLAGS flags)
{
	if (!DLM_MAGIC_IS_MEM(dlm_mem_get_magic(mem)))
		return NULL;

	return mem->ops->map(mem, flags);
}

static inline int dlm_mem_unmap(struct dlm_mem *mem, void *va)
{
	if (!DLM_MAGIC_IS_MEM(dlm_mem_get_magic(mem)))
		return -EINVAL;

	return mem->ops->unmap(mem, va);
}

static inline int dlm_mem_copy(struct dlm_mem *src, struct dlm_mem *dst)
{
	if (!DLM_MAGIC_IS_MEM(dlm_mem_get_magic(src))
		|| !DLM_MAGIC_IS_MEM(dlm_mem_get_magic(dst)))
		return -EINVAL;

	return src->ops->copy(src, dst);
}

#define dlm_mem_to_dlm(memobj, type, magic) container_of((memobj), type, mem)
#define dlm_obj_to_mem(dlm_obj) container_of((dlm_obj), struct dlm_mem, obj)

/*
 * Events
 */

#define DLM_EVENT_INFINITY (~((u32)0))

struct dlm_event_ops {
	int	(*wait)(struct dlm_event *, u32 ms);
	int	(*signal)(struct dlm_event *);
	bool	(*ready)(struct dlm_event *);
};

struct dlm_event {
	list_head_t head;
	list_head_t deps;

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

static inline int dlm_event_retain(struct dlm_event *mem)
{
	return dlm_obj_retain(&mem->obj);
}

static inline int dlm_event_release(struct dlm_event *mem)
{
	return dlm_obj_release(&mem->obj);
}

#define dlm_obj_to_event(dlm_obj) container_of((dlm_obj), struct dlm_event, obj)

#endif /* DLM_MEMORY_H__ */
