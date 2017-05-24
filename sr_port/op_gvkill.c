/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#ifdef GTM_TRIGGER
#include "gv_trigger.h"		/* for IS_EXPLICIT_UPDATE_NOASSERT macro used by IS_OK_TO_INVOKE_GVCST_KILL macro */
#endif
#include "filestruct.h"		/* needed by "jnl.h" */
#include "jnl.h"		/* for "jnl_gbls_t" typedef needed by IS_EXPLICIT_UPDATE_NOASSERT macro */

GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gv_key		*gv_currkey;
GBLREF	bool		gv_replication_error;
GBLREF	bool		gv_replopen_error;
#ifdef DEBUG
/* The following GBLREF is needed by the IS_OK_TO_INVOKE_GVCST_KILL macro. Normally we will include this inside
 * the macro definition, but since the macro is an expression and returns a value, we cannot easily do this GBLREF
 * inclusion there and hence placing it in the caller C module.
 */
GBLREF	jnl_gbls_t	jgbl;
#endif

error_def(ERR_DBPRIVERR);
error_def(ERR_PCTYRESERVED);

void kill_var(void);

void op_gvkill(void)
{
	gd_region	*reg;
	gvnh_reg_t	*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If specified var name is global ^%Y*, the name is illegal to use in a SET or KILL command, only GETs are allowed.
	 * The only exception is if we are removing a ^%YGS record from the statsdb (caller "gvcst_remove_statsDB_linkage"
	 * but in that case we would have reg->read_only set to FALSE for a statsdb region name. Account for that.
	 */
	if (((RESERVED_NAMESPACE_LEN <= gv_currkey->end) && (0 == MEMCMP_LIT(gv_currkey->base, RESERVED_NAMESPACE)))
			&& (!IS_STATSDB_REGNAME(gv_cur_region) || gv_cur_region->read_only))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_PCTYRESERVED);
	if (gv_cur_region->read_only)
	{
		assert(cs_addrs == &FILE_INFO(gv_cur_region)->s_addrs);
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
	}
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	if (IS_REG_BG_OR_MM(gv_cur_region))
	{
		gvnh_reg = TREF(gd_targ_gvnh_reg);
		if (NULL == gvnh_reg)
		{
			if (IS_OK_TO_INVOKE_GVCST_KILL(gv_target))
				gvcst_kill(TRUE);
		} else
			INVOKE_GVCST_SPR_XXX(gvnh_reg, gvcst_spr_kill());
	} else if (REG_ACC_METH(gv_cur_region) == dba_cm)
		gvcmx_kill(TRUE);
	else
		gvusr_kill(TRUE);
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
				kill_var();
			} else
				gv_replication_error = TRUE;
		}
		gv_cur_region = reg;
		change_reg();
		if (gv_replication_error)
			sgnl_gvreplerr();
	}
}

void kill_var(void)
{
	ESTABLISH(replication_ch);
	if (gv_cur_region->read_only)
	{
		gv_replication_error = TRUE;
		REVERT;
		return;
	}
	assert(REG_ACC_METH(gv_cur_region) == dba_cm);
	gvcmx_kill(TRUE);
	REVERT;
}
