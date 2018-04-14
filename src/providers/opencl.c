#include <stdlib.h>
#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>

#include "generic.h"

#define dlm_obj_to_cl(dlm_obj) dlm_mem_to_cl(dlm_obj_to_mem((dlm_obj)))

static bool is_cl_mem(struct dlm_mem *mem)
{
	magic_t magic;

	if (!mem)
		return false;

	magic = dlm_mem_get_magic(mem);
	return magic == DLM_MAGIC_MEM_OPENCL;
}

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
	struct dlm_cl_mem *mem;

	if (!is_cl_mem(dlm_mem))
		return NULL;

	mem = dlm_mem_to_cl(dlm_mem);
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
	struct dlm_cl_mem *mem;

	if (!is_cl_mem(dlm_mem))
		return -EFAULT;

	mem = dlm_mem_to_cl(dlm_mem);
	mem->err = clEnqueueUnmapMemObject(mem->queue, mem->clmem, va,
					0, NULL, NULL);

	return error_cl2unix(mem->err);
}

static int cl_try_copy_cl2cl(struct dlm_cl_mem *restrict src,
			     struct dlm_mem *restrict dlm_dst)
{
	cl_int ret;
	struct dlm_cl_mem *dst;

	if (!is_cl_mem(dlm_dst))
		return -ENOSYS;
	dst = dlm_mem_to_cl(dlm_dst);

	if (src->context != dst->context || src->queue != dst->queue)
		return -ENOSYS;

	ret = clEnqueueCopyBuffer(src->queue, src->clmem, dst->clmem,
				  0, 0, src->mem.size,
				  0, NULL, NULL);
	if (ret != CL_SUCCESS)
		goto error;

	ret = clEnqueueBarrier(src->queue);
	if (ret != CL_SUCCESS)
		goto error;

	return 0;
error:
	return error_cl2unix(ret);
}

static int cl_copy_generic(struct dlm_cl_mem * restrict src,
			   struct dlm_mem * restrict dst)
{
	void *dst_va;
	int unmap_err, ret = -EFAULT;
	cl_int clret;

	dst_va = dlm_mem_map(dst, DLM_MAP_WRITE);
	if (!dst_va)
		goto exit_foo;

	clret = clEnqueueReadBuffer(src->queue, src->clmem, CL_TRUE,
					0, src->mem.size,
					dst_va, 0, NULL, NULL);
	if (clret != CL_SUCCESS) {
		ret = error_cl2unix(ret);
		goto unmap_dst;
	}

	ret = 0;
unmap_dst:
	unmap_err = dlm_mem_unmap(dst, dst_va);
	if (!ret)
		ret = unmap_err;
exit_foo:
	return ret;
}

static int cl_copy(struct dlm_mem * restrict src,
		   struct dlm_mem * restrict dst)
{
	struct dlm_cl_mem *mem;
	int ret;

	if (!is_cl_mem(src) || !dst)
		return -EFAULT;
	if (!dlm_mem_copy_size_valid(src, dst))
		return -ENOSPC;

	mem = dlm_mem_to_cl(src);

	ret = cl_try_copy_cl2cl(mem, dst);
	if (ret != -ENOSYS)
		return ret;

	return cl_copy_generic(mem, dst);
}

static int
cl_release(struct dlm_obj *dlm_obj)
{
	struct dlm_cl_mem *mem;

	if (!dlm_obj || dlm_obj->magic != DLM_MAGIC_MEM_OPENCL)
		return -EFAULT;
	mem = dlm_obj_to_cl(dlm_obj);

	mem->err = clReleaseMemObject(mem->clmem);
	free(mem);

	return error_cl2unix(mem->err);
}

static const struct dlm_obj_ops cl_obj_ops = {
	.release = cl_release,
};

static const struct dlm_mem_ops opencl_memory_ops = {
	.map = cl_map,
	.unmap = cl_unmap,
	.copy = cl_copy,
};

