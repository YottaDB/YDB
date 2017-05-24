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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"
#include "fullbool.h"

GBLREF	int4		pending_errtriplecode;

error_def(ERR_INDEXTRACHARS);
error_def(ERR_SPOREOL);

typedef struct jmpchntype
{
	struct
	{
		struct jmpchntype *fl,*bl;
	} link;
	triple  *jmptrip;
} jmpchn;

int m_if(void)
/* compiler module for IF */
{
	boolean_t	first_time, is_commarg, t_set;
	int		sense;
	jmpchn		*jmpchain, *nxtjmp;
	oprtype		*ta_opr, x, y;
	triple		ifpos_in_chain, *jmpref, *ref1, *ref2, *oldchain, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ifpos_in_chain = TREF(pos_in_chain);
	jmpchain = (jmpchn *)mcalloc(SIZEOF(jmpchn));
	dqinit(jmpchain, link);
	if (TK_EOL == TREF(window_token))
		return TRUE;
	is_commarg = (1 == TREF(last_source_column));
	FOR_END_OF_SCOPE(0, x);
	assert(INDR_REF == x.oprclass);
	oldchain = NULL;
	if (TK_SPACE == TREF(window_token))
	{
		jmpref = newtriple(OC_JMPTCLR);
		jmpref->operand[0] = x;
		nxtjmp = (jmpchn *)mcalloc(SIZEOF(jmpchn));
		nxtjmp->jmptrip = jmpref;
		dqins(jmpchain, link, nxtjmp);
	} else
	{
		first_time = TRUE;
		for (;;)
		{
			ta_opr = (oprtype *)mcalloc(SIZEOF(oprtype));
			if (!bool_expr(TRUE, ta_opr))
				return FALSE;
			for (triptr = (TREF(curtchain))->exorder.bl; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
				;
			if ((OC_JMPNEQ == triptr->opcode)					/* WARNING: assignments */
				&& (OC_COBOOL == (ref1 = triptr->exorder.bl)->opcode)
				&& (OC_INDGLVN == (ref2 = ref1->exorder.bl)->opcode))
			{	/* short-circuit only optimization that turns a trailing INDGLVN COBOOL into separate indirect IF */
				triptr->opcode = OC_JMPTSET;
				triptr->operand[0] = put_indr(ta_opr);
				dqdel(ref1, exorder);;
				ref2->opcode = OC_COMMARG;
				ref2->operand[1] = put_ilit((mint)indir_if);
			}
			if (OC_LIT == triptr->opcode)
			{	/* it is a literal so we optimize it */
				dqdel(triptr, exorder);
				unuse_literal(&triptr->operand[0].oprval.mlit->v);
				if (t_set = (0 == triptr->operand[0].oprval.mlit->v.m[1]))	/* WARNING: assignment */
				{	/* it's FALSE, insert clear of $TEST */
					newtriple(OC_CLRTEST);
					if (TK_SPACE == TREF(director_token))			/* if there are trailing spaces */
						while (TK_SPACE == TREF(director_token))	/* eat them up */
							advancewindow();
					if (TK_EOL == TREF(director_token))
						break;						/* line empty: no discard needed */
					if (NULL == oldchain)
					{	/* not already discarding, so get ready to discard the rest of the line */
						dqinit(&tmpchain, exorder);
						oldchain = setcurtchain(&tmpchain);
					}
				} else
				{	/* it's TRUE so insert set of $TEST and step beyond the argument */
					newtriple(OC_SETTEST);
					if (TK_COMMA != TREF(window_token))
						break;
					if (TK_EOL == TREF(window_token))
						return TRUE;
					advancewindow();
					continue;	/* leave first_time in case next arg is also a literal */
				}
			} else
				t_set = (OC_JMPTSET == (TREF(curtchain))->exorder.bl->opcode);
			if (!t_set)
				newtriple(OC_CLRTEST);
			if (TREF(expr_start) != TREF(expr_start_orig) && (OC_NOOP != (TREF(expr_start))->opcode))
			{
				assert((OC_GVSAVTARG == (TREF(expr_start))->opcode));
				if ((OC_GVRECTARG != (TREF(curtchain))->exorder.bl->opcode)
					|| ((TREF(curtchain))->exorder.bl->operand[0].oprval.tref != TREF(expr_start)))
						newtriple(OC_GVRECTARG)->operand[0] = put_tref(TREF(expr_start));
			}
			jmpref = newtriple(OC_JMP);
			jmpref->operand[0] = x;
			nxtjmp = (jmpchn *)mcalloc(SIZEOF(jmpchn));
			nxtjmp->jmptrip = jmpref;
			dqins(jmpchain, link, nxtjmp);
			tnxtarg(ta_opr);
			if (first_time)
			{
				if (!t_set)
					newtriple(OC_SETTEST);
				if (TREF(expr_start) != TREF(expr_start_orig) && (OC_NOOP != (TREF(expr_start))->opcode))
				{
					assert((OC_GVSAVTARG == (TREF(expr_start))->opcode));
					if ((OC_GVRECTARG != (TREF(curtchain))->exorder.bl->opcode)
						|| ((TREF(curtchain))->exorder.bl->operand[0].oprval.tref != TREF(expr_start)))
							newtriple(OC_GVRECTARG)->operand[0] = put_tref(TREF(expr_start));
				}
				first_time = FALSE;
			}
			if (TK_COMMA != TREF(window_token))
				break;
			advancewindow();
		}
	}
	if (is_commarg)
	{
		while (TK_SPACE == TREF(window_token))		/* Eat up trailing white space */
			advancewindow();
		if (TK_EOL != TREF(window_token))
		{
			stx_error(ERR_INDEXTRACHARS);
			return FALSE;
		}
		return TRUE;
	}
	if ((TK_EOL != TREF(window_token)) && (TK_SPACE != TREF(window_token)))
	{
		if (NULL != oldchain)
			setcurtchain(oldchain);			/* reset from discard chain */
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	if (!linetail())
	{	tnxtarg(&x);
		dqloop(jmpchain,link,nxtjmp)
		{
			ref1 = nxtjmp->jmptrip;
			ref1->operand[0] = x;
		}
		if (NULL != oldchain)
		{	/* for a literal 0 postconditional, we just throw the command & args away along with any pending error */
			pending_errtriplecode = 0;
			setcurtchain(oldchain);			/* reset from discard chain */
		}
		TREF(pos_in_chain) = ifpos_in_chain;
		return FALSE;
	}
	if (NULL != oldchain)
		setcurtchain(oldchain);				/* reset from discard chain */
	return TRUE;
}
