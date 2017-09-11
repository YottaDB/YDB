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
#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"
#include "lb_init.h"
#include "error.h"

GBLREF boolean_t	run_time;
GBLREF int		(*indir_fcn[])(), source_column;
GBLREF int4		aligned_source_buffer, pending_errtriplecode;

STATICDEF parse_save_block	*parse_state_ptr;
STATICDEF char			*local_source_buffer;

error_def(ERR_INDRMAXLEN);

int m_xecute(void)
/* compiler module for XECUTE */
{
	DEBUG_ONLY(boolean_t	my_run_time = run_time);
	int		rval;
	mstr		*src;
	oprtype		*cr, x;
	triple		*obp, *oldchain, *ref0, *ref1, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	switch (expr(&x, MUMPS_STR))
	{
	case EXPR_FAIL:
		setcurtchain(oldchain);
		return FALSE;
	case EXPR_INDR:
		if (TK_COLON != TREF(window_token))
		{
			make_commarg(&x, indir_xecute);
			break;
		}
		/* caution: fall through ??? maybe ??? */
	case EXPR_GOOD:
		if (! run_time && (OC_LIT == (ref0 = (TREF(curtchain))->exorder.bl)->opcode)
			&& (ref0->exorder.bl) == TREF(curtchain))
		{	/* just found a literal and only one and we are not already at run time; WARNING assignment above */
			/* Can't drive the parsing with the source because there may be emedded quotes, rather must use the literal
			 * The code in this block sorta/kinda does things like comp_init and op_commarg between a save and restore
			 * of parser state. If the parse fails or runs into a GOTO, NEW, QUIT or (nested) XECUTE it reverts to
			 * producing the code for a a run time evaluation of the literal. GOTO and QUIT must deal with the XECUTE
			 * frame and that's why they cause it to defer to run time; they use TREF(xecute_literal_parse), as does
			 * stc_error and BLOWN_FOR, to recognize this "trial" parse and prevent it from causing too much carnage
			 * Should anthing else call for a similar approach things might should be considered for refactoring and
			 * adjustments to some names.
			 */
			src = &x.oprval.tref->operand[0].oprval.mlit->v.str;
			dqinit(&tmpchain, exorder);
			if (MAX_SRCLINE < (unsigned)src->len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
			/* save the parse state, point the compiler at the literal and see what happens */
			if (NULL == parse_state_ptr)
				parse_state_ptr = malloc(SIZEOF(parse_save_block));
			SAVE_PARSE_STATE(parse_state_ptr);
			if (NULL == local_source_buffer)
				local_source_buffer = malloc(MAX_SRCLINE + 2);
			TREF(lexical_ptr) = (TREF(source_buffer)).addr = local_source_buffer;
			memcpy((TREF(source_buffer)).addr,src->addr,src->len);
			(TREF(source_buffer)).len = src->len + 1;
			*((TREF(source_buffer)).addr + src->len) = *((TREF(source_buffer)).addr + src->len + 1) = '\0';
			TREF(block_level) = 	source_column = 0;
			lb_init();
			assert(!TREF(xecute_literal_parse));
			run_time = TREF(xecute_literal_parse) = TRUE;
			for (;;)
			{
				if (EXPR_FAIL == (rval = (*indir_fcn[indir_linetail])()))
					break;
				if (TK_EOL == TREF(window_token))
					break;
				if (TK_COMMA == TREF(window_token))
					advancewindow();
				else
				{	/* Allow trailing spaces/comments that we will ignore */
					while (TK_SPACE == TREF(window_token))
						advancewindow();
				}
			}
			run_time = TREF(xecute_literal_parse) = FALSE;
			if (TK_EOL != TREF(window_token))
				rval = EXPR_FAIL;
			RESTORE_PARSE_STATE(parse_state_ptr);	/* restore the parse state to the original source */
			if (EXPR_FAIL == rval)
			{	/* not so good - remove the failed chain and just leave the literal for the run time form */
				dqinit(&tmpchain, exorder);
				ins_triple(ref0);
				pending_errtriplecode = 0;	/* forget the error - leave it to runtime */
			}
		} else
			rval = EXPR_FAIL;
		if (EXPR_FAIL == rval)
		{	/* either not a literal or compiling the literal failed, in which case leave it to run time */
			ref0 = maketriple(OC_COMMARG);
			ref0->operand[0] = x;
			ref0->operand[1] = put_ilit(indir_linetail);
			ins_triple(ref0);
		}
	}
	setcurtchain(oldchain);
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		cr = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cr))
			return FALSE;
		for (triptr = (TREF(curtchain))->exorder.bl; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
			;
		if (OC_LIT == triptr->opcode)
		{	/* it is a literal so optimize it */
			unuse_literal(&triptr->operand[0].oprval.mlit->v);
			dqdel(triptr, exorder);
			if (0 == triptr->operand[0].oprval.mlit->v.m[1])
				setcurtchain(oldchain);	/* it's FALSE so just discard the whole thing */
			else
			{	/* it's TRUE so treat as if no argument postconditional */
				obp = oldchain->exorder.bl;
				dqadd(obp, &tmpchain, exorder);		/* violates info hiding */
			}
			return TRUE;
		}
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);				/* violates info hiding */
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			ref0 = newtriple(OC_JMP);
			ref1 = newtriple(OC_GVRECTARG);
			ref1->operand[0] = put_tref(TREF(expr_start));
			*cr = put_tjmp(ref1);
			tnxtarg(&ref0->operand[0]);
		} else
			tnxtarg(cr);
		return TRUE;
	}
	obp = oldchain->exorder.bl;
	dqadd(obp, &tmpchain, exorder);					/* violates info hiding */
	assert(my_run_time == run_time);
	return TRUE;
}
