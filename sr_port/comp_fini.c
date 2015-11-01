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

#include "compiler.h"
#include "opcode.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "cgp.h"
#include "alloc_reg.h"

/*	WARNING: comp_fini restores the currently-active stringpool from the
 *	indirection stringpool (indr_stringpool) to the runtime stringpool
 *	(rts_stringpool).  It depends on comp_init having changed it from
 *	rts_stringpool to indr_stringpool during compilation setup.
 */
GBLREF spdesc stringpool,rts_stringpool,indr_stringpool;

GBLREF oprtype *for_stack[],**for_stack_ptr;
GBLREF bool compile_time;
GBLREF short int source_column;
GBLREF char cg_phase;
GBLREF unsigned char *source_buffer;
GBLREF bool		transform;


int comp_fini(bool status, mstr *obj, opctype retcode, oprtype *retopr, int src_len)
{

	triple *ref;
	error_def(ERR_INDEXTRACHARS);

	if (status  &&  source_column != src_len + 2  &&  source_buffer[source_column] != '\0')
	{
		status = FALSE;
		stx_error(ERR_INDEXTRACHARS);
	}
	if (status)
	{
		cg_phase = CGP_RESOLVE;
		assert(for_stack_ptr == for_stack);
		if (*for_stack_ptr)
			tnxtarg(*for_stack_ptr);
		ref = newtriple(retcode);
		if (retopr)
			ref->operand[0] = *retopr;
		start_fetches(OC_NOOP);
		resolve_ref(0);	/* cannot fail because there are no MLAB_REF's in indirect code */
		alloc_reg();
		stp_gcol(0);
		assert(indr_stringpool.base == stringpool.base);
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
		compile_time = FALSE;
 		ind_code(obj);
		indr_stringpool.free = indr_stringpool.base;
	}
	else
	{
		assert(indr_stringpool.base == stringpool.base);
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
		indr_stringpool.free = indr_stringpool.base;
		compile_time = FALSE;
		cg_phase = CGP_NOSTATE;
	}
	transform = TRUE;
	mcfree();
	return status;

}
