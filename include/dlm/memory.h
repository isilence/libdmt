#ifndef DLM_MEMORY_H__
#define DLM_MEMORY_H__

#include <dlm/compiler.h>
#include <dlm/object.h>

/* memory */

#define DLM_MAGIC_MEM_BASE DLM_MAGIC_BASE_CREATE(0xDEE9)
#define DLM_MAGIC_IS_MEM(magic) \
	(DLM_MAGIC_BASE_EXTRACT(magic) == DLM_MAGIC_MEM_BASE)
#define DLM_MAGIC_MEM_CREATE(num) \
	DLM_MAGIC_CREATE(DLM_MAGIC_MEM_BASE, num)

#define DLM_MAGIC_MEM_VMS	DLM_MAGIC_MEM_CREATE(100)
#define DLM_MAGIC_MEM_OPENCL	DLM_MAGIC_MEM_CREATE(200)
#define DLM_MAGIC_MEM_IB	DLM_MAGIC_MEM_CREATE(300)
#define DLM_MAGIC_MEM_VIEW	DLM_MAGIC_MEM_CREATE(400)

enum DLM_MEM_LINK_TYPE {
	DLM_MEM_LINK_TYPE_FALLBACK = 0,
	DLN_MEM_LINK_TYPE_SYS,
	DLM_MEM_LINK_TYPE_P2P,
};

enum DLM_MEM_MAP_FLAGS {
	DLM_MAP_READ	= 0x1,
	DLM_MAP_WRITE	= 0x2,
};

enum DLM_COPY_DIR {
	DLM_COPY_FORWARD,
	DLM_COPY_BACKWARD
};

struct dlm_mem_ops {
	int (*copy) (	struct dlm_mem * restrict src,
			struct dlm_mem * restrict dst,
			enum DLM_COPY_DIR dir);
};

struct dlm_mem {
	size_t size;
	int fd;
	int err;

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

static inline void dlm_mem_retain(struct dlm_mem *mem)
{
	if (dlm_mem_valid(mem))
		dlm_obj_retain(&mem->obj);
}

static inline void dlm_mem_release(struct dlm_mem *mem)
{
	if (dlm_mem_valid(mem))
		dlm_obj_release(&mem->obj);
}

static inline int dlm_mem_copy(struct dlm_mem *src,
			       struct dlm_mem *dst)
{
	if (!dlm_mem_valid(src) || !dlm_mem_valid(dst) || src == dst)
		return -EINVAL;
	if (!dlm_mem_copy_size_valid(src, dst))
		return -ENOSPC;

	dst->err = 0;
	src->err = 0;

	return src->ops->copy(src, dst, DLM_COPY_FORWARD);
}

#define dlm_mem_to_dlm(memobj, type, magic) container_of((memobj), type, mem)
#define dlm_obj_to_mem(dlm_obj) container_of((dlm_obj), struct dlm_mem, obj)

#define dlm_to_mem(memobj) ({ \
	typeof((memobj)) __internal_tmp = (memobj); \
	__internal_tmp ? &__internal_tmp->mem : NULL; \
})

#endif /* DLM_MEMORY_H__ */
