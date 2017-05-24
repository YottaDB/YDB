/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
#include "mdq.h"
#include "mmemory.h"
#include "fullbool.h"
#include "stringpool.h"

LITREF mval		literal_zero, literal_one;
LITREF octabstruct	oc_tab[];

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

void bx_tail(triple *t, boolean_t sense, oprtype *addr)
/*
 * work a Boolean expression along to final form
 * triple	*t;	triple to be processed
 * boolean_t	sense;	code to be generated is jmpt or jmpf
 * oprtype	*addr;	address to jmp
 */
{
	boolean_t	tv;
	opctype		c;
	oprtype		*p;
	triple		*ref, *ref1, *ref2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (OC_LIT == t->opcode)
	{	/* bx_boollit/ex_tail left literal operand, but this invocation indicates we need a JMP form, so adjust and leave */
		t->opcode = ((0 != t->operand[0].oprval.mlit->v.m[1]) ^ sense) ? OC_JMPTRUE : OC_JMPFALSE;
		assert(INDR_REF == t->operand[1].oprclass);
		t->operand[0] = t->operand[1];
		return;
	}
	assert((1 & sense) == sense);
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	if ((OC_COBOOL != (c = t->opcode)) && (OC_COM != c))				/* WARNING assignment */
		bx_boollit(t, sense, addr);
	if ((OC_JMPTRUE != (c = t->opcode)) && (OC_JMPFALSE != c))			/* WARNING assignment */
	{
		if (OCT_NEGATED & oc_tab[t->opcode].octype)
			sense ^= 1;
		switch (c)
		{
		case OC_COBOOL:
			ex_tail(&t->operand[0]);
			if (OC_GETTRUTH == t->operand[0].oprval.tref->opcode)
			{
				dqdel(t->operand[0].oprval.tref, exorder);
				t->opcode = sense ? OC_JMPTSET : OC_JMPTCLR;
				t->operand[0] = put_indr(addr);
				return;
			}
			ref = maketriple(sense ? OC_JMPNEQ : OC_JMPEQU);
			ref->operand[0] = put_indr(addr);
			dqins(t, exorder, ref);
			return;
		case OC_COM:
			bx_tail(t->operand[0].oprval.tref, !sense, addr);		/* WARNING assignment below */
			if ((OC_JMPTRUE == (ref = t->operand[0].oprval.tref)->opcode) || (OC_JMPFALSE == ref->opcode))
			{	/* maybe operand or target, so slide over, rather than dqdel the com */
				t->opcode = ref->opcode;
				t->operand[0] = ref->operand[0];
				t->operand[1] = ref->operand[1];
				dqdel(ref, exorder);
			} else
			{	/* even if we are just done with it, NOOP rather than dqdel for reason above */
				t->opcode = OC_NOOP;
				t->operand[0].oprclass = NO_REF;
			}
			return;
		case OC_NEQU:
		case OC_EQU:
			bx_relop(t, OC_EQU, sense ? OC_JMPNEQ : OC_JMPEQU, addr);
			break;
		case OC_NPATTERN:
		case OC_PATTERN:
			bx_relop(t, OC_PATTERN, sense ? OC_JMPNEQ : OC_JMPEQU, addr);
			break;
		case OC_NFOLLOW:
		case OC_FOLLOW:
			bx_relop(t, OC_FOLLOW, sense ? OC_JMPGTR : OC_JMPLEQ, addr);
			break;
		case OC_NSORTS_AFTER:
		case OC_SORTS_AFTER:
			bx_relop(t, OC_SORTS_AFTER, sense ? OC_JMPGTR : OC_JMPLEQ, addr);
			break;
		case OC_NCONTAIN:
		case OC_CONTAIN:
			bx_relop(t, OC_CONTAIN, sense ? OC_JMPNEQ : OC_JMPEQU, addr);
			break;
		case OC_NGT:
		case OC_GT:
			bx_relop(t, OC_NUMCMP, sense ? OC_JMPGTR : OC_JMPLEQ, addr);
			break;
		case OC_NLT:
		case OC_LT:
			bx_relop(t, OC_NUMCMP, sense ? OC_JMPLSS : OC_JMPGEQ, addr);
			break;
		case OC_NAND:
		case OC_AND:
			bx_boolop(t, FALSE, sense, sense, addr);
			break;
		case OC_NOR:
		case OC_OR:
			bx_boolop(t, TRUE, !sense, sense, addr);
			break;
		case OC_NUMCMP:
			break;								/* already processed by bx_boollit */
		default:
			assertpro(t->opcode && FALSE);
		}
	}
	if (OCT_REL & oc_tab[c].octype)
	{
		for (p = t->operand ; p < ARRAYTOP(t->operand); p++)
			if (TRIP_REF == p->oprclass)
				ex_tail(p);
	}
	return;
}
