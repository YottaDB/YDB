/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef T_WRITE_MAP
#define T_WRITE_MAP

void t_write_map(
		srch_blk_status	*blkhist,	/* Search History of the block to be written. Currently the
						 *	following members in this structure are used by "t_write_map"
						 *	    "blk_num"		--> Block number being modified
						 *	    "buffaddr"		--> Address of before image of the block
						 *	    "cr"		--> cache-record that holds the block (BG only)
						 *	    "cycle"		--> cycle when block was read by t_qread (BG only)
						 *	    "cr->ondsk_blkver"	--> Actual block version on disk
						 */
		unsigned char 	*upd_addr,	/* Address of the update array containing list of blocks to be cleared in bitmap */
		trans_num	tn,		/* Transaction Number when this block was read. Used for cdb_sc_blkmod validation */
		int4		reference_cnt);	/* Same meaning as cse->reference_cnt (see gdscc.h for comments) */

#endif
