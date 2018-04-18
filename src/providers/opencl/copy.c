#include <unistd.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <string.h>

#include <dlm/providers/opencl.h>
#include <dlm/providers/vms.h>

#include "generic.h"
#include "dlm_opencl.h"

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

static void CL_CALLBACK copy_clb(cl_event event, cl_int rc, void *data)
{
	struct dlm_mem **ms = (struct dlm_mem **)data;

	if (rc != CL_SUCCESS) {
		int ret = dlm_rc_cl2unix(rc);

		ms[0]->err = ret;
		ms[1]->err = ret;
	}

	dlm_mem_pair_free(ms);
	clReleaseEvent(event);
}

static int cl_try_copy_cl2cl(struct dlm_mem_cl *restrict src,
			     struct dlm_mem *restrict dlm_dst,
			     size_t size)
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

	pair = dlm_mem_create_pair_locked(&src->mem, &dst->mem);
	if (!pair)
		goto error;

	err = clEnqueueCopyBuffer(src->queue, src->clmem, dst->clmem,
				  0, 0, size,
				  0, NULL, &event);
	if (err != CL_SUCCESS)
		goto error;

	err = clSetEventCallback(event, CL_SUCCESS, copy_clb, (void *)pair);
	if (err)
		goto error;

	return 0;
error:
	if (pair && err != CL_SUCCESS)
		dlm_mem_pair_free(pair);

	src->err = err;
	dst->err = err;
	return dlm_rc_cl2unix(err);
}

static int cl_try_copy_cl_vms(struct dlm_mem_cl *restrict cl,
			      struct dlm_mem *restrict dlm_mem,
			      size_t size,
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
	pair = dlm_mem_create_pair_locked(&cl->mem, &vms->mem);
	if (!pair)
		goto error;

	foo = (clEnqueueBuffer_t)((dir == DLM_COPY_FORWARD)
		? (clEnqueueBuffer_t)clEnqueueReadBuffer: clEnqueueWriteBuffer);
	err = foo(cl->queue, cl->clmem, CL_FALSE,
		  0, size, vms->va,
		  0, NULL, &event);
	if (err != CL_SUCCESS)
		goto error;

	err = clSetEventCallback(event, CL_SUCCESS, copy_clb, (void *)pair);
	if (err)
		goto error;

	return 0;
error:
	if (pair && err != CL_SUCCESS)
		dlm_mem_pair_free(pair);
	cl->err = err;
	return dlm_rc_cl2unix(err);
}

int dlm_mem_cl_copy(struct dlm_mem * restrict src,
		    struct dlm_mem * restrict dst,
		    size_t size,
		    enum DLM_COPY_DIR dir)
{
	struct dlm_mem_cl *mem;
	int ret;

	if (!is_cl_mem(src))
		return -EINVAL;
	mem = dlm_mem_to_cl(src);

	ret = cl_try_copy_cl2cl(mem, dst, size);
	if (ret != -ENOSYS)
		goto finalise;

	ret = cl_try_copy_cl_vms(mem, dst, size, dir);
	if (ret != -ENOSYS)
		goto finalise;

	ret = -ENOSYS;
finalise:
	return ret;
}
