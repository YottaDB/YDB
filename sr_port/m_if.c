/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF char window_token;
GBLREF short int last_source_column;
GBLREF triple *curtchain, pos_in_chain, *expr_start, *expr_start_orig;

typedef struct jmpchntype
{
	struct
	{
		struct jmpchntype *fl,*bl;
	} link;
	triple  *jmptrip;
} jmpchn;

int m_if(void)
{
	triple		*ref0, *ref1, *ref2, *jmpref, ifpos_in_chain, *triptr;
	oprtype		x, y, *ta_opr;
	boolean_t	first_time, t_set, is_commarg;
	jmpchn		*jmpchain, *nxtjmp;

	error_def(ERR_SPOREOL);
	error_def(ERR_INDEXTRACHARS);

	ifpos_in_chain = pos_in_chain;
	jmpchain = (jmpchn*)mcalloc(sizeof(jmpchn));
	dqinit(jmpchain,link);
	if (TK_EOL == window_token)
		return TRUE;
	is_commarg = (last_source_column == 1);
	x = for_end_of_scope(0);
	assert(INDR_REF == x.oprclass);
	if (TK_SPACE == window_token)
	{
		jmpref = newtriple(OC_JMPTCLR);
		jmpref->operand[0] = x;
		nxtjmp = (jmpchn *)mcalloc(sizeof(jmpchn));
		nxtjmp->jmptrip = jmpref;
		dqins(jmpchain,link,nxtjmp);
	} else
	{
		first_time = TRUE;
		for (;;)
		{
			ta_opr = (oprtype *)mcalloc(sizeof(oprtype));
			if (!bool_expr((bool)TRUE, ta_opr))
				return FALSE;
			if ((ref0 = curtchain->exorder.bl)->opcode == OC_JMPNEQ
				&& (ref1 = ref0->exorder.bl)->opcode == OC_COBOOL
				&& (ref2 = ref1->exorder.bl)->opcode == OC_INDGLVN)
			{
				dqdel(ref0,exorder);
				ref1->opcode = OC_JMPTSET;
				ref1->operand[0] = put_indr(ta_opr);
				ref2->opcode = OC_COMMARG;
				ref2->operand[1] = put_ilit((mint)indir_if);
			}
			t_set = (OC_JMPTSET == curtchain->exorder.bl->opcode);
			if (!t_set)
				newtriple(OC_CLRTEST);
			if (expr_start != expr_start_orig)
			{
                		triptr = newtriple(OC_GVRECTARG);
				triptr->operand[0] = put_tref(expr_start);
			}
			jmpref = newtriple(OC_JMP);
			jmpref->operand[0] = x;
			nxtjmp = (jmpchn *)mcalloc(sizeof(jmpchn));
			nxtjmp->jmptrip = jmpref;
			dqins(jmpchain,link,nxtjmp);
			tnxtarg(ta_opr);
			if (first_time)
			{
				if (!t_set)
					newtriple(OC_SETTEST);
				if (expr_start != expr_start_orig)
				{
					triptr = newtriple(OC_GVRECTARG);
					triptr->operand[0] = put_tref(expr_start);
				}
				first_time = FALSE;
			}
			if (TK_COMMA != window_token)
				break;
			advancewindow();
		}
	}
	if (is_commarg)
	{
		while(TK_SPACE == window_token)		/* Eat up trailing white space */
			advancewindow();
		if (TK_EOL != window_token)
		{
			stx_error(ERR_INDEXTRACHARS);
			return FALSE;
		}
		return TRUE;
	}
	if (TK_EOL != window_token && TK_SPACE != window_token)
	{
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
		pos_in_chain = ifpos_in_chain;
		return FALSE;
	}
	return TRUE;
}
