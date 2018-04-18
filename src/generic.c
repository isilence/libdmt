#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "generic.h"

void dlm_mem_init(struct dlm_mem *mem,
		  size_t size,
		  magic_t magic)
{
	memset((void *)mem, 0, sizeof(*mem));
	dlm_obj_init(&mem->obj, &root);

	mem->obj.magic = magic;
	mem->size = size;
	mem->ops = NULL;
	mem->err = 0;
	mem->fd = -1;
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
		if (rc1)
			dlm_mem_eventfd_unlock(mem1);
		if (rc2)
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

int dlm_mem_copy_back(struct dlm_mem * restrict src,
		      struct dlm_mem * restrict dst,
		      size_t size,
		      enum DLM_COPY_DIR dir)
{
	if (dir != DLM_COPY_FORWARD)
		return -ENOSYS;

	return dst->ops->copy(dst, src, size, DLM_COPY_BACKWARD);
}

struct dlm_mem** dlm_mem_create_pair(struct dlm_mem *m1,
				     struct dlm_mem *m2)
{
	struct dlm_mem **pair;

	pair = (struct dlm_mem **)malloc(sizeof(*pair) * 2);
	pair[0] = m1;
	pair[1] = m2;

	return pair;
}

struct dlm_mem** dlm_mem_create_pair_locked(struct dlm_mem *m1,
					    struct dlm_mem *m2)
{
	int ret;
	struct dlm_mem **pair = dlm_mem_create_pair(m1, m2);

	if (!pair)
		return NULL;

	ret = dlm_mem_eventfd_lock_pair(m1, m2);
	if (ret) {
		free(pair);
		return NULL;
	}
	return pair;
}

int dlm_mem_pair_free(struct dlm_mem **pair)
{
	int ret1, ret2;

	ret1 = dlm_mem_eventfd_unlock(pair[0]);
	ret2 = dlm_mem_eventfd_unlock(pair[1]);
	free(pair);

	return ret1 ? ret1 : ret2;
}