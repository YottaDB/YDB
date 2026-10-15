/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INDR_FAST_LVN_H_INCLUDED
#define INDR_FAST_LVN_H_INCLUDED

/* A local variable reference parsed from an indirection string without compiling it (see indr_fast_lvn.c) */
typedef struct
{
	mval	*name;				/* base variable name */
	mval	*subs[MAX_LVSUBSCRIPTS];	/* subscript values */
	int	nsubs;
} indr_fast_lvn;

boolean_t	indr_fast_lvn_parse(mval *src, indr_fast_lvn *lvn);
lv_val		*indr_fast_lvn_node(indr_fast_lvn *lvn, int depth);
boolean_t	indr_fast_lvn_save(indr_fast_lvn *lvn, uint4 slot);
void		indr_fast_lvn_done(indr_fast_lvn *lvn);
void		indr_fast_lvn_record_name(mval *dst);
void		indr_fast_lvn_release(mval *src);

#endif
