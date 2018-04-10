#ifndef DLM_MEMORY_H__
#define DLM_MEMORY_H__

#include <stddef.h>
#include <errno.h>
#include <dlm/list.h>
#include <dlm/compiler.h>
typedef unsigned int u32;

struct dlm_mem_operations;
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

struct dlm_mem_operations {
	void * (*map) (struct dlm_mem *, enum DLM_MEM_MAP_FLAGS flags);
	int (*unmap) (struct dlm_mem *, void *va);
	int (*release) (struct dlm_mem *);
	int (*copy) (	struct dlm_mem * restrict src,
			struct dlm_mem * restrict dst);
};

struct dlm_mem {
	size_t size;
//	u32 flags;
	u32 magic;

	const struct dlm_mem_operations *ops;
};

static inline void* dlm_mem_map(struct dlm_mem *mem, enum DLM_MEM_MAP_FLAGS flags)
{
	return mem->ops->map(mem, flags);
}

static inline void dlm_mem_unmap(struct dlm_mem *mem, void *va)
{
	mem->ops->unmap(mem, va);
}

static inline int dlm_mem_release(struct dlm_mem *mem)
{
	return mem->ops->release(mem);
}

#endif /* DLM_MEMORY_H__ */
