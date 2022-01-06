/*-------------------------------------------------------------------------
 *
 * object_hooks.c
 *		Facility to test object type hooks.
 *
 * Copyright (c) 1996-2022, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		object_hooks/object_hooks.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>
#include <sys/time.h>

#include "postgres.h"
#include "fmgr.h"

#include "catalog/dependency.h"
#include "catalog/objectaccess.h"

/* Allow load of this module in shared libs */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

/* Hold previous logging hook */
static object_access_hook_type prev_object_access_hook = NULL;


/*
 * object_hooks_entry
 *
 * Entrypoint of the object_access_hook, generating log information
 * about the objects gone through.
 */
static void
object_hooks_access_entry(ObjectAccessType access,
						  Oid classId,
						  Oid objectId,
						  int subId,
						  void *arg)
{
	char	   *type;
	char	   *identity;
	char	   *accessname;
	ObjectAddress address;

	address.classId = classId;
	address.objectId = objectId;
	address.objectSubId = subId;

	switch (access)
	{
		case OAT_POST_CREATE:
			{
				ObjectAccessPostCreate *pc_arg = arg;

				/* leave if the change is internal */
				if (pc_arg && pc_arg->is_internal)
					return;

				accessname = "OAT_POST_CREATE";
			}
			break;
		case OAT_DROP:
			{
				ObjectAccessDrop *drop_arg = (ObjectAccessDrop *) arg;

				/* leave if the change is internal */
				if ((drop_arg->dropflags & PERFORM_DELETION_INTERNAL) != 0)
					return;

				accessname = "OAT_DROP";
			}
			break;
		case OAT_POST_ALTER:
			{
				ObjectAccessPostAlter *pa_arg = arg;

				/* leave if the change is internal */
				if (pa_arg->is_internal)
					goto finish;

				accessname = "OAT_POST_ALTER";
			}
			break;
		case OAT_NAMESPACE_SEARCH:
			accessname = "OAT_NAMESPACE_SEARCH";
			break;
		case OAT_FUNCTION_EXECUTE:
			accessname = "OAT_FUNCTION_EXECUTE";
			break;
		case OAT_TRUNCATE:
			accessname = "OAT_TRUNCATE";
			break;

		/* no default to let the compiler warn about missing values */
	}

	/*
	 * Generate a LOG entry based on the information passed around, for
	 * regression tests.  First fetch some information about the object,
	 * and just leave if it does not exist.
	 */

	identity = getObjectIdentity(&address, true);
	if (identity == NULL)
	{
		/* leave as there is nothing */
		goto finish;
	}

	/* the object type can never be NULL */
	type = getObjectTypeDescription(&address, true);

	/* generate the log entry */
	ereport(NOTICE,
			(errmsg("access: %s type: %s, identity: %s",
					accessname, type, identity)));;

finish:
	if (prev_object_access_hook)
		(*prev_object_access_hook) (access, classId, objectId,
									subId, arg);
}


/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
	prev_object_access_hook = object_access_hook;
	object_access_hook = object_hooks_access_entry;
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
	object_access_hook = prev_object_access_hook;
}
