#include <unistd.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <string.h>

#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>

#include "dlm_opencl.h"
#include "generic.h"

typedef cl_int (*cl_export_dma_buf_t)(cl_context, cl_mem, int *);

int dlm_rc_cl2unix(cl_int err)
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


int dlm_mem_cl_export_dma_buf(struct dlm_mem_cl *mem)
{
	cl_export_dma_buf_t export_dma_buf;
	cl_int ret;
	int fd;

	if (!mem->export)
		return -EFAULT;
	export_dma_buf = (cl_export_dma_buf_t)mem->export;

	ret = export_dma_buf(mem->context, mem->clmem, &fd);
	if (!ret)
		return ret;

	return fd;
}


static int cl_release_cl_mem(struct dlm_mem_cl *mem)
{
	cl_int ret, err;
	int rc = 0;

	err = clReleaseMemObject(mem->clmem);
	if (mem->queue != NULL) {
		ret = clReleaseCommandQueue(mem->queue);
		err = err ? err : ret;
	}
	if (mem->context != NULL) {
		ret = clReleaseContext(mem->context);
		err = err ? err : ret;
	}
	if (mem->dev != NULL) {
		ret = clReleaseDevice(mem->dev);
		err = err ? err : ret;
	}
	if (mem->master) {
		dlm_mem_release(mem->master);
		mem->master = NULL;
	}
	if (mem->mem.fd >= 0)
		rc = close(mem->mem.fd);

	mem->mem.fd = -1;
	mem->queue = NULL;
	mem->context = NULL;
	mem->dev = NULL;
	mem->clmem = NULL;

	return rc ? rc : dlm_rc_cl2unix(err);
}

static int cl_destroy_cl_mem(struct dlm_mem_cl *mem)
{
	int ret = cl_release_cl_mem(mem);

	free(mem);
	return ret;
}

static void cl_release(struct dlm_obj *dlm_obj)
{
	struct dlm_mem_cl *mem;

	if (dlm_obj->magic != DLM_MAGIC_MEM_OPENCL)
		return;

	mem = dlm_obj_to_cl(dlm_obj);
	cl_destroy_cl_mem(mem);
}

static const struct dlm_mem_ops opencl_memory_ops = {
	.copy = dlm_mem_cl_copy,
};

static const struct dlm_obj_ops cl_obj_ops = {
	.release = cl_release,
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

static void init_extensions(struct dlm_mem_cl *mem)
{
	cl_export_dma_buf_t exp = NULL;

#ifdef CL_VERSION_1_2
	exp = (cl_export_dma_buf_t)clGetExtensionFunctionAddressForPlatform(
		mem->platform, "clGetMemObjectFdIntel");
#else
	exp = (cl_export_dma_buf_t)clGetExtensionFunctionAddress(
				"clGetMemObjectFdIntel");
#endif

	mem->export = (void *)exp;
}

static cl_int init_mem_from_ctx(struct dlm_mem_cl *mem,
				const struct dlm_mem_cl_context *ctx)
{
	cl_int ret;

	ret = clRetainCommandQueue(ctx->queue);
	if (ret != CL_SUCCESS)
		return ret;
	mem->queue = ctx->queue;

	ret = clRetainContext(ctx->context);
	if (ret != CL_SUCCESS)
		return ret;
	mem->context = ctx->context;

	ret = clRetainDevice(ctx->device);
	if (ret != CL_SUCCESS)
		return ret;
	mem->dev = ctx->device;

	mem->platform = ctx->platform;
	mem->clmem = NULL;

	init_extensions(mem);

	return CL_SUCCESS;
}

static int init_cl_mem(struct dlm_mem_cl *mem,
		       const struct dlm_mem_cl_context *ctx)
{
	cl_int ret;

	dlm_mem_init(&mem->mem, 0, DLM_MAGIC_MEM_OPENCL);
	mem->master = NULL;
	mem->export = NULL;

	ret = init_mem_from_ctx(mem, ctx);
	if (ret != CL_SUCCESS)
		return dlm_rc_cl2unix(ret);

	mem->mem.fd = eventfd(666, EFD_CLOEXEC);
	if (mem->mem.fd < 0)
		return errno;

	dlm_obj_set_ops(&mem->mem.obj, &cl_obj_ops);
	mem->mem.ops = &opencl_memory_ops;
	mem->busy = false;
	mem->err = CL_SUCCESS;

	return 0;
}

static struct dlm_mem_cl *allocate_cl_mem(const struct dlm_mem_cl_context *ctx)
{
	struct dlm_mem_cl *mem;
	int ret;

	mem = (struct dlm_mem_cl *)malloc(sizeof(*mem));
	if (!mem)
		return NULL;

	ret = init_cl_mem(mem, ctx);
	if (ret) {
		free(mem);
		mem = NULL;
	}
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
	return dlm_rc_cl2unix(err);
}

static int
dlm_cl_create_memory(struct dlm_mem_cl *mem,
		     const struct dlm_mem_cl_context *ctx,
		     size_t size,
		     cl_mem_flags flags)
{
	return dlm_cl_allocate_memory_ex(mem, ctx, size, NULL, flags);
}

struct dlm_mem_cl *
dlm_mem_cl_alloc(const struct dlm_mem_cl_context *ctx,
		 size_t size,
		 cl_mem_flags flags)
{
	int err;
	struct dlm_mem_cl *mem;

	mem = allocate_cl_mem(ctx);
	if (!mem)
		return NULL;

	err = dlm_cl_create_memory(mem, ctx, size, flags);
	if (err)
		cl_destroy_cl_mem(mem);

	return mem;
}

struct dlm_mem_cl *
dlm_mem_cl_create_from_clmem(const struct dlm_mem_cl_context *ctx,
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

	return mem;
error:
	if (mem)
		cl_destroy_cl_mem(mem);
	return NULL;
}


static int create_from_vms(struct dlm_mem_cl *mem,
			   struct dlm_mem_vms *vms,
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

struct dlm_mem_cl *dlm_mem_cl_create_from(const struct dlm_mem_cl_context *ctx,
					  struct dlm_mem *master,
					  cl_mem_flags flags)
{
	struct dlm_mem_cl *mem;
	int err = 0;

	mem = allocate_cl_mem(ctx);
	if (!mem)
		return NULL;

	if (master->obj.magic == DLM_MAGIC_MEM_VMS) {
		struct dlm_mem_vms *vms = dlm_mem_to_vms(master);

		err = create_from_vms(mem, vms, ctx, flags);
		if (err != -ENOSYS)
			goto clean;
	}

	err = dlm_cl_create_memory(mem, ctx, master->size, flags);
clean:
	if (err) {
		(void)cl_destroy_cl_mem(mem);
		return NULL;
	}
	return mem;
}