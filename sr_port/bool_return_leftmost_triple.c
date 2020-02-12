/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"

LITREF octabstruct	oc_tab[];

/* Given a boolean typed triple (e.g. OC_OR), it returns the leftmost leaf-level triple in the tree rooted at this triple.
 * This is used by various functions to insert a triple BEFORE this leftmost leaf-level triple (e.g. OC_BOOLINIT/OC_ANDOR).
 */
triple *bool_return_leftmost_triple(triple *t)
{
	triple		*t1, *t2;

	if (!(OCT_BOOL & oc_tab[t->opcode].octype))
		return t;
	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		t2 = t1->operand[0].oprval.tref;
		if (!(OCT_BOOL & oc_tab[t2->opcode].octype))
			break;
	}
	return t1;
}
