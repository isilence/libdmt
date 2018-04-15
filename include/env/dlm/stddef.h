#ifndef DLM_STDDEF_H__
#define DLM_STDDEF_H__
#include <env/dlm/compiler.h>

#define DLM_QUOTE_(str)         #str
#define DLM_QUOTE(str)          DLM_QUOTE_(str)
#define DLM_CONCAT_(str1, str2) str1 ## str2
#define DLM_CONCAT(str1, str2)  DLM_CONCAT_(str1, str2)

#undef offsetof
#ifdef __compiler_offsetof
    #define offsetof(TYPE, MEMBER)  __compiler_offsetof(TYPE, MEMBER)
#else
    #define offsetof(TYPE, MEMBER)((size_t)&((TYPE *)0)->MEMBER)
#endif


/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	((type *)(__mptr - offsetof(type, member))); })


#ifndef NULL
    #define NULL ((void *)0)
#endif

#ifndef DLM_CPP
    typedef _Bool			bool;
    enum {
        false   = 0,
        true    = 1
    };
#endif

#ifdef DLM_CPP
	#define DLM_STRUCT_TYPE(type) type
#else
	#define DLM_STRUCT_TYPE(type) struct type
#endif

#define DLM_PREDEFINE_CSTRUCT(type) \
	struct type; \
	typedef DLM_STRUCT_TYPE(type) DLM_CONCAT(type, _t)

#define DLM_DEFINE_CSTRUCT(type) \
	DLM_PREDEFINE_CSTRUCT(type); \
	struct type


#endif /* DLM_STDDEF_H__ */
