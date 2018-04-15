#include <stdlib.h>
#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>
#include <string.h>

#include "common.h"
#include "cl_event.h"

#define dlm_obj_to_cl(dlm_obj) dlm_mem_to_cl(dlm_obj_to_mem((dlm_obj)))

static inline bool is_cl_mem(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_OPENCL;
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

static void * cl_map(struct dlm_mem *dlm_mem,
		     enum DLM_MEM_MAP_FLAGS flags)
{
	void *va;
	struct dlm_mem_cl *mem;

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

static int cl_unmap(struct dlm_mem *dlm_mem, void *va)
{
	struct dlm_mem_cl *mem;

	if (!is_cl_mem(dlm_mem))
		return -EFAULT;

	mem = dlm_mem_to_cl(dlm_mem);
	mem->err = clEnqueueUnmapMemObject(mem->queue, mem->clmem, va,
					0, NULL, NULL);

	return rc_cl2unix(mem->err);
}

static int cl_try_copy_cl2cl(struct dlm_mem_cl *restrict src,
			     struct dlm_mem *restrict dlm_dst)
{
	cl_int err;
	struct dlm_mem_cl *dst;

	if (!is_cl_mem(dlm_dst))
		return -ENOSYS;
	dst = dlm_mem_to_cl(dlm_dst);

	if (src->context != dst->context || src->queue != dst->queue)
		return -ENOSYS;

	err = clEnqueueCopyBuffer(src->queue, src->clmem, dst->clmem,
				  0, 0, src->mem.size,
				  0, NULL, NULL);
	if (err != CL_SUCCESS)
		goto error;

	err = clEnqueueBarrier(src->queue);
	if (err != CL_SUCCESS)
		goto error;

	return 0;
error:
	src->err = err;
	return rc_cl2unix(err);
}

static int cl_copy_generic(struct dlm_mem_cl * restrict src,
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
		ret = rc_cl2unix(ret);
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
	struct dlm_mem_cl *mem = dlm_mem_to_cl(src);
	int ret;

	ret = cl_try_copy_cl2cl(mem, dst);
	if (ret != -ENOSYS)
		return ret;

	return cl_copy_generic(mem, dst);
}

static cl_int cl_release_cl_mem(struct dlm_mem_cl *mem)
{
	cl_int lerr, err;

	err = clReleaseMemObject(mem->clmem);
	if (mem->queue != NULL) {
		lerr = clReleaseCommandQueue(mem->queue);
		err = err ? err : lerr;
	}
	if (mem->context != NULL) {
		lerr = clReleaseContext(mem->context);
		err = err ? err : lerr;
	}
	if (mem->dev != NULL) {
		lerr = clReleaseDevice(mem->dev);
		err = err ? err : lerr;
	}
	if (mem->master) {
		dlm_mem_release(mem->master);
		mem->master = NULL;
	}

	mem->queue = NULL;
	mem->context = NULL;
	mem->dev = NULL;
	mem->clmem = NULL;
	return err;
}

static cl_int cl_destroy_cl_mem(struct dlm_mem_cl *mem)
{
	cl_int ret = cl_release_cl_mem(mem);

	free(mem);
	return ret;
}

static int cl_release(struct dlm_obj *dlm_obj)
{
	struct dlm_mem_cl *mem;
	cl_int err;

	if (dlm_obj->magic != DLM_MAGIC_MEM_OPENCL)
		return -EFAULT;

	mem = dlm_obj_to_cl(dlm_obj);
	err = cl_destroy_cl_mem(mem);

	return rc_cl2unix(err);
}

static const struct dlm_obj_ops cl_obj_ops = {
	.release = cl_release,
};

static const struct dlm_mem_ops opencl_memory_ops = {
	.map = cl_map,
	.unmap = cl_unmap,
	.copy = cl_copy,
};

/**
 * 	=============================================================
 * 	Either \init_cl_mem or \allocate_cl_mem should be used
 * 	to initialise/create \dlm_cl_mem. The former one also
 * 	allocates new memory for the object. The functions
 * 	retain \ctx objects.
 *
 * 	\cl_release_cl_mem or \cl_destroy_cl_mem respectively should
 * 	 be used to cleanup \dlm_cl_mem after error.*
 *
 * 	\set_clmem is responsible for setting \cl_mem object.
 * 	The object should be retained.
 *
 * 	Rule of thumb: use \cl_release_cl_mem/\cl_destroy_cl_mem in
 * 	function where \init_cl_mem/\allocate_cl_mem have been called
 */

static cl_int init_mem_ctx(struct dlm_mem_cl *mem,
			   const struct dlm_mem_cl_context *ctx)
{
	cl_int ret;

	ret = clRetainCommandQueue(mem->queue);
	if (ret != CL_SUCCESS)
		return ret;
	mem->queue = ctx->queue;

	ret = clRetainContext(mem->context);
	if (ret != CL_SUCCESS)
		return ret;
	mem->context = ctx->context;

	ret = clRetainDevice(mem->dev);
	if (ret != CL_SUCCESS)
		return ret;
	mem->dev = ctx->device;

	return CL_SUCCESS;
}

static cl_int init_cl_mem(struct dlm_mem_cl *mem,
			  const struct dlm_mem_cl_context *ctx)
{
	cl_int ret;

	memset((void *)mem, 0, sizeof(*mem));

	ret = init_mem_ctx(mem, ctx);
	if (!ret)
		return ret;

	dlm_mem_init(&mem->mem, 0, DLM_MAGIC_MEM_OPENCL);
	dlm_obj_set_ops(&mem->mem.obj, &cl_obj_ops);
	mem->mem.ops = &opencl_memory_ops;

	return CL_SUCCESS;
}

static struct dlm_mem_cl *allocate_cl_mem(const struct dlm_mem_cl_context *ctx)
{
	struct dlm_mem_cl *mem;

	mem = (struct dlm_mem_cl *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	init_cl_mem(mem, ctx);
	return mem;
}

static void set_clmem(struct dlm_mem_cl *mem,
		      cl_mem clmem,
		      size_t size)
{
	mem->clmem = clmem;
	mem->mem.size = size;
}


/**
 * 	=============================================================
 * 		dlm_cl_mem object creation
 */

static int
dlm_cl_allocate_memory_ex(struct dlm_mem_cl *mem,
			  const struct dlm_mem_cl_context *ctx,
			  size_t size,
			  void *host_ptr,
			  cl_mem_flags flags)
{
	cl_mem clmem;
	cl_int err;

	clmem = clCreateBuffer(ctx->context, flags, size, host_ptr, &err);
	if (err != CL_SUCCESS)
		goto error;

	set_clmem(mem, clmem, size);
	dlm_mem_retain(&mem->mem);

	return 0;
error:
	mem->err = err;
	return rc_cl2unix(err);
}

static int
dlm_cl_create_memory(struct dlm_mem_cl *mem,
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
	struct dlm_mem_cl *mem;

	mem = allocate_cl_mem(ctx);
	if (!mem)
		return NULL;

	err = dlm_cl_create_memory(mem, ctx, size, flags);
	if (err) {
		cl_destroy_cl_mem(mem);
		return NULL;
	}
	return &mem->mem;
}

struct dlm_mem *
dlm_cl_create_from_clmem(const struct dlm_mem_cl_context *ctx,
			 cl_mem clmem)
{
	struct dlm_mem_cl *mem = NULL;
	cl_int ret;
	size_t size;

	ret = clGetMemObjectInfo(clmem, CL_MEM_SIZE, sizeof(size),
				 (void *)&size, NULL);
	if (ret != CL_SUCCESS)
		goto error;

	mem = allocate_cl_mem(ctx);
	if (!mem)
		goto error;

	ret = clRetainMemObject(clmem);
	if (ret != CL_SUCCESS)
		goto error;
	set_clmem(mem, clmem, size);
	dlm_mem_retain(&mem->mem);

	return dlm_cl_to_mem(mem);
error:
	if (mem)
		cl_destroy_cl_mem(mem);
	return NULL;
}


static int create_from_vms(struct dlm_mem_cl *mem,
			   struct dlm_vms_mem *vms,
			   const struct dlm_mem_cl_context *ctx,
			   cl_mem_flags flags)
{
	int err;

	flags |= CL_MEM_USE_HOST_PTR;
	err = dlm_cl_allocate_memory_ex(mem, ctx,
					vms->mem.size, vms->va, flags);

	if (!err) {
		dlm_mem_retain(&vms->mem);
		mem->master = &vms->mem;
	}
	return err;
}

struct dlm_mem * dlm_cl_create_from(const struct dlm_mem_cl_context *ctx,
				    struct dlm_mem *master,
				    cl_mem_flags flags)
{
	struct dlm_mem_cl *mem;
	int err = 0;

	mem = allocate_cl_mem(ctx);
	if (!mem)
		return NULL;

	if (master->obj.magic == DLM_MAGIC_MEM_VMS) {
		struct dlm_vms_mem *vms = dlm_mem_to_vms(master);

		err = create_from_vms(mem, vms, ctx, flags);
		if (err != -ENOSYS)
			goto clean;
	}

	err = dlm_cl_create_memory(mem, ctx, master->size, flags);
clean:
	if (err) {
		cl_destroy_cl_mem(mem);
		return NULL;
	}
	return dlm_cl_to_mem(mem);
}
