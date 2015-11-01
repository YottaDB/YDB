/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------
 * op_merge_arg.c
 * ==============
 * Description:
 *  	Save arguments of merge command.
 * 	Called twice for MERGE glvn1=glvn2
 *	Once for glvn1 and once for glvn2
 *
 * Arguments:
 *      m_opr_type      - MARG1_LCL or, MARG2_LCL or, MARG1_GBL or, MARG2_GBL
 *      lvp             - For local variable's *lv_val. Otherwise it is null.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	merge_args saves m_opr_type of each call.
 *	mglvnp->{gblp,lclp}[] saves glvn1 or glvn2.
 *
 * Notes:
 *	For global variable op_gvname saved necessary info in GBLDEFs
 *	and we cannot pass them as arguments.
 * -----------------------------------------------
 */
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
#include "merge_def.h"
#include "subscript.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab.h"
#include "buddy_list.h"
#include "tp.h"
#include "lvname_info.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "format_targ_key.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF int		merge_args;
GBLREF merge_glvn_ptr	mglvnp;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

void op_merge_arg(int m_opr_type, lv_val *lvp)
{
	int 			maxkeysz;
	unsigned char		buff[MAX_STRLEN], *end;
	char			*err_str;

	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);
	error_def(ERR_GVIS);

	if (!mglvnp)
	{
		mglvnp = (merge_glvn_ptr) malloc(sizeof(struct merge_glvn_struct_type));
		memset(mglvnp, 0, sizeof(struct merge_glvn_struct_type));
	}
	if ((MARG1_GBL == m_opr_type || MARG2_GBL == m_opr_type) && !mglvnp->gblp[IND1])
	{
		assert(!mglvnp->gblp[IND1] && !mglvnp->gblp[IND2]);
		maxkeysz = (MAX_KEY_SZ + MAX_NUM_SUBSC_LEN + 4) & (-4);
		mglvnp->gblp[IND1] = (gvname_info *)malloc(sizeof(struct gvname_info_struct));
		mglvnp->gblp[IND1]->s_gv_currkey =  (gv_key *)malloc(sizeof(gv_key) + maxkeysz - 1);
		mglvnp->gblp[IND1]->s_gv_currkey->top = maxkeysz;
		mglvnp->gblp[IND2] = (gvname_info *)malloc(sizeof(struct gvname_info_struct));
		mglvnp->gblp[IND2]->s_gv_currkey =  (gv_key *)malloc(sizeof(gv_key) + maxkeysz - 1);
		mglvnp->gblp[IND2]->s_gv_currkey->top = maxkeysz;
	}
	merge_args |= m_opr_type;
	switch(m_opr_type)
	{
	case MARG1_LCL:
		mglvnp->lclp[IND1] = lvp;
		break;
	case MARG1_GBL:
		gvname_env_save(mglvnp->gblp[IND1]);
		break;
	case MARG2_LCL:
		mglvnp->lclp[IND2] = lvp;
		break;
	case MARG2_GBL:
		if (dba_bg  == gv_cur_region->dyn.addr->acc_meth || dba_mm == gv_cur_region->dyn.addr->acc_meth ||
		    dba_usr == gv_cur_region->dyn.addr->acc_meth ||
		    (dba_cm == gv_cur_region->dyn.addr->acc_meth &&
		     ((link_info *)gv_cur_region->dyn.addr->cm_blk->usr)->query_is_queryget))
			gvname_env_save(mglvnp->gblp[IND2]);
		else
		{ /* M ^LHS=^RHS where RHS resides on a remote node served by a GTCM server that does not support QUERYGET
		   * operation won't work */
			assert(dba_cm == gv_cur_region->dyn.addr->acc_meth); /* we should've covered all access methods */
			end = format_targ_key(buff, MAX_STRLEN, gv_currkey, TRUE);
			rts_error(VARLSTCNT(14) ERR_UNIMPLOP, 0,
				  	        ERR_TEXT, 2, LEN_AND_LIT("GT.CM server does not support MERGE operation"),
				  		ERR_GVIS, 2, end - buff, buff,
				  		ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
		}
		break;
	default:
		GTMASSERT;
	}
	assert ((merge_args == (MARG1_LCL | MARG2_LCL)) ||
		(merge_args == (MARG1_LCL | MARG2_GBL)) ||
		(merge_args == (MARG1_GBL | MARG2_LCL)) ||
		(merge_args == (MARG1_GBL | MARG2_GBL)) ||
		(merge_args == MARG2_GBL) ||
		(merge_args == MARG2_LCL));
}
