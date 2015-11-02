/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "copy.h"
#include "spec_type.h"
#include "stringpool.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_root_search prototype */
#include "get_spec.h"
#include "collseq.h"

GBLREF	gv_key		*gv_currkey, *gv_altkey;
GBLREF	int4		gv_keysize;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		dollar_tlevel;
GBLREF	uint4		dollar_trestart;
GBLREF	unsigned int	t_tries;
GBLREF	gv_namehead	*reset_gv_target;
GBLREF	boolean_t	mupip_jnl_recover;

static	mstr	global_collation_mstr;

void gvcst_root_search(void)
{
	srch_blk_status	*h0;
	uchar_ptr_t	c, c1;
	sm_uc_ptr_t	rp;
	unsigned short	rlen, hdr_len;
	uchar_ptr_t	subrec_ptr;
	enum cdb_sc	status;
	boolean_t	gbl_target_was_set;
	gv_namehead	*save_targ;
	mname_entry	*gvent;
	int		altkeylen;
	block_id	lcl_root;

	assert((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));
	assert(gv_altkey->top == gv_currkey->top);
	assert(gv_altkey->top == gv_keysize);
	assert(gv_currkey->end < gv_currkey->top);
	for (c = gv_altkey->base, c1 = gv_currkey->base;  *c1;)
		*c++ = *c1++;
	*c++ = 0;
	*c = 0;
	gv_altkey->end = c - gv_altkey->base;
	assert(gv_altkey->end < gv_altkey->top);
	assert(gv_target != cs_addrs->dir_tree);
	save_targ = gv_target;
	/* Check if "gv_target->gvname" matches "gv_altkey->base". If not, there is a name mismatch (out-of-design situation).
	 * This check is temporary until we catch the situation that caused D9H02-002641 */
	/* --- Check BEGIN --- */
	gvent = &save_targ->gvname;
	altkeylen = gv_altkey->end - 1;
	if (!altkeylen || (altkeylen != gvent->var_name.len) || memcmp(gv_altkey->base, gvent->var_name.addr, gvent->var_name.len))
		GTMASSERT;
	/* --- Check END   --- */
	if (INVALID_GV_TARGET != reset_gv_target)
		gbl_target_was_set = TRUE;
	else
	{
		gbl_target_was_set = FALSE;
		reset_gv_target = save_targ;
	}
	gv_target = cs_addrs->dir_tree;
	lcl_root = 0;
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVGETFAIL);
	/* We better hold crit in the final retry (TP & non-TP). Only exception is journal recovery */
	assert((t_tries < CDB_STAGNATE) || cs_addrs->now_crit || mupip_jnl_recover);
	for (;;)
	{
		hdr_len = rlen = 0;
		gv_target = cs_addrs->dir_tree;
		if (dollar_trestart)
			gv_target->clue.end = 0;
		assert(0 == save_targ->root);
		if (cdb_sc_normal == (status = gvcst_search(gv_altkey, 0)))
		{
			if (gv_altkey->end + 1 == gv_target->hist.h[0].curr_rec.match)
			{
				h0 = gv_target->hist.h;
				rp = (h0->buffaddr + h0->curr_rec.offset);
				hdr_len = SIZEOF(rec_hdr) + gv_altkey->end + 1 - ((rec_hdr_ptr_t)rp)->cmpc;
				GET_USHORT(rlen, rp);
				if (FALSE == (CHKRECLEN(rp, h0->buffaddr, rlen)) || (rlen < hdr_len + SIZEOF(block_id)))
				{
					gv_target->clue.end = 0;
					RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
					t_retry(cdb_sc_rmisalign);
					continue;
				}
				GET_LONG(lcl_root, (rp + hdr_len));
				if (rlen > hdr_len + SIZEOF(block_id))
				{
					assert(NULL != global_collation_mstr.addr || 0 == global_collation_mstr.len);
					if (global_collation_mstr.len < rlen - (hdr_len + SIZEOF(block_id)))
					{
						if (NULL != global_collation_mstr.addr)
							free(global_collation_mstr.addr);
						global_collation_mstr.len = rlen - (hdr_len + SIZEOF(block_id));
						global_collation_mstr.addr = (char *)malloc(global_collation_mstr.len);
					}
					/* the memcpy needs to be done here instead of out of for loop for
					 * concurrency consideration. We don't use s2pool because the pointer rp is 64 bits
					 */
					memcpy(global_collation_mstr.addr, rp + hdr_len + SIZEOF(block_id),
							rlen - (hdr_len + SIZEOF(block_id)));
				}
				if (dollar_tlevel)
				{
					status = tp_hist(NULL);
					if (cdb_sc_normal != status)
					{
						gv_target->clue.end = 0;
						RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
						t_retry(status);
						continue;
					}
					break;
				}
			}
			if (!dollar_tlevel)
			{
				if ((trans_num)0 != t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED))
					break;
			} else
			{
				status = tp_hist(NULL);
				if (cdb_sc_normal == status)
					break;
				gv_target->clue.end = 0;
				RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
				t_retry(status);
				continue;
			}
		} else
		{
			gv_target->clue.end = 0;
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
			t_retry(status);
			continue;
		}
	}
	save_targ->root = lcl_root;	/* now that we know the transaction validated fine, set root block in gv_target */
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
	if (rlen > hdr_len + SIZEOF(block_id))
	{
		assert(NULL != global_collation_mstr.addr);
		subrec_ptr = get_spec((uchar_ptr_t)global_collation_mstr.addr,
				      (int)(rlen - (hdr_len + SIZEOF(block_id))), COLL_SPEC);
		if (subrec_ptr)
		{
			gv_target->nct = *(subrec_ptr + COLL_NCT_OFFSET);
			gv_target->act = *(subrec_ptr + COLL_ACT_OFFSET);
			gv_target->ver = *(subrec_ptr + COLL_VER_OFFSET);
		} else
		{
			gv_target->nct = 0;
			gv_target->act = 0;
			gv_target->ver = 0;
		}
	} else
	{
		gv_target->nct = 0;
		gv_target->act = cs_addrs->hdr->def_coll;
		gv_target->ver = cs_addrs->hdr->def_coll_ver;
	}
	if (gv_target->act)
		act_in_gvt();
	assert(gv_target->act || NULL == gv_target->collseq);
	return;
}
