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

#include "gtm_string.h"
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "mlkdef.h"
#include "zshow.h"
#include "subscript.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "merge_def.h"
#include "gvname_info.h"
#include "lvname_info.h"
#include "op_merge.h"
#include "op.h"
#include "gtmmsg.h"

GBLREF lvzwrite_struct	lvzwrite_block;
GBLREF zshow_out	*zwr_output;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF int		merge_args;
GBLREF merge_glvn_ptr	mglvnp;


void lvzwr_out( mval *val)
{
	char 		*cp, *cq, buff;
	int 		n;
	lv_val		*dst_lv, *res_lv;
	mstr 		one;
	mval 		outdesc, *subscp;

	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_MERGEINCOMPL);

	if (!merge_args)
	{

		one.addr = &buff;
		one.len = 1;
		if (!MV_DEFINED(val))
		    return;
		MV_FORCE_STR(val);
		outdesc.mvtype = MV_STR;
		outdesc.str.addr = cp = (char *)lvzwrite_block.curr_name;
		for (cq = cp + sizeof(mident) ; cp < cq && *cp ; cp++)
			;
		outdesc.str.len = cp - outdesc.str.addr;
		zshow_output(zwr_output,&outdesc.str);
		if (lvzwrite_block.curr_subsc)
		{
			*one.addr = '(';
			zshow_output(zwr_output,&one);
			for (n = 0 ; ; )
			{
				mval_write(zwr_output,((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual,FALSE);
				if (++n < lvzwrite_block.curr_subsc)
				{	*one.addr = ',';
					zshow_output(zwr_output,&one);
				}else
				{
					*one.addr = ')';
					zshow_output(zwr_output,&one);
					break;
				}
			}
		}
		*one.addr = '=';
		zshow_output(zwr_output,&one);
		mval_write(zwr_output,val,TRUE);
	} else
	{
		if (MARG1_IS_GBL(merge_args))
		{
			memcpy(gv_currkey->base, mglvnp->gblp[IND1]->s_gv_currkey->base, mglvnp->gblp[IND1]->s_gv_currkey->end + 1);
			gv_currkey->end = mglvnp->gblp[IND1]->s_gv_currkey->end;
			for (n = 0 ; n < lvzwrite_block.curr_subsc ; n++)
			{
				subscp = ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual;
				MV_FORCE_STR(subscp);
				mval2subsc(subscp, gv_currkey);
			}
			MV_FORCE_STR(val);
			op_gvput(val);
		} else
		{
			assert(MARG1_IS_LCL(merge_args));
			dst_lv = mglvnp->lclp[IND1];
			if (MV_SBS == dst_lv->ptrs.val_ent.parent.sbs->ident && MAX_LVSUBSCRIPTS
				< dst_lv->ptrs.val_ent.parent.sbs->level + lvzwrite_block.curr_subsc)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				rts_error(VARLSTCNT(1) ERR_MERGEINCOMPL);
			}
			/*
			 * Followings are not efficient. Saving mval * for each level we last worked on,
			 * we can skip some of the op_putindx calls.
			 * Following will do op_putindx even the index exsts
			 */
			for (n = 0 ; n < lvzwrite_block.curr_subsc ; n++)
			{
				subscp = ((zwr_sub_lst *)lvzwrite_block.sub)->subsc_list[n].actual;
				if (res_lv = op_srchindx(VARLSTCNT(2) dst_lv, subscp))
					dst_lv = res_lv;
				else
					dst_lv = op_putindx(VARLSTCNT(2) dst_lv, subscp);
			}
			MV_FORCE_STR(val);
			dst_lv->v = *val;
		}
	}
}
