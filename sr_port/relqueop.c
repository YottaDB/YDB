/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	relqueop - C-callable relative queue routines
 *
 *	These routines perform operations on doubly-linked relative
 *	queues.  They are designed to emulate the VAX machine
 *	instructions (and corresponding VAX C library routines) after
 *	which they are names.
 *
 *	insqh - insert entry into queue at head
 *	insqt - insert entry into queue at tail
 *	remqh - remove entry from queue at head
 *	remqt - remove entry from queue at tail
 */

#include "mdef.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "relqop.h"



void	insqh (que_ent_ptr_t new, que_ent_ptr_t base)
{
	new->bl = (uchar_ptr_t)base - (uchar_ptr_t)new;
	new->fl = new->bl + base->fl;
	base->fl = -new->bl;
	((que_ent_ptr_t)((uchar_ptr_t)new + new->fl))->bl = -new->fl;
	return;
}


void	insqt (que_ent_ptr_t new, que_ent_ptr_t base)
{
	new->fl = (uchar_ptr_t)base - (uchar_ptr_t)new;
	new->bl = new->fl + base->bl;
	base->bl = -new->fl;
	((que_ent_ptr_t)((uchar_ptr_t)new + new->bl))->fl = -new->bl;
	return;
}


void_ptr_t remqh (que_ent_ptr_t base)
{
	que_ent_ptr_t	ret;

	ret = (que_ent_ptr_t)(INTPTR_T)base->fl;			/* Will be 0 or offset to element */
	if ((que_ent_ptr_t)0 != ret)
	{
		ret = (que_ent_ptr_t)((uchar_ptr_t)base + (INTPTR_T)ret);
		base->fl += ret->fl;
		((que_ent_ptr_t)((uchar_ptr_t)base + base->fl))->bl += ret->bl;
	}

	return (void_ptr_t)ret;
}


void_ptr_t remqt (que_ent_ptr_t base)
{
	que_ent_ptr_t	ret;

	ret = (que_ent_ptr_t)(INTPTR_T)base->bl;			/* Will be 0 or offset to element */
	if ((que_ent_ptr_t)0 != ret)
	{
		ret = (que_ent_ptr_t)((uchar_ptr_t)base + (INTPTR_T)ret);
		base->bl += ret->bl;
		((que_ent_ptr_t)((uchar_ptr_t)base + base->bl))->fl += ret->fl;
	}

	return (void_ptr_t)ret;
}
