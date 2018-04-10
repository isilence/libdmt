#include <stdlib.h>
#include <dlm/compiler.h>
#include <dlm/providers/vms.h>

#include "generic_operations.h"

static void *
vms_map(struct dlm_mem *dlm_mem, enum DLM_MEM_MAP_FLAGS flags)
{
	struct dlm_vms_memory *mem = dlm_mem_to_vms(dlm_mem);

	return mem->va;
}


static int
vms_unmap(struct dlm_mem *dlm_mem DLM_PARAM_UNUSED, void *va DLM_PARAM_UNUSED)
{
	return 0;
}

static int
vms_release(struct dlm_mem *dlm_mem)
{
	struct dlm_vms_memory *mem = dlm_mem_to_vms(dlm_mem);

	free(mem->va);
	free(dlm_mem);
	return 0;
}

static const struct dlm_mem_operations vms_memory_ops = {
	.map = vms_map,
	.unmap = vms_unmap,
	.release = vms_release,
	.copy = dlm_mem_generic_copy,
};

struct dlm_mem *
dlm_vms_allocate_memory(size_t size)
{
	struct dlm_vms_memory *mem;

	mem = (struct dlm_vms_memory *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	dlm_init_mem(&mem->mem, size, DLM_MEM_VMS_MAGIC);
	mem->va = valloc(size);
	dlm_mem_retain(&mem->mem);

	return dlm_vms_to_mem(mem);
}

struct dlm_mem *
dlm_vms_create_from(struct dlm_mem *master)
{
	return dlm_vms_allocate_memory(master->size);
}
