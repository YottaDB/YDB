/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gvusr.h"
#include "sgnl.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF bool		gv_replication_error;
GBLREF bool		gv_replopen_error;

error_def(ERR_DBPRIVERR);
error_def(ERR_GVIS);
error_def(ERR_REC2BIG);

void put_var(mval *var);

void op_gvput(mval *var)
{
	gd_region	*save_reg;
	int		temp;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((!TREF(gv_last_subsc_null) && !TREF(gv_some_subsc_null)) || (ALWAYS == gv_cur_region->null_subs))
	{
		if (!gv_cur_region->read_only)
		{
			assert(gv_currkey->end + 1 <= gv_cur_region->max_key_size);
			MV_FORCE_STR(var);
			if (VMS_ONLY(gv_currkey->end + 1 + SIZEOF(rec_hdr) +) var->str.len <= gv_cur_region->max_rec_size)
			{
				switch (gv_cur_region->dyn.addr->acc_meth)
				{
					case dba_bg:
					case dba_mm:
						gvcst_put(var);
						break;
					case dba_cm:
						gvcmx_put(var);
						break;
					case dba_usr:
						gvusr_put(var);
						break;
					default:
						GTMASSERT;
				}
				if (NULL == gv_cur_region->dyn.addr->repl_list)
					return;
				gv_replication_error = gv_replopen_error;
				gv_replopen_error = FALSE;
				save_reg = gv_cur_region;
				while (gv_cur_region = gv_cur_region->dyn.addr->repl_list) /* set replicated segments */
				{
					if (gv_cur_region->open && !gv_cur_region->read_only
						&& ((temp = gv_currkey->end + 1) <= gv_cur_region->max_key_size)
						&& (temp + var->str.len + SIZEOF(rec_hdr) <= gv_cur_region->max_rec_size))
					{
						change_reg();
						put_var(var);
					} else
						gv_replication_error = TRUE;
				}
				gv_cur_region = save_reg;
				change_reg();
				if (gv_replication_error)
					sgnl_gvreplerr();
				else
					return;
			} else
			{
				if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
					end = &buff[MAX_ZWR_KEY_SZ - 1];
				rts_error(VARLSTCNT(10) ERR_REC2BIG, 4, VMS_ONLY(gv_currkey->end + 1 + SIZEOF(rec_hdr) +)
					  var->str.len, (int4)gv_cur_region->max_rec_size,
					  REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, end - buff, buff);
			}
		} else
			rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	} else
		sgnl_gvnulsubsc();
}

void put_var(mval *var)
{

	ESTABLISH(replication_ch);
	assert(dba_cm == gv_cur_region->dyn.addr->acc_meth);
	gvcmx_put(var);
	REVERT;
}
