/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "send_msg.h"
#include "gtmio.h"
#include "repl_sp.h"
#include "iosp.h"	/* for SS_NORMAL */
#include "wbox_test_init.h"

GBLREF 	jnl_gbls_t		jgbl;

error_def(ERR_JNLFILOPN);
error_def(ERR_JNLSWITCHFAIL);
error_def(ERR_JNLSWITCHRETRY);
error_def(ERR_PREVJNLLINKCUT);

uint4 jnl_file_open_switch(gd_region *reg, uint4 sts)
{
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	jnl_create_info		create;
	char			prev_jnl_fn[JNL_NAME_SIZE];
	int			status;
#	if defined(GTM_TRIGGER) && defined(DEBUG)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	csa = &FILE_INFO(reg)->s_addrs;
	jpc = csa->jnl;
	assert(sts GTMTRIG_ONLY(|| TREF(in_trigger_upgrade)));
	assert(!sts || ((ERR_JNLFILOPN != sts) && (NOJNL != jpc->channel)) || ((ERR_JNLFILOPN == sts) && (NOJNL == jpc->channel))
			|| (ERR_JNLSWITCHRETRY == sts));
	if (NOJNL != jpc->channel)
		JNL_FD_CLOSE(jpc->channel, status);	/* sets jpc->channel to NOJNL */
	if (sts)
		jnl_send_oper(jpc, sts);
	/* attempt to create a new journal file */
	memset(&create, 0, SIZEOF(create));
	create.status = create.status2 = SS_NORMAL;
	create.prev_jnl = &prev_jnl_fn[0];
	set_jnl_info(reg, &create);
	/* ERR_JNLSWITCHRETRY indicates that jnl_file_open_common found that the current journal had been marked for a
	 * switch, but the switch failed at a later point, so don't cut links in that case.
	 */
	create.no_prev_link = (ERR_JNLSWITCHRETRY != sts);
	create.no_rename = FALSE;
	assert(!jgbl.forw_phase_recovery || WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC) || WBTEST_ENABLED(WBTEST_JNL_CREATE_FAIL));
	if (!jgbl.dont_reset_gbl_jrec_time)
		SET_GBL_JREC_TIME;	/* needed for cre_jnl_file() */
	/* else mur_output_record() would have already set jgbl.gbl_jrec_time */
	assert(jgbl.gbl_jrec_time);
	if (EXIT_NRM != cre_jnl_file(&create))
	{
		jpc->status = create.status;
		jpc->status2 = create.status2;
		return ERR_JNLSWITCHFAIL;
	} else
	{
		jpc->status = SS_NORMAL;
		csa->hdr->jnl_checksum = create.checksum;
		csa->hdr->jnl_eovtn = csa->hdr->trans_hist.curr_tn;
	}
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_PREVJNLLINKCUT, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(reg));
	assert(csa->hdr->jnl_file_len == create.jnl_len);
	assert(0 == memcmp(csa->hdr->jnl_file_name, create.jnl, create.jnl_len));
	return 0;
}
