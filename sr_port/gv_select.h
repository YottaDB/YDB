/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GV_SELECT_INCLUDED
#define GV_SELECT_INCLUDED

void gv_select(char *cli_buff, int n_len, boolean_t freeze, char opname[], glist *gl_head,
	int *reg_max_rec, int *reg_max_key, int *reg_max_blk, boolean_t restrict_reg);

#endif /* GV_SELECT_INCLUDED */
