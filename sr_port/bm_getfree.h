/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BM_GETFREE_INCLUDED
#define BM_GETFREE_INCLUDED

block_id bm_getfree(block_id orig_hint, boolean_t *blk_used, unsigned int cw_work, cw_set_element *cs, int *cw_depth_ptr);
boolean_t	is_free_blks_ctr_ok(void);

#endif /* BM_GETFREE_INCLUDED */
