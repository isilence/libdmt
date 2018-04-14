#include <dlm/memory.h>
#include <dlm/providers/vms.h>

#include "../test_base.h"
#include "test_mem.h"

static int test_vms_setup(void **state)
{
	struct dlm_mem *mem;
	struct dlm_obj *obj;

	mem = dlm_vms_allocate_memory(MEM_TEST_SIZE);
	if (!mem)
		return -EFAULT;

	obj = &mem->obj;
	*state = (void *)obj;

	return 0;
}

TEST_IMPL_TWIN_INIT(vms, test_vms_setup, test_obj_teardown)
#define vms_obj_test(foo) \
	cmocka_unit_test_setup_teardown(foo, test_vms_setup, test_obj_teardown)

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_object_destruction, test_vms_setup),
		vms_obj_test(test_mem_map),
		cmocka_unit_test_setup_teardown(test_copy, test_vms_pair_setup, test_vms_pair_teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}