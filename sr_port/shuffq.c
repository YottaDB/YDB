/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "relqop.h"

/* shuffle shuffle sniff and snuffle */

void	shuffqtt (que_ent_ptr_t base1, que_ent_ptr_t base2)
{
	que_ent_ptr_t curr, prev;	/* curr is immediately previous to base1, prev is immediately previous to curr */

	/* Remove the entry immediately previous to base1 from the queue */
	curr = (que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl);
	prev = (que_ent_ptr_t)((uchar_ptr_t)curr + curr->bl);
	assert(curr->fl == -base1->bl);
	assert(curr->bl == -prev->fl);
	base1->bl += curr->bl;
	prev->fl += curr->fl;
	/* Add the entry removed in the previous step to the tail of the new queue (immediately before base2) */
	prev = (que_ent_ptr_t)((uchar_ptr_t)base2 + base2->bl);
	assert(prev->fl == -base2->bl);
	curr->fl = (int4)((uchar_ptr_t)base2 - (uchar_ptr_t)curr);
	curr->bl = (int4)((uchar_ptr_t)prev - (uchar_ptr_t)curr);
	base2->bl = -curr->fl;
	prev->fl = -curr->bl;
	return;
}

void	shuffqth (que_ent_ptr_t base1, que_ent_ptr_t base2)
{
	que_ent_ptr_t curr, prev, next;	/* curr is immediately previous to base1, prev is immediately previous to curr,
					 * next is immediatley next to curr. */
	/* Remove the entry immediately previous to base1 from the queue */
	curr = (que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl);
	prev = (que_ent_ptr_t)((uchar_ptr_t)curr + curr->bl);
	assert(curr->fl == -base1->bl);
	assert(curr->bl == -prev->fl);
	base1->bl += curr->bl;
	prev->fl += curr->fl;
	/* Add the entry removed in the previous step to the head of the new queue (immediately ahead of base2) */
	next = (que_ent_ptr_t)((uchar_ptr_t)base2 + base2->fl);
	assert(next->bl == -base2->fl);
	curr->bl = (int4)((uchar_ptr_t)base2 - (uchar_ptr_t)curr);
	curr->fl = (int4)((uchar_ptr_t)next - (uchar_ptr_t)curr);
	base2->fl = -curr->bl;
	next->bl = -curr->fl;
	return;
}
