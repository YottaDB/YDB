/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "error.h"
#include "gvcst_protos.h"	/* for gvcst_kill prototype */
#include "change_reg.h"
#include "gvcmx.h"
#include "gvusr.h"
#include "sgnl.h"
#include "op.h"

GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF bool		gv_replication_error;
GBLREF bool		gv_replopen_error;

error_def(ERR_DBPRIVERR);

void with_var(void);

void op_gvzwithdraw(void)
{
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();

	if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{
		if (gv_target->root)
		{	gvcst_kill(FALSE);
		}
	}
	else if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
	{	gvcmx_kill(FALSE);
	}else
	{	gvusr_kill(FALSE);
	}

	if (gv_cur_region->dyn.addr->repl_list)
	{
		gv_replication_error = gv_replopen_error;
		gv_replopen_error = FALSE;
		reg = gv_cur_region;
		while (gv_cur_region = gv_cur_region->dyn.addr->repl_list)	/* set replicated segments */
		{
			if (gv_cur_region->open)
			{
				change_reg();
				with_var();
			}
			else
				gv_replication_error = TRUE;
		}
		gv_cur_region = reg;
		change_reg();
		if (gv_replication_error)
			sgnl_gvreplerr();
	}
}

void with_var(void)
{

	ESTABLISH(replication_ch);
	if (gv_cur_region->read_only)
	{	gv_replication_error = TRUE;
		REVERT;
		return;
	}
	assert(gv_cur_region->dyn.addr->acc_meth == dba_cm);
	gvcmx_kill(FALSE);
	REVERT;
}
