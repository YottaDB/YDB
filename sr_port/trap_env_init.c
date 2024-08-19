/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "iosp.h"		/* for SS_ */
#include "min_max.h"
#include "gtm_string.h"
#include "gtm_strings.h" 	/* for STRNCASECMP */
#include "trap_env_init.h"
#include "gtmio.h"
#include "gtmimagename.h"
#include "ydb_logical_truth_value.h"
#include "compiler.h"
#include "op.h"
#include "stack_frame.h"
#include "indir_enum.h"
#include "ydb_trans_log_name.h"

#define ZTRAP_FORM_CODE				"code"
#define ZTRAP_FORM_ENTRYREF			"entryref"
#define ZTRAP_FORM_ADAPTIVE			"adaptive"
#define ZTRAP_FORM_POP				"pop"

GBLREF	boolean_t		is_updproc, run_time;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		shebang_invocation;	/* TRUE if yottadb is invoked through the "ydbsh" soft link */

#ifdef GTM_TRIGGER
LITREF mval			default_etrap;
#endif

error_def(ERR_LOGTOOLONG);
error_def(ERR_TRNLOGFAIL);

static readonly unsigned char init_break[1] = {'B'};
static readonly unsigned char init_shebang_etrap[] = "write:(0=$stack) \"Error occurred: \",$zstatus,! zhalt +$zstatus";

/* Initialize ztrap_form appropriately. Note this routine is not resident in gtm_env_init() because it raises errors
 * and error handling is not set up yet in gtm_env_init().
 */
void trap_env_init(void)
{
	int4		status;
	mstr		trans;
	char		buf[MAX_SRCLINE + 1], *buf_ptr = &buf[0];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assertpro(IS_MUMPS_IMAGE || is_updproc);
	assert(run_time || is_updproc);
	run_time = TRUE;							/* so updproc gets a pass if there's an error */
	/* Initialize which ever error trap we are using (ignored in the utilities except the update process) */
	if ((SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_ETRAP, &trans, buf, SIZEOF(buf), IGNORE_ERRORS_TRUE, NULL)))
				&& (0 < trans.len))
	{
		(TREF(dollar_etrap)).str.addr = malloc(trans.len + 1);		/* +1 for '\0'; This memory is never freed */
		memcpy((TREF(dollar_etrap)).str.addr, trans.addr, trans.len);
		(TREF(dollar_etrap)).str.addr[trans.len] = '\0';
		(TREF(dollar_etrap)).str.len = trans.len;
		(TREF(dollar_etrap)).mvtype = MV_STR;
		TREF(ind_source) = &(TREF(dollar_etrap));
		op_commarg(TREF(ind_source), indir_linetail);
		op_unwind();
	} else if (0 == (TREF(dollar_etrap)).mvtype)
	{
		if (!shebang_invocation)
		{	/* If didn't setup $ETRAP, set default $ZTRAP instead */
			(TREF(dollar_ztrap)).mvtype = MV_STR;
			(TREF(dollar_ztrap)).str.len = SIZEOF(init_break);
			(TREF(dollar_ztrap)).str.addr = (char *)init_break;
		} else
		{	/* For a shebang invocation, set $ETRAP to halt out after printing $zstatus */
			(TREF(dollar_etrap)).mvtype = MV_STR;
			(TREF(dollar_etrap)).str.len = SIZEOF(init_shebang_etrap);
			(TREF(dollar_etrap)).str.addr = (char *)init_shebang_etrap;
		}
	}
#	ifdef GTM_TRIGGER
	(TREF(ydb_trigger_etrap)).mvtype = MV_STR;
	if ((SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_TRIGGER_ETRAP, &trans, buf, SIZEOF(buf),
										IGNORE_ERRORS_TRUE, NULL)))
		&& (0 < trans.len))
	{
		(TREF(ydb_trigger_etrap)).str.addr = malloc(trans.len + 1);	 /* +1 for '\0'; This memory is never freed */
		memcpy((TREF(ydb_trigger_etrap)).str.addr, trans.addr, trans.len);
		(TREF(ydb_trigger_etrap)).str.addr[trans.len] = '\0';
		(TREF(ydb_trigger_etrap)).str.len = trans.len;
		TREF(ind_source) = &(TREF(ydb_trigger_etrap));
		op_commarg(TREF(ind_source), indir_linetail);
		op_unwind();
	} else if (IS_MUPIP_IMAGE)
		(TREF(ydb_trigger_etrap))= default_etrap;
#	endif
	/* Initialize $ZSTEP from $ydb_zstep enviroment variable. */
	if ((SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_ZSTEP, &trans, buf, SIZEOF(buf), IGNORE_ERRORS_TRUE, NULL)))
		&& (0 < trans.len))
	{
		(TREF(dollar_zstep)).str.addr = malloc(trans.len + 1); 		/* +1 for '\0'; This memory is never freed */
		memcpy((TREF(dollar_zstep)).str.addr, trans.addr, trans.len);
		(TREF(dollar_zstep)).str.addr[trans.len] = '\0';
		(TREF(dollar_zstep)).str.len = trans.len;
		(TREF(dollar_zstep)).mvtype = MV_STR;
		TREF(ind_source) = &(TREF(dollar_zstep));
		op_commarg(TREF(ind_source), indir_linetail);
		op_unwind();
	} else if (0 == (TREF(dollar_zstep)).mvtype)
	{
		(TREF(dollar_zstep)).mvtype = MV_STR;
		(TREF(dollar_zstep)).str.len = SIZEOF(init_break);
		(TREF(dollar_zstep)).str.addr = (char *)init_break;
	}
	run_time = !is_updproc;							/* restore to appropriate state */
	TREF(ind_source) = NULL;						/* probably superfluous, but profilactic */
	TREF(ztrap_form) = ZTRAP_CODE;						/* default */
	if (SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_ZTRAP_FORM, &trans, buf, SIZEOF(buf), IGNORE_ERRORS_FALSE, NULL)))
	{
		assert(SS_NOLOGNAM == status);
		return;
	}
	if ((STR_LIT_LEN(ZTRAP_FORM_POP) < trans.len) && !STRNCASECMP(buf_ptr, ZTRAP_FORM_POP, STR_LIT_LEN(ZTRAP_FORM_POP)))
	{	/* "pop" can be a prefix to both entryref and adaptive */
		buf_ptr += STR_LIT_LEN(ZTRAP_FORM_POP);
		trans.len -= STR_LIT_LEN(ZTRAP_FORM_POP);
		TREF(ztrap_form) |= ZTRAP_POP;
	}
	if ((STR_LIT_LEN(ZTRAP_FORM_ENTRYREF) == trans.len)
		&& !STRNCASECMP(buf_ptr, ZTRAP_FORM_ENTRYREF, MIN(STR_LIT_LEN(ZTRAP_FORM_ENTRYREF), trans.len)))
	{
		TREF(ztrap_form) |= ZTRAP_ENTRYREF;
		TREF(ztrap_form) &= ~ZTRAP_CODE;
	} else if ((STR_LIT_LEN(ZTRAP_FORM_ADAPTIVE) == trans.len)
			&& !STRNCASECMP(buf_ptr, ZTRAP_FORM_ADAPTIVE, MIN(STR_LIT_LEN(ZTRAP_FORM_ADAPTIVE), trans.len)))
		TREF(ztrap_form) |= ZTRAP_ENTRYREF;
	return;
}
