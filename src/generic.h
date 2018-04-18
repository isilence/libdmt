#ifndef DLM_MEMORY_OPERATIONS_H__
#define DLM_MEMORY_OPERATIONS_H__

#include <dlm/memory.h>

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic);

void *dlm_mem_map_eventfd(struct dlm_mem *mem,
			  enum DLM_MEM_MAP_FLAGS flags,
			  void *(*map)(struct dlm_mem *,
				       enum DLM_MEM_MAP_FLAGS));

int dlm_mem_unmap_eventfd(struct dlm_mem *dlm_mem,
			  void *va,
			  int (*unmap)(struct dlm_mem *, void *));

int dlm_mem_eventfd_lock(struct dlm_mem *mem);
int dlm_mem_eventfd_unlock(struct dlm_mem *mem);
int dlm_mem_eventfd_lock_pair(struct dlm_mem *mem1,
			      struct dlm_mem *mem2);

int dlm_mem_copy_back(struct dlm_mem * restrict src,
		      struct dlm_mem * restrict dst,
		      size_t size,
		      enum DLM_COPY_DIR forward);

struct dlm_mem** dlm_mem_create_pair(struct dlm_mem *m1,
					struct dlm_mem *m2);

struct dlm_mem** dlm_mem_create_pair_locked(struct dlm_mem *m1,
					    struct dlm_mem *m2);

int dlm_mem_pair_free(struct dlm_mem **pair);

#endif /* DLM_MEMORY_OPERATIONS_H__ */
