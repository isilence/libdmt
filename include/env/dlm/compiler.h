#ifndef DLM_COMPILER_H__
#define DLM_COMPILER_H__

#ifdef __GNUC__
	#define DLM_PARAM_UNUSED __attribute__((__unused__))
#else
	#define DLM_PARAM_UNUSED
#endif

#if defined(__cplusplus)
	#define restrict __restrict
#endif

#endif /* DLM_COMPILER_H__ */
