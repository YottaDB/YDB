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

#ifndef T_WRITE_DEFINED

/**
 * t_write
 *
 * @param blkhist	Search History of the block to be written.
 *			Currently the following members in this structure are used by "t_write":
 *				"blk_num"		--> Block number being modified
 *				"buffaddr"		--> Address of before image of the block
 *				"cr->ondsk_blkver"	--> Actual block version on disk
 * @param upd_addr	Address of the update array that contains the changes for this block
 * @param ins_off	Offset to the position in the buffer that is to receive a block number when one is created.
 * @param index		Index into the create/write set. The specified entry is always a create entry.
 *			When the create gets assigned a block number, the block number is inserted into this buffer
 *			at the location specified by ins_off.
 * @param level		Level of the block in the tree
 * @param first_copy	Is first copy needed if overlaying same buffer?
 * @param forward	Is forward processing required?
 * @param write_type	Whether "killtn" of the bt needs to be simultaneously updated or not
 * @return		A cw_set element representing the proposed write
 */
cw_set_element *t_write(srch_blk_status *blkhist, struct blk_segment_struct *upd_addr, block_offset ins_off, block_index index,
			unsigned char level, boolean_t first_copy, boolean_t forward, uint4 write_type);

#define T_WRITE_DEFINED

#endif
