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
 * op_zydecode_arg.c
 * ==============
 * Description:
 *  	Save arguments of zydecode command.
 * 	Called twice for ZYDECODE glvn1=glvn2
 *	Once for glvn1 and once for glvn2
 *
 * Arguments:
 *      d_opr_type      - ARG1_LCL or, ARG2_LCL or, ARG1_GBL or, ARG2_GBL
 *      lvp             - For local variable's *lv_val. Otherwise it is null.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	zydecode_args saves d_opr_type of each call.
 *	dglvnp->{gblp,lclp}[] saves glvn1 or glvn2.
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
#include "op_zyencode_zydecode.h"	/* for zydecode_glvn_ptr */
#include "format_targ_key.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gvt_inline.h"

GBLREF int			zydecode_args;
GBLREF int			zyencode_args;
GBLREF zydecode_glvn_ptr	dglvnp;
GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;

void op_zydecode_arg(int d_opr_type, lv_val *lvp)
{
	int 		maxkeysz;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!dglvnp)
	{
		dglvnp = (zydecode_glvn_ptr) malloc(SIZEOF(struct zydecode_glvn_struct_type));
		memset(dglvnp, 0, SIZEOF(struct zydecode_glvn_struct_type));
	}
	if ((ARG1_GBL == d_opr_type || ARG2_GBL == d_opr_type) && !dglvnp->gblp[IND1])
	{
		assert(!dglvnp->gblp[IND1] && !dglvnp->gblp[IND2]);
		maxkeysz = DBKEYSIZE(MAX_KEY_SZ);
		dglvnp->gblp[IND1] = (gvname_info *)malloc(SIZEOF(struct gvname_info_struct));
		dglvnp->gblp[IND1]->s_gv_currkey = NULL;	/* needed for GVKEY_INIT macro */
		GVKEY_INIT(dglvnp->gblp[IND1]->s_gv_currkey, maxkeysz);
		dglvnp->gblp[IND2] = (gvname_info *)malloc(SIZEOF(struct gvname_info_struct));
		dglvnp->gblp[IND2]->s_gv_currkey = NULL;	/* needed for GVKEY_INIT macro */
		GVKEY_INIT(dglvnp->gblp[IND2]->s_gv_currkey, maxkeysz);
	}
	if (zyencode_args || (zydecode_args & d_opr_type))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ZYDECODEINCOMPL, 0, ERR_BADZYENZYDENEST);
	zydecode_args |= d_opr_type;
	switch(d_opr_type)
	{
	case ARG1_LCL:
		dglvnp->lclp[IND1] = lvp;
		break;
	case ARG1_GBL:
		gvname_env_save(dglvnp->gblp[IND1]);
		break;
	case ARG2_LCL:
		dglvnp->lclp[IND2] = lvp;
		break;
	default:
		assert(ARG2_GBL == d_opr_type);
		assert(dba_usr != REG_ACC_METH(gv_cur_region));
		if (IS_REG_BG_OR_MM(gv_cur_region)
			|| ((dba_cm == gv_cur_region->dyn.addr->acc_meth)
				&& ((link_info *)gv_cur_region->dyn.addr->cm_blk->usr)->query_is_queryget))
			gvname_env_save(dglvnp->gblp[IND2]);
		else
		{ /* ZYDECODE ^LHS=^RHS where RHS resides on a remote node served by a GTCM server that does not support QUERYGET
		   * operation won't work */
			assert(dba_cm == gv_cur_region->dyn.addr->acc_meth); /* we should've covered all access methods */
			end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(14) ERR_UNIMPLOP, 0,
				ERR_TEXT, 2, LEN_AND_LIT("GT.CM server does not support ZYDECODE operation"),
				ERR_GVIS, 2, end - buff, buff,
				ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
		}
		break;
	}
	if ((zydecode_args != (ARG1_LCL | ARG2_LCL))
		&& (zydecode_args != (ARG1_LCL | ARG2_GBL))
		&& (zydecode_args != (ARG1_GBL | ARG2_LCL))
		&& (zydecode_args != (ARG1_GBL | ARG2_GBL))
		&& (zydecode_args != ARG2_GBL)
		&& (zydecode_args != ARG2_LCL))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ZYDECODEINCOMPL, 0, ERR_BADZYENZYDENEST);
}
