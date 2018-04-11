#ifndef DLM_VMS_H__
#define DLM_VMS_H__

#include <dlm/memory.h>

#define DLM_MEM_VMS_MAGIC 0x1251

struct dlm_vms_mem {
	struct dlm_mem mem;
	void *va;
};

#define dlm_mem_to_vms(mem) ((struct dlm_vms_mem *)(mem))
#define dlm_vms_to_mem(mem) ((struct dlm_mem *)(mem))

struct dlm_mem * dlm_vms_allocate_memory(size_t size);
struct dlm_mem * dlm_vms_create_from(struct dlm_mem *master);

#endif /* DLM_VMS_H__ */
