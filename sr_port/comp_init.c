/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "stp_parms.h"
#include "compiler.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "opcode.h"
#include "cgp.h"
#include "lb_init.h"
#include "start_fetches.h"

/*	WARNING: comp_init changes the currently-active stringpool from the
 *	the runtime stringpool (rts_stringpool) to the indirection stringpool
 *	(indr_stringpool).  comp_fini changes it back from indr_stringpool to
 *	rts_stringpool when the compilation is finished.
 */
GBLREF char		cg_phase;
GBLREF spdesc		stringpool,rts_stringpool;
GBLREF spdesc		indr_stringpool;
GBLREF boolean_t	tref_transform;

error_def(ERR_INDRMAXLEN);

void comp_init(mstr *src, oprtype *dst)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((MAX_SRCLINE < (unsigned)src->len) && ((TREF(source_buffer)).addr == (char *)aligned_source_buffer))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);	/* no error for ojchildparms / other long */
	memcpy((TREF(source_buffer)).addr,src->addr,src->len);
	(TREF(source_buffer)).len = src->len + 1;
	*((TREF(source_buffer)).addr + src->len) = *((TREF(source_buffer)).addr + src->len + 1) = '\0';
	TREF(compile_time) = TRUE;
	tref_transform = FALSE;
	cg_phase = CGP_PARSE;
	TREF(source_error_found) = 0;
	TREF(last_source_column) = 0;
	DEBUG_ONLY(TREF(source_line) = 0;);
	assert(rts_stringpool.base == stringpool.base);
	rts_stringpool = stringpool;
	if (!indr_stringpool.base)
	{
		stp_init(STP_INITSIZE);
		indr_stringpool = stringpool;
	} else
		stringpool = indr_stringpool;
	tripinit();		/* Also does what mcfree() would do */
	lb_init();
	assert(TREF(for_stack_ptr) == TADR(for_stack));
	*TREF(for_stack_ptr) = NULL;
	(TREF(fetch_control)).curr_fetch_trip = newtriple(OC_FETCH);
	(TREF(fetch_control)).curr_fetch_count = 0;
	START_FETCHES(OC_FETCH);
	/* op_igetdst fetches the destination (ind_result) onto the M-stack at the start of execution so that if we end up doing
	 * nested indirection, in which case ind_result could change, op_iretmval can put the result in the correct location.
	 * op_igetsrc serves a very similar purpose, placing a copy of the source mval (ind_source) on the M-stack at the start
	 * of execution.
	 */
	if (dst)
		*dst = put_tref(newtriple(OC_IGETDST));
	return;
}
