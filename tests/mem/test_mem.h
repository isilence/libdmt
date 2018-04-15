#ifndef DLM_TESTS_TEST_MEM_H__
#define DLM_TESTS_TEST_MEM_H__

#include <poll.h>
#include <dlm/memory.h>
#include <dlm/providers/vms.h>

#include "../test_base.h"
#include "generic.h"

#define PAGE_SIZE 4096
#define MEM_TEST_SIZE (PAGE_SIZE * 2048)

static void (*old_release)(struct dlm_obj *);
static int is_released_called_cnt;

static inline void check_release(struct dlm_obj *obj) {
	is_released_called_cnt += 1;
	old_release(obj);
}

static inline void test_object_root(void **state)
{
	assert_int_equal(root.nref, 1);
}


static inline void test_object_destruction(void **state)
{
	struct dlm_obj *obj = (struct dlm_obj *)*state;
	const struct dlm_obj_ops ops = {
		.release = check_release,
	};

	old_release = obj->release;
	dlm_obj_set_ops(obj, &ops);

	is_released_called_cnt = 0;
	dlm_obj_retain(obj);
	assert_int_equal(is_released_called_cnt, 0);
	dlm_obj_release(obj);
	assert_int_equal(is_released_called_cnt, 0);
	dlm_obj_release(obj);
	assert_int_equal(is_released_called_cnt, 1);
}

static inline int dlm_wait_mem(struct dlm_mem *mem)
{
	int ret;
	struct pollfd pfd = {
		.fd = mem->fd,
		.events = POLLIN | POLLOUT,
	};

	ret = poll(&pfd, 1, 0);
	if (!ret)
		return ret;

	if (!(pfd.revents & POLLIN))
		return -EFAULT;

	return 0;
}

static inline int
__internal_test_impl_pair_setup(void **state,
				int setup1(void **),
				int teardown1(void **),
				int setup2(void **),
				int teardown2(void **))
{
	void *obj1, *obj2;
	void **pair;
	int err;

	pair = (void **)malloc(sizeof(*pair) * 2);
	if (!pair) {
		err = -ENOMEM;
		goto error;
	}

	err = setup1(&obj1);
	if (err)
		goto free_pair;

	err = setup2(&obj2);
	if (err)
		goto teardown1;

	pair[0] = obj1;
	pair[1] = obj2;
	*state = (void *)pair;

	return 0;

/* teardown2: */
teardown1:
	teardown1(obj1);
free_pair:
	free((void *)pair);
error:
	return err;
}

static inline int
__internal_test_impl_pair_teardown(void **state,
				int teardown1(void **),
				int teardown2(void **))
{
	void *obj1, *obj2;
	void **pair;
	int err;

	pair = (void **)*state;
	obj1 = pair[0];
	obj2 = pair[1];

	err = teardown1(&obj1);
	err |= teardown2(&obj2);

	free(*state);

	return err;
}


#define TEST_IMPL_PAIR_INIT(name, setup1, teardown1, setup2, teardown2) \
	static inline int test_##name##_pair_setup (void **state) \
	{ \
		return __internal_test_impl_pair_setup(state, \
			setup1, teardown1, \
			setup2, teardown2); \
	} \
	static inline int test_##name##_pair_teardown (void **state) \
	{ \
		return __internal_test_impl_pair_teardown(state, teardown1, teardown2); \
	}

#define TEST_IMPL_TWIN_INIT(name, setup, teardown) \
	TEST_IMPL_PAIR_INIT(name, setup, teardown, setup, teardown)

#endif /* DLM_TESTS_TEST_MEM_H__ */
