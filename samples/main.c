#include <dlm/memory.h>
#include <dlm/providers/vms.h>
#include <dlm/providers/opencl.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_VALUE 0x66
#define PAGE_SIZE 4096
#define MEM_TEST_SIZE (PAGE_SIZE * 32)

#define check(x) assert(x)

static void fill_random(char *va, size_t size)
{
	size_t i;

	for (i = 0; i < size; ++i)
		va[i] = (char)rand();
}

static void cmp_mem(const char *va1, const char *va2, size_t size)
{
	size_t i;

	for (i = 0; i < size; ++i)
		check(va1[i] == va2[i]);
}

static void test_map(struct dlm_mem *mem)
{
	char *va, *tmp;

	check(mem->size > 0);

	check((tmp = malloc(mem->size)));
	fill_random(tmp, mem->size);

	check((va = dlm_mem_map(mem, DLM_MAP_WRITE)));
	memcpy(va, tmp, mem->size);
	check(!dlm_mem_unmap(mem, va));

	check((va = dlm_mem_map(mem, DLM_MAP_READ)));
	cmp_mem(va, tmp, mem->size);
	check(!dlm_mem_unmap(mem, va));

	free(tmp);
}

static void test_copy(struct dlm_mem *src, struct dlm_mem *dst)
{
	char *src_va, *dst_va;

	check(src->size == dst->size);
	check((src_va = dlm_mem_map(src, DLM_MAP_WRITE)));
	check((dst_va = dlm_mem_map(src, DLM_MAP_WRITE)));

	memset(dst_va, DEFAULT_VALUE, dst->size);
	fill_random(src_va, src->size);

	check(!dlm_mem_unmap(src, src_va));
	check(!dlm_mem_unmap(dst, dst_va));
	check(!dlm_mem_copy(src, dst));

	check((src_va = dlm_mem_map(src, DLM_MAP_READ)));
	check((dst_va = dlm_mem_map(src, DLM_MAP_READ)));
	cmp_mem(src_va, dst_va, src->size);

	check(!dlm_mem_unmap(src, src_va));
	check(!dlm_mem_unmap(dst, dst_va));
}

int main()
{
	struct dlm_mem *svms, *dvms;

	check((svms = dlm_vms_allocate_memory(MEM_TEST_SIZE)));
	check((dvms = dlm_vms_allocate_memory(MEM_TEST_SIZE)));

	test_map(svms);
	test_copy(svms, dvms);

	check(!dlm_mem_release(svms));
	check(!dlm_mem_release(dvms));

	printf("ALL TESTS PASSED!");
	return 0;
}
