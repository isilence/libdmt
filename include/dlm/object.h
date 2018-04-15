#ifndef DLM_OBJECT_H__
#define DLM_OBJECT_H__
#include <env/dlm/magic.h>
#include <env/dlm/list.h>
#include <errno.h>
#include <stddef.h>

typedef unsigned int u32;

struct dlm_obj;
struct dlm_obj_ops;
struct dlm_mem_ops;
struct dlm_mem;
struct dlm_event_ops;
struct dlm_event;
struct dlm_event_list;

struct dlm_obj_ops {
	int (*release)(struct dlm_obj *);
};

struct dlm_obj {
	struct list_head head;
	struct dlm_obj *master;
	magic_t magic;
	int nref;

	int (*release)(struct dlm_obj *);
};

static inline void dlm_obj_init(struct dlm_obj *obj)
{
	INIT_LIST_HEAD(&obj->head);
	obj->master = NULL;
	obj->magic = DLM_MAGIC_UNDEFINED;
	obj->nref = 0;
	obj->release = NULL;
}

static inline void dlm_obj_set_ops(struct dlm_obj *obj,
				   const struct dlm_obj_ops *ops)
{
	obj->release = ops->release;
}

static inline int dlm_obj_retain(struct dlm_obj *obj)
{
	obj->nref += 1;
	return 0;
}

static inline int dlm_obj_release(struct dlm_obj *obj)
{
	if (!obj)
		return -EINVAL;
	if (obj->nref <= 0)
		return -EFAULT;

	obj->nref -= 1;
	if (obj->nref == 0)
		return obj->release(obj);

	return 0;
}

#endif /* DLM_OBJECT_H__ */
