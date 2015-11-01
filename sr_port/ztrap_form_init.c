/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "iosp.h"
#include "gtm_logicals.h"
#include "min_max.h"
#include "gtm_string.h" 	/* for STRNCASECMP */
#include "trans_log_name.h"
#include "ztrap_form_init.h"

#define ZTRAP_FORM_CODE		"code"
#define ZTRAP_FORM_ENTRYREF	"entryref"
#define ZTRAP_FORM_ADAPTIVE	"adaptive"

GBLREF	int	ztrap_form;

void ztrap_form_init(void)
{
	uint4		status;
	mstr		val, tn;
	char		buf[1024];

	error_def(ERR_TRNLOGFAIL);

	ztrap_form = ZTRAP_CODE; /* default */
	val.addr = ZTRAP_FORM;
	val.len = sizeof(ZTRAP_FORM) - 1;
	if (SS_NORMAL == (status = trans_log_name(&val, &tn, buf)))
	{
		if (0 == STRNCASECMP(buf, ZTRAP_FORM_ENTRYREF, MIN(sizeof(ZTRAP_FORM_ENTRYREF) - 1, tn.len)))
			ztrap_form = ZTRAP_ENTRYREF;
		else if (0 == STRNCASECMP(buf, ZTRAP_FORM_ADAPTIVE, MIN(sizeof(ZTRAP_FORM_ADAPTIVE) - 1, tn.len)))
			ztrap_form |= ZTRAP_ENTRYREF;
	} else if (SS_NOLOGNAM != status)
		rts_error(VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(ZTRAP_FORM), status);
	return;
}
