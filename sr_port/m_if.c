/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
{
	triple		*ref0, *ref1, *ref2, *jmpref, ifpos_in_chain, *triptr;
	oprtype		x, y, *ta_opr;
	boolean_t	first_time, t_set, is_commarg;
	jmpchn		*jmpchain, *nxtjmp;
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
			if (((OC_JMPNEQ == (ref0 = (TREF(curtchain))->exorder.bl)->opcode))
				&& (OC_COBOOL == (ref1 = ref0->exorder.bl)->opcode)
				&& (OC_INDGLVN == (ref2 = ref1->exorder.bl)->opcode))
			{	/* short-circuit only optimization that turns a trailing INDGLVN COBOOL into separate indirect IF */
				dqdel(ref0, exorder);
				ref1->opcode = OC_JMPTSET;
				ref1->operand[0] = put_indr(ta_opr);
				ref2->opcode = OC_COMMARG;
				ref2->operand[1] = put_ilit((mint)indir_if);
			}
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
		TREF(pos_in_chain) = ifpos_in_chain;
		return FALSE;
	}
	return TRUE;
}
