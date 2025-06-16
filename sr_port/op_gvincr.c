/****************************************************************
 *								*
 * Copyright (c) 2004-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "error.h"
#include "op.h"			/* for op_gvincr prototype */
#include "change_reg.h"
#include "format_targ_key.h"
#include "gvcmx.h"
#include "gvcst_protos.h"	/* for gvcst_incr prototype */
#include "sgnl.h"		/* for sgnl_gvundef prototype */

GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;

#define	NONULLSUBS	"$INCREMENT() failed because"

error_def(ERR_DBPRIVERR);
error_def(ERR_GVIS);
error_def(ERR_PCTYRESERVED);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

void	op_gvincr(mval *increment, mval *result)
{
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If specified var name is global ^%Y*, the name is illegal to use in a SET or KILL command, only GETs are allowed */
	if ((RESERVED_NAMESPACE_LEN <= gv_currkey->end) && (0 == MEMCMP_LIT(gv_currkey->base, RESERVED_NAMESPACE)))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_PCTYRESERVED);
	if (gv_cur_region->read_only)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if ((TREF(gv_last_subsc_null) || TREF(gv_some_subsc_null)) && (ALWAYS != gv_cur_region->null_subs))
		sgnl_gvnulsubsc(NONULLSUBS);
	assert(gv_currkey->end + 1 <= gv_cur_region->max_key_size);
	MV_FORCE_NUM(increment);
	if (IS_REG_BG_OR_MM(gv_cur_region))
		gvcst_incr(increment, result);
	else
	{
		assert(REG_ACC_METH(gv_cur_region) == dba_cm);
		gvcmx_increment(increment, result);
	}
	assert(MV_DEFINED(result));
}
