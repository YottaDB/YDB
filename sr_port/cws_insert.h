/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __CWS_INSERT_H__
#define __CWS_INSERT_H__

GBLREF hashtab          *cw_stagnate;

/* macro CWS_INSERT appends a block_id onto an hashtable of block_id's
 *	cw_stagnate		is the address of the hashtable
 *				it is initially NULL
 *	CWS_INITIAL_SIZE	is the initial size of the hash table
 *				table is enlarged each time it fills up
 * The hashtable is allocated the first time this routine is
 * called, and the contents of it are reset by calling cws_reset
 * in t_begin, t_begin_crit and tp_hist. The hashtable expands
 * by itself
 */

#define CWS_INSERT(block)					\
{								\
	uint4	dummy;						\
	add_hashtab_ent(&cw_stagnate, (void *)block, &dummy);	\
}

#define CWS_RESET										\
{												\
	longset((uchar_ptr_t)cw_stagnate->tbl, sizeof(hashtab_ent) * cw_stagnate->size, 0);	\
	cw_stagnate->first = NULL;								\
	cw_stagnate->last = NULL;								\
	cw_stagnate->count = 0;									\
}
#endif
