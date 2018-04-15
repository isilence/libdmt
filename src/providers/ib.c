#include <stdlib.h>
#include <dlm/providers/ib.h>
#include <dlm/providers/vms.h>

#include "generic.h"

#define dlm_obj_to_ib(dlm_obj) dlm_mem_to_ib(dlm_obj_to_mem((dlm_obj)))

static bool is_ib_mem(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_IB;
}

static void ib_release(struct dlm_obj *dlm_obj)
{
	struct dlm_mem_ib *mem;

	if (dlm_obj->magic != DLM_MAGIC_MEM_IB)
		return;
	mem = dlm_obj_to_ib(dlm_obj);

	ibv_dereg_mr(mem->mr);
	dlm_mem_release(mem->vms);
	free(mem);
}

static int ib_copy(struct dlm_mem * restrict src,
		   struct dlm_mem * restrict dst,
		   enum DLM_COPY_DIR dir)
{
	struct dlm_mem_ib *ib;

	if (!is_ib_mem(src))
		return -EINVAL;
	ib = dlm_mem_to_ib(src);

	return src->ops->copy(ib->vms, dst, dir);
}

static const struct dlm_obj_ops ib_obj_ops = {
	.release = ib_release,
};

static const struct dlm_mem_ops ib_memory_ops = {
	.copy = ib_copy,
};

struct dlm_mem *
dlm_ib_allocate_memory(struct ibv_pd *pd, size_t size, int mr_reg_flags)
{
	struct dlm_mem_ib *ib;
	struct dlm_mem_vms *vms;
	struct ibv_mr *mr;

	ib = (struct dlm_mem_ib *)malloc(sizeof(*ib));
	if (!ib)
		return NULL;

	vms = dlm_vms_allocate_memory(size);
	if (!vms)
		goto err_free;

	mr = ibv_reg_mr(pd, vms->va, vms->mem.size, mr_reg_flags);
	if (!mr)
		goto err_vms;

	dlm_mem_init(&ib->mem, vms->mem.size, DLM_MAGIC_MEM_IB);
	dlm_obj_set_ops(&ib->mem.obj, &ib_obj_ops);
	ib->mem.ops = &ib_memory_ops;
	ib->pd = pd;
	ib->mr = mr;
	ib->vms = &vms->mem;
	ib->mem.fd = vms->mem.fd;
	dlm_mem_retain(&ib->mem);

	return &ib->mem;

err_vms:
	dlm_mem_release(&vms->mem);
err_free:
	free(ib);
	return NULL;
}
