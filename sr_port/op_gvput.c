/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_put prototype */
#include "change_reg.h"
#include "format_targ_key.h"
#include "gvcmx.h"
#include "sgnl.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF bool		gv_replication_error;
GBLREF bool		gv_replopen_error;

#define	NONULLSUBS	"Update failed because"

error_def(ERR_DBPRIVERR);
error_def(ERR_GVIS);
error_def(ERR_PCTYRESERVED);
error_def(ERR_REC2BIG);

void put_var(mval *var);

void op_gvput(mval *var)
{
	gd_region	*save_reg;
	int		temp;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	boolean_t	is_trigger;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If specified var name is global ^%Y*, the name is illegal to use in a SET or KILL command, only GETs are allowed.
	 * The only exception is if we are adding a ^%YGS record into the statsdb from "gvcst_init_statsDB" but in that
	 * case we would have reg->read_only set to FALSE for a statsdb region name. Account for that.
	 */
	if (((RESERVED_NAMESPACE_LEN <= gv_currkey->end) && (0 == MEMCMP_LIT(gv_currkey->base, RESERVED_NAMESPACE)))
			&& (!IS_STATSDB_REGNAME(gv_cur_region) || gv_cur_region->read_only))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_PCTYRESERVED);
	if (gv_cur_region->read_only)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if ((TREF(gv_last_subsc_null) || TREF(gv_some_subsc_null)) && (ALWAYS != gv_cur_region->null_subs))
		sgnl_gvnulsubsc(NONULLSUBS);
	is_trigger = (STRNCMP_LIT((char *) gv_currkey->base, "#t") == 0) ? TRUE : FALSE;
	if(is_trigger)
		assert(gv_currkey->end + 1 <= MAX_KEY_SZ - 4);
	else
		assert(gv_currkey->end + 1 <= gv_cur_region->max_key_size);
	MV_FORCE_STR(var);
	if ((var->str.len <= gv_cur_region->max_rec_size) || is_trigger)
	{
		if (IS_REG_BG_OR_MM(gv_cur_region))
			gvcst_put(var);
		else
		{
			assert(REG_ACC_METH(gv_cur_region) == dba_cm);
			gvcmx_put(var);
		}
		return;
	} else
	{
		if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
			end = &buff[MAX_ZWR_KEY_SZ - 1];
		gv_currkey->end = 0;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(10) ERR_REC2BIG, 4, var->str.len, (int4)gv_cur_region->max_rec_size,
			REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, end - buff, buff);
	}
}

void put_var(mval *var)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(replication_ch);
	assert(dba_cm == gv_cur_region->dyn.addr->acc_meth);
	gvcmx_put(var);
	REVERT;
}
