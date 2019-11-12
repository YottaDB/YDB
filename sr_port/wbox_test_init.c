/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2005, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2005-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 3d3cd0dd... GT.M V6.3-010
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
<<<<<<< HEAD
#	ifdef DEBUG
	mstr	trans_name;
=======
#	if defined (DEBUG) && !defined (STATIC_ANALYSIS)
	mstr	envvar_logical, trans_name;
>>>>>>> 3d3cd0dd... GT.M V6.3-010
	char	trans_bufr[MAX_TRANS_NAME_LEN];

	if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_ENABLE, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
	{
		ydb_white_box_test_case_enabled = TRUE;
		if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_NUMBER, &trans_name,
							trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
		{
			ydb_white_box_test_case_number = ATOI(trans_name.addr);
			if (SS_NORMAL == ydb_trans_log_name(YDBENVINDX_WHITE_BOX_TEST_CASE_COUNT, &trans_name,
								trans_bufr, SIZEOF(trans_bufr), IGNORE_ERRORS_TRUE, NULL))
				ydb_white_box_test_case_count = ATOI(trans_name.addr);
		}
	}
#	endif
}
