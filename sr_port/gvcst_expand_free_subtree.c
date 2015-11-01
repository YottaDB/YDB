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

#include "mdef.h"

#include "cdb_sc.h"		/* atleast for cdb_sc_* codes */
#include "copy.h"		/* atleast for the GET_USHORT macros */
#include "gdsroot.h"		/* atleast for gds_file_id used by sgmnt_data in gdsfhead.h */
#include "gdskill.h"		/* atleast for the kill_set and blk_ident structures */
#include "gdsblk.h"		/* atleast for the blk_hdr and rec_hdr structures */
#include "gtm_facility.h"	/* atleast for gdsfhead.h */
#include "fileinfo.h"		/* atleast for gdsfhead.h */
#include "gdsbt.h"		/* atleast for gdsfhead.h */
#include "gdsfhead.h"		/* atleast for cs_addrs, cs_data etc. */
#include "filestruct.h"		/* atleast for the FILE_INFO macro */
#include "gdscc.h"		/* atleast for cw_set_element in tp.h */
#include "jnl.h"		/* atleast for tp.h */
#include "hashtab.h"		/* atleast for tp.h */
#include "buddy_list.h"		/* atleast for tp.h */
#include "tp.h"			/* atleast for off_chain */
#include "t_qread.h"
#include "gvcst_bmp_mark_free.h"
#include "gvcst_delete_blk.h"
#include "gvcst_kill_sort.h"
#include "gvcst_expand_free_subtree.h"
#include "rc_cpt_ops.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	short			dollar_tlevel;
GBLREF	unsigned char		rdfail_detail;

void	gvcst_expand_free_subtree(kill_set *ks_head)
{
	blk_hdr_ptr_t		bp;
	blk_ident		*ksb;
	block_id		blk;
	boolean_t		flush_cache = FALSE, was_crit;
	cache_rec_ptr_t		cr;
	int			cnt, cycle;
	int4			kill_error, temp_long;
	kill_set		*ks;
    	off_chain		chain;
	rec_hdr_ptr_t		rp, rp1, rtop;
	short			save_dollar_tlevel;
	sm_uc_ptr_t		temp_buff;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	unsigned short		temp_ushort;

	error_def(ERR_GVKILLFAIL);

	csa = cs_addrs;
	csd = cs_data;
	/* If ever the following assert is removed, "flush_cache" shouldn't be set to FALSE unconditionally as it is now */
	assert(!csd->dsid);	/* see related comment in gvcst_kill before the call to this routine */
	temp_buff = (unsigned char *)malloc(cs_data->blk_size);
	for (ks = ks_head; NULL != ks; ks = ks->next_kill_set)
	{
		for (cnt = 0; cnt < ks->used; ++cnt)
		{
			ksb = &ks->blk[cnt];
			if (0 != ksb->level)
			{
				if (!(was_crit = csa->now_crit))
					grab_crit(gv_cur_region);
				if (dollar_tlevel && ksb->flag)
				{
					chain.flag = 1;
					chain.next_off = 0;
					chain.cw_index = ksb->block;
					assert(sizeof(chain) == sizeof(blk));
					blk = *(block_id *)&chain;
				} else
					blk = ksb->block;
				if (!(bp = (blk_hdr_ptr_t)t_qread(blk, (sm_int_ptr_t)&cycle, &cr)))
				{	/* This should have worked because t_qread was done in crit */
					free(temp_buff);
					rts_error(VARLSTCNT(4) ERR_GVKILLFAIL, 2, 1, &rdfail_detail);
				}
				memcpy(temp_buff, bp, bp->bsiz);
				if (!was_crit)
					rel_crit(gv_cur_region);
				for (rp = (rec_hdr_ptr_t)(temp_buff + sizeof(blk_hdr)),
					rtop = (rec_hdr_ptr_t)(temp_buff + ((blk_hdr_ptr_t)temp_buff)->bsiz);
				     rp < rtop;
				     rp = rp1)
				{
					GET_USHORT(temp_ushort, &rp->rsiz);
					rp1 = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + temp_ushort);
					if ((sm_uc_ptr_t)rp1 < (sm_uc_ptr_t)(rp + 1) + sizeof(block_id))
					{	/* This should have worked because a local copy was made while crit */
						assert(FALSE);
						kill_error = cdb_sc_rmisalign;
						free(temp_buff);
						rts_error(VARLSTCNT(4) ERR_GVKILLFAIL, 2, 1, &kill_error);
					}
					GET_LONG(temp_long, (block_id_ptr_t)((sm_uc_ptr_t)rp1 - sizeof(block_id)));
					if (dollar_tlevel)
					{
						chain = *(off_chain *)&temp_long;
						if ((1 == chain.flag)
								&& ((int)chain.cw_index >= sgm_info_ptr->cw_set_depth))
						{
							assert(&FILE_INFO(sgm_info_ptr->gv_cur_region)->s_addrs
								== cs_addrs);
							GTMASSERT;
						}
					}
					gvcst_delete_blk(temp_long, ksb->level - 1, TRUE);
					if ((1 == ksb->level)  &&  (0 == dollar_tlevel)  &&
							(0 != cs_data->dsid)  &&  (FALSE == flush_cache))
						rc_cpt_entry(temp_long);	/* Invalidate single block */
				}
				ksb->level = 0;
			} else
			{
				if ((0 == dollar_tlevel)  &&  (0 != cs_data->dsid)  &&  (FALSE == flush_cache))
					rc_cpt_entry(ksb->block);
			}
		}
		gvcst_kill_sort(ks);
		save_dollar_tlevel = dollar_tlevel;
		assert(1 >= dollar_tlevel);
		dollar_tlevel = 0;	/* temporarily for gvcst_bmp_mark_free */
		gvcst_bmp_mark_free(ks);
		dollar_tlevel = save_dollar_tlevel;
	}
	free(temp_buff);
}

