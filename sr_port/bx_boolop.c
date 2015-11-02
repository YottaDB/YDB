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
#include "mdq.h"
#include "mmemory.h"
#include "emit_code.h"
#include "fullbool.h"

LITREF		octabstruct	oc_tab[];

void bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr)
{
	boolean_t	expr_fini;
	oprtype		*i, *p;
	triple		*ref0, *ref1, *t0, *t1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(((1 & sense) == sense) && ((1 & jmp_to_next) == jmp_to_next) && ((1 & jmp_type_one) == jmp_type_one));
	assert((TRIP_REF == t->operand[0].oprclass) && (TRIP_REF == t->operand[1].oprclass));
	if (jmp_to_next)
	{
		p = (oprtype *)mcalloc(SIZEOF(oprtype));
		*p = put_tjmp(t);
	} else
		p = addr;
	if (GTM_BOOL == TREF(gtm_fullbool) || !TREF(saw_side_effect))
	{	/* nice simple short circuit */
		assert(NULL == TREF(boolchain_ptr));
		bx_tail(t->operand[0].oprval.tref, jmp_type_one, p);
		bx_tail(t->operand[1].oprval.tref, sense, addr);
	} else
	{	/* got a side effect and don't want them short circuited - this violates info hiding big-time
		 * This code relies on the original technique of setting up a jump ladder
		 * then it changes the jumps into stotemps and creates a new ladder using the saved evaluations
		 * for the relocated jumps to work with
		 * The most interesting part is getting the addresses for the new jump operands (targets)
		 * In theory we could turn this technique on and off around each side effect, but that's even more
		 * complicated, requiring additional instructions, and we don't predict the typical boolean expression
		 * has enough subexpressions to justify the extra trouble, although the potential pay-back would be to
		 * avoid unnecessary global references - again not expecting that many in a typical boolean expresion
		 */
		assert(TREF(shift_side_effects));
		t0 = t->exorder.fl;
		if (expr_fini = (NULL == TREF(boolchain_ptr)))			/* NOTE assignment */
		{
			if (OC_BOOLFINI == t0->opcode)
			{	/* ex_tail wraps bools that produce a value with OC_BOOLINIT and OC_BOOLFINI */
				assert(OC_COMVAL == t0->exorder.fl->opcode);
				assert(TRIP_REF == t0->operand[0].oprclass);
			} else
				assert(((OC_NOOP == t0->opcode) && (t0 == TREF(curtchain)))
					|| (oc_tab[t0->opcode].octype & OCT_BOOL));
			TREF(boolchain_ptr) = &(TREF(boolchain));
			dqinit(TREF(boolchain_ptr), exorder);
		}
		for (i = t->operand; i < ARRAYTOP(t->operand); i++)
		{
			t1 = i->oprval.tref;
			if (&(t->operand[0]) == i)
				bx_tail(t1, jmp_type_one, p);
			else
			{	/* operand[1] */
				bx_tail(t1, sense, addr);
				if (!expr_fini)
					break;					/* only need to relocate last operand[1] */
			}
			if (OC_NOOP == t1->opcode)
			{	/* the technique of sprinkling noops means fishing around for the actual instruction */
				do
				{
					t1 = t1->exorder.bl;
				} while (OC_NOOP == t1->opcode);
				if (oc_tab[t1->opcode].octype & OCT_JUMP)
					t1 = t1->exorder.bl;
				else
				{
					for (t1 = i->oprval.tref; OC_NOOP == t1->opcode; t1 = t1->exorder.fl)
						;
				}
			}
			assert(NULL != TREF(boolchain_ptr));
			switch (t1->opcode)
			{							/* time to subvert the original jump ladder entry */
			case OC_COBOOL:
				/* insert COBOOL and copy of following JMP in boolchain; overlay them with STOTEMP and NOOP  */
				assert(oc_tab[t1->exorder.fl->opcode].octype & OCT_JUMP);
				ref0 = maketriple(OC_COBOOL);			/* coerce later while pulling it out of temp */
				ref0->operand[0] = put_tref(t1);
				ref1 = (TREF(boolchain_ptr))->exorder.bl;
				dqins(ref1, exorder, ref0);
				t1->opcode = OC_STOTEMP;			/* save the value instead of coercing now */
				t1 = t1->exorder.fl;
				ref0 = maketriple(t1->opcode);			/* create new jump on result of coerce */
				ref0->operand[0] = t1->operand[0];
				t1->operand[0].oprclass = NOCLASS;
				t1->opcode = OC_NOOP;				/* wipe out original jump */
				break;
			case OC_CONTAIN:
			case OC_EQU:
			case OC_FOLLOW:
			case OC_NUMCMP:
			case OC_PATTERN:
			case OC_SORTS_AFTER:
				/* insert copies of orig OC and following JMP in boolchain & overly originals with STOTEMPs */
				assert(oc_tab[t1->exorder.fl->opcode].octype & OCT_JUMP);
				assert(TRIP_REF == t1->operand[0].oprclass);
				assert(TRIP_REF == t1->operand[1].oprclass);
				ref0 = maketriple(t1->opcode);			/* copy operands with the stotemps as args */
				ref0->operand[0] = put_tref(t1);
				ref0->operand[1] = put_tref(t1->exorder.fl);
				ref1 = (TREF(boolchain_ptr))->exorder.bl;
				dqins(ref1, exorder, ref0);
				t1->opcode = OC_STOTEMP;			/* overlay the original op with 1st stotemp */
				t1 = t1->exorder.fl;
				ref0 = maketriple(t1->opcode);			/* copy jmp */
				ref0->operand[0] = t1->operand[0];
				t1->operand[0] = t1->exorder.bl->operand[1];
				t1->opcode = OC_STOTEMP;			/* overlay jmp with 2nd stotemp */
				break;
			case OC_JMPTSET:
			case OC_JMPTCLR:
				/* move copy of jmp to boolchain and NOOP it */
				ref0 = maketriple(t1->opcode);
				ref0->operand[0] = t1->operand[0];
				t1->operand[0].oprclass = NOCLASS;
				t1->opcode = OC_NOOP;				/* wipe out original jump */
				break;
			default:
				GTMASSERT;
			}
			if (jmp_to_next)					/* mark target for later adjustment */
					ref0->operand[1].oprval.tref = ref0->operand[0].oprval.tref;
			ref1 = (TREF(boolchain_ptr))->exorder.bl;
			dqins(ref1, exorder, ref0);
		}
		if (expr_fini)
		{	/* time to deal with new jump ladder */
			assert(NULL != TREF(boolchain_ptr));
			t0 = t0->exorder.bl;
			assert(oc_tab[t0->opcode].octype & OCT_BOOL);
			assert(t0 == t);
			dqadd(t0, TREF(boolchain_ptr), exorder);			/* insert the new jump ladder */
			ref0 = (TREF(boolchain_ptr))->exorder.bl->exorder.fl;
			if (ref0 == TREF(curtchain))
			{
				newtriple(OC_NOOP);
				ref0 = (TREF(curtchain))->exorder.bl;
			}
			assert(ref0);
			t0 = t->exorder.fl;
			if ((OC_JMPTSET != t0->opcode) && (OC_JMPTCLR != t0->opcode))
				t0 = t0->exorder.fl;
			for (; (t0 != TREF(curtchain)) && oc_tab[t0->opcode].octype & OCT_JUMP; t0 = t1)
			{	/* check for jumps with targets */
				assert(INDR_REF == t0->operand[0].oprclass);
				t1 = t0->exorder.fl;
				if (oc_tab[t1->opcode].octype & OCT_BOOL)
					t1 = ref1 = t1->exorder.fl;
				else
				{
					if ((OC_JMPTSET == t1->opcode) || (OC_JMPTCLR == t1->opcode))
						ref1 = t1;
					else
						break;
				}
				if (t0->operand[1].oprval.tref == t0->operand[0].oprval.tref)
				{	/* adjust relocated jump to "next" */
					if (oc_tab[ref1->opcode].octype & OCT_JUMP)
						ref1 = ref1->exorder.fl;
					if ((ref1 == TREF(curtchain)
						|| (t == t0->operand[0].oprval.tref->exorder.fl)))
							ref1 = ref0;
					assert((OC_NOOP == ref1->opcode) || (OC_BOOLFINI == ref1->opcode)
						|| (OC_COMVAL == ref1->opcode) || (oc_tab[ref1->opcode].octype & OCT_BOOL));
					t0->operand[0] = put_tjmp(ref1);
					t0->operand[1].oprval.tref = NULL;
				} else if (TJMP_REF == t0->operand[0].oprval.indr->oprclass)
					t0->operand[0] = put_tjmp(ref0);	/* adjust jump to "addr" */
			}
			TREF(boolchain_ptr) = NULL;
		}
	}
	t->opcode = OC_NOOP;
	t->operand[0].oprclass = t->operand[1].oprclass = NOCLASS;
	return;
}
