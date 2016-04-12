/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_logicals.h"
#include "iosp.h"
#include "trans_log_name.h"
#include "wbox_test_init.h"

void wbox_test_init(void)
{
#	ifdef DEBUG
	mstr	envvar_logical, trans_name;
	char	trans_bufr[MAX_TRANS_NAME_LEN];

	envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_ENABLE;
	envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_ENABLE) - 1;
	if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr), do_sendmsg_on_log2long))
	{
		gtm_white_box_test_case_enabled = TRUE;
		envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_NUMBER ;
		envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_NUMBER) - 1;
		if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
							do_sendmsg_on_log2long))
		{
			gtm_white_box_test_case_number = ATOI(trans_name.addr);
			envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_COUNT ;
			envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_COUNT) - 1;
			if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
								do_sendmsg_on_log2long))
				gtm_white_box_test_case_count = ATOI(trans_name.addr);
		}
	}
#	endif
}
