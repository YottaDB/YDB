/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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

/* Include prototypes */
#include "t_qread.h"
#include "mupip_reorg.h"

GBLREF gd_region        *gv_cur_region;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF gv_namehead      *gv_target;
GBLREF unsigned int     t_tries;
GBLREF unsigned char    rdfail_detail;

/*******************************************************************************************
Input Parameter:
	blk_base = Block's base which has the key
	rec_top = record top of the record which will be expanded
Output Parameter:
	expanded_key = expanded key
	rec_size = last record size whic has the key
	keylen = key size
	keycmpc = key compression cound
	hist_ptr = history of blocks read, while expanding a *-key
		History excludes the working block from which key is expanded and
		includes the blocks read below the current block to expand a *-key
	NOTE: hist_ptr.depth will be unchanged
Return:
	cdb_sc_normal on success
	failure code on concurrency failure
 *******************************************************************************************/
enum cdb_sc gvcst_expand_any_key (srch_blk_status *blk_stat, sm_uc_ptr_t rec_top, sm_uc_ptr_t expanded_key,
	int *rec_size, int *keylen, int *keycmpc, srch_hist *hist_ptr)
{
	enum cdb_sc	 	status;
	unsigned char		expanded_star_key[MAX_KEY_SZ];
	unsigned short		temp_ushort;
	int			cur_level;
	int			star_keycmpc;
	int			star_keylen;
	int			star_rec_size;
	int			tblk_size;
	block_id		tblk_num;
	sm_uc_ptr_t 		rPtr1, rPtr2, curptr;
	sm_uc_ptr_t		blk_base;

	blk_base = blk_stat->buffaddr;
	cur_level = blk_stat->level;
	curptr = blk_base + SIZEOF(blk_hdr);
	*rec_size = *keycmpc = *keylen = 0;
	while (curptr < rec_top)
	{
		GET_RSIZ(*rec_size, curptr);
		if (0 == cur_level || BSTAR_REC_SIZE != *rec_size)
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
			assert(NULL == hist_ptr);
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_rmisalign;
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
