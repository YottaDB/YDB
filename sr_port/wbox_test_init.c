/****************************************************************
 *								*
 * Copyright (c) 2005-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "iosp.h"
#include "ydb_trans_log_name.h"
#include "wbox_test_init.h"

void wbox_test_init(void)
{
#	if defined (DEBUG) && !defined (STATIC_ANALYSIS)
	mstr	trans_name;
	char	trans_bufr[MAX_TRANS_NAME_LEN];

	if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_ENABLE, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
	{
<<<<<<< HEAD
		ydb_white_box_test_case_enabled = TRUE;
		if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_NUMBER, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
||||||| parent of 19e495f7cb (GT.M V7.1-003)
		gtm_white_box_test_case_enabled = TRUE;
		envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_NUMBER ;
		envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_NUMBER) - 1;
		if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
							do_sendmsg_on_log2long))
=======
		gtm_white_box_test_case_enabled = TRUE;
		envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_NUMBER;
		envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_NUMBER) - 1;
		if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
							do_sendmsg_on_log2long))
>>>>>>> 19e495f7cb (GT.M V7.1-003)
		{
<<<<<<< HEAD
			ydb_white_box_test_case_number = ATOI(trans_name.addr);
			if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_COUNT, &trans_name,
								trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
				ydb_white_box_test_case_count = ATOI(trans_name.addr);
||||||| parent of 19e495f7cb (GT.M V7.1-003)
			gtm_white_box_test_case_number = ATOI(trans_name.addr);
			envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_COUNT ;
			envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_COUNT) - 1;
			if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
								do_sendmsg_on_log2long))
				gtm_white_box_test_case_count = ATOI(trans_name.addr);
=======
			gtm_white_box_test_case_number = ATOI(trans_name.addr);
			envvar_logical.addr = GTM_WHITE_BOX_TEST_CASE_COUNT;
			envvar_logical.len = SIZEOF(GTM_WHITE_BOX_TEST_CASE_COUNT) - 1;
			if (SS_NORMAL == TRANS_LOG_NAME(&envvar_logical, &trans_name, trans_bufr, SIZEOF(trans_bufr),
								do_sendmsg_on_log2long))
				gtm_white_box_test_case_count = ATOI(trans_name.addr);
>>>>>>> 19e495f7cb (GT.M V7.1-003)
		}
	}
#	endif
}
