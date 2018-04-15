#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>

#include "generic.h"

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic)
{
	dlm_obj_init(&mem->obj, &root);

	mem->obj.magic = magic;
	mem->size = size;
	mem->ops = NULL;
	mem->err;
}

int dlm_mem_eventfd_lock(struct dlm_mem *mem)
{
	uint64_t buf;
	int ret;

	ret = (int)read(mem->fd, &buf, sizeof(buf));
	if (ret != sizeof(buf))
		return ret ? errno : -EFAULT;

	return 0;
}

int dlm_mem_eventfd_lock_pair(struct dlm_mem *mem1,
			      struct dlm_mem *mem2)
{
	int rc1 = dlm_mem_eventfd_lock(mem1);
	int rc2 = dlm_mem_eventfd_lock(mem2);

	if (rc1 || rc2) {
		dlm_mem_eventfd_unlock(mem1);
		dlm_mem_eventfd_unlock(mem2);

		return -EFAULT;
	}
	return 0;
}

int dlm_mem_eventfd_unlock(struct dlm_mem *mem)
{
	uint64_t buf = 666;
	int ret;

	ret = (int)write(mem->fd, &buf, sizeof(buf));
	if (ret != sizeof(buf))
		return ret ? errno : -EFAULT;

	return 0;
}

void *dlm_mem_map_eventfd(struct dlm_mem *mem,
			  enum DLM_MEM_MAP_FLAGS flags,
			  void *(*map)(struct dlm_mem *,
				       enum DLM_MEM_MAP_FLAGS))
{
	void *res;

	if (!dlm_mem_eventfd_lock(mem))
		return NULL;

	res = map(mem, flags);
	if (res)
		return res;

/* error: */
	dlm_mem_eventfd_unlock(mem);
	return NULL;
}


int dlm_mem_unmap_eventfd(struct dlm_mem *mem,
			  void *va,
			  int (*unmap)(struct dlm_mem *, void *))
{
	int ret ;

	ret = dlm_mem_eventfd_lock(mem);
	if (ret)
		return ret;

	return unmap(mem, va);
}

int dlm_mem_copy_back(struct dlm_mem * restrict src,
		      struct dlm_mem * restrict dst,
		      enum DLM_COPY_DIR dir)
{
	if (dir != DLM_COPY_FORWARD)
		return -ENOSYS;

	return dst->ops->copy(dst, src, DLM_COPY_BACKWARD);
}
