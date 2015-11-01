/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"

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
GBLREF unsigned char	*util_outptr;
GBLREF int4		exi_condition;

CONDITION_HANDLER(gtcm_exi_ch)
{
	int		rc;
	now_t		now;	/* for GET_CUR_TIME macro */
	char		time_str[CTIME_BEFORE_NL + 2], *time_ptr; /* for GET_CUR_TIME macro */
	error_def(ERR_TEXT);

	if (gtcm_firsterr)
		gtcm_open_cmerrlog();
	if (gtcm_errfile)
	{
		if ((char *)util_outptr != util_outbuff)
		{	/* msg yet to be flushed. Properly terminate it in the buffer. If msg has
			   already been flushed (to stderr) then this has already been done. */
			*util_outptr = '\n';
			*(util_outptr + 1) = 0;
		}
		GET_CUR_TIME;
		time_str[CTIME_BEFORE_NL] = 0;
		FPRINTF(gtcm_errfs, "%s: %s", time_str, util_outbuff);
		fflush(gtcm_errfs);
	}
	send_msg(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_TEXT("GT.CM TERMINATION RUNDOWN ERROR"));

	PROCDIE(exi_condition);
}
