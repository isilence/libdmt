#include <errno.h>
#include <string.h>

#include "generic.h"

int dlm_mem_generic_copy(struct dlm_mem * restrict src,
			 struct dlm_mem * restrict dst)
{
	void *dst_va, *src_va;
	int ret = -EFAULT;

	if (src->size > dst->size)
		return -ENOSPC;

	src_va = dlm_mem_map(src, DLM_MAP_READ);
	if (!src_va)
		goto exit_foo;

	dst_va = dlm_mem_map(dst, DLM_MAP_WRITE);
	if (!dst_va)
		goto unmap_src;

	memcpy(dst_va, src_va, src->size);

	ret = 0;

/* unmap_dst: */
	dlm_mem_unmap(dst, dst_va);
unmap_src:
	dlm_mem_unmap(src, src_va);
exit_foo:
	return ret;
}

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic)
{
	dlm_obj_init(&mem->obj);

	mem->obj.magic = magic;
	mem->size = size;
	mem->ops = NULL;
}
