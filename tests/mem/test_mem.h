#ifndef DLM_TESTS_TEST_MEM_H__
#define DLM_TESTS_TEST_MEM_H__

#include <dlm/memory.h>
#include <dlm/providers/vms.h>

#include "../test_base.h"

#define PAGE_SIZE 4096
#define MEM_TEST_SIZE (PAGE_SIZE * 32)

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

static inline void test_mem_map(void **state)
{
	struct dlm_obj *obj = (struct dlm_obj *)*state;
	struct dlm_mem *mem = dlm_obj_to_mem(obj);
	char *va, *tmp;

	assert_true(mem->size >= MEM_TEST_SIZE);
	assert_non_null((tmp = malloc(mem->size)));
	test_fill_random(tmp, mem->size);

	assert_non_null((va = dlm_mem_map(mem, DLM_MAP_WRITE)));
	memcpy(va, tmp, mem->size);

	assert_return_code(dlm_mem_unmap(mem, va), 0);
	assert_non_null((va = dlm_mem_map(mem, DLM_MAP_READ)));
	assert_memory_equal(va, tmp, mem->size);
	assert_return_code(dlm_mem_unmap(mem, va), 0);

	free(tmp);
}

static inline void test_copy(void **state)
{
	struct dlm_obj **pair = (struct dlm_obj **)*state;
	char *src_va, *dst_va;
	struct dlm_mem *src, *dst;

	assert_true(dlm_obj_is_mem(pair[0]));
	assert_true(dlm_obj_is_mem(pair[1]));

	src = dlm_obj_to_mem(pair[0]);
	dst = dlm_obj_to_mem(pair[1]);

	assert_true(src->size == dst->size);
	assert_non_null((src_va = dlm_mem_map(src, DLM_MAP_WRITE)));
	assert_non_null((dst_va = dlm_mem_map(dst, DLM_MAP_WRITE)));

	memset(dst_va, MAGIC_VALUE, dst->size);
	test_fill_random(src_va, src->size);

	assert_return_code(dlm_mem_unmap(src, src_va), 0);
	assert_return_code(dlm_mem_unmap(dst, dst_va), 0);
	assert_return_code(dlm_mem_copy(src, dst), 0);

	assert_non_null((src_va = dlm_mem_map(src, DLM_MAP_READ)));
	assert_non_null((dst_va = dlm_mem_map(dst, DLM_MAP_READ)));
	assert_memory_equal(src_va, dst_va, src->size);

	assert_return_code(dlm_mem_unmap(src, src_va), 0);
	assert_return_code(dlm_mem_unmap(dst, dst_va), 0);
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
