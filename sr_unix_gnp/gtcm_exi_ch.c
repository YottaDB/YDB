/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"

#include "gtm_multi_thread.h"
#include "have_crit.h"
#include "gtmio.h"
#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "error.h"
#include "util.h"
#include "gtm_stdio.h"
#include "send_msg.h"
#include "io.h"
#include "trans_log_name.h"
#include "gtm_string.h"
#include "gtcm_open_cmerrlog.h"

GBLREF bool		gtcm_errfile;
GBLREF bool		gtcm_firsterr;
GBLREF FILE		*gtcm_errfs;
GBLREF int4		exi_condition;

error_def(ERR_TEXT);

CONDITION_HANDLER(gtcm_exi_ch)
{
	int		rc;
	char		time_str[CTIME_BEFORE_NL + 2];	/* for GET_CUR_TIME macro */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_IS_LIBGNPSERVER;
	if (gtcm_firsterr)
		gtcm_open_cmerrlog();
	if (gtcm_errfile)
	{
		ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
		if (TREF(util_outptr) != TREF(util_outbuff_ptr))
		{	/* msg yet to be flushed. Properly terminate it in the buffer. If msg has
			   already been flushed (to stderr) then this has already been done. */
			*(TREF(util_outptr)) = '\n';
			*(TREF(util_outptr) + 1) = 0;
		}
		GET_CUR_TIME(time_str);
		time_str[CTIME_BEFORE_NL] = 0;
		FPRINTF(gtcm_errfs, "%s: %s", time_str, TREF(util_outbuff_ptr));
		FFLUSH(gtcm_errfs);
	}
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_TEXT("GT.CM TERMINATION RUNDOWN ERROR"));
	PROCDIE(exi_condition);
}
