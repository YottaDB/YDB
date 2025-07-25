/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* -------------------------------------------------------------------------
gvcst_expand_any_key.c
	Expands key in a block. It can expands a *-key too.
	Given block base and record top of the key to be expanded, this will expand the key.
	Result is placed in expanded_key. Can expand *=key too.
------------------------------------------------------------------------- */
#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "mu_reorg.h"
#include "filestruct.h"		/* for struct RAB type recognition by C compiler before prototype usage in muextr.h */
#include "muextr.h"
#include "tp.h"

/* Include prototypes */
#include "t_qread.h"
#include "mupip_reorg.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gv_namehead	*gv_target;
GBLREF unsigned int	t_tries;
GBLREF unsigned char	rdfail_detail;

/*******************************************************************************************
Input Parameter:
	blk_base = Block's base which has the key
	rec_top = record top of the record which will be expanded
Output Parameter:
	expanded_key = expanded key
	rec_size = last record size which has the key
	keylen = key size
	keycmpc = key compression count
	hist_ptr = history of blocks read, while expanding a *-key
		History excludes the working block from which key is expanded and
		includes the blocks read below the current block to expand a *-key
	NOTE: hist_ptr.depth will be unchanged
Return:
	cdb_sc_normal on success
	failure code on concurrency failure
 *******************************************************************************************/
enum cdb_sc gvcst_expand_any_key(srch_blk_status *blk_stat, sm_uc_ptr_t rec_top, sm_uc_ptr_t expanded_key,
	int *rec_size, int *keylen, int *keycmpc, srch_hist *hist_ptr)
{
	block_id		tblk_num;
	boolean_t		long_blk_id;
	enum cdb_sc		status;
	int			cur_level;
	int			star_keycmpc, star_keylen, star_rec_size;
	int			tblk_size;
	int4			blk_id_sz;
	sm_uc_ptr_t		blk_base, curptr, rPtr1, rPtr2;
	unsigned char		expanded_star_key[MAX_KEY_SZ];
	unsigned short		temp_ushort;

	blk_base = blk_stat->buffaddr;
	long_blk_id = IS_64_BLK_ID(blk_base);
	cur_level = blk_stat->level;
	curptr = blk_base + SIZEOF(blk_hdr);
	*rec_size = *keycmpc = *keylen = 0;
	while (curptr < rec_top)
	{
		GET_RSIZ(*rec_size, curptr);
		if ((0 == cur_level) || (bstar_rec_size(long_blk_id) != *rec_size))
		{
			READ_RECORD(status, rec_size, keycmpc, keylen, expanded_key, cur_level, blk_stat, curptr);
			if (cdb_sc_normal != status)
			{
				assert(t_tries < CDB_STAGNATE);
				return status;
			}
			else
			{
				curptr += *rec_size;
				if (curptr >= rec_top)
					break;
			}
		} else /* a star record in index block */
		{
			if ((curptr + *rec_size != rec_top) || (NULL == hist_ptr))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_rmisalign;
			}
			blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
			while (0 != cur_level)
			{
				tblk_size = ((blk_hdr_ptr_t)blk_base)->bsiz;
				READ_BLK_ID(long_blk_id, &tblk_num, blk_base + tblk_size - blk_id_sz);
				if (0 == tblk_num  || cs_data->trans_hist.total_blks - 1 < tblk_num)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_badlvl;
				}
				cur_level--;
				hist_ptr->h[cur_level].tn =  cs_addrs->ti->curr_tn;
				if (!(blk_base = t_qread(tblk_num, (sm_int_ptr_t)(&(hist_ptr->h[cur_level].cycle)),
					&(hist_ptr->h[cur_level].cr) )))
				{
					assert(t_tries < CDB_STAGNATE);
					return (enum cdb_sc)rdfail_detail;
				}
				/* Recalculating the 2 below values when a new block is read accounts for a DB with a mix of
				 * V7 and V6 blocks which will be necessary for the V6->V7 upgrade
				 */
				long_blk_id = IS_64_BLK_ID(blk_base);
				blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
				if (((blk_hdr_ptr_t)blk_base)->levl != cur_level)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_badlvl;
				}
				hist_ptr->h[cur_level].buffaddr = blk_base;
				hist_ptr->h[cur_level].blk_num = tblk_num;
				hist_ptr->h[cur_level].prev_rec.match = 0;
				hist_ptr->h[cur_level].prev_rec.offset = 0;
				hist_ptr->h[cur_level].curr_rec.match = 0;
				hist_ptr->h[cur_level].curr_rec.offset = 0;
			}
			tblk_size = ((blk_hdr_ptr_t)blk_base)->bsiz;
			/* expand *-key from right most leaf level block of the
			 * sub-tree, of which, the original block is root
			 */
			if (cdb_sc_normal != (status = (gvcst_expand_any_key(&hist_ptr->h[cur_level], blk_base + tblk_size,
				expanded_star_key, &star_rec_size, &star_keylen, &star_keycmpc, hist_ptr))))
				return status;
			if (*keylen + *keycmpc) /* Previous key exists */
			{
				GET_CMPC(*keycmpc, expanded_key, expanded_star_key);
			}
			memcpy(expanded_key, expanded_star_key, star_keylen + star_keycmpc);
			*keylen = star_keylen + star_keycmpc - *keycmpc;
			*rec_size = *keylen + *keycmpc + bstar_rec_size(long_blk_id);
			return cdb_sc_normal;
		}	/* end else if *-record */
	}	/* end of "while" loop */
	if (curptr == rec_top)
		return cdb_sc_normal;
	else
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_rmisalign;
	}
}
