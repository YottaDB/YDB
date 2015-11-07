/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "stringpool.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "cgp.h"
#include "alloc_reg.h"
#include "advancewindow.h"
#include "hashtab_str.h"

/*	WARNING: comp_fini restores the currently-active stringpool from the
 *	indirection stringpool (indr_stringpool) to the runtime stringpool
 *	(rts_stringpool).  It depends on comp_init having changed it from
 *	rts_stringpool to indr_stringpool during compilation setup.
 */
GBLREF spdesc		stringpool, rts_stringpool, indr_stringpool;
GBLREF short int	source_column;
GBLREF char		cg_phase;
GBLREF unsigned char	*source_buffer;

error_def(ERR_INDEXTRACHARS);
error_def(ERR_INDRCOMPFAIL);

int comp_fini(int status, mstr *obj, opctype retcode, oprtype *retopr, oprtype *dst, mstr_len_t src_len)
{
	triple *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (status)
	{
		while (TK_SPACE == TREF(window_token))	/* Eat up trailing white space */
			advancewindow();
		if (TK_ERROR == TREF(window_token))
		{
			status = EXPR_FAIL;
			stx_error(ERR_INDRCOMPFAIL);
		} else if ((TK_EOL != TREF(window_token)) || (source_column < src_len))
		{
			status = EXPR_FAIL;
			stx_error(ERR_INDEXTRACHARS);
		} else
		{
			cg_phase = CGP_RESOLVE;
			assert(TREF(for_stack_ptr) == TADR(for_stack));
			if (*TREF(for_stack_ptr))
				tnxtarg(*TREF(for_stack_ptr));
			ref = newtriple(retcode);
			if (retopr)
				ref->operand[0] = *retopr;
			if (OC_IRETMVAL == retcode)
				ref->operand[1] = *dst;
			start_fetches(OC_NOOP);
			resolve_ref(0);	/* cannot fail because there are no MLAB_REF's in indirect code */
			alloc_reg();
			INVOKE_STP_GCOL(0);
			/* The above invocation of stp_gcol with a parameter of 0 is a critical part of compilation
			 * (both routine compilations and indirect dynamic compilations). This collapses the indirect
			 * (compilation) stringpool so that only the literals are left. This stringpool is then written
			 * out to the compiled object as the literal pool for that compilation. Temporary stringpool
			 * use for conversions or whatever are eliminated. Note the path is different in stp_gcol for
			 * the indirect stringpool which is only used during compilations.
			 */
			assert(indr_stringpool.base == stringpool.base);
			indr_stringpool = stringpool;
			stringpool = rts_stringpool;
			TREF(compile_time) = FALSE;
			ind_code(obj);
			indr_stringpool.free = indr_stringpool.base;
		}
	} else
	{	/* If this assert fails, it means a syntax problem could have been caught earlier. Consider placing a more useful
		 * and specific error message at that location.
		 */
		assert(FALSE);
		stx_error(ERR_INDRCOMPFAIL);
	}
	if (EXPR_FAIL == status)
	{
		assert(indr_stringpool.base == stringpool.base);
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
		indr_stringpool.free = indr_stringpool.base;
		TREF(compile_time) = FALSE;
		cg_phase = CGP_NOSTATE;
	}
	TREF(transform) = TRUE;
	COMPILE_HASHTAB_CLEANUP;
	mcfree();
	return status;
}
