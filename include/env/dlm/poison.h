#ifndef DLM_POISON_H__
#define DLM_POISON_H__

# define POISON_POINTER_DELTA 0

#define LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x200 + POISON_POINTER_DELTA)


#endif /* DLM_POISON_H__ */
