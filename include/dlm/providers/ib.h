#ifndef DLM_IBVERBS_MEMORY_H__
#define DLM_IBVERBS_MEMORY_H__

#include <infiniband/verbs.h>

#include <dlm/memory.h>

struct dlm_mem_ib {
	struct dlm_mem mem;
	struct dlm_mem *master;


	struct ibv_pd *pd;
	struct ibv_mr *mr;
};

#define dlm_mem_to_ib(memobj) \
	dlm_mem_to_dlm((memobj), struct dlm_mem_ib, DLM_MAGIC_MEM_IB)

struct dlm_mem *dlm_mem_ib_alloc(struct ibv_pd *pd,
				 size_t size,
				 int mr_reg_flags);

static inline bool is_ib_mem(struct dlm_mem *mem)
{
	return dlm_mem_get_magic(mem) == DLM_MAGIC_MEM_IB;
}

#endif /* DLM_IBVERBS_MEMORY_H__ */
