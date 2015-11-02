/****************************************************************
 *								*
 *	Copyright 2004, 2010 Fidelity Information Services, Inc	*
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
#include "gvusr.h"
#include "gvcst_protos.h"	/* for gvcst_incr prototype */
#include "sgnl.h"		/* for sgnl_gvundef prototype */

GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;

error_def(ERR_DBPRIVERR);
error_def(ERR_GVIS);
error_def(ERR_UNIMPLOP);
error_def(ERR_TEXT);

void	op_gvincr(mval *increment, mval *result)
{
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((!TREF(gv_last_subsc_null) && !TREF(gv_some_subsc_null)) || (ALWAYS == gv_cur_region->null_subs))
	{
		if (!gv_cur_region->read_only)
		{
			assert(gv_currkey->end + 1 <= gv_cur_region->max_key_size);
			MV_FORCE_NUM(increment);
			switch (gv_cur_region->dyn.addr->acc_meth)
			{
				case dba_bg:
				case dba_mm:
					gvcst_incr(increment, result);
					break;
				case dba_cm:
					gvcmx_increment(increment, result);
					break;
				case dba_usr:
					/* $INCR not supported for DDP/USR access method */
					if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
						end = &buff[MAX_ZWR_KEY_SZ - 1];
					rts_error(VARLSTCNT(10) ERR_UNIMPLOP, 0,
						ERR_TEXT, 2, LEN_AND_LIT("GTCM DDP server does not support $INCREMENT"),
						ERR_GVIS, 2, end - buff, buff,
						ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
					break;
				default:
					GTMASSERT;
			}
			assert(MV_DEFINED(result));
		} else
			rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	} else
		sgnl_gvnulsubsc();
}
