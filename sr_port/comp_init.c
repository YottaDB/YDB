/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
#include "mv_stent.h"
#include "opcode.h"
#include "cgp.h"
#include "lb_init.h"

/*	WARNING: comp_init changes the currently-active stringpool from the
 *	the runtime stringpool (rts_stringpool) to the indirection stringpool
 *	(indr_stringpool).  comp_fini changes it back from indr_stringpool to
 *	rts_stringpool when the compilation is finished.
 */
GBLREF spdesc stringpool,rts_stringpool;
GBLREF spdesc indr_stringpool;
GBLREF unsigned char *source_buffer;
GBLREF int4 curr_fetch_count;
GBLREF triple *curr_fetch_trip;
GBLREF char cg_phase;

error_def(ERR_INDRMAXLEN);

void comp_init(mstr *src)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((unsigned)src->len >= MAX_SRCLINE)
		rts_error(VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
	memcpy(source_buffer,src->addr,src->len);
	source_buffer[src->len + 1] = source_buffer[src->len] = 0;
	TREF(compile_time) = TRUE;
	TREF(transform) = FALSE;
	cg_phase = CGP_PARSE;
	TREF(source_error_found) = 0;
	TREF(last_source_column) = 0;
	assert(rts_stringpool.base == stringpool.base);
	rts_stringpool = stringpool;
	if (!indr_stringpool.base)
	{
		stp_init(STP_INITSIZE);
		indr_stringpool = stringpool;
	} else
		stringpool = indr_stringpool;
	tripinit();
	lb_init();
	assert(TREF(for_stack_ptr) == TADR(for_stack));
	*TREF(for_stack_ptr) = NULL;
	curr_fetch_trip = newtriple(OC_FETCH);
	curr_fetch_count = 0;
	start_fetches(OC_FETCH);
	return;
}
