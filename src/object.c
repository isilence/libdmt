#include <dlm/object.h>
#include <stdio.h>
#include <stdlib.h>

static void dlm_root_release(struct dlm_obj *obj)
{
	fprintf(stderr, "[DLM][ERROR] Attempt to delete root object\n");
}

struct dlm_obj root = {
	.node = LIST_HEAD_INIT(root.node),
	.deps = LIST_HEAD_INIT(root.deps),
	.master = NULL,
	.magic = DLM_MAGIC_ROOT,
	.nref = 1,
	.release = dlm_root_release
};

void dlm_obj_retain(struct dlm_obj *obj)
{
	obj->nref += 1;
}

void dlm_obj_release(struct dlm_obj *obj)
{
	struct dlm_obj *master;

	if (!obj || obj->nref <= 0)
		return;

	obj->nref -= 1;
	if (obj->nref != 0)
		return;

	master = obj->master;
	list_del(&obj->node);
	obj->release(obj);
	if (master)
		dlm_obj_release(master);
}

void dlm_obj_init(struct dlm_obj *obj, struct dlm_obj *master)
{
	dlm_obj_retain(master);
	list_add(&obj->node, &master->deps);
	INIT_LIST_HEAD(&obj->deps);
	obj->master = master;

	obj->magic = DLM_MAGIC_UNDEFINED;
	obj->nref = 0;
	obj->release = NULL;
}
