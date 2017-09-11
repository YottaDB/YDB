/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_string.h"
#include "gtm_strings.h" 	/* for STRNCASECMP */
#include "trans_log_name.h"
#include "ztrap_form_init.h"

#define ZTRAP_FORM_CODE		"code"
#define ZTRAP_FORM_ENTRYREF	"entryref"
#define ZTRAP_FORM_ADAPTIVE	"adaptive"
#define ZTRAP_FORM_POP		"pop"

GBLREF	int	ztrap_form;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

/* Initialize ztrap_form appropriately. Note this routine is not resident in gtm_env_init() because it raises errors
 * and error handling is not set up yet in gtm_env_init().
 */
void ztrap_form_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf[1024], *buf_ptr = &buf[0];

	ztrap_form = ZTRAP_CODE;	/* default */
	val.addr = ZTRAP_FORM;
	val.len = STR_LIT_LEN(ZTRAP_FORM);
	if (SS_NORMAL != (status = TRANS_LOG_NAME(&val, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long)))
	{
		if (SS_NOLOGNAM == status)
			return;
		else if (SS_LOG2LONG == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, SIZEOF(buf) - 1);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TRNLOGFAIL, 2, LEN_AND_LIT(ZTRAP_FORM), status);
	}
	if ((STR_LIT_LEN(ZTRAP_FORM_POP) < tn.len) && !STRNCASECMP(buf_ptr, ZTRAP_FORM_POP, STR_LIT_LEN(ZTRAP_FORM_POP)))
	{	/* "pop" can be a prefix to both entryref and adaptive */
		buf_ptr += STR_LIT_LEN(ZTRAP_FORM_POP);
		tn.len -= STR_LIT_LEN(ZTRAP_FORM_POP);
		ztrap_form |= ZTRAP_POP;
	}
	if ((STR_LIT_LEN(ZTRAP_FORM_ENTRYREF) == tn.len)
			&& !STRNCASECMP(buf_ptr, ZTRAP_FORM_ENTRYREF, MIN(STR_LIT_LEN(ZTRAP_FORM_ENTRYREF), tn.len)))
	{
		ztrap_form |= ZTRAP_ENTRYREF;
		ztrap_form &= ~ZTRAP_CODE;
	} else if ((STR_LIT_LEN(ZTRAP_FORM_ADAPTIVE) == tn.len)
			&& !STRNCASECMP(buf_ptr, ZTRAP_FORM_ADAPTIVE, MIN(STR_LIT_LEN(ZTRAP_FORM_ADAPTIVE), tn.len)))
		ztrap_form |= ZTRAP_ENTRYREF;
	return;
}
