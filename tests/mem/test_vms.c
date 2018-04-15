#include <dlm/memory.h>
#include <dlm/providers/vms.h>
#include <dlm/events/seq.h>

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

static int cnt = 0;
static void cevent_callback(struct dlm_event *e, void *data)
{
	cnt += 1;
}

static inline void test_copy_rep(void **state)
{
	struct dlm_obj **pair = (struct dlm_obj **)*state;
	char *src_va, *dst_va;
	struct dlm_mem *src, *dst;
	struct dlm_event_seq *seq;
	struct dlm_sync sync;
	
	assert_non_null(seq = dlm_event_seq_create(&root));
	assert_non_null(sync.waitfor = allocate_event_list(1, &root));
	dlm_event_list_set(sync.waitfor, &seq->event, 0);
	sync.sync = true;

	assert_true(dlm_obj_is_mem(pair[0]));
	assert_true(dlm_obj_is_mem(pair[1]));

	src = dlm_obj_to_mem(pair[0]);
	dst = dlm_obj_to_mem(pair[1]);

	assert_true(src->size == dst->size);
	assert_non_null(src_va = dlm_mem_map(src, DLM_MAP_WRITE));
	assert_non_null(dst_va = dlm_mem_map(dst, DLM_MAP_WRITE));

	memset(dst_va, MAGIC_VALUE, dst->size);
	test_fill_random(src_va, src->size);

	assert_return_code(dlm_mem_unmap(src, src_va), 0);
	assert_return_code(dlm_mem_unmap(dst, dst_va), 0);
	assert_return_code(dlm_mem_copy(src, dst, &sync), 0);

	dlm_event_reg_clb(sync.event, cevent_callback, NULL);
	dlm_event_wait(sync.event, DLM_EVENT_INFINITY);
	assert_int_equal(cnt, 1);

	assert_non_null(src_va = dlm_mem_map(src, DLM_MAP_READ));
	assert_non_null(dst_va = dlm_mem_map(dst, DLM_MAP_READ));
	assert_memory_equal(src_va, dst_va, src->size);

	assert_return_code(dlm_mem_unmap(src, src_va), 0);
	assert_return_code(dlm_mem_unmap(dst, dst_va), 0);

	dlm_event_release(&seq->event);
	dlm_event_release(sync.event);
	dlm_obj_release(&sync.waitfor->obj);
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
		cmocka_unit_test_setup_teardown(test_copy_rep, test_vms_pair_setup, test_vms_pair_teardown),
		cmocka_unit_test(test_object_root),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}