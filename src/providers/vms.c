#include <sys/eventfd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <dlm/providers/vms.h>
#include <dlm/providers/memview1.h>

#include "generic.h"

#define dlm_obj_to_vms(dlm_obj) dlm_mem_to_vms(dlm_obj_to_mem((dlm_obj)))

static void vms_release(struct dlm_obj *dlm_obj)
{
	struct dlm_mem_vms *mem;

	if (dlm_obj->magic != DLM_MAGIC_MEM_VMS)
		return;
	mem = dlm_obj_to_vms(dlm_obj);

	if (mem->mem.fd >= 0)
		close(mem->mem.fd);
	mem->mem.fd = -1;
	free(mem->va);
	free(mem);
}

static int vms_copy(struct dlm_mem * restrict src,
		    struct dlm_mem * restrict dst,
		    enum DLM_COPY_DIR dir)
{
	struct dlm_mem_vms *vms_src;

	if (!is_mem_vms(src))
		return -EINVAL;

	if (is_mem_vms(dst)) {
		struct dlm_mem_vms *vms_dst;

		vms_src = dlm_mem_to_vms(src);
		vms_dst = dlm_mem_to_vms(dst);

		memcpy(vms_dst->va, vms_src->va, vms_src->mem.size);
		return 0;
	}

	return dlm_mem_copy_back(src, dst, dir);
}

static const struct dlm_obj_ops vms_obj_ops = {
	.release = vms_release,
};

static const struct dlm_mem_ops vms_memory_ops = {
	.copy = vms_copy,
};

struct dlm_mem_vms *dlm_vms_allocate_memory(size_t size)
{
	struct dlm_mem_vms *mem;
	int fd;

	mem = (struct dlm_mem_vms *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	fd = eventfd(666, EFD_CLOEXEC);
	if (fd < 0)
		return NULL;

	dlm_mem_init(&mem->mem, size, DLM_MAGIC_MEM_VMS);
	dlm_obj_set_ops(&mem->mem.obj, &vms_obj_ops);
	mem->mem.ops = &vms_memory_ops;
	mem->va = valloc(size);
	mem->mem.fd = fd;
	dlm_mem_retain(&mem->mem);

	return mem;
}

struct dlm_mem_vms *dlm_vms_create_from(struct dlm_mem *master)
{
	return dlm_vms_allocate_memory(master->size);
}
