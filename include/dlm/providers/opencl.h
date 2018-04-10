#ifndef DLM_OPENCL_MEMORY_H__
#define DLM_OPENCL_MEMORY_H__

#include <dlm/memory.h>
#ifdef __APPLE__
	#include <OpenCL/opencl.h>
	#include <OpenCL/cl_ext.h>
#else
	#include <CL/cl.h>
	#include <CL/cl_ext.h>
#endif

#define DLM_MEM_OPENCL_MAGIC 0x1252

struct dlm_cl_memory {
	struct dlm_mem mem;

	cl_device_id		dev;
	cl_context		context;
	cl_command_queue	queue;
	cl_mem			clmem;

	cl_int			err;
};

#define dlm_mem_to_cl(mem) ((struct dlm_cl_memory *)(mem))
#define dlm_cl_to_mem(mem) ((struct dlm_mem *)(mem))

struct dlm_mem_cl_context {
	cl_device_id		device;
	cl_context		context;
	cl_command_queue	queue;
};

struct dlm_mem *dlm_cl_allocate_memory(size_t size,
					struct dlm_mem_cl_context *ctx,
					cl_mem_flags flags);

struct dlm_mem *dlm_cl_create_from_clmem(cl_mem clmem,
					struct dlm_mem_cl_context *ctx);


struct dlm_mem *dlm_cl_create_from(struct dlm_mem *master,
				   struct dlm_mem_cl_context *ctx,
				   cl_mem_flags flags);

#endif /* DLM_OPENCL_MEMORY_H__ */
