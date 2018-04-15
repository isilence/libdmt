#ifndef DLM_TESTS_TESTHELPER_H__
#define DLM_TESTS_TESTHELPER_H__

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cmocka.h>

#define MAGIC_VALUE 0x66

static inline void test_fill_random(char *va, size_t size)
{
	size_t i;

	for (i = 0; i < size; ++i)
		va[i] = (char)rand();
}

static inline int test_obj_teardown(void **state)
{
	struct dlm_obj *obj = (struct dlm_obj *)*state;

	dlm_obj_release(obj);
	return 0;
}


#endif /* DLM_TESTS_TESTHELPER_H__ */
