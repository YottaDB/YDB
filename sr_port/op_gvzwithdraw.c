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
#include "gv_trigger.h"		/* for IS_EXPLICIT_UPDATE macro */
#endif
#include "filestruct.h"		/* needed by "jnl.h" */
#include "jnl.h"		/* for "jnl_gbls_t" typedef */

GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
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

void with_var(void);

void op_gvzwithdraw(void)
{
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If specified var name is global ^%Y*, the name is illegal to use in a SET or KILL command, only GETs are allowed */
	if ((RESERVED_NAMESPACE_LEN <= gv_currkey->end) && (0 == MEMCMP_LIT(gv_currkey->base, RESERVED_NAMESPACE)))
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
		/* No special code needed for spanning globals here since we are in the region we want to be
		 * and all we want to do is kill one node in this region (not a subtree underneath) even if
		 * the global spans multiple regions. Hence there is no need to invoke gvcst_spr_kill here.
		 */
		if (IS_OK_TO_INVOKE_GVCST_KILL(gv_target))
			gvcst_kill(FALSE);
	} else if (REG_ACC_METH(gv_cur_region) == dba_cm)
		gvcmx_kill(FALSE);
	else
		gvusr_kill(FALSE);
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
			} else
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
	{
		gv_replication_error = TRUE;
		REVERT;
		return;
	}
	assert(REG_ACC_METH(gv_cur_region) == dba_cm);
	gvcmx_kill(FALSE);
	REVERT;
}
