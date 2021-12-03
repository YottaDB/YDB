/****************************************************************
 *								*
 * Copyright (c) 2005-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsdbver.h"
#include "gds_blk_upgrade.h"
#include "iosp.h"
#include "copy.h"
#include "cdb_sc.h"
#include "gdskill.h"
#include "muextr.h"
#include "mupip_reorg.h"

#define SPACE_NEEDED (SIZEOF(blk_hdr) - SIZEOF(v15_blk_hdr))

GBLREF	boolean_t		gtm_blkupgrade_override;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			gtm_blkupgrade_flag;	/* control whether dynamic upgrade is attempted or not */
GBLREF	unsigned char		rdfail_detail;

error_def(ERR_DYNUPGRDFAIL);
error_def(ERR_GTMCURUNSUPP);

/* REORG encrypt does not pass the ondsk_blkver down through dsk_read, so update it only when provided by the caller */
#define SET_ON_DISK_BLKVER_IF_NEEDED(PTR, VER) if (NULL != (PTR)) *(PTR) = VER;

int4 gds_blk_upgrade(sm_uc_ptr_t gds_blk_src, sm_uc_ptr_t gds_blk_trg, int4 blksize, enum db_ver *ondsk_blkver)
{
	boolean_t		apply_offset;
	blk_hdr_ptr_t		bp, v6bp;
	block_id		blkid;
	int			blks_created, lvls_increased;	/* TODO: share these back to mupip_upgrade for reporting */
	int4			sz_needed, *rec_offsets, num_recs, index;
	rec_hdr_ptr_t		rp, rtop;
	v15_blk_hdr_ptr_t	v15bp;
	v15_trans_num		v15tn;
	uint4			v15bsiz, v15levl;

	assert(gds_blk_src);
	assert(gds_blk_trg);
	assert(0 == ((long)gds_blk_src & 0x7));				/* code assumes 8 byte alignment */
	assert(0 == ((long)gds_blk_trg & 0x7));
	v6bp = (blk_hdr_ptr_t)gds_blk_src;
	bp = (blk_hdr_ptr_t)gds_blk_trg;
	if (GDSV7 == v6bp->bver)
	{
		SET_ON_DISK_BLKVER_IF_NEEDED(ondsk_blkver, GDSV7);
		return SS_NORMAL;
	}
	/* Should be a V[56] block here */
	assert(GDSV6 == v6bp->bver);
	if ((0 == v6bp->levl) || (LCL_MAP_LEVL == v6bp->levl))
	{	/* TODO: better done in dsk_read? directory tree lvl 0 blocks pre-upgraded so must be data block or local bitmap */
		memmove(gds_blk_trg, gds_blk_src, v6bp->bsiz);
		bp->bver = GDSV7;
		SET_ON_DISK_BLKVER_IF_NEEDED(ondsk_blkver, GDSV7);
		return SS_NORMAL;
	}
	/* Unless modified, the block is not getting upgraded */
	SET_ON_DISK_BLKVER_IF_NEEDED(ondsk_blkver, GDSV6);
	/* TODO: fix call below; all work done by mu_split or a new version of it ? */
	if (cdb_sc_normal != (rdfail_detail = mu_split(v6bp->levl, 0, 0, &blks_created, &lvls_increased)))
	rp = (rec_hdr_ptr_t)(v6bp + SIZEOF(blk_hdr));
	rtop = (rec_hdr_ptr_t)(v6bp + v6bp->bsiz);
	rec_offsets = malloc((blksize/bstar_rec_size(BLKID_32)) * SIZEOF(int4));  /* TODO: change to high water marking */
	apply_offset = (GDSV6p == bp->bver); /* does block need offset master map move? */
	num_recs = 0;
	while (rp < rtop)	/* TODO: should this loop apply offsets? */
	{	/* walk the records to find how many, and hence how much space we needed */
		rec_offsets[num_recs++] = rp - (rec_hdr_ptr_t)v6bp;
		rp += rp->rsiz;
	}
	sz_needed = num_recs * 4; /* Need an extra 4-bytes per record to expand block pointers */
	if (v6bp != bp)
	{	/* The src and trg are different buffers, so copy the block to the target buffer */
		memcpy(gds_blk_trg, gds_blk_src, blksize);
	}
	assert((sz_needed + v6bp->bsiz) <= blksize); /* mu_split should have ensured this */
#ifdef notnow
	if ((sz_needed + v6bp->bsiz) > blksize)
	{	/* Would exceed maximum block size. Can't dynamically upgrade. Apply offset if needed and return. */
		if (apply_offset)
		{
			rp = (rec_hdr_ptr_t)(bp + SIZEOF(blk_hdr));
			rtop = (rec_hdr_ptr_t)(bp + bp->bsiz);
			while (rp < rtop)
			{
				READ_BLK_ID(BLKID_32, &blkid, (sm_uc_ptr_t)(rp + rp->rsiz - SIZEOF_BLK_ID(BLKID_32)));
				blkid -= cs_data->offset;
				WRITE_BLK_ID(BLKID_32, blkid, (sm_uc_ptr_t)(rp + rp->rsiz - SIZEOF_BLK_ID(BLKID_32)));
				rp += rp->rsiz;
			}
			bp->bver = GDSV6m; /* Set the OFFSET_FLAG since the block is staying V6 format */
		}
		return SS_NORMAL;
	}
#endif
	for (index = (num_recs - 1); index >= 0; index--)
	{	/* Extend pointers to 64-bit and apply offset if needed */
		sz_needed -= 4;
		rp = (rec_hdr_ptr_t)(bp + rec_offsets[index]);
		READ_BLK_ID(BLKID_32, &blkid, (sm_uc_ptr_t)(rp + rp->rsiz - SIZEOF_BLK_ID(BLKID_32)));
		blkid -= (apply_offset ? cs_data->offset : 0);
		rp->rsiz += 4;
		WRITE_BLK_ID(BLKID_64, blkid, (sm_uc_ptr_t)(rp + rp->rsiz - SIZEOF_BLK_ID(BLKID_64)));
		memmove((rp + sz_needed), rp, rp->rsiz);
	}
	bp->bver = GDSV7m;
	bp->bsiz += (num_recs * 4);
	free(rec_offsets);
	rec_offsets = NULL;
	return SS_NORMAL;
}
