/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef BM_GETFREE_INCLUDED
#define BM_GETFREE_INCLUDED

block_id bm_getfree(block_id orig_hint, boolean_t *blk_used, unsigned int cw_work, cw_set_element *cs, int *cw_depth_ptr,
		cw_set_element **last_lmap_cse);
boolean_t	is_free_blks_ctr_ok(void);

#endif /* BM_GETFREE_INCLUDED */
