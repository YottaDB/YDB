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

#ifndef T_WRITE_ROOT_DEFINED

/* Declare parms for t_write_root.c */

void	t_write_root (
			 block_offset 	ins_off,	/*  Offset to the position in the buffer that is to receive
							 *  a block number when one is created. */
			 block_index 	index         	/*  Index into the create/write set.  The specified entry is
							 *  always a create entry. When the create gets assigned a
							 *  block number, the block number is inserted into this
							 *  buffer at the location specified by ins_off. */
			 );

#define T_WRITE_ROOT_DEFINED

#endif
