/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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

#define	CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region, cs, blk_ptr, gv_target)				\
{														\
	assert(gds_t_create != cs->mode); /* should have morphed into gds_t_acquired before getting here */	\
	if (certify_all_blocks)											\
	{	/* GTMASSERT on integ error */									\
		/* If cs->mode is of type kill_t_create, then it would have been assigned an arbitrary block#	\
		 * inside of the transaction which could match a real block # in the database at this point.	\
		 * For example, it could be identical to the root block of the global in which case cert_blk	\
		 * will assume this is the root block when actually this is a block that is no longer part of	\
		 * the tree. So pass an invalid block# that indicates it is a created block.			\
		 */												\
		cert_blk(gv_cur_region,										\
			((kill_t_create != cs->mode) ? cs->blk : GDS_CREATE_BLK_MAX), (blk_hdr_ptr_t)blk_ptr,	\
				dollar_tlevel ? ((NULL != cs->blk_target) ? cs->blk_target->root : 0)	\
							: ((NULL != gv_target) ? gv_target->root : 0), TRUE);	\
	}													\
}

#endif
