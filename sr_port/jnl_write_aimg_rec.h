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

#ifndef __JNL_WRITE_AIMG_REC_H__
#define __JNL_WRITE_AIMG_REC_H__

/* We do not put this in jnl.h, because it needs all including jnl.h must include gdsblk.h */
void jnl_write_aimg_rec(sgmnt_addrs *csa, block_id block, blk_hdr_ptr_t buffer);

#endif
