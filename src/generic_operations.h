#ifndef DLM_MEMORY_OPERATIONS_H__
#define DLM_MEMORY_OPERATIONS_H__

#include <dlm/memory.h>

int dlm_mem_generic_copy(struct dlm_mem * restrict src,
			 struct dlm_mem * restrict dst);

void dlm_init_mem(struct dlm_mem *mem,
		  size_t size,
		  u32 magic);

#endif /* DLM_MEMORY_OPERATIONS_H__ */
