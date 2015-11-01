/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLDEF triple pos_in_chain;

GBLREF char window_token;
GBLREF mident window_ident;
GBLREF short int source_line;
GBLREF oprtype *for_stack[],**for_stack_ptr;
GBLREF short int last_source_column;
GBLREF triple *curtchain;
GBLREF int mlmax;
GBLREF mline *mline_tail;
GBLREF short int block_level;
GBLREF mlabel *mlabtab;
GBLREF bool code_generated;
GBLREF command_qualifier  	cmd_qlf;

int line(uint4 *lnc)
{
	mlabel *x;
	triple *first_triple, *parmbase, *parmtail, *r;
	int parmcount, varnum;
	mline *curlin;
	short int dot_count;
	bool success;
	error_def(ERR_MULTLAB);
	error_def(ERR_LSEXPECTED);
	error_def(ERR_COMMAORRPARENEXP);
	error_def(ERR_MULTFORMPARM);
	error_def(ERR_NAMEEXPECTED);
	error_def(ERR_BLKTOODEEP);
	error_def(ERR_NESTFORMP);

	first_triple = curtchain->exorder.bl;
	parmbase = 0;
	dot_count = 0;
	success = TRUE;
	curlin = (mline *) mcalloc(sizeof(*curlin));
	curlin->line_number = 0;
	curlin->table = FALSE;
	last_source_column = 0;
	if (window_token == TK_INTLIT)
		int_label();

	if (window_token == TK_IDENT || (cmd_qlf.qlf & CQ_LINE_ENTRY))
		start_fetches(OC_LINEFETCH);
	else
		newtriple(OC_LINESTART);

	curlin->line_number = *lnc;
	*lnc = *lnc + 1;
	curlin->table = TRUE;
	pos_in_chain = *curtchain;

	if (window_token == TK_IDENT)
	{
		x = get_mladdr(&window_ident);
		if (x->ml)
		{
			stx_error(ERR_MULTLAB);
			success = FALSE;
		}
		else
		{
			assert(x->formalcnt == NO_FORMALLIST);
			x->ml = curlin;
			advancewindow();
			if (window_token != TK_COLON)
				mlmax++;
			else
			{	x->gbl = FALSE;
				advancewindow();
			}
		}
		if (success && window_token == TK_LPAREN)
		{
			r = maketriple (OC_ISFORMAL);
			dqins (curtchain->exorder.bl->exorder.bl, exorder, r);
			advancewindow();
			parmbase = parmtail = newtriple(OC_BINDPARM);
			for (parmcount = 0 ; window_token != TK_RPAREN ; parmcount++)
			{
				if (window_token != TK_IDENT)
				{
					stx_error (ERR_NAMEEXPECTED);
					success = FALSE;
					break;
				}
				else
				{
					varnum = get_mvaddr(&window_ident)->mvidx;
					for (r = parmbase->operand[1].oprval.tref ; r ; r = r->operand[1].oprval.tref)
					{
						assert(r->operand[0].oprclass == TRIP_REF);
						assert(r->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
						assert(r->operand[1].oprclass == TRIP_REF ||
							r->operand[1].oprclass == 0);
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
				if (window_token == TK_COMMA)
					advancewindow();
				else if (window_token != TK_RPAREN)
				{
					stx_error(ERR_COMMAORRPARENEXP);
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
				if (mlabtab->rson == x && !code_generated)
					mlabtab->formalcnt = parmcount;
			}
		}
	}
	if (window_token != TK_EOL && success)
	{
		if (window_token != TK_SPACE)
		{
			stx_error(ERR_LSEXPECTED);
			success = FALSE;
		} else
		{
			assert(dot_count == 0);
			for (;;)
			{
				if (window_token == TK_SPACE)
					advancewindow();
				else if (window_token == TK_PERIOD)
				{
					dot_count++;
					advancewindow();
				} else
					break;
			}
		}
		if (dot_count > block_level + 1)
		{
			dot_count = (block_level > 0 ? block_level : 0);
			stx_error(ERR_BLKTOODEEP);
			success = FALSE;
		}
	}
	if (parmbase != 0 && dot_count != 0)
	{
		stx_error(ERR_NESTFORMP);	/* Should be warning */
		success = FALSE;
		dot_count = (block_level > 0 ? block_level : 0);
	}
	if (dot_count >= block_level + 1)
	{
		mline_tail->child = curlin;
		curlin->parent = mline_tail;
		block_level = dot_count;
	} else
	{
		for ( ; dot_count < block_level ; block_level--)
			mline_tail = mline_tail->parent;
		mline_tail->sibling = curlin;
		curlin->parent = mline_tail->parent;
	}
	mline_tail = curlin;
	if (success)
	{
		for_stack_ptr = for_stack;
		*for_stack_ptr = 0;
		success = linetail();
		if (success)
		{	assert(for_stack_ptr == for_stack);
			if (*for_stack_ptr)
			{	tnxtarg(*for_stack_ptr);
			}
		}
	}
	if (first_triple->exorder.fl == curtchain)
		newtriple(OC_NOOP);			/* empty line (comment, blank, etc) */
	curlin->externalentry = first_triple->exorder.fl;

	/* first_triple points to the last triple before this line was processed.  Its forward
	   link will point to a LINEFETCH or a LINESTART, or possibly a NOOP.  It the line was a comment, there is
	   only a LINESTART, and hence no "real" code yet */

	code_generated = code_generated | (first_triple->exorder.fl->opcode != OC_NOOP &&
		first_triple->exorder.fl->exorder.fl != curtchain);
	return success;
}
