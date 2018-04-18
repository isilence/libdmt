#ifndef DLM_INTERNAL_PROVIDER_OPENCL_H__
#define DLM_INTERNAL_PROVIDER_OPENCL_H__

#include <dlm/providers/opencl.h>

int dlm_rc_cl2unix(cl_int err);
#define dlm_obj_to_cl(dlm_obj) dlm_mem_to_cl(dlm_obj_to_mem((dlm_obj)))

int dlm_mem_cl_copy(struct dlm_mem * restrict src,
		    struct dlm_mem * restrict dst,
		    size_t size,
		    enum DLM_COPY_DIR dir);

#endif /* DLM_INTERNAL_PROVIDER_OPENCL_H__ */
