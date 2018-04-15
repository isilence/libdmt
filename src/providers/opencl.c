#include <unistd.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <string.h>

#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>

#include "generic.h"

#define dlm_obj_to_cl(dlm_obj) dlm_mem_to_cl(dlm_obj_to_mem((dlm_obj)))

typedef CL_API_ENTRY cl_int CL_API_CALL
(*clEnqueueBuffer_t)(cl_command_queue   /* command_queue */,
		     cl_mem             /* buffer */,
		     cl_bool            /* blocking_write */,
		     size_t             /* offset */,
		     size_t             /* size */,
		     const void *       /* ptr */,
		     cl_uint            /* num_events_in_wait_list */,
		     const cl_event *   /* event_wait_list */,
		     cl_event *         /* event */);

static int rc_cl2unix(cl_int err)
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

static inline bool is_cl_mem(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_OPENCL;
}

static void CL_CALLBACK copy_clb(cl_event event, cl_int rc, void *data)
{
	struct dlm_mem **ms = (struct dlm_mem **)data;

	if (rc != CL_SUCCESS) {
		int ret = rc_cl2unix(rc);

		ms[0]->err = ret;
		ms[1]->err = ret;
	}

	dlm_mem_eventfd_unlock(ms[0]);
	dlm_mem_eventfd_unlock(ms[1]);
	clReleaseEvent(event);
	free(data);
}

static int cl_try_copy_cl2cl(struct dlm_mem_cl *restrict src,
			     struct dlm_mem *restrict dlm_dst)
{
	cl_int err = CL_INVALID_VALUE;
	cl_event event;
	struct dlm_mem_cl *dst;
	struct dlm_mem **pair = NULL;

	if (!is_cl_mem(dlm_dst))
		return -ENOSYS;

	dst = dlm_mem_to_cl(dlm_dst);
	if (src->context != dst->context || src->queue != dst->queue)
		return -ENOSYS;

	pair = (struct dlm_mem **)malloc(sizeof(*pair) * 2);
	pair[0] = &src->mem;
	pair[1] = &dst->mem;

	if (!dlm_mem_eventfd_lock_pair(&src->mem, &dst->mem))
		goto error;

	err = clEnqueueCopyBuffer(src->queue, src->clmem, dst->clmem,
				  0, 0, src->mem.size,
				  0, NULL, &event);
	if (err != CL_SUCCESS)
		goto error;

	err = clSetEventCallback(event, CL_SUCCESS, copy_clb, (void *)pair);
	if (err)
		goto error;

	return 0;
error:
	if (pair && err != CL_SUCCESS)
		free(pair);
	src->err = err;
	return rc_cl2unix(err);
}

static int cl_try_copy_cl_vms(struct dlm_mem_cl *restrict cl,
			      struct dlm_mem *restrict dlm_mem,
			      enum DLM_COPY_DIR dir)
{
	cl_int err = CL_INVALID_VALUE;
	cl_event event;
	struct dlm_mem_vms *vms;
	struct dlm_mem **pair = NULL;
	clEnqueueBuffer_t foo;

	if (!is_mem_vms(dlm_mem))
		return -ENOSYS;

	vms = dlm_mem_to_vms(dlm_mem);
	pair = (struct dlm_mem **)malloc(sizeof(*pair) * 2);
	pair[0] = &cl->mem;
	pair[1] = &vms->mem;

	if (dlm_mem_eventfd_lock_pair(&cl->mem, &vms->mem))
		goto error;

	foo = (clEnqueueBuffer_t)((dir == DLM_COPY_FORWARD)
		? (clEnqueueBuffer_t)clEnqueueReadBuffer: clEnqueueWriteBuffer);
	err = foo(cl->queue, cl->clmem, CL_FALSE,
				0, cl->mem.size, vms->va,
				0, NULL, &event);
	if (err != CL_SUCCESS)
		goto error;

	err = clSetEventCallback(event, CL_SUCCESS, copy_clb, (void *)pair);
	if (err)
		goto error;

	return 0;
error:
	if (pair && err != CL_SUCCESS)
		free(pair);
	cl->err = err;
	return rc_cl2unix(err);
}

static int cl_copy(struct dlm_mem * restrict src,
		   struct dlm_mem * restrict dst,
		   enum DLM_COPY_DIR dir)
{
	struct dlm_mem_cl *mem;
	int ret;

	if (!is_cl_mem(src))
		return -EINVAL;
	mem = dlm_mem_to_cl(src);

	ret = cl_try_copy_cl2cl(mem, dst);
	if (ret != -ENOSYS)
		goto finalise;

	ret = cl_try_copy_cl_vms(mem, dst, dir);
	if (ret != -ENOSYS)
		goto finalise;

	ret = -ENOSYS;
finalise:
	return ret;
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
	if (mem->mem.fd >= 0) {
		rc = close(mem->mem.fd);
	}

	mem->mem.fd = -1;
	mem->queue = NULL;
	mem->context = NULL;
	mem->dev = NULL;
	mem->clmem = NULL;

	return rc ? rc : rc_cl2unix(err);
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

static const struct dlm_obj_ops cl_obj_ops = {
	.release = cl_release,
};

static const struct dlm_mem_ops opencl_memory_ops = {
	.copy = cl_copy
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

	return CL_SUCCESS;
}

static int init_cl_mem(struct dlm_mem_cl *mem,
		       const struct dlm_mem_cl_context *ctx)
{
	cl_int ret;

	memset((void *)mem, 0, sizeof(*mem));
	mem->mem.fd = -1;
	mem->busy = false;

	ret = init_mem_from_ctx(mem, ctx);
	if (ret != CL_SUCCESS)
		return rc_cl2unix(ret);

	mem->mem.fd = eventfd(666, EFD_CLOEXEC);
	if (mem->mem.fd < 0)
		return errno;

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

struct dlm_mem_cl *
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
	if (err)
		cl_destroy_cl_mem(mem);

	return mem;
}

struct dlm_mem_cl *
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

struct dlm_mem_cl *dlm_cl_create_from(const struct dlm_mem_cl_context *ctx,
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
		cl_destroy_cl_mem(mem);
		return NULL;
	}
	return mem;
}
