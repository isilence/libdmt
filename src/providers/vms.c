#include <stdlib.h>
#include <dlm/compiler.h>
#include <dlm/providers/vms.h>

#include "generic_operations.h"

#define dlm_obj_to_vms(dlm_obj) dlm_mem_to_vms(dlm_obj_to_mem((dlm_obj)))

static void *
vms_map(struct dlm_mem *dlm_mem, enum DLM_MEM_MAP_FLAGS flags)
{
	struct dlm_vms_mem *mem = dlm_mem_to_vms(dlm_mem);

	return mem->va;
}


static int
vms_unmap(struct dlm_mem *dlm_mem DLM_PARAM_UNUSED, void *va DLM_PARAM_UNUSED)
{
	return 0;
}

static int
vms_release(struct dlm_obj *dlm_obj)
{
	struct dlm_vms_mem *mem = dlm_obj_to_vms(dlm_obj);

	free(mem->va);
	free(mem);

	return 0;
}

static const struct dlm_obj_ops vms_obj_ops = {
	.release = vms_release,
};

static const struct dlm_mem_ops vms_memory_ops = {
	.map = vms_map,
	.unmap = vms_unmap,
	.copy = dlm_mem_generic_copy,
};

struct dlm_mem *
dlm_vms_allocate_memory(size_t size)
{
	struct dlm_vms_mem *mem;

	mem = (struct dlm_vms_mem *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	dlm_mem_init(&mem->mem, size, DLM_MEM_VMS_MAGIC);
	dlm_obj_set_ops(&mem->mem.obj, &vms_obj_ops);
	mem->mem.ops = &vms_memory_ops;
	mem->va = valloc(size);
	dlm_mem_retain(&mem->mem);

	return dlm_vms_to_mem(mem);
}

struct dlm_mem *
dlm_vms_create_from(struct dlm_mem *master)
{
	return dlm_vms_allocate_memory(master->size);
}
