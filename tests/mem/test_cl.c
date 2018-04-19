#include <dlm/memory.h>
#include <dlm/providers/opencl.h>

#include "../test_base.h"
#include "test_mem.h"

static cl_device_id device;
static cl_platform_id platform;
static cl_context context;
static cl_command_queue queue;

static int init_opencl(void)
{
	cl_int ret;
	cl_uint num_devices;
	cl_uint num_platforms = 0;

	ret = clGetPlatformIDs(1, &platform, &num_platforms);
	if (ret != CL_SUCCESS)
		return -EFAULT;
	if (num_platforms < 1)
		return -ENOSYS;

	ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, &num_devices);
	if (ret != CL_SUCCESS || num_devices < 1)
		return -EFAULT;

	context = clCreateContext(NULL, 1, &device, NULL, NULL, &ret);
	if (ret != CL_SUCCESS)
		return -EFAULT;

	queue = clCreateCommandQueue(context, device, 0, &ret);
	if (ret != CL_SUCCESS)
		return -EFAULT;

	return 0;
}

static int deinit_opencl(void)
{
	cl_int ret = 0;

	ret |= clReleaseCommandQueue(queue);
	ret |= clReleaseContext(context);
	ret |= clReleaseDevice(device);

	return ret;
}


static int test_opencl_setup(void **state)
{
	struct dlm_mem_cl *mem;
	struct dlm_obj *obj;
	const struct dlm_mem_cl_context ctx = {
		.device = device,
		.context = context,
		.queue = queue,
		.platform = platform,
	};

	mem = dlm_mem_cl_alloc(&ctx, MEM_TEST_SIZE, CL_MEM_READ_WRITE);
	if (!mem)
		return -EFAULT;

	obj = &mem->mem.obj;
	*state = (void *)obj;

	return 0;
}

static void test_copy(void **state)
{
	struct dlm_mem_vms *vms1, *vms2;
	struct dlm_mem_cl *cl =
		dlm_mem_to_cl(dlm_obj_to_mem((struct dlm_obj *)*state));

	assert_true(dlm_obj_is_mem(&cl->mem.obj));
	assert_non_null(vms1 = dlm_vms_allocate_memory(cl->mem.size));
	assert_non_null(vms2 = dlm_vms_allocate_memory(cl->mem.size));

	memset(vms2->va, MAGIC_VALUE, vms2->mem.size);
	test_fill_random(vms1->va, vms1->mem.size);

	char *ss = vms1->va;
	(void)ss;

	assert_return_code(dlm_mem_copy(&vms1->mem, &cl->mem), 0);
	dlm_wait_mem(&cl->mem);
	assert_return_code(cl->mem.err, 0);
	assert_return_code(vms1->mem.err, 0);

	assert_return_code(dlm_mem_copy(&cl->mem, &vms2->mem), 0);
	dlm_wait_mem(&vms2->mem);
	assert_return_code(cl->mem.err, 0);
	assert_return_code(vms2->mem.err, 0);

	assert_memory_equal(vms1->va, vms2->va, vms1->mem.size);

	dlm_mem_release(&vms1->mem);
	dlm_mem_release(&vms2->mem);
}

static void test_map_read(void **state)
{
	void *va;
	struct dlm_mem_vms *vms;
	struct dlm_mem_cl *cl =
		dlm_mem_to_cl(dlm_obj_to_mem((struct dlm_obj *)*state));

	assert_true(dlm_obj_is_mem(&cl->mem.obj));
	assert_non_null(vms = dlm_vms_allocate_memory(cl->mem.size));

	test_fill_random(vms->va, vms->mem.size);

	assert_return_code(dlm_mem_copy(&vms->mem, &cl->mem), 0);
	assert_return_code(dlm_wait_mem(&cl->mem), 0);
	assert_return_code(cl->mem.err, 0);

	va = dlm_mem_cl_map(cl, DLM_MAP_READ);
	assert_return_code(dlm_wait_mem(&cl->mem), 0);
	assert_return_code(cl->mem.err, 0);
	assert_non_null(va);

	assert_memory_equal(va, vms->va, cl->mem.size);

	assert_return_code(dlm_mem_cl_unmap(cl, va), 0);
	assert_return_code(dlm_wait_mem(&cl->mem), 0);
	assert_return_code(cl->mem.err, 0);

	dlm_mem_release(&vms->mem);
}

static void test_map_write(void **state)
{
	void *va;
	struct dlm_mem_vms *vms, *vms_tmp;
	struct dlm_mem_cl *cl =
		dlm_mem_to_cl(dlm_obj_to_mem((struct dlm_obj *)*state));

	assert_true(dlm_obj_is_mem(&cl->mem.obj));
	assert_non_null(vms = dlm_vms_allocate_memory(cl->mem.size));
	assert_non_null(vms_tmp = dlm_vms_allocate_memory(cl->mem.size));

	test_fill_random(vms_tmp->va, vms->mem.size);

	va = dlm_mem_cl_map(cl, DLM_MAP_WRITE);
	assert_return_code(dlm_wait_mem(&cl->mem), 0);
	assert_return_code(cl->mem.err, 0);
	assert_non_null(va);
	memcpy(va, vms_tmp->va, cl->mem.size);
	assert_return_code(dlm_mem_cl_unmap(cl, va), 0);
	assert_return_code(dlm_wait_mem(&cl->mem), 0);
	assert_return_code(cl->mem.err, 0);

	assert_return_code(dlm_mem_copy(&cl->mem, &vms->mem), 0);
	assert_return_code(dlm_wait_mem(&vms->mem), 0);
	assert_return_code(vms->mem.err, 0);

	assert_memory_equal(vms->va, vms_tmp->va, cl->mem.size);

	dlm_mem_release(&vms->mem);
	dlm_mem_release(&vms_tmp->mem);
}


TEST_IMPL_TWIN_INIT(opencl, test_opencl_setup, test_obj_teardown)
#define opencl_obj_test(foo) \
	cmocka_unit_test_setup_teardown(foo, test_opencl_setup, test_obj_teardown)

int main(void)
{
	int ret;

	ret = init_opencl();
	if (ret == -ENOSYS) {
		printf("No opencl devices found!\n");
		return 0;
	}
	if (ret)
		return EXIT_FAILURE;

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_object_destruction, test_opencl_setup),
		opencl_obj_test(test_copy),
		opencl_obj_test(test_map_read),
		opencl_obj_test(test_map_write),
		cmocka_unit_test(test_object_root),
	};

	ret = cmocka_run_group_tests(tests, NULL, NULL);
	ret |= deinit_opencl();

	return ret;
}