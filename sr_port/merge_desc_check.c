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
#include "min_max.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zshow.h"
#include "zwrite.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "merge_def.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "format_targ_key.h"
#include "ddphdr.h"

GBLREF int              merge_args;
GBLREF merge_glvn_ptr	mglvnp;

error_def(ERR_MERGEDESC);

boolean_t merge_desc_check(void)
{
	unsigned char		buff1[MAX_ZWR_KEY_SZ], buff2[MAX_ZWR_KEY_SZ], *end1, *end2;
	enum db_acc_method	acc_meth1, acc_meth2;
	gd_region		*reg1, *reg2;
	gv_namehead		*gvt1, *gvt2;

	if (MARG1_IS_GBL(merge_args) && MARG2_IS_GBL(merge_args))
	{
		reg1 = mglvnp->gblp[IND1]->s_gv_cur_region;
		reg2 = mglvnp->gblp[IND2]->s_gv_cur_region;
		gvt1 = mglvnp->gblp[IND1]->s_gv_target;
		gvt2 = mglvnp->gblp[IND2]->s_gv_target;
		acc_meth1 = reg1->dyn.addr->acc_meth;
		acc_meth2 = reg2->dyn.addr->acc_meth;
		assert(!(dba_bg == acc_meth1 || dba_mm == acc_meth1) || (NULL != gvt1->gd_csa));
		assert(!(dba_bg == acc_meth2 || dba_mm == acc_meth2) || (NULL != gvt2->gd_csa));
		/* if (!(both are bg/mm regions && dbs are same && same global) &&
		 *     !(both are cm regions && on the same remote node && same region)
		 *     !(both are usr regions && in the same volume set))
		 *   NO DESCENDANTS
		 * endif
		 */
		if (!(((dba_bg == acc_meth1 || dba_mm == acc_meth2) && (dba_bg == acc_meth1 || dba_mm == acc_meth2))
			&& (gvt1->gd_csa == gvt2->gd_csa) && (gvt1->root == gvt2->root))
		   && !((dba_cm == acc_meth1) && (dba_cm == acc_meth2)
			&& (reg1->dyn.addr->cm_blk == reg2->dyn.addr->cm_blk) && (reg1->cmx_regnum == reg2->cmx_regnum))
		   VMS_ONLY (&&
		   	!((dba_usr == acc_meth1) && (dba_usr == acc_meth2)
			&& ((ddp_info *)(&FILE_INFO(reg1)->file_id))->volset == ((ddp_info *)(&FILE_INFO(reg2)->file_id))->volset)))
		{
			UNIX_ONLY(assert(dba_usr != acc_meth1 && dba_usr != acc_meth2);)
			return 1;
		}
		if (0 == memcmp(mglvnp->gblp[IND1]->s_gv_currkey->base, mglvnp->gblp[IND2]->s_gv_currkey->base,
			        MIN(mglvnp->gblp[IND1]->s_gv_currkey->end, mglvnp->gblp[IND2]->s_gv_currkey->end)))
		{
			if (mglvnp->gblp[IND1]->s_gv_currkey->end == mglvnp->gblp[IND2]->s_gv_currkey->end)
				return 0; /* NOOP - merge self */
			if (0 == (end1 = format_targ_key(buff1, MAX_ZWR_KEY_SZ, mglvnp->gblp[IND1]->s_gv_currkey, TRUE)))
				end1 = &buff1[MAX_ZWR_KEY_SZ - 1];
			if (0 == (end2 = format_targ_key(buff2, MAX_ZWR_KEY_SZ, mglvnp->gblp[IND2]->s_gv_currkey, TRUE)))
				end2 = &buff2[MAX_ZWR_KEY_SZ - 1];
			if (mglvnp->gblp[IND1]->s_gv_currkey->end > mglvnp->gblp[IND2]->s_gv_currkey->end)
				rts_error(VARLSTCNT(6) ERR_MERGEDESC, 4, end1 - buff1, buff1, end2 - buff2, buff2);
			else
				rts_error(VARLSTCNT(6) ERR_MERGEDESC, 4, end2 - buff2, buff2, end1 - buff1, buff1);
		}
	} else if (MARG1_IS_LCL(merge_args) && MARG2_IS_LCL(merge_args))
	{
		if (mglvnp->lclp[IND1] == mglvnp->lclp[IND2])
			return 0; /* NOOP - merge self */
		if (lcl_arg1_is_desc_of_arg2(mglvnp->lclp[IND1], mglvnp->lclp[IND2]))
		{
			end1 = format_key_lv_val(mglvnp->lclp[IND1], buff1, SIZEOF(buff1));
			end2 = format_key_lv_val(mglvnp->lclp[IND2], buff2, SIZEOF(buff2));
			rts_error(VARLSTCNT(6) ERR_MERGEDESC, 4, end1 - buff1, buff1, end2 - buff2, buff2);
		} else if (lcl_arg1_is_desc_of_arg2(mglvnp->lclp[IND2], mglvnp->lclp[IND1]))
		{
			end1 = format_key_lv_val(mglvnp->lclp[IND1], buff1, SIZEOF(buff1));
			end2 = format_key_lv_val(mglvnp->lclp[IND2], buff2, SIZEOF(buff2));
			rts_error(VARLSTCNT(6) ERR_MERGEDESC, 4, end2 - buff2, buff2, end1 - buff1, buff1);
		}
	}
	return 1;
}
