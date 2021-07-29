/*-------------------------------------------------------------------------
 *
 * object_hooks.c
 *		Facility to test object type hooks.
 *
 * Copyright (c) 1996-2021, PostgreSQL Global Development Group
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

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_class_d.h"
#include "catalog/pg_proc_d.h"
#include "catalog/pg_type.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/* Allow load of this module in shared libs */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

/* Hold previous logging hook */
static object_access_hook_type prev_object_access_hook = NULL;

static Oid
get_type_namespace(Oid typid)
{
	HeapTuple   tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		Oid		result;

		result = typtup->typnamespace;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidOid;
}

static char *
get_type_name(Oid typid)
{
	HeapTuple   tp;

	tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_type typtup = (Form_pg_type) GETSTRUCT(tp);
		char	   *result;

		result = pstrdup(NameStr(typtup->typname));
		ReleaseSysCache(tp);
		return result;
	}
	else
		return NULL;
}

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

	/* Skip any temporary objects or some special patterns */
	if (classId == RelationRelationId)
	{
		char	   *relname;

		/* temporary relations */
		if (isTempOrTempToastNamespace(get_rel_namespace(objectId)))
			return;

		/*
		 * Skip relations named pg_temp_N, coming from table rewrites
		 * in ALTER TABLE for example.
		 */
		relname = get_rel_name(objectId);
		if (relname && strncmp("pg_temp_", relname, 8) == 0)
			return;
	}
	else if (classId == ProcedureRelationId)
	{
		/* temporary functions */
		if (isTempOrTempToastNamespace(get_func_namespace(objectId)))
			return;
	}
	else if (classId == TypeRelationId)
	{
		char	*typname;

		/* temporary types */
		if (isTempOrTempToastNamespace(get_type_namespace(objectId)))
			return;

		/*
		 * Skip types named pg_temp_N, coming from table rewrites
		 * in ALTER TABLE for example.
		 */
		typname = get_type_name(objectId);
		if (typname &&
			(strncmp("pg_temp_", typname, 8) == 0 ||
			 strncmp("_pg_temp_", typname, 9) == 0))
			return;
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

	switch (access)
	{
		case OAT_POST_CREATE:
			accessname = "OAT_POST_CREATE";
			break;
		case OAT_DROP:
			accessname = "OAT_DROP";
			break;
		case OAT_POST_ALTER:
			accessname = "OAT_POST_ALTER";
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
