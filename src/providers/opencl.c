#include <stdlib.h>
#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>

#include "generic_operations.h"

static int error_cl2unix(cl_int err)
{
	int ret;

	switch (err) {
		case CL_SUCCESS:
			ret = 0;
			break;
		case CL_OUT_OF_HOST_MEMORY:
			ret = -ENOMEM;
			break;
		case CL_OUT_OF_RESOURCES:
			ret = -ENOSPC;
			break;
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			ret = -EFAULT;
			break;
		/* case CL_MAP_FAILURE: */
		default:
			ret = -EINVAL;
	}

	return ret;
}

static cl_mem_flags memflags_dlm2cl(int flags)
{
	cl_mem_flags res = 0;

	if (flags & DLM_MAP_READ)
		res |= CL_MAP_READ;
	if (flags & DLM_MAP_WRITE)
		res |= CL_MAP_WRITE;

	return res;
}

static void *
cl_map(struct dlm_mem *dlm_mem, enum DLM_MEM_MAP_FLAGS flags)
{
	void *va;
	struct dlm_cl_memory *mem = dlm_mem_to_cl(dlm_mem);

	va = clEnqueueMapBuffer(mem->queue, mem->clmem,
				CL_TRUE, memflags_dlm2cl(flags),
				0, dlm_mem->size,
				0, NULL, NULL, &mem->err);

	if (mem->err != CL_SUCCESS)
		return NULL;
	return va;
}

static int
cl_unmap(struct dlm_mem *dlm_mem, void *va)
{
	struct dlm_cl_memory *mem = dlm_mem_to_cl(dlm_mem);

	mem->err = clEnqueueUnmapMemObject(mem->queue, mem->clmem, va,
					0, NULL, NULL);

	return error_cl2unix(mem->err);
}

static int
cl_release(struct dlm_mem *dlm_mem)
{
	struct dlm_cl_memory *mem = dlm_mem_to_cl(dlm_mem);

	mem->err = clReleaseMemObject(mem->clmem);
	free(dlm_mem);

	return error_cl2unix(mem->err);
}

static const struct dlm_mem_operations opencl_memory_ops = {
	.map = cl_map,
	.unmap = cl_unmap,
	.release = cl_release,
	.copy = dlm_mem_generic_copy,
};

static void init_mem_base(struct dlm_cl_memory *mem, size_t size)
{
	dlm_init_mem(&mem->mem, size, DLM_MEM_OPENCL_MAGIC);
	mem->mem.ops = &opencl_memory_ops;
}

static void init_mem_ctx(struct dlm_cl_memory *mem,
			struct dlm_mem_cl_context *ctx)
{
	mem->dev = ctx->device;
	mem->queue = ctx->queue;
	mem->context = ctx->context;
}


static struct dlm_mem *
dlm_cl_allocate_memory_ex(size_t size,
			  struct dlm_mem_cl_context *ctx,
			  void *host_ptr,
			  cl_mem_flags flags)
{
	cl_mem clmem;
	cl_int ret;
	struct dlm_cl_memory *mem;

	mem = (struct dlm_cl_memory *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	clmem = clCreateBuffer(ctx->context, flags, size, host_ptr, &ret);
	if (ret != CL_SUCCESS)
		goto error;

	mem->clmem = clmem;
	init_mem_base(mem, size);
	init_mem_ctx(mem, ctx);
	dlm_mem_retain(&mem->mem);

	return dlm_cl_to_mem(mem);
error:
	free(mem);
	return NULL;
}

struct dlm_mem *
dlm_cl_allocate_memory(size_t size,
			struct dlm_mem_cl_context *ctx,
			cl_mem_flags flags)
{
	return dlm_cl_allocate_memory_ex(size, ctx, NULL, flags);
}

struct dlm_mem *
dlm_cl_create_from_clmem(cl_mem clmem,
		    struct dlm_mem_cl_context *ctx)
{
	struct dlm_cl_memory *mem = NULL;
	cl_int ret;
	size_t size;

	ret = clGetMemObjectInfo(clmem, CL_MEM_SIZE, sizeof(size),
				 (void *)&size, NULL);
	if (ret != CL_SUCCESS)
		goto error;

	mem = (struct dlm_cl_memory *)malloc(sizeof(*mem));
	if (!mem)
		goto error;

	ret = clRetainMemObject(clmem);
	if (ret != CL_SUCCESS)
		goto error;

	mem->clmem = clmem;
	init_mem_base(mem, size);
	init_mem_ctx(mem, ctx);
	dlm_mem_retain(&mem->mem);

	return dlm_cl_to_mem(mem);
error:
	free(mem);
	return NULL;
}


static struct dlm_mem *
create_from_vms(struct dlm_vms_memory *vms,
		struct dlm_mem_cl_context *ctx,
		cl_mem_flags flags)
{
	struct dlm_mem *mem;

	flags |= CL_MEM_USE_HOST_PTR;
	dlm_mem_retain(&vms->mem);
	mem = dlm_cl_allocate_memory_ex(vms->mem.size, ctx, vms->va, flags);

	if (!mem)
		dlm_mem_release(&vms->mem);
	return mem;
}

struct dlm_mem *
dlm_cl_create_from(struct dlm_mem *master,
		   struct dlm_mem_cl_context *ctx,
		   cl_mem_flags flags)
{
	struct dlm_mem *mem;

	if (!dlm_mem_retain(master))
		return NULL;

	if (master->magic == DLM_MEM_VMS_MAGIC)
		mem = create_from_vms(dlm_mem_to_vms(master), ctx, flags);
	else
		mem = dlm_cl_allocate_memory(master->size, ctx, flags);

	dlm_mem_release(master);
	return mem;
}
