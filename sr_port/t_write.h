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

#ifndef T_WRITE_DEFINED

/* Declare parms for t_write.c */

cw_set_element *t_write (
			srch_blk_status	*blkhist,	/* Search History of the block to be written. Currently the
							 *	following members in this structure are used by "t_write"
							 *	    "blk_num"		--> Block number being modified
							 *	    "buffaddr"		--> Address of before image of the block
							 *	    "cr->ondsk_blkver"	--> Actual block version on disk
							 */
			unsigned char 	*upd_addr,	/* Address of the update array that contains the changes for this block */
			block_offset 	ins_off,	/* Offset to the position in the buffer that is to receive
							 * 	a block number when one is created. */
			block_index 	index,		/* Index into the create/write set.  The specified entry is
							 * 	always a create entry. When the create gets assigned a
							 * 	block number, the block number is inserted into this
							 * 	buffer at the location specified by ins_off. */
			char		level,		/* Level of the block in the tree */
			boolean_t	first_copy,	/* Is first copy needed if overlaying same buffer? */
			boolean_t	forward,	/* Is forward processing required? */
			uint4		write_type);	/* Whether "killtn" of the bt needs to be simultaneously updated or not */

#define T_WRITE_DEFINED

#endif
