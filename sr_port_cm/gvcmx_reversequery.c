/****************************************************************
 *								*
 * Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

/* Code in this module is based on gvcmx_query.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

#include "mdef.h"

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "mvalconv.h"
#include "format_targ_key.h"	/* for format_targ_key prototype */

GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;

error_def(ERR_GVIS);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

/* This function is invoked by "op_gvreversequery" if the access method is "dba_cm".
 * In this case, we are the client and the $query operation is done by a GT.CM GNP server.
 * So invoke the necessary functions for that. Before that though, check that the GT.CM GNP server
 * does support reverse $query (new feature introduced in YottaDB R1.10) and if not issue error.
 *
 * "val" is an output parameter that is currently not set (see comment below).
 *
 * Also note that the general flow below is similar to that of "gvcmx_query.c".
 */
bool gvcmx_reversequery(mval *val)
{
	mval		temp;
	struct CLB	*lnk;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;

	if (!((link_info *)gv_cur_region->dyn.addr->cm_blk->usr)->server_supports_reverse_query)
	{
		assert(dba_cm == gv_cur_region->dyn.addr->acc_meth); /* we should've covered all other access methods elsewhere */
		end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
		rts_error(VARLSTCNT(14) ERR_UNIMPLOP, 0,
					ERR_TEXT, 2, LEN_AND_LIT("GT.CM server does not support $QUERY(gvn,-1) operation"),
					ERR_GVIS, 2, end - buff, buff,
					ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
	}
	/* Note: We do not initialize "val" as caller "op_gvreversequery" does not use this currently.
	 * The caller instead uses "gv_altkey" to derive the return value of the $query.
	 */
	gvcmz_doop(CMMS_Q_REVERSEQUERY, CMMS_R_REVERSEQUERY, &temp);
	return MV_FORCE_INTD(&temp);
}
