/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2025 SP.ARM					*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include "stringpool.h"
#include "error.h"
#include "mvalconv.h"

#include "compiler.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mdq.h"
#include "cgp.h"
#include "mmemory.h"
#include "stp_parms.h"
#include "list_file.h"
#include "source_file.h"
#include "lb_init.h"
#include "reinit_compilation_externs.h"
#include "comp_esc.h"
#include "resolve_blocks.h"
#include "hashtab_str.h"
#include "rtn_src_chksum.h"
#include "gtmmsg.h"
#include "iosp.h"	/* for SS_NORMAL */
#include "start_fetches.h"

error_def(ERR_UNKNOWNSYSERR);
error_def(ERR_INDRMAXLEN);

GBLREF int			source_column;
GBLREF char			cg_phase;	/* phase code generator */
GBLREF command_qualifier	cmd_qlf;	/* you need to reset it to work in "quiet" mode */
GBLREF int			mlmax;
GBLREF mline			mline_root;
GBLREF spdesc			indr_stringpool, rts_stringpool, stringpool;
GBLREF src_line_struct		src_head;
GBLREF triple			t_orig;
GBLREF boolean_t		tref_transform;

LITREF	mval			literal_null;

/* Note: The below code is based on "sr_port/compiler_startup.c". It strips out logic that deals
 * with an M source file or an M object file but is mostly the same otherwise.
 */
void op_fnzycompile(mval *string, mval *ret)
{
	int			errknt, errpos;
	uint4			line_count;
	mlabel			*null_lab;
	mident			null_mident;
	size_t			len, lent;
	char			*out;
	command_qualifier	cmd_qlf_save;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(string);
	if (MAX_SRCLINE < (unsigned)string->str.len)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
	if (string->str.len == 0)
	{
		*ret = literal_null;
		return;
	}
	/* The below 3 lines are similar to that in "sr_port/comp_init.c" */
	memcpy((TREF(source_buffer)).addr, string->str.addr, string->str.len);
	(TREF(source_buffer)).len = string->str.len + 1;
	*((TREF(source_buffer)).addr + string->str.len) = *((TREF(source_buffer)).addr + string->str.len + 1) = '\0';

	if (rts_stringpool.base == stringpool.base)
	{
		rts_stringpool = stringpool;
		if (!indr_stringpool.base)
		{
			stp_init(STP_INITSIZE);
			indr_stringpool = stringpool;
		} else
			stringpool = indr_stringpool;
	}
	cmd_qlf_save = cmd_qlf;
	cmd_qlf.qlf = 0;
	run_time = FALSE;
	TREF(compile_time) = TRUE;
	tref_transform = FALSE;
	TREF(dollar_zcstatus) = SS_NORMAL;
	reinit_compilation_externs();
	memset(&null_mident, 0, SIZEOF(null_mident));

	ESTABLISH(compiler_ch);
	COMPILE_HASHTAB_CLEANUP;

	/* To check M code nested dotted DO block lines, we will mask/remove the leading dots (level depth) in the line */
	out = (TREF(source_buffer)).addr;
	while ('\0' != *out)
	{
		if ('.' == *out)
			*out = ' ';
		if (('\t' != *out) && (' ' != *out))
			break;
		out++;
	}
	COMPILE_HASHTAB_CLEANUP;
	TREF(source_error_found) = errknt = 0;
	/* Note: Setting "cg_phase" below to CGP_PARSE will cause warnings like SVNOSET to be ignored as valid M code
	 * by $ZYCOMPILE and so we set it to CGP_NOSTATE below. Example invocation is [$zycompile(" set (x,$TEST)=1")].
	 */
	cg_phase = CGP_NOSTATE;
	dqinit(&src_head, que);
	tripinit();
	null_lab = get_mladdr(&null_mident);
	null_lab->ml = &mline_root;
	mlmax++;
	(TREF(fetch_control)).curr_fetch_trip =
	(TREF(fetch_control)).curr_fetch_opr = newtriple(OC_LINEFETCH);
	(TREF(fetch_control)).curr_fetch_count = 0;
	TREF(code_generated) = FALSE;
	TREF(source_line) = line_count = 1;
	TREF(source_error_found) = 0;
	lb_init();
	if (!line(&line_count))
		errknt++;
	newtriple(OC_LINESTART);
	// always provide a default QUIT
	newtriple(OC_RET);
	mline_root.externalentry = t_orig.exorder.fl;
	assert(indr_stringpool.base == stringpool.base);
	INVOKE_STP_GCOL(0);
	COMPILE_HASHTAB_CLEANUP;
	reinit_compilation_externs();
	run_time = TRUE;
	TREF(compile_time) = FALSE;
	tref_transform = TRUE;
	assert(indr_stringpool.base == stringpool.base);
	if (indr_stringpool.base == stringpool.base)
	{
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	(TREF(source_buffer)).len = 0;
	cmd_qlf = cmd_qlf_save;
	REVERT;

	errknt = TREF(source_error_found);
	errpos = TREF(last_source_column);
	if (!errknt)
		*ret = literal_null;
	else
	{	/* Some of the below logic is similar to that in "sr_unix/gtm_getmsg.c" */
		const err_ctl	*ec;

		ec = err_check(errknt);
		if (NULL != ec)
		{
			const err_msg	*em;

			GET_MSG_INFO(errknt, ec, em);
			len = STRLEN(em->msg);
			lent = STRLEN(em->tag);
			len += (lent + MAX_NUM_SIZE + 9);
			ENSURE_STP_FREE_SPACE(len);
			lent = SNPRINTF((char*)stringpool.free, len, "%i,%%YDB-E-%s,%s", errpos, em->tag, em->msg);
			ret->mvtype = MV_STR;
			ret->str.addr = (char *)stringpool.free;
			ret->str.len = lent;
			stringpool.free += lent;
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_UNKNOWNSYSERR, 1, errknt);
	}
}
