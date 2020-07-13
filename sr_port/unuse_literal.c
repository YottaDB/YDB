/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2016-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 5e466fd7... GT.M V6.3-013
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
<<<<<<< HEAD
/* This module is derived from FIS code.
 ****************************************************************
 */

=======
>>>>>>> 5e466fd7... GT.M V6.3-013
#include "mdef.h"

#include "mdq.h"
#include "hashtab_str.h"
#include "compiler.h"
#include "opcode.h"
#include "mmemory.h"
#include "is_equ.h"

GBLREF mliteral 	literal_chain;
GBLREF hash_table_str	*complits_hashtab;

boolean_t unuse_literal(mval *x)
{
	boolean_t	in_hashtab = FALSE;
	ht_ent_str	*litent;
	mliteral	*a = NULL;
	stringkey	litkey;

	if (complits_hashtab && complits_hashtab->base)
	{
		litkey.str = x->str;
		COMPUTE_HASH_STR(&litkey);
		if (NULL != (litent = lookup_hashtab_str(complits_hashtab, &litkey)))
		{
			a = (mliteral *)litent->value;
			assert(a->reference_count != 0);
			a->reference_count -= 1;
			in_hashtab = TRUE;
		}
	} else
	{
		dqloop(&literal_chain, que, a)
		{
			if (is_equ(x, &(a->v)))
			{
				assert(a->reference_count != 0);
				a->reference_count -= 1;
				break;
			}
		}
	}
	/* The first assert here covers the case of no literal in the hashtab, the second in the literal chain */
	assert(a != NULL); /* ATTEMPT TO REMOVE MVAL NOT IN HASHTABLE; THIS WAS CALLED IN ERROR */
	assert(a != &literal_chain); /* This probably means you attempted to remove a literal not in the literal chain */
	if (a->reference_count == 0)
	{
		/* Remove mval */
		if (in_hashtab)
			delete_hashtab_ent_str(complits_hashtab, litent);
		dqdel(a, que);
		return TRUE;
	}
	return FALSE;
}
