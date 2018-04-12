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
	u32 magic;

	struct dlm_obj obj;
	const struct dlm_mem_ops *ops;
};

static inline int dlm_mem_retain(struct dlm_mem *mem)
{
	return dlm_obj_retain(&mem->obj);
}

static inline int dlm_mem_release(struct dlm_mem *mem)
{
	dlm_obj_release(&mem->obj);

	return 0;
}

static inline void* dlm_mem_map(struct dlm_mem *mem, enum DLM_MEM_MAP_FLAGS flags)
{
	return mem->ops->map(mem, flags);
}

static inline int dlm_mem_unmap(struct dlm_mem *mem, void *va)
{
	return mem->ops->unmap(mem, va);
}

static inline int dlm_mem_copy(struct dlm_mem *src, struct dlm_mem *dst)
{
	return src->ops->copy(src, dst);
}

#define dlm_mem_to_dlm(memobj, type, magic) container_of((memobj), type, mem)
#define dlm_obj_to_mem(dlm_obj) container_of((dlm_obj), struct dlm_mem, obj)

#endif /* DLM_MEMORY_H__ */
