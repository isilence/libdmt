#include <unistd.h>
#include <stdlib.h>

#include <dlm/providers/ib.h>
#include <dlm/providers/vms.h>
#include <dlm/providers/opencl.h>

#include "generic.h"

#define dlm_obj_to_ib(dlm_obj) dlm_mem_to_ib(dlm_obj_to_mem((dlm_obj)))

static void ib_mem_release(struct dlm_mem_ib *ib_mr)
{
	ibv_dereg_mr(ib_mr->mr);
	dlm_mem_release(ib_mr->master);
	free(ib_mr);
}


static void ib_release(struct dlm_obj *dlm_obj)
{
	struct dlm_mem_ib *mem;

	if (dlm_obj->magic != DLM_MAGIC_MEM_IB)
		return;
	mem = dlm_obj_to_ib(dlm_obj);

	ib_mem_release(mem);
}

static int ib_copy_vms(struct dlm_mem_ib * restrict ib,
		       struct dlm_mem * restrict dst,
		       size_t size,
		       enum DLM_COPY_DIR dir)
{
	int ret;

	if (dir == DLM_COPY_FORWARD)
		ret = dlm_mem_copysz(ib->master, dst, size);
	else
		ret = dlm_mem_copysz(dst, ib->master, size);

	return ret;
}

static int ib_copy(struct dlm_mem * restrict src,
		   struct dlm_mem * restrict dst,
		   size_t size,
		   enum DLM_COPY_DIR dir)
{
	struct dlm_mem_ib *ib;

	if (!is_ib_mem(src))
		return -EINVAL;
	ib = dlm_mem_to_ib(src);

	if (!is_mem_cl(ib->master))
		return ib_copy_vms(ib, dst, size, dir);

	/* The user should wait
	 * dma_buf fd & completion of opencl operations. */
	return 0;
}

static const struct dlm_obj_ops ib_obj_ops = {
	.release = ib_release,
};

static const struct dlm_mem_ops ib_memory_ops = {
	.copy = ib_copy,
};

struct dlm_mem *
dlm_mem_ib_alloc(struct ibv_pd *pd, size_t size, int mr_reg_flags)
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
	ib->master = &vms->mem;
	ib->mem.fd = vms->mem.fd;
	dlm_mem_retain(&ib->mem);

	return &ib->mem;
err_vms:
	dlm_mem_release(&vms->mem);
err_free:
	free(ib);
	return NULL;
}

struct dlm_mem *dlm_mem_ib_create_from(struct ibv_pd *pd,
				       int mr_reg_flags,
				       struct dlm_mem *master)
{
	struct dlm_mem_ib *ib = NULL;
	struct dlm_mem_cl *cl = NULL;
	struct ibv_mr *mr = NULL;
	int fd = -1;

	if (!is_mem_cl(master))
		return dlm_mem_ib_alloc(pd, master->size, mr_reg_flags);
	cl = dlm_mem_to_cl(master);

	fd = dlm_mem_cl_export_dma_buf(cl);
	if (fd < 0)
		return dlm_mem_ib_alloc(pd, master->size, mr_reg_flags);

	ib = (struct dlm_mem_ib *)malloc(sizeof(*ib));
	if (!ib)
		goto error;

	mr = ibv_reg_mr_dma_buf(pd, fd, mr_reg_flags);
	if (!mr)
		goto error;

	dlm_mem_init(&ib->mem, master->size, DLM_MAGIC_MEM_IB);
	dlm_obj_set_ops(&ib->mem.obj, &ib_obj_ops);
	ib->mem.ops = &ib_memory_ops;
	ib->pd = pd;
	ib->mr = mr;
	ib->master = master;
	ib->mem.fd = fd;
	dlm_mem_retain(&ib->mem);

	return &ib->mem;
error:
	if (fd >= 0)
		close(fd);
	if (ib)
		free(ib);
	if (!mr)
		ibv_dereg_mr(mr);
	return NULL;
}
