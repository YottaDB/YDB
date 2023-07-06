/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef T_CREATE_DEFINED
#define T_CREATE_DEFINED

#define	ALLOCATION_CLUE(totblks)	(((totblks) / 64) + 8)	/* roger 19990607 - arbitrary & should be improved */

block_index t_create(block_id hint, struct blk_segment_struct *upd_addr, block_offset ins_off, block_index index, char level);

#endif
