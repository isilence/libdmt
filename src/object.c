#include <dlm/object.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_TOKEN "[DLM][OBJ] "


static void dlm_root_release(struct dlm_obj *obj)
{
	fprintf(stderr, "[DLM][ERROR] Attempt to delete root object\n");
}

struct dlm_obj root = {
	.node = LIST_HEAD_INIT(root.node),
	.deps = LIST_HEAD_INIT(root.deps),
	.parent = NULL,
	.magic = DLM_MAGIC_ROOT,
	.nref = 1,
	.release = dlm_root_release
};

void dlm_obj_retain(struct dlm_obj *obj)
{
	atomic_fetch_add(&obj->nref, 1);
}

void dlm_obj_release(struct dlm_obj *obj)
{
	struct dlm_obj *master;
	int nref_old;

	nref_old = atomic_fetch_sub(&obj->nref, 1);
	if (nref_old <= 0) {
		fprintf(stderr, DEBUG_TOKEN "Invalid ref counting");
		return;
	}

	if (nref_old == 1) {
		master = obj->parent;
		list_del(&obj->node);
		obj->release(obj);
		if (master)
			dlm_obj_release(master);
	}
}

void dlm_obj_init(struct dlm_obj *obj, struct dlm_obj *master)
{
	dlm_obj_retain(master);
	list_add(&obj->node, &master->deps);
	INIT_LIST_HEAD(&obj->deps);
	obj->parent = master;

	obj->magic = DLM_MAGIC_UNDEFINED;
	obj->release = NULL;
	atomic_init(&obj->nref, 0);
}