static void init_mem_base(struct dlm_cl_mem *mem, size_t size)
{
	dlm_mem_init(&mem->mem, size, DLM_MAGIC_MEM_OPENCL);
	dlm_obj_set_ops(&mem->mem.obj, &cl_obj_ops);
	mem->mem.ops = &opencl_memory_ops;
}

static void init_mem_ctx(struct dlm_cl_mem *mem,
			 const struct dlm_mem_cl_context *ctx)
{
	mem->dev = ctx->device;
	mem->queue = ctx->queue;
	mem->context = ctx->context;
}

static int
dlm_cl_allocate_memory_ex(struct dlm_cl_mem *mem,
			  const struct dlm_mem_cl_context *ctx,
			  size_t size,
			  void *host_ptr,
			  cl_mem_flags flags)
{
	cl_mem clmem;
	cl_int ret;

	clmem = clCreateBuffer(ctx->context, flags, size, host_ptr, &ret);
	if (ret != CL_SUCCESS)
		goto error;

	mem->clmem = clmem;
	init_mem_base(mem, size);
	init_mem_ctx(mem, ctx);
	dlm_mem_retain(&mem->mem);

	return 0;
error:
	free(mem);
	return error_cl2unix(ret);
}

static int
dlm_cl_create_memory(struct dlm_cl_mem *mem,
		     const struct dlm_mem_cl_context *ctx,
		     size_t size,
		     cl_mem_flags flags)
{
	return dlm_cl_allocate_memory_ex(mem, ctx, size, NULL, flags);
}

struct dlm_mem *
dlm_cl_allocate_memory(const struct dlm_mem_cl_context *ctx,
		       size_t size,
		       cl_mem_flags flags)
{
	int err;
	struct dlm_cl_mem *mem;

	mem = (struct dlm_cl_mem *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	err = dlm_cl_create_memory(mem, ctx, size, flags);
	if (err) {
		free(mem);
		return NULL;
	}

	return &mem->mem;
}

struct dlm_mem *
dlm_cl_create_from_clmem(const struct dlm_mem_cl_context *ctx,
			 cl_mem clmem)
{
	struct dlm_cl_mem *mem = NULL;
	cl_int ret;
	size_t size;

	ret = clGetMemObjectInfo(clmem, CL_MEM_SIZE, sizeof(size),
				 (void *)&size, NULL);
	if (ret != CL_SUCCESS)
		goto error;

	mem = (struct dlm_cl_mem *)malloc(sizeof(*mem));
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


static int
create_from_vms(struct dlm_cl_mem *mem,
		struct dlm_vms_mem *vms,
		const struct dlm_mem_cl_context *ctx,
		cl_mem_flags flags)
{
	int err;

	dlm_mem_retain(&vms->mem);
	flags |= CL_MEM_USE_HOST_PTR;

	err = dlm_cl_allocate_memory_ex(mem, ctx,
					vms->mem.size, vms->va, flags);

	if (err)
		dlm_mem_release(&vms->mem);
	return err;
}

struct dlm_mem *
dlm_cl_create_from(const struct dlm_mem_cl_context *ctx,
		   struct dlm_mem *master,
		   cl_mem_flags flags)
{
	struct dlm_cl_mem *mem;
	int err = 0;

	if (!dlm_mem_retain(master))
		return NULL;

	mem = (struct dlm_cl_mem *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	if (master->obj.magic == DLM_MAGIC_MEM_VMS) {
		struct dlm_vms_mem *vms;

		vms = dlm_mem_to_vms(master);
		err = create_from_vms(mem, vms, ctx, flags);
		if (err != -ENOSYS)
			goto clean;
	}

	err = dlm_cl_create_memory(mem, ctx, master->size, flags);
clean:
	dlm_mem_release(master);
	if (err) {
		free(mem);
		return NULL;
	}
	return dlm_cl_to_mem(mem);
}
