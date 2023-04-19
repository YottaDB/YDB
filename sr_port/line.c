/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "cmd_qlf.h"
#include "toktyp.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "show_source_line.h"
#include "start_fetches.h"
#include "error.h"
#include "gtm_string.h"

GBLREF command_qualifier	cmd_qlf;
GBLREF int			mlmax;
GBLREF mlabel			*mlabtab;
GBLREF mline			mline_root;
GBLREF mline			*mline_tail;
GBLREF triple			t_orig;		/* head of triples */
GBLREF boolean_t		cur_line_entry;	/* TRUE if control can reach this line in a -NOLINE_ENTRY compilation */

error_def(ERR_BLKTOODEEP);
error_def(ERR_COMMAORRPAREXP);
error_def(ERR_FALLINTOFLST);
error_def(ERR_LSEXPECTED);
error_def(ERR_MULTFORMPARM);
error_def(ERR_MULTLAB);
error_def(ERR_NAMEEXPECTED);
error_def(ERR_NESTFORMP);
error_def(ERR_TEXT);

boolean_t line(uint4 *lnc)
{
	boolean_t	success, embed_error = FALSE;
	int		parmcount, varnum;
	short int	dot_count;
	mlabel		*x;
	mline		*added_ret, *curlin;
	triple		*first_triple, *parmbase, *parmtail, *r, *e;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 == TREF(expr_depth));
	first_triple = (TREF(curtchain))->exorder.bl;
	dot_count = 0;
	parmbase = NULL;
	success = TRUE;
	curlin = (mline *)mcalloc(SIZEOF(*curlin));
	curlin->line_number = *lnc;
	*lnc = *lnc + 1;
	curlin->table = TRUE;
	curlin->block_ok = FALSE;
	(TREF(side_effect_base))[0] = FALSE;
	TREF(last_source_column) = 0;
	if (TK_INTLIT == TREF(window_token))
		int_label();
	if ((TK_IDENT == TREF(window_token)) || (cmd_qlf.qlf & CQ_LINE_ENTRY))
		START_FETCHES(OC_LINEFETCH);
	else
		newtriple(OC_LINESTART);
	CHKTCHAIN(TREF(curtchain), exorder, FALSE);
	assert(&t_orig == TREF(curtchain));
	TREF(pos_in_chain) = *(TREF(curtchain));
	if (TK_IDENT == TREF(window_token))
	{
		cur_line_entry = TRUE;	/* We are in a line that begins with a LABEL */
		x = get_mladdr(&(TREF(window_ident)));
		if (x->ml)
		{
			stx_error(ERR_MULTLAB);
			success = FALSE;
		} else
		{
			assert(NO_FORMALLIST == x->formalcnt);
			x->ml = curlin;
			advancewindow();
			if (TK_COLON != TREF(window_token))
				mlmax++;
			else
			{
				x->gbl = FALSE;
				advancewindow();
			}
		}
		if (success && (TK_LPAREN == TREF(window_token)))
		{
			advancewindow();
			parmbase = parmtail = newtriple(OC_BINDPARM);
			/* To error out on fall-throughs to labels with a formallist, we are inserting an error immediately before
			 * the LINESTART/LINEFETCH opcode. So, first we need to find the LINESTART/LINEFETCH preceding the
			 * BINDPARM we just inserted.
			 */
			assert((OC_LINESTART == parmbase->exorder.bl->opcode) || (OC_LINEFETCH == parmbase->exorder.bl->opcode));
			assert(0 != parmbase->exorder.bl->src.line);
			if ((mlabtab->rson != x) || TREF(code_generated))
			{	/* Don't insert an error before the first label of the routine. as no fallthrough possible */
				if (TREF(block_level))
				{	/* block_level means leaving embedded subroutine - assure OC_RET before inserted error */
					if ((OC_RET != first_triple->opcode)
						&& !(TREF(compile_time) && !(cmd_qlf.qlf & CQ_WARNINGS)))
					{	/* warn of added QUIT */
						show_source_line(TRUE);
						dec_err(VARLSTCNT(6) MAKE_MSG_WARNING(ERR_FALLINTOFLST), 0,
							ERR_TEXT, 2, RTS_ERROR_STRING("Adding implicit QUIT above"));
					}
					e = maketriple(OC_RET);	/* this needs an entry in line table to serve as jmp target */
					r = first_triple->exorder.fl;
					dqins(r, exorder, e);
					first_triple = r->exorder.fl;
					added_ret = (mline *)mcalloc(SIZEOF(*added_ret));
					added_ret->externalentry = first_triple;
					added_ret->table = FALSE;
					added_ret->line_number = *lnc;
					added_ret->block_ok = FALSE;
					added_ret->parent = &mline_root;
					added_ret->child = added_ret->sibling = NULL;
					mline_tail = added_ret;
				}
				e = maketriple(OC_RTERROR);
				e->operand[0] = put_ilit(ERR_FALLINTOFLST);
				e->operand[1] = put_ilit(FALSE);	/* Not a subroutine/func reference. */
				r = parmbase->exorder.bl->exorder.bl;
				dqins(r, exorder, e);
				embed_error = TRUE;
			}
			for (parmcount = 0; TK_RPAREN != TREF(window_token); parmcount++)
			{
				if (TK_IDENT != TREF(window_token))
				{
					stx_error(ERR_NAMEEXPECTED);
					success = FALSE;
					break;
				} else
				{
					varnum = get_mvaddr(&(TREF(window_ident)))->mvidx;
					for (r = parmbase->operand[1].oprval.tref; r; r = r->operand[1].oprval.tref)
					{
						assert(TRIP_REF == r->operand[0].oprclass);
						assert(ILIT_REF == r->operand[0].oprval.tref->operand[0].oprclass);
						assert((TRIP_REF == r->operand[1].oprclass)
							|| (NO_REF == r->operand[1].oprclass));
						if (r->operand[0].oprval.tref->operand[0].oprval.ilit == varnum)
						{
							stx_error(ERR_MULTFORMPARM);
							success = FALSE;
							break;
						}
					}
					if (!success)
						break;
					r = newtriple(OC_PARAMETER);
					parmtail->operand[1] = put_tref(r);
					r->operand[0] = put_ilit(varnum);
					parmtail = r;
					advancewindow();
				}
				if (TK_COMMA == TREF(window_token))
					advancewindow();
				else if (TK_RPAREN != TREF(window_token))
				{
					stx_error(ERR_COMMAORRPAREXP);
					success = FALSE;
					break;
				}
			}
			if (success)
			{
				advancewindow();
				parmbase->operand[0] = put_ilit(parmcount);
				x->formalcnt = parmcount;
				assert(!mlabtab->lson);
				if ((mlabtab->rson == x) && !TREF(code_generated))
					mlabtab->formalcnt = parmcount;
			}
		}
	} else if (1 == curlin->line_number)
		cur_line_entry = TRUE;	/* First line in M file. Line entry possible even with -NOLINE_ENTRY. */
	if (success && (TK_EOL != TREF(window_token)))
	{
		if (TK_SPACE != TREF(window_token))
		{
			stx_error(ERR_LSEXPECTED);
			success = FALSE;
		} else
		{
			assert(0 == dot_count);
			for (;;)
			{
				if (TK_SPACE == TREF(window_token))
					advancewindow();
				else if (TK_PERIOD == TREF(window_token))
				{
					dot_count++;
					advancewindow();
				} else
					break;
			}
		}
		if ((NULL != parmbase) && dot_count)
		{
			dot_count = TREF(block_level);
			stx_error(ERR_NESTFORMP);	/* Should be warning */
			success = FALSE;
		}
		if ((TREF(block_level) + (int4)(mline_tail->block_ok)) < dot_count)
		{
			dot_count = TREF(block_level);
			assert(TREF(compile_time));
			if (cmd_qlf.qlf & CQ_WARNINGS)
			{
				show_source_line(TRUE);
				dec_err(VARLSTCNT(1) ERR_BLKTOODEEP);
			}
			TREF(source_error_found) = ERR_BLKTOODEEP;
			success = FALSE;
		}
	}
	if ((TREF(block_level) < dot_count) || (mline_tail == &mline_root))
	{
		mline_tail->child = curlin;
		curlin->parent = mline_tail;
		TREF(block_level) = dot_count;
	} else
	{
		for (; dot_count < TREF(block_level); (TREF(block_level))--)
			if (NULL != mline_tail->parent)
				mline_tail = mline_tail->parent;
		mline_tail->sibling = curlin;
		curlin->parent = mline_tail->parent;
	}
	mline_tail = curlin;
	if (success)
	{
		assert(TREF(for_stack_ptr) == TADR(for_stack));
		*(TREF(for_stack_ptr)) = NULL;
		success = linetail();
		if (success)
		{
			assert(TREF(for_stack_ptr) == TADR(for_stack));
			if (*(TREF(for_stack_ptr)))
				tnxtarg(*(TREF(for_stack_ptr)));
		}
	}
	assert(TREF(for_stack_ptr) == TADR(for_stack));
	if (first_triple->exorder.fl == TREF(curtchain))
		newtriple(OC_NOOP);			/* empty line (comment, blank, etc) */
	if (embed_error)
	{	/* The entry point to the label should be LINESTART/LINEFETCH, not the RTERROR. */
		curlin->externalentry = e->exorder.fl;
		TREF(code_generated) = TRUE;
	} else
	{
		curlin->externalentry = first_triple->exorder.fl;
		/* First_triple points to the last triple before this line was processed. Its forward link will point to a
		 * LINEFETCH or a LINESTART, or possibly a NOOP. If the line was a comment, there is only a LINESTART, and
		 * hence no "real" code yet.
		 */
		TREF(code_generated) = TREF(code_generated) | ((OC_NOOP != first_triple->exorder.fl->opcode)
			&& (first_triple->exorder.fl->exorder.fl != TREF(curtchain)));
	}
	return success;
}
