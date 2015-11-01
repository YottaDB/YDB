/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "hashtab.h"
#include "longset.h"
#include "cws_insert.h"

GBLDEF	hashtab		*cw_stagnate;

#define CWS_INITIAL_SIZE	32


/*
 *  This routine appends a block_id onto an hashtable of block_id's
 *
 *	cw_stagnate		is the address of the hashtable
 *				it is initially NULL
 *
 *	CWS_INITIAL_SIZE	is the initial size of the hash table
 *				is enlarged each time it fills up
 *
 *	The hashtable is allocated the first time this routine is called, and the contents
 *		of it are reset by calling cws_reset in t_begin, t_begin_crit and tp_hist.
 *		The hashtable expands by itself.
 */

void 	cws_insert(block_id block)
{
	uint4	dummy;

	if (cw_stagnate == NULL)
		init_hashtab(&cw_stagnate, CWS_INITIAL_SIZE);
			/* any value is ok since initial allocation is 1013 */

	add_hashtab_ent(&cw_stagnate, (void *)block, &dummy);
}

void	cws_reset(void)
{
	if (NULL != cw_stagnate)
	{
    		longset((uchar_ptr_t)cw_stagnate->tbl, sizeof(hashtab_ent) * cw_stagnate->size, 0);
		cw_stagnate->first = NULL;
		cw_stagnate->last = NULL;
		cw_stagnate->count = 0;
	}
}
