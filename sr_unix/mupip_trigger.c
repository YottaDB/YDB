/****************************************************************
 *								*
 * Copyright (c) 2010-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER

#include <errno.h>
#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_limits.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "mupip_trigger.h"
#include "mu_trig_trgfile.h"
#include "trigger_select_protos.h"
#include "util.h"
#include "mupip_exit.h"
#include "change_reg.h"
#include "targ_alloc.h"
#include "gvcst_protos.h"
#include "trigger_upgrade_protos.h"
#include "restrict.h"

GBLREF	gd_addr		*gd_header;
GBLREF	boolean_t	is_replicator;

error_def(ERR_INVSTRLEN);
error_def(ERR_MUNOACTION);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOSELECT);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TRIGMODREGNOTRW);

void mupip_trigger(void)
{
	char		trigger_file_name[MAX_FN_LEN + 1], select_list[MAX_LINE], select_file_name[MAX_FN_LEN + 1];
	unsigned short	trigger_file_len = MAX_FN_LEN + 1, select_list_len = MAX_LINE;
	unsigned short	sf_name_len;
	int		local_errno;
	struct stat	statbuf;
	boolean_t	noprompt, trigger_error;
	gd_region	*reg, *reg_top;
	sgmnt_addrs	*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (CLI_PRESENT == cli_present("TRIGGERFILE"))
	{
		noprompt = (CLI_PRESENT == cli_present("NOPROMPT"));
		if (!cli_get_str("TRIGGERFILE", trigger_file_name, &trigger_file_len))
		{
			util_out_print("Error parsing TRIGGERFILE name", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		assert('\0' == trigger_file_name[trigger_file_len]); /* should have been made sure by caller */
		if (0 == trigger_file_len)
		{
			util_out_print("Missing input file name", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (RESTRICTED(trigger_mod))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "MUPIP TRIGGER -TRIGGERFILE");
			mupip_exit(ERR_MUNOACTION);
		}
		is_replicator = TRUE;
		TREF(ok_to_see_statsdb_regs) = TRUE;
		gvinit();
		mu_trig_trgfile(trigger_file_name, (uint4)trigger_file_len, noprompt);
	}
	if (CLI_PRESENT == cli_present("SELECT"))
	{
		if (FALSE == cli_get_str("SELECT", select_list, &select_list_len))
			mupip_exit(ERR_MUPCLIERR);
		sf_name_len = MAX_FN_LEN;
		if (FALSE == cli_get_str("FILE", select_file_name, &sf_name_len))
			mupip_exit(ERR_MUPCLIERR);
		if (0 == sf_name_len)
			select_file_name[0] = '\0';
		else if (-1 == Stat((char *)select_file_name, &statbuf))
		{
			if (ENOENT != errno)
			{
				local_errno = errno;
				perror("Error opening output file");
				mupip_exit(local_errno);
			}
		} else
		{
			util_out_print("Error opening output file: !AD -- File exists", TRUE, sf_name_len, select_file_name);
			mupip_exit(ERR_MUNOACTION);
		}
		trigger_error = trigger_select_tpwrap(select_list, (uint4)select_list_len, select_file_name, (uint4)sf_name_len);
		if (trigger_error)
			mupip_exit(ERR_MUNOACTION);
	}
	if (CLI_PRESENT == cli_present("UPGRADE"))
	{	/* Invoke MUPIP TRIGGER -UPGRADE */
		if (RESTRICTED(trigger_mod))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "MUPIP TRIGGER -UPGRADE");
			mupip_exit(ERR_MUNOACTION);
		}
		gvinit();
		DEBUG_ONLY(TREF(in_trigger_upgrade) = TRUE;)
		for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
		{
			if (IS_STATSDB_REGNAME(reg))
				continue;
			GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
			csa = cs_addrs;
			if (NULL == csa)	/* not BG or MM access method OR a statsdb region */
				continue;
			if (!csa->hdr->hasht_upgrade_needed)
			{
				util_out_print("Triggers in region !AD have already been upgraded", TRUE, REG_LEN_STR(reg));
				continue;	/* ^#t already upgraded */
			}
			if (reg->read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGMODREGNOTRW, 2, REG_LEN_STR(reg));
			if (0 == gv_target->root)
			{
				util_out_print("No triggers found in region !AD and so no upgrade needed", TRUE, REG_LEN_STR(reg));
				csa->hdr->hasht_upgrade_needed = FALSE;	/* Reset now that we know there is no ^#t global in this db.
									 * Note: It is safe to do so even if we dont hold crit.
									 */
				continue;	/* no ^#t records exist in this region */
			}
			assert(!dollar_tlevel);
			assert(!is_replicator);
			trigger_upgrade(reg);
			assert(!csa->hdr->hasht_upgrade_needed);	/* should have been cleared inside trigger_upgrade */
			util_out_print("Triggers in region !AD have been upgraded", TRUE, REG_LEN_STR(reg));
		}
		DEBUG_ONLY(TREF(in_trigger_upgrade) = FALSE;)
	}
}
#endif
