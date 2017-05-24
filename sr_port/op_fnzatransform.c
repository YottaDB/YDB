/****************************************************************
 *								*
 * Copyright (c) 2012-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "collseq.h"
#include "error.h"
#include "op.h"
#include "patcode.h"
#include "mvalconv.h"
#include "lv_val.h"
#include "alias.h"
#include "gtmimagename.h"
#include "format_targ_key.h"
#include "gtm_ctype.h"		/* for ISDIGIT_ASCII macro */
#include "gvn2gds.h"
#include "gvsub2str.h"
#include "io.h"

#define MAX_KEY_SIZE	(MAX_KEY_SZ - 4)	/* internal and external maximums differ */

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF spdesc 		stringpool;
LITREF mval		literal_null;

STATICDEF boolean_t	save_transform;
STATICDEF boolean_t	transform_direction;

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_ZATRANSERR);

CONDITION_HANDLER(op_fnzatransform_ch)
{
	START_CH(TRUE);
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	TREF(transform) = save_transform;
	if (transform_direction)
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE);
	if (ERR_ZATRANSERR != SIGNAL)
	{	/* Override downstream GVSUBOFLOW error. This should not happen because we already
		 * limit the string lenth to less than the maximum key size.
		 */
		assert(FALSE);
		SIGNAL = ERR_ZATRANSERR;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
	}
	NEXTCH;
}

/*
 * -----------------------------------------------
 * op_fnzatransform()
 * Converts between an MVAL and database internal representation (GDS) of the MVAL using a specified collation
 * sequence.
 *
 * Arguments:
 *	msrc	 - Pointer to a string
 * 	col	 - Collation algorithm index
 * 	reverse	 - Specifies whether to convert msrc from GVN to GDS representation (0) or whether to convert
 *		   from GDS to GVN representation (nonzero).
 * 	forceStr - Force convert the mval to a string. Use this to make numbers collate like strings
 * 	dst	 - The destination string containing the converted string.
 * -----------------------------------------------
 */
void op_fnzatransform(mval *msrc, int col, int reverse, int forceStr, mval *dst)
{
	gv_key		save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_key		*gvkey;
	unsigned char	*key;
	unsigned char	buff[MAX_KEY_SZ + 1], msrcbuff[MAX_KEY_SZ + 1];
	collseq 	*csp;
	gv_namehead	temp_gv_target;
	mval		*src, lcl_src;
	mstr		opstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 != col)
	{
		csp = ready_collseq(col);
		if (NULL == csp)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_COLLATIONUNDEF, 1, col);
	} else
		csp = NULL; /* Do not issue COLLATIONUNDEF for 0 collation */

	if (0 == msrc->str.len)
	{	/* Null string, return it back */
		*dst=*msrc;
		return;
	}
	/* Temporarily repoint global variables "gv_target" and "transform".
	 * They are needed by mval2subsc/gvsub2str "transform" and "gv_target->collseq".
	 * Note that transform is usually ON, specifying that collation transformation is "enabled",
	 * and is only shut off for minor periods when something is being critically formatted (like
	 * we're doing here).
	 */
	save_transform = TREF(transform);
	assert(save_transform);
	assert(INVALID_GV_TARGET == reset_gv_target);
	reset_gv_target = gv_target;
	gv_target = &temp_gv_target;
	memset(gv_target, 0, SIZEOF(gv_namehead));
	gv_target->collseq = csp;
	gvkey = &save_currkey[0];
	gvkey->prev = 0;
	gvkey->top = DBKEYSIZE(MAX_KEY_SZ);
	gvkey->end = 0;
	/* Avoid changing the characteristics of the caller's MVAL */
	lcl_src = *msrc;
	src = &lcl_src;
	if (forceStr)
	{
		TREF(transform) = TRUE;
		MV_FORCE_STR(src);
		src->mvtype |= MV_NUM_APPROX; /* Force the mval to be treated as a string */
	} else
		TREF(transform) = FALSE;
	ESTABLISH(op_fnzatransform_ch);
	transform_direction = (0 == reverse);
	if (transform_direction)
	{	/* convert to subscript format; mval2subsc returns NULL in place of GVSUBOFLOW */
		if (MAX_KEY_SIZE < src->str.len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
		/* Setup false key in case a GVSUBOFLOW occurs where format_targ_key processes the key for printing */
		key = gvkey->base;
		*key++ = KEY_DELIMITER;
		gvkey->end++;
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = TRUE;)
		if (NULL == (key = mval2subsc(src, gvkey, TRUE)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
		COPY_ARG_TO_STRINGPOOL(dst, &gvkey->base[gvkey->end], &gvkey->base[1]);
		DEBUG_ONLY(TREF(skip_mv_num_approx_assert) = FALSE);

	} else
	{	/* convert back from subscript format; cannot exceed MAX_KEY_SZ for gvsub2str to work */
		if (MAX_KEY_SZ < src->str.len)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZATRANSERR);
		memset(msrcbuff, 0, MAX_KEY_SZ);		/* Ensure null termination */
		memcpy(msrcbuff, src->str.addr, src->str.len);
		opstr.addr = (char *)buff;
		opstr.len = MAX_KEY_SZ;
		if (opstr.addr == (char *)(key = gvsub2str((unsigned char *)msrcbuff, &opstr, FALSE)))
			dst = (mval *)&literal_null;
		else
			COPY_ARG_TO_STRINGPOOL(dst, key, &buff[0]);
	}
	/* Restore transform and gv_target */
	REVERT;
	RESET_GV_TARGET(DO_GVT_GVKEY_CHECK);
	TREF(transform) = save_transform;
}


