#include <dlm/memory.h>
#include <dlm/providers/vms.h>

#include "../test_base.h"
#include "test_mem.h"

static int test_vms_setup(void **state)
{
	struct dlm_mem_vms *mem;
	struct dlm_obj *obj;

	mem = dlm_vms_allocate_memory(MEM_TEST_SIZE);
	if (!mem)
		return -EFAULT;

	obj = &mem->mem.obj;
	*state = (void *)obj;

	return 0;
}

static inline void test_copy(void **state)
{
	struct dlm_obj **pair = (struct dlm_obj **)*state;
	struct dlm_mem_vms *src, *dst;

	assert_true(dlm_obj_is_mem(pair[0]));
	assert_true(dlm_obj_is_mem(pair[1]));

	src = dlm_mem_to_vms(dlm_obj_to_mem(pair[0]));
	dst = dlm_mem_to_vms(dlm_obj_to_mem(pair[1]));

	assert_true(src->mem.size == dst->mem.size);
	memset(dst->va, MAGIC_VALUE, dst->mem.size);
	test_fill_random(src->va, src->mem.size);

	assert_return_code(dlm_mem_copy(&src->mem, &dst->mem), 0);
	dlm_wait_mem(&dst->mem);

	assert_memory_equal(src->va, dst->va, dst->mem.size);
}

TEST_IMPL_TWIN_INIT(vms, test_vms_setup, test_obj_teardown)
#define vms_obj_test(foo) \
	cmocka_unit_test_setup_teardown(foo, test_vms_setup, test_obj_teardown)

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_object_destruction, test_vms_setup),
		cmocka_unit_test_setup_teardown(test_copy, test_vms_pair_setup, test_vms_pair_teardown),
		cmocka_unit_test(test_object_root),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}