#ifndef DLM_VMS_H__
#define DLM_VMS_H__

#include <dlm/memory.h>

struct dlm_mem_vms {
	struct dlm_mem mem;
	void *va;
};

#define dlm_mem_to_vms(memobj) \
	dlm_mem_to_dlm((memobj), struct dlm_mem_vms, DLM_MAGIC_MEM_VMS)

struct dlm_mem_vms *dlm_vms_allocate_memory(size_t size);
struct dlm_mem_vms *dlm_vms_create_from(struct dlm_mem *master);

static inline bool is_mem_vms(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_VMS;
}

#endif /* DLM_VMS_H__ */
