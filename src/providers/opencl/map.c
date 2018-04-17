#include "generic.h"
#include "dlm_opencl.h"

static void CL_CALLBACK map_clb(cl_event event, cl_int rc, void *data)
{
	struct dlm_mem_cl *mem = (struct dlm_mem_cl *)data;

	if (rc != CL_SUCCESS) {
		int ret = dlm_rc_cl2unix(rc);

		mem->err = ret;
	}

	dlm_mem_eventfd_unlock(&mem->mem);
	clReleaseEvent(event);
}

void *dlm_mem_cl_map(struct dlm_mem_cl *mem, int flags)
{
	cl_int err;
	cl_event event;
	void *va;
	cl_mem_flags clflags = 0;

	if (flags & DLM_MAP_WRITE)
		clflags |= CL_MAP_WRITE;
	if (flags & DLM_MAP_READ)
		clflags |= CL_MAP_READ;

	if (dlm_mem_eventfd_lock(&mem->mem))
		return NULL;

	va = clEnqueueMapBuffer(mem->queue, mem->clmem, CL_FALSE,
		clflags, 0, mem->mem.size,
		0, NULL, &event, &err);

	if (err != CL_SUCCESS)
		goto error;

	err = clSetEventCallback(event, CL_SUCCESS, map_clb, (void *)mem);
	if (err)
		goto error;

	return va;
error:
	dlm_mem_eventfd_unlock(&mem->mem);
	mem->err = err;
	return NULL;
}

int dlm_mem_cl_unmap(struct dlm_mem_cl *mem, void *va)
{
	cl_int err;
	cl_event event;

	err = clEnqueueUnmapMemObject(mem->queue, mem->clmem,
		va, 0, NULL, &event);

	if (err != CL_SUCCESS)
		goto cleanup;
	err = clSetEventCallback(event, CL_SUCCESS, map_clb, (void *)mem);
	if (err)
		goto cleanup;

cleanup:
	dlm_mem_eventfd_unlock(&mem->mem);
	mem->err = err;
	return dlm_rc_cl2unix(err);
}