/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2024 YottaDB LLC and/or its subsidiaries.	*
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
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include "fullbool.h"

LITREF octabstruct oc_tab[];

/*	structure of jmps is as follows:
 *
 *	sense		OC_AND		OC_OR
 *
 *	TRUE		op1		op1
 *			jmpf next	jmpt addr
 *			op2		op2
 *			jmpt addr	jmpt addr
 *
 *	FALSE		op1		op1
 *			jmpf addr	jmpt next
 *			op2		op2
 *			jmpf addr	jmpf addr
 **/

void bx_tail(triple *t, boolean_t sense, oprtype *addr, int depth, opctype andor_opcode,
						boolean_t caller_is_bool_expr, int jmp_depth, boolean_t is_last_bool_operand)
/*
 * work a Boolean expression along to final form
 * triple	*t;     triple to be processed
 * boolean_t	sense;  code to be generated is jmpt or jmpf
<<<<<<< HEAD
 * oprtype	*addr;  address to jmp
 * int		depth;	boolean expression recursion depth
=======
 * oprtype		*addr;  address to jmp
 * All called functions are leaf functions except for the (s)boolop ones, which call this recursively only
 * to construct unified chains for directly nested pure bools.
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
 */
{
	opctype		c;
	triple		*ref;
	opctype		jmp_opcode, new_andor_opcode;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((1 & sense) == sense);
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	if (OCT_NEGATED & oc_tab[c = t->opcode].octype)
		sense = !sense;
	if (EXT_BOOL == TREF(gtm_fullbool))
	{
		switch (c)
		{
		case OC_AND:
		case OC_NAND:
		case OC_OR:
		case OC_NOR:
			CONVERT_TO_SE(t);
			c = t->opcode;
			break;
		default:
			break;
		}
	}
	switch (c)
	{
	case OC_COBOOL:
<<<<<<< HEAD
		ex_tail(&t->operand[0], depth);
		RETURN_IF_RTS_ERROR;
		jmp_opcode = sense ? OC_JMPNEQ : OC_JMPEQU;
		ref = maketriple(jmp_opcode);
=======
		if (OC_GETTRUTH == t->operand[0].oprval.tref->opcode)
		{
			assert(NO_REF == t->operand[0].oprval.tref->operand[0].oprclass);
			t->operand[0].oprval.tref->opcode = OC_NOOP;	/* must NOOP rather than delete as might be expr_start */
			t->opcode = sense ? OC_JMPTSET : OC_JMPTCLR;
			t->operand[0] = put_indr(addr);
			return;
		}
		ref = maketriple(sense ? OC_JMPNEQ : OC_JMPEQU);
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		ref->operand[0] = put_indr(addr);
		dqins(t, exorder, ref);
		ADD_BOOL_ZYSQLNULL_PARMS(t, depth, jmp_opcode, andor_opcode, caller_is_bool_expr, is_last_bool_operand, jmp_depth);
		return;
	case OC_SCOBOOL:
		/* Unlike S(N)AND and S(N)OR, COBOOLS are typically left in the execution chain by CG time.
		 * Therefore this pseudo-op MUST be reverted before it arrives there. It is necessary to protect $TEST from
		 * side effects - the original logic assumes the locality of GETTRUTH and its cobool. Conversion to scobool
		 * must only ever take place when there is a guarantee that this function will be called on the converted triple.
		 */
		ref = maketriple(sense ? OC_JMPNEQ : OC_JMPEQU);
		ref->operand[0] = put_indr(addr);
		dqins(t, exorder, ref);
		t->opcode = OC_COBOOL;
		return;
	case OC_COM:
<<<<<<< HEAD
		ref = t->operand[0].oprval.tref;
		new_andor_opcode = bx_get_andor_opcode(ref->opcode, andor_opcode);
		bx_tail(ref, !sense, addr, depth, new_andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		RETURN_IF_RTS_ERROR;
=======
		bx_tail(t->operand[0].oprval.tref, !sense, addr);
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		t->opcode = OC_NOOP;				/* maybe operand or target, so noop, rather than dqdel the com */
		t->operand[0].oprclass = NO_REF;
		return;
	case OC_NEQU:
	case OC_EQU:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NPATTERN:
	case OC_PATTERN:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NFOLLOW:
	case OC_FOLLOW:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NSORTSAFTER:
	case OC_SORTSAFTER:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NCONTAIN:
	case OC_CONTAIN:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NGT:
	case OC_GT:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NLT:
	case OC_LT:
		bx_relop(t, c, sense, addr, depth, andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
		break;
	case OC_NAND:
	case OC_AND:
		bx_boolop(t, FALSE, sense, sense, addr, depth + 1, andor_opcode, caller_is_bool_expr,
										jmp_depth, is_last_bool_operand);
		RETURN_IF_RTS_ERROR;
		break;
	case OC_SNAND:
	case OC_SAND:
		bx_sboolop(t, FALSE, sense, sense, addr);
		RETURN_IF_RTS_ERROR;
		break;
	case OC_NOR:
	case OC_OR:
		bx_boolop(t, TRUE, !sense, sense, addr, depth + 1, andor_opcode, caller_is_bool_expr,
										jmp_depth, is_last_bool_operand);
		RETURN_IF_RTS_ERROR;
		break;
	case OC_SNOR:
	case OC_SOR:
		bx_sboolop(t, TRUE, !sense, sense, addr);
		RETURN_IF_RTS_ERROR;
		break;
	default:
		assertpro(FALSE && t);
	}
	return;
}
