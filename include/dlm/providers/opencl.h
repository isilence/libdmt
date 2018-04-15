#ifndef DLM_OPENCL_MEMORY_H__
#define DLM_OPENCL_MEMORY_H__

#include <dlm/memory.h>
#include <dlm/event.h>
#ifdef __APPLE__
	#include <OpenCL/opencl.h>
	#include <OpenCL/cl_ext.h>
#else
	#include <CL/cl.h>
	#include <CL/cl_ext.h>
#endif

struct dlm_mem_cl {
	struct dlm_mem mem;
	struct dlm_mem *master;

	cl_device_id		dev;
	cl_context		context;
	cl_command_queue	queue;
	cl_mem			clmem;

	cl_int			err;
};

#define dlm_mem_to_cl(memobj) \
	dlm_mem_to_dlm((memobj), struct dlm_mem_cl, DLM_MAGIC_MEM_OPENCL)
#define dlm_cl_to_mem(memobj) (&(memobj)->mem)

struct dlm_mem_cl_context {
	cl_device_id		device;
	cl_context		context;
	cl_command_queue	queue;
};

struct dlm_mem *dlm_cl_allocate_memory( const struct dlm_mem_cl_context *ctx,
					size_t size,
					cl_mem_flags flags);

struct dlm_mem *dlm_cl_create_from_clmem(const struct dlm_mem_cl_context *ctx,
					 cl_mem clmem);


struct dlm_mem *dlm_cl_create_from(const struct dlm_mem_cl_context *ctx,
				   struct dlm_mem *master,
				   cl_mem_flags flags);

struct dlm_event_cl {
	struct dlm_event event;

	cl_event clevent;
	bool ready;
};

#define dlm_event_to_cl(e) container_of((e), struct dlm_event_cl, event)

#endif /* DLM_OPENCL_MEMORY_H__ */
