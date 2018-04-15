#include "cl_event.h"
#include "common.h"

static const struct dlm_event_ops event_cl_ops;
static const struct dlm_event_ops event_cl_ops_user;

int rc_cl2unix(cl_int err)
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

static int event_cl_wait(struct dlm_event *dlm_event, u32 ms)
{
	struct dlm_event_cl *e;
	cl_int ret;

	e = dlm_extract_event_cl(dlm_event);
	if (!e)
		return -EINVAL;

	if (ms != DLM_EVENT_INFINITY)
		return -ENOSYS;

	ret = clWaitForEvents(1, &e->clevent);
	return rc_cl2unix(ret);
}

static bool event_cl_ready(struct dlm_event *dlm_event)
{
	struct dlm_event_cl *e;

	e = dlm_extract_event_cl(dlm_event);
	if (!e)
		return false;

	return e->ready;
}

static int event_cl_signal(struct dlm_event *event)
{
	return -ENOSYS;
}

static int event_cl_signal_user(struct dlm_event *dlm_event)
{
	struct dlm_event_cl *e;
	cl_int ret;

	e = dlm_extract_event_cl(dlm_event);
	if (!e)
		return -EINVAL;

	if (e->event.ops != &event_cl_ops_user)
		return -EINVAL;

	ret = clSetUserEventStatus(e->clevent, CL_COMPLETE);
	if (ret == CL_SUCCESS)
		e->ready = true;

	return rc_cl2unix(ret);
}

static int event_cl_release(struct dlm_obj *obj)
{
	struct dlm_event *dlm_event;
	struct dlm_event_cl *e;
	cl_int ret;

	dlm_event = dlm_obj_to_event(obj);
	e = dlm_extract_event_cl(dlm_event);
	if (!e)
		return -EINVAL;

	ret = clReleaseEvent(e->clevent);
	return rc_cl2unix(ret);
}

static const struct dlm_event_ops event_cl_ops = {
	.wait = event_cl_wait,
	.signal = event_cl_signal,
	.ready = event_cl_ready,
};

static const struct dlm_event_ops event_cl_ops_user = {
	.wait = event_cl_wait,
	.signal = event_cl_signal_user,
	.ready = event_cl_ready,
};

static const struct dlm_obj_ops event_cl_obj_ops = {
	.release = event_cl_release,
};

static struct dlm_event_cl *allocate_event_cl_init(cl_event clevent, bool user)
{
	struct dlm_event_cl *e;
	const struct dlm_event_ops *ops;

	ops = (user ? &event_cl_ops_user : &event_cl_ops);

	e = (struct dlm_event_cl *)malloc(sizeof(*e));
	dlm_event_init(&e->event, DLM_MAGIC_EVENT_CL,
		       ops, &event_cl_obj_ops);

	e->clevent = clevent;
	dlm_event_retain(&e->event);

	return 0;
}

struct dlm_event_cl *allocate_event_cl(cl_event clevent)
{
	cl_int ret;
	static struct dlm_event_cl *e;

	ret = clRetainEvent(clevent);
	if (ret != CL_SUCCESS)
		return NULL;

	e = allocate_event_cl_init(clevent, false);
	if (!e)
		clReleaseEvent(clevent);
	return e;
}

struct dlm_event_cl *allocate_event_cl_user(struct dlm_mem_cl *mem)
{
	cl_int ret;
	cl_event clevent;
	static struct dlm_event_cl *e;

	clevent = clCreateUserEvent(mem->context, &ret);
	if (ret != CL_SUCCESS)
		return NULL;

	e = allocate_event_cl_init(clevent, false);
	if (!e)
		clReleaseEvent(clevent);
	return e;
}
