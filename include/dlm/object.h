#ifndef DLM_OBJECT_H__
#define DLM_OBJECT_H__
#include <env/dlm/magic.h>
#include <env/dlm/list.h>
#include <errno.h>
#include <stddef.h>

typedef unsigned int u32;

#define DLM_MAGIC_ROOT \
	DLM_MAGIC_CREATE(DLM_MAGIC_BASE_CREATE(0x9007), 0)

struct dlm_obj;
struct dlm_obj_ops;
struct dlm_mem_ops;
struct dlm_mem;
struct dlm_event_ops;
struct dlm_event;
struct dlm_event_list;

struct dlm_obj_ops {
	void (*release)(struct dlm_obj *);
};

struct dlm_obj {
	struct list_head node;
	struct list_head deps;
	struct dlm_obj *master;

	magic_t magic;
	int nref;

	void (*release)(struct dlm_obj *);
};

struct dlm_obj root;

void	dlm_obj_retain(struct dlm_obj *obj);
void	dlm_obj_release(struct dlm_obj *obj);
void	dlm_obj_init(struct dlm_obj *obj, struct dlm_obj *master);

static inline void dlm_obj_set_ops(struct dlm_obj *obj,
				   const struct dlm_obj_ops *ops)
{
	obj->release = ops->release;
}



#endif /* DLM_OBJECT_H__ */
