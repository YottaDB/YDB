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

#ifndef __MU_UPGRD_ADJUST_BLKPTR_H__
#define __MU_UPGRD_ADJUST_BLKPTR_H__

void mu_upgrd_adjust_blkptr(block_id blk, bool dirtree, sgmnt_data *new_head, int fd, char fn[],
	int fn_len);

#endif
