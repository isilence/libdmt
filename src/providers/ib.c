#include <stdlib.h>
#include <dlm/providers/ib.h>
#include <dlm/providers/vms.h>

#include "generic.h"

#define dlm_obj_to_ib(dlm_obj) dlm_mem_to_ib(dlm_obj_to_mem((dlm_obj)))

static bool is_ib_mem(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_IB;
}

static void *
ib_map(struct dlm_mem *dlm_mem, enum DLM_MEM_MAP_FLAGS flags)
{
	struct dlm_ib_mem *mem;

	if (!is_ib_mem(dlm_mem))
		return NULL;

	mem = dlm_mem_to_ib(dlm_mem);
	return mem->mr->addr;
}

static int
ib_unmap(struct dlm_mem *dlm_mem DLM_PARAM_UNUSED, void *va DLM_PARAM_UNUSED)
{
	if (!is_ib_mem(dlm_mem))
		return -EFAULT;

	return 0;
}

static int
ib_release(struct dlm_obj *dlm_obj)
{
	int err_mr, err_dlm;
	struct dlm_ib_mem *mem;

	if (dlm_obj->magic != DLM_MAGIC_MEM_IB)
		return -EFAULT;
	mem = dlm_obj_to_ib(dlm_obj);

	err_mr = ibv_dereg_mr(mem->mr);
	err_dlm = dlm_mem_release(mem->vms);
	free(mem);

	return err_mr ? err_mr : err_dlm;
}

static const struct dlm_obj_ops ib_obj_ops = {
	.release = ib_release,
};

static const struct dlm_mem_ops ib_memory_ops = {
	.map = ib_map,
	.unmap = ib_unmap,
	.copy = dlm_mem_generic_copy,
};

struct dlm_mem *
dlm_ib_allocate_memory(struct ibv_pd *pd, size_t size, int mr_reg_flags)
{
	struct dlm_ib_mem *mem;
	struct dlm_mem *vms_mem;
	struct dlm_vms_mem *vms;
	struct ibv_mr *mr;

	mem = (struct dlm_ib_mem *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	vms_mem = dlm_vms_allocate_memory(size);
	if (!vms_mem)
		goto err_free;

	vms = dlm_mem_to_vms(vms_mem);
	mr = ibv_reg_mr(pd, vms->va, vms->mem.size, mr_reg_flags);
	if (!mr)
		goto err_vms;

	dlm_mem_init(&mem->mem, vms->mem.size, DLM_MAGIC_MEM_IB);
	dlm_obj_set_ops(&mem->mem.obj, &ib_obj_ops);
	mem->mem.ops = &ib_memory_ops;
	mem->pd = pd;
	mem->mr = mr;
	mem->vms = vms_mem;
	dlm_mem_retain(&mem->mem);

	return dlm_ib_to_mem(mem);

err_vms:
	dlm_mem_release(vms_mem);
err_free:
	free(mem);
	return NULL;
}
