/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_limits.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "muextr.h"		/* for glist */
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "mupip_trigger.h"
#include "mu_trig_trgfile.h"
#include "trigger_select_protos.h"
#include "util.h"
#include "mupip_exit.h"

error_def(ERR_MUPCLIERR);
error_def(ERR_NOSELECT);
error_def(ERR_MUNOACTION);
error_def(ERR_INVSTRLEN);

void mupip_trigger(void)
{
	char		trigger_file_name[MAX_FN_LEN + 1], select_list[MAX_LINE], select_file_name[MAX_FN_LEN + 1];
	unsigned short	trigger_file_len = MAX_FN_LEN + 1, select_list_len = MAX_LINE;
	int		reg_max_rec, reg_max_key, reg_max_blk;
	unsigned short	sf_name_len;
	int		local_errno;
	struct stat	statbuf;
	boolean_t	noprompt;

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
		(void)trigger_select(select_list, (uint4)select_list_len, select_file_name, (uint4)sf_name_len);
	}
}
