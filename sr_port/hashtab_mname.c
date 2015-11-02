/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "min_max.h"
#include "gtm_string.h"
#include "error.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include <rtnhdr.h>
#include "hashtab_mname.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF symval			*curr_symval;

#define MNAME_HASH

/* The below include generates the hash table routines for the "mname" hash type */
#include "hashtab_implementation.h"

/* While the above include creates the bulk of the routines in this modules, for the mname hash type we
   add one more routine that is a wrapper for the add_hashtab_mname created above. The situation this
   wrapper (add_hashtab_mname_symval) covers is if the hash table was expanded, it goes back through the
   stack and fixes the stack references kept in l_symval entries in each stackframe.

   Note that there are currently no direct callers of expand_hashtab_mname() (except in op_view) that we
   don't already protect or of delete_hashtab_mname() or they would need similar wrappers here.
*/
boolean_t add_hashtab_mname_symval(hash_table_mname *table, mname_entry *key, void *value, ht_ent_mname **tabentptr)
{
	boolean_t		retval;
	int			table_size_orig;
	ht_ent_mname		*table_base_orig;

	/* Currently only two values we expect here shown below. If calls are added with other values, they need
	   to be taken care of here and in EXPAND_HASHTAB in hashtab_implementation.h
	*/
	assert(table == &curr_symval->h_symtab || table == &curr_symval->last_tab->h_symtab);
	assert(FALSE == key->marked);

	/* remember table we started with */
	table_base_orig = table->base;
	table_size_orig = table->size;

	/* We'll do the base release once we do the reparations */
	DEFER_BASE_REL_HASHTAB(table, TRUE);

	/* Call real table function */
	retval = add_hashtab_mname(table, key, value, tabentptr);

	/* If the hash table has not changed, we are done */
	if (table_base_orig == table->base)
	{
		DEFER_BASE_REL_HASHTAB(table, FALSE);
		return retval;
	}

	/* Otherwise we have some work to do to repair the l_symtab entries in the stack */
	als_lsymtab_repair(table, table_base_orig, table_size_orig);

	/* We're done with the previous base */
	FREE_BASE_HASHTAB(table, table_base_orig);
	DEFER_BASE_REL_HASHTAB(table, FALSE);

	return retval;
}
