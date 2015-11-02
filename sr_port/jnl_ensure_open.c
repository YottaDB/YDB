/****************************************************************
 *                                                              *
 *      Copyright 2010, 2011 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gdsdbver.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosp.h"
#include "repl_sp.h"
#include "gtmio.h"
#include "gtmimagename.h"
#include "wbox_test_init.h"
#include "gtcm_jnl_switched.h"

GBLREF	boolean_t		is_src_server;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

/* make sure that the journal file is available if appropriate */
uint4   jnl_ensure_open(void)
{
	uint4			jnl_status;
	jnl_private_control	*jpc;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	boolean_t		first_open_of_jnl, need_to_open_jnl;
	int			close_res;
#       if defined(VMS)
	static const gds_file_id	file;
	uint4				status;
#       endif

	error_def(ERR_JNLFILOPN);

	csa = cs_addrs;
	csd = csa->hdr;
	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(NULL != jpc);
	assert(JNL_ENABLED(csa->hdr));
	/* The goal is to change the code below to do only one JNL_FILE_SWITCHED(jpc) check instead of the additional
	 * (NOJNL == jpc->channel) check done below. The assert below ensures that the NOJNL check can indeed
	 * be subsumed by the JNL_FILE_SWITCHED check (with the exception of the source-server which has a special case that
	 * needs to be fixed in C9D02-002241). Over time, this has to be changed to one check.
	 */
	assert((NOJNL != jpc->channel) || JNL_FILE_SWITCHED(jpc) || is_src_server);
	need_to_open_jnl = FALSE;
	jnl_status = 0;
	if (NOJNL == jpc->channel)
	{
#               ifdef VMS
		if (NOJNL != jpc->old_channel)
		{
			if (lib$ast_in_prog())          /* called from wcs_wipchk_ast */
				jnl_oper_user_ast(gv_cur_region);
			else
			{
				status = sys$setast(DISABLE);
				jnl_oper_user_ast(gv_cur_region);
				if (SS$_WASSET == status)
					ENABLE_AST;
			}
		}
#               endif
		need_to_open_jnl = TRUE;
	} else if (JNL_FILE_SWITCHED(jpc))
	{       /* The journal file has been changed "on the fly"; close the old one and open the new one */
		VMS_ONLY(assert(FALSE);)        /* everyone having older jnl open should have closed it at time of switch in VMS */
		JNL_FD_CLOSE(jpc->channel, close_res);  /* sets jpc->channel to NOJNL */
		need_to_open_jnl = TRUE;
	}
	if (need_to_open_jnl)
	{
		/* Whenever journal file get switch, reset the pini_addr and new_freeaddr. */
		jpc->pini_addr = 0;
		jpc->new_freeaddr = 0;
		if (IS_GTCM_GNP_SERVER_IMAGE)
			gtcm_jnl_switched(jpc->region); /* Reset pini_addr of all clients that had any older journal file open */
		UNIX_ONLY(first_open_of_jnl = (0 == csa->nl->jnl_file.u.inode);)
		VMS_ONLY(first_open_of_jnl = (0 == memcmp(csa->nl->jnl_file.jnl_file_id.fid, file.fid, SIZEOF(file.fid))));
		jnl_status = jnl_file_open(gv_cur_region, first_open_of_jnl, NULL);
	}
	DEBUG_ONLY(
		else
			GTM_WHITE_BOX_TEST(WBTEST_JNL_FILE_OPEN_FAIL, jnl_status, ERR_JNLFILOPN);
	)
	assert((0 != jnl_status) || !JNL_FILE_SWITCHED(jpc)
		UNIX_ONLY(|| (is_src_server && !JNL_ENABLED(csa) && REPL_WAS_ENABLED(csa))));
	return jnl_status;
}
