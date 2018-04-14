#ifndef DLM_VMS_H__
#define DLM_VMS_H__

#include <dlm/memory.h>

struct dlm_vms_mem {
	struct dlm_mem mem;
	void *va;
};

#define dlm_mem_to_vms(memobj) \
	dlm_mem_to_dlm((memobj), struct dlm_vms_mem, DLM_MAGIC_MEM_VMS)
#define dlm_vms_to_mem(memobj) (&(memobj)->mem)

struct dlm_mem * dlm_vms_allocate_memory(size_t size);
struct dlm_mem * dlm_vms_create_from(struct dlm_mem *master);

#endif /* DLM_VMS_H__ */
