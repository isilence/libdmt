#ifndef DLM_MEMORY_H__
#define DLM_MEMORY_H__

#include <env/dlm/compiler.h>
#include <dlm/object.h>

/* memory */

#define DLM_MAGIC_MEM_BASE DLM_MAGIC_BASE_CREATE(0xDEE9)
#define DLM_MAGIC_IS_MEM(magic) \
	(DLM_MAGIC_BASE_EXTRACT(magic) == DLM_MAGIC_MEM_BASE)
#define DLM_MAGIC_MEM_CREATE(num) \
	DLM_MAGIC_CREATE(DLM_MAGIC_MEM_BASE, num)

#define DLM_MAGIC_MEM_VMS	DLM_MAGIC_MEM_CREATE(11)
#define DLM_MAGIC_MEM_OPENCL	DLM_MAGIC_MEM_CREATE(22)
#define DLM_MAGIC_MEM_IB	DLM_MAGIC_MEM_CREATE(33)

enum DLM_MEM_LINK_TYPE {
	DLM_MEM_LINK_TYPE_FALLBACK = 0,
	DLN_MEM_LINK_TYPE_SYS,
	DLM_MEM_LINK_TYPE_P2P,
};

enum DLM_MEM_MAP_FLAGS {
	DLM_MAP_READ	= 0x1,
	DLM_MAP_WRITE	= 0x2,
};

struct dlm_sync {
	struct dlm_event_list *waitfor;

	/* if set return event in \event */
	bool sync;
	struct dlm_event *event;
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
	return mem->obj.magic;
}

static inline bool dlm_obj_is_mem(struct dlm_obj *obj)
{
	return DLM_MAGIC_IS_MEM(obj->magic);
}

static inline bool dlm_mem_copy_size_valid(struct dlm_mem *restrict src,
					   struct dlm_mem *restrict dst)
{
	return (dst->size >= src->size);
}

static inline bool dlm_mem_valid(struct dlm_mem *mem)
{
	return (mem != NULL) && dlm_obj_is_mem(&mem->obj);
}

static inline int dlm_mem_retain(struct dlm_mem *mem)
{
	if (!dlm_mem_valid(mem))
		return -EINVAL;

	return dlm_obj_retain(&mem->obj);
}

static inline int dlm_mem_release(struct dlm_mem *mem)
{
	if (!dlm_mem_valid(mem))
		return -EINVAL;

	return dlm_obj_release(&mem->obj);
}

static inline void *dlm_mem_map(struct dlm_mem *mem, enum DLM_MEM_MAP_FLAGS flags)
{
	if (!dlm_mem_valid(mem))
		return NULL;

	return mem->ops->map(mem, flags);
}

static inline int dlm_mem_unmap(struct dlm_mem *mem, void *va)
{
	if (!dlm_mem_valid(mem))
		return -EINVAL;

	return mem->ops->unmap(mem, va);
}

static inline int dlm_mem_copy(struct dlm_mem *src,
			       struct dlm_mem *dst)
{
	if (!dlm_mem_valid(src) || !dlm_mem_valid(dst) || src == dst)
		return -EINVAL;
	if (!dlm_mem_copy_size_valid(src, dst))
		return -ENOSPC;

	return src->ops->copy(src, dst);
}

#define dlm_mem_to_dlm(memobj, type, magic) container_of((memobj), type, mem)
#define dlm_obj_to_mem(dlm_obj) container_of((dlm_obj), struct dlm_mem, obj)

#endif /* DLM_MEMORY_H__ */
