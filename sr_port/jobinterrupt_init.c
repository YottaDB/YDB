/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Job interrupt initialization. Accomplish following setups:
 *
 * - Setup handler for SIGUSR1 signal to be handled by jobinterrupt_event() (UNIX only).
 * - Provide initial setting for $ZINTERRUPT from default or logical or
 *   environment variable if present.
 */

#include "mdef.h"

#include "gtm_signal.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "io.h"
#include "iosp.h"
#include "stringpool.h"
#include "jobinterrupt_process.h"
#include "ydb_trans_log_name.h"
#include "libyottadb.h"
#include "sig_init.h"

GBLREF	mval		dollar_zinterrupt;
GBLREF	boolean_t	ydb_treat_sigusr2_like_sigusr1;

#define DEF_ZINTERRUPT "IF $ZJOBEXAM()"

void jobinterrupt_init(void)
{
	char	trans_bufr[MAX_TRANS_NAME_LEN];
	struct sigaction new_action;

	if (!USING_ALTERNATE_SIGHANDLING)
	{
		int	i, num_sigs, sig_num[2];

		num_sigs = 0;
		sig_num[num_sigs++] = SIGUSR1;
		if (ydb_treat_sigusr2_like_sigusr1)
			sig_num[num_sigs++] = SIGUSR2;
		for (i = 0; i < num_sigs; i++)
		{	/* Setup signal handler for SIGUSR1 (and optionally SIGUSR2) */
			memset(&new_action, 0, SIZEOF(new_action));
			sigemptyset(&new_action.sa_mask);
			new_action.sa_flags = YDB_SIGACTION_FLAGS;
			new_action.sa_sigaction = jobinterrupt_event;
			sigaction(sig_num[i], &new_action, NULL);
		}
	} else
		SET_ALTERNATE_SIGHANDLER(SIGUSR1, &ydb_altusr1_sighandler);

	/* Provide initial setting for $ZINTERRUPT */
	if ((SS_NORMAL != ydb_trans_log_name(YDBENVINDX_ZINTERRUPT, &dollar_zinterrupt.str,
					     trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
	    || (0 == dollar_zinterrupt.str.len))
	{	/* Translation failed - use default */
		dollar_zinterrupt.str.addr = DEF_ZINTERRUPT;
		dollar_zinterrupt.str.len = SIZEOF(DEF_ZINTERRUPT) - 1;
	} else	/* put value in stringpool if translation succeeded */
		s2pool(&dollar_zinterrupt.str);
	dollar_zinterrupt.mvtype = MV_STR;
	return;
}
