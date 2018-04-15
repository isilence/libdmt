#include <dlm/events/seq.h>

#include <errno.h>
#include <string.h>

#include "common.h"

int dlm_mem_generic_copy(struct dlm_mem * restrict src,
			 struct dlm_mem * restrict dst,
			 struct dlm_sync *sync)
{
	void *dst_va, *src_va;
	struct dlm_event_seq *e;
	int ret = -EFAULT;

	if (!dlm_mem_copy_size_valid(src, dst))
		return -ENOSPC;

	if (sync->waitfor) {
		ret = dlm_event_list_wait(sync->waitfor);
		if (ret)
			return ret;
	}

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
	if (!ret && sync->sync) {
		e = dlm_event_seq_create(&src->obj);
		if (!e)
			ret = -EFAULT;
		else
			sync->event = &e->event;
	}
	return ret;
}

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic)
{
	dlm_obj_init(&mem->obj, &root);

	mem->obj.magic = magic;
	mem->size = size;
	mem->ops = NULL;
}
