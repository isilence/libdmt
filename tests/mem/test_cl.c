#include <dlm/memory.h>
#include <dlm/providers/opencl.h>
#include <dlm/events/seq.h>

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
	struct dlm_mem *mem;
	struct dlm_obj *obj;
	const struct dlm_mem_cl_context ctx = {
		.device = device,
		.context = context,
		.queue = queue,
	};

	mem = dlm_cl_allocate_memory(&ctx, MEM_TEST_SIZE, CL_MEM_READ_WRITE);
	if (!mem)
		return -EFAULT;

	obj = &mem->obj;
	*state = (void *)obj;

	return 0;
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
		opencl_obj_test(test_mem_map),
		cmocka_unit_test_setup_teardown(test_copy, test_opencl_pair_setup, test_opencl_pair_teardown),
	};

	ret = cmocka_run_group_tests(tests, NULL, NULL);
	ret |= deinit_opencl();

	return ret;
}