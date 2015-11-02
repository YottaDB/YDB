/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CERT_BLK_DEFINED

#define CERT_BLK_DEFINED

int cert_blk(gd_region *reg, block_id blk, blk_hdr_ptr_t bp, block_id root, boolean_t gtmassert_on_error);

#define	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, blk_ptr, gv_target)			\
if (certify_all_blocks)											\
{	/* GTMASSERT on integ error */									\
        cert_blk(gv_cur_region, cs->blk, (blk_hdr_ptr_t)blk_ptr,					\
                        (0 < dollar_tlevel) ? ((NULL != cs->blk_target) ? cs->blk_target->root : 0)	\
						: ((NULL != gv_target) ? gv_target->root : 0), TRUE);	\
}

#endif
