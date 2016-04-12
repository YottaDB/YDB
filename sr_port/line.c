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
#include "cmd_qlf.h"
#include "toktyp.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"

GBLREF short int		source_line;
GBLREF int			mlmax;
GBLREF mline			*mline_tail;
GBLREF short int		block_level;
GBLREF mlabel			*mlabtab;
GBLREF command_qualifier	cmd_qlf;

error_def(ERR_BLKTOODEEP);
error_def(ERR_COMMAORRPAREXP);
error_def(ERR_FALLINTOFLST);
error_def(ERR_LSEXPECTED);
error_def(ERR_MULTFORMPARM);
error_def(ERR_MULTLAB);
error_def(ERR_NAMEEXPECTED);
error_def(ERR_NESTFORMP);

boolean_t line(uint4 *lnc)
{
	boolean_t	success, embed_error = FALSE;
	int		parmcount, varnum;
	short int	dot_count;
	mlabel		*x;
	mline		*curlin;
	triple		*first_triple, *parmbase, *parmtail, *r, *e;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	first_triple = (TREF(curtchain))->exorder.bl;
	dot_count = 0;
	parmbase = NULL;
	success = TRUE;
	curlin = (mline *)mcalloc(SIZEOF(*curlin));
	curlin->line_number = 0;
	curlin->table = FALSE;
	assert(0 == TREF(expr_depth));
	(TREF(side_effect_base))[0] = FALSE;
	TREF(last_source_column) = 0;
	if (TK_INTLIT == TREF(window_token))
		int_label();
	if ((TK_IDENT == TREF(window_token)) || (cmd_qlf.qlf & CQ_LINE_ENTRY))
		start_fetches(OC_LINEFETCH);
	else
		newtriple(OC_LINESTART);
	curlin->line_number = *lnc;
	*lnc = *lnc + 1;
	curlin->table = TRUE;
	CHKTCHAIN(TREF(curtchain));
	TREF(pos_in_chain) = *(TREF(curtchain));
	if (TK_IDENT == TREF(window_token))
	{
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
			/* No error should be inserted before the first label of the routine. */
			if ((mlabtab->rson != x) || TREF(code_generated))
			{
				e = maketriple(OC_RTERROR);
				e->operand[0] = put_ilit(ERR_FALLINTOFLST);
				/* Not a subroutine/func reference. */
				e->operand[1] = put_ilit(FALSE);
				r = parmbase->exorder.bl->exorder.bl;
				dqins(r, exorder, e);
				embed_error = TRUE;
			}
			if (success)
			{
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
	}
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
		if ((block_level + 1) < dot_count)
		{
			dot_count = (block_level > 0) ? block_level : 0;
			stx_error(ERR_BLKTOODEEP);
			success = FALSE;
		}
	}
	if ((0 != parmbase) && (0 != dot_count))
	{
		stx_error(ERR_NESTFORMP);	/* Should be warning */
		success = FALSE;
		dot_count = (block_level > 0 ? block_level : 0);
	}
	if ((block_level + 1) <= dot_count)
	{
		mline_tail->child = curlin;
		curlin->parent = mline_tail;
		block_level = dot_count;
	} else
	{
		for (; dot_count < block_level; block_level--)
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
