/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
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
#include "mdq.h"
#include "opcode.h"
#include "fullbool.h"

LITREF octabstruct	oc_tab[];

void bx_relop(triple *t, opctype cmp, boolean_t sense, oprtype *addr, int depth, opctype andor_opcode,
			boolean_t caller_is_bool_expr, int jmp_depth, boolean_t is_last_bool_operand)
/* work Boolean relational arguments
 * *t points to the Boolean operation
 * cmp and jmp_opcode give (respectively) the opcode and the associated jump
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	oprtype		*p;
	opctype		jmp_opcode;
	tbp		*tripbp;
	triple		*bini, *comv, *ref, *ref0, ref1, *parm1, *parm2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(t->opcode)
	{
	case OC_NEQU:
	case OC_EQU:
	case OC_NPATTERN:
	case OC_PATTERN:
	case OC_NCONTAIN:
	case OC_CONTAIN:
		jmp_opcode = sense ? OC_JMPNEQ : OC_JMPEQU;
		break;
	case OC_NFOLLOW:
	case OC_FOLLOW:
	case OC_NSORTS_AFTER:
	case OC_SORTS_AFTER:
	case OC_NGT:
	case OC_GT:
		jmp_opcode = sense ? OC_JMPGTR : OC_JMPLEQ;
		break;
	case OC_NLT:
	case OC_LT:
		jmp_opcode = sense ? OC_JMPLSS : OC_JMPGEQ;
		break;
	default:
		assert(FALSE);
		break;
	}
	ref = maketriple(jmp_opcode);
	ref->operand[0] = put_indr(addr);
	dqins(t, exorder, ref);
	/* `t` already has 2 parameters. But we need to add a few more parameters. Make way for that. */
	assert(NO_REF != t->operand[0].oprclass);
	assert(NO_REF != t->operand[1].oprclass);
	parm1 = maketriple(OC_PARAMETER);
	parm1->operand[0] = t->operand[1];
	t->operand[1] = put_tref(parm1);
	parm2 = maketriple(OC_PARAMETER);
	parm1->operand[1] = put_tref(parm2);
	parm2->operand[0] = make_ilit((mint)cmp);
	ADD_BOOL_ZYSQLNULL_PARMS(parm2, depth, jmp_opcode, andor_opcode, caller_is_bool_expr, is_last_bool_operand, jmp_depth);
	t->opcode = OC_BXRELOP;
	for (p = t->operand ; p < ARRAYTOP(t->operand); )
	{	/* Some day investigate whether this is still needed */
		if (TRIP_REF == p->oprclass)
		{
			ref = p->oprval.tref;
			if (OC_PARAMETER == ref->opcode)
			{
				t = ref;
				p = t->operand;
				continue;
			}
			CHECK_AND_RETURN_IF_BOOLEXPRTOODEEP(depth + 1);
			ex_tail(p, depth + 1);
			RETURN_IF_RTS_ERROR;
			if ((NULL != TREF(boolchain_ptr)) && (OC_COMVAL == (comv = p->oprval.tref)->opcode))
			{	/* RELOP needed a BOOLINIT/BOOLFINI/COMVAL, which now must move to the boolchain */
				assert(TREF(saw_side_effect) && (YDB_BOOL != TREF(ydb_fullbool)));
				assert(OC_BOOLINIT == comv->operand[0].oprval.tref->opcode);
				assert(OC_BOOLFINI == comv->exorder.bl->opcode);
				assert(comv->operand[0].oprval.tref == comv->exorder.bl->operand[0].oprval.tref);
				for (ref = (bini = comv->operand[0].oprval.tref)->exorder.fl; ref != comv; ref = ref->exorder.fl)
				{	/* process to matching end of sequence so to conform with addr adjust at end of bx_boolop */
					if (!(OCT_JUMP & oc_tab[ref->opcode].octype))
						TRACK_JMP_TARGET(ref, ref);
				}
				TRACK_JMP_TARGET(comv, comv);
				dqdelchain(bini->exorder.bl, comv->exorder.fl, exorder);	/* snip out the sequence */
				ref0 = &ref1;							/* anchor it */
				ref0->exorder.fl = bini;
				ref0->exorder.bl = comv;
				ref = (TREF(boolchain_ptr))->exorder.bl;
				dqadd(ref, ref0, exorder);					/* and insert it in new chain */
			}
		}
		p++;
	}
	return;
}
