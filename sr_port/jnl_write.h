/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __JNL_WRITE_H__
#define __JNL_WRITE_H__

/* We do not put this in jnl.h, because it needs all including jnl.h must include gdsblk.h */
void	jnl_write(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, blk_hdr_ptr_t blk_ptr,
	jnl_format_buffer *jfb);

#endif
