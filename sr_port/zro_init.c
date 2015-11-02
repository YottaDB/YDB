/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "iosp.h"
#include "io.h"
#include "zroutines.h"
#include "trans_log_name.h"
#include "gtm_logicals.h"

GBLREF	mstr	dollar_zroutines;

#define MAX_NUMBER_FILENAMES	256*MAX_TRANS_NAME_LEN

void zro_init (void)
{
	int4	status;
	mstr	val, tn;
	char	buf1[MAX_NUMBER_FILENAMES]; /* buffer to hold translated name */

	error_def(ERR_LOGTOOLONG);

	if (dollar_zroutines.addr)
		free(dollar_zroutines.addr);
	val.addr = GTM_ZROUTINES;
	val.len = sizeof(GTM_ZROUTINES) - 1;
	status = TRANS_LOG_NAME(&val, &tn, buf1, sizeof(buf1), dont_sendmsg_on_log2long);
	if ((SS_NORMAL != status) && (SS_NOLOGNAM != status))
	{
#		ifdef UNIX
		if (SS_LOG2LONG == status)
			rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, sizeof(buf1) - 1);
		else
#		endif
			rts_error(VARLSTCNT(1) status);
	}
	if (status == SS_NOLOGNAM)
		dollar_zroutines.len = 0;
	else
	{
		dollar_zroutines.len = tn.len;
		dollar_zroutines.addr = (char *) malloc (tn.len);
		memcpy (dollar_zroutines.addr, buf1, tn.len);
	}
	zro_load(&dollar_zroutines);
}
