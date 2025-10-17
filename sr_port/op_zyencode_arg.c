/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------
 * op_zyencode_arg.c
 * ==============
 * Description:
 *  	Save arguments of zyencode command.
 * 	Called twice for ZYENCODE glvn1=glvn2
 *	Once for glvn1 and once for glvn2
 *
 * Arguments:
 *      e_opr_type      - ARG1_LCL or, ARG2_LCL or, ARG1_GBL or, ARG2_GBL
 *      lvp             - For local variable's *lv_val. Otherwise it is null.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	zyencode_args saves e_opr_type of each call.
 *	eglvnp->{gblp,lclp}[] saves glvn1 or glvn2.
 *
 * Notes:
 *	For global variable op_gvname saved necessary info in GBLDEFs
 *	and we cannot pass them as arguments.
 * -----------------------------------------------
 */
#include "mdef.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zyencode_zydecode_def.h"	/* for ARG1_LCL, ARG1_GBL, ARG2_LCL, and ARG2_GBL */
#include "op_zyencode_zydecode.h"	/* for zyencode_glvn_ptr */
#include "format_targ_key.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gvt_inline.h"

GBLREF int			zyencode_args;
GBLREF int			zydecode_args;
GBLREF zyencode_glvn_ptr	eglvnp;
GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;

void op_zyencode_arg(int e_opr_type, lv_val *lvp)
{
	int 		maxkeysz;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!eglvnp)
	{
		eglvnp = (zyencode_glvn_ptr) malloc(SIZEOF(struct zyencode_glvn_struct_type));
		memset(eglvnp, 0, SIZEOF(struct zyencode_glvn_struct_type));
	}
	if ((ARG1_GBL == e_opr_type || ARG2_GBL == e_opr_type) && !eglvnp->gblp[IND1])
	{
		assert(!eglvnp->gblp[IND1] && !eglvnp->gblp[IND2]);
		maxkeysz = DBKEYSIZE(MAX_KEY_SZ);
		eglvnp->gblp[IND1] = (gvname_info *)malloc(SIZEOF(struct gvname_info_struct));
		eglvnp->gblp[IND1]->s_gv_currkey = NULL;	/* needed for GVKEY_INIT macro */
		GVKEY_INIT(eglvnp->gblp[IND1]->s_gv_currkey, maxkeysz);
		eglvnp->gblp[IND2] = (gvname_info *)malloc(SIZEOF(struct gvname_info_struct));
		eglvnp->gblp[IND2]->s_gv_currkey = NULL;	/* needed for GVKEY_INIT macro */
		GVKEY_INIT(eglvnp->gblp[IND2]->s_gv_currkey, maxkeysz);
	}
	if (zydecode_args || (zyencode_args & e_opr_type))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ZYENCODEINCOMPL, 0, ERR_BADZYENZYDENEST);
	zyencode_args |= e_opr_type;
	switch(e_opr_type)
	{
	case ARG1_LCL:
		eglvnp->lclp[IND1] = lvp;
		break;
	case ARG1_GBL:
		gvname_env_save(eglvnp->gblp[IND1]);
		break;
	case ARG2_LCL:
		eglvnp->lclp[IND2] = lvp;
		break;
	default:
		assert(ARG2_GBL == e_opr_type);
		assert(dba_usr != REG_ACC_METH(gv_cur_region));
		if (IS_REG_BG_OR_MM(gv_cur_region)
			|| ((dba_cm == gv_cur_region->dyn.addr->acc_meth)
				&& ((link_info *)gv_cur_region->dyn.addr->cm_blk->usr)->query_is_queryget))
			gvname_env_save(eglvnp->gblp[IND2]);
		else
		{ /* ZYENCODE ^LHS=^RHS where RHS resides on a remote node served by a GTCM server that does not support QUERYGET
		   * operation won't work */
			assert(dba_cm == gv_cur_region->dyn.addr->acc_meth); /* we should've covered all access methods */
			end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(14) ERR_UNIMPLOP, 0,
				ERR_TEXT, 2, LEN_AND_LIT("GT.CM server does not support ZYENCODE operation"),
				ERR_GVIS, 2, end - buff, buff,
				ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
		}
		break;
	}
	if ((zyencode_args != (ARG1_LCL | ARG2_LCL))
		&& (zyencode_args != (ARG1_LCL | ARG2_GBL))
		&& (zyencode_args != (ARG1_GBL | ARG2_LCL))
		&& (zyencode_args != (ARG1_GBL | ARG2_GBL))
		&& (zyencode_args != ARG2_GBL)
		&& (zyencode_args != ARG2_LCL))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ZYENCODEINCOMPL, 0, ERR_BADZYENZYDENEST);
}
