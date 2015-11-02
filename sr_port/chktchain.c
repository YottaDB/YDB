/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"

/* Check that the triple chain is a well formed doubly linked list */

void	chktchain(triple *head)
{
	triple *tp, *tp2;

	/* tp2 is needed to detect cycles in the list (so we avoid looping indefinitely) that dont return to the head */
	for (tp = head->exorder.fl, tp2 = tp->exorder.fl; tp != head; tp = tp->exorder.fl, tp2 = tp2->exorder.fl->exorder.fl)
	{
		if (tp->exorder.bl->exorder.fl != tp)
			GTMASSERT;	/* if this assert fails, then recompile with DEBUG_TRIPLES to catch the issue sooner */
		if (tp->exorder.fl->exorder.bl != tp)
			GTMASSERT;	/* if this assert fails, then recompile with DEBUG_TRIPLES to catch the issue sooner */
		if (tp == tp2)		/* cycle found, but without returning to the head of the linked list */
			GTMASSERT;	/* if this assert fails, then recompile with DEBUG_TRIPLES to catch the issue sooner */
	}
}
