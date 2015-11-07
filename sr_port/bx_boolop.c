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
#include "cmd_qlf.h"
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include <emit_code.h>
#include "fullbool.h"

LITREF		octabstruct	oc_tab[];

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;

#define STOTEMP_IF_NEEDED(REF0, I, T1, OPND)											\
{	/* Input:														\
	 * --- REF0:	a boolean triple, which may have either 1 input (OC_COBOOL) or 2 (other opcodes).			\
	 * --- I:	whichever operand of REF0 we are STOTEMPing								\
	 * --- T1:	STOTEMP triple. NOOPed if not needed									\
	 * --- OPND:	operand referring to value we need need to pass as input into boolean operation				\
	 * If OPND refers to a variable (OC_VAR), we need to STOTEMP it to protect it from subsequent side effects.		\
	 * If it refers to a literal, and dynamic literals are enabled, we need to insert an OC_LITC anyway. Doing it		\
	 * here in bx_boolop is convenient and ensures the OC_LITC is not skipped at run time.					\
	 */															\
	assert(TRIP_REF == OPND.oprclass);											\
	switch (OPND.oprval.tref->opcode)											\
	{															\
		case OC_VAR:													\
			T1->opcode = OC_STOTEMP;										\
			T1->operand[0] = OPND;											\
			REF0->operand[I] = put_tref(T1);									\
			break;													\
		case OC_LIT:													\
			if (!run_time && (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))							\
			{													\
				T1->opcode = OC_LITC;										\
				T1->operand[0] = OPND;										\
				REF0->operand[I] = put_tref(T1);								\
				break;												\
			}													\
		default:													\
			T1->opcode = OC_NOOP;											\
			T1->operand[0].oprclass = NO_REF;									\
			REF0->operand[I] = put_tref(OPND.oprval.tref);								\
	}															\
}

void bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr)
{
	boolean_t	expr_fini;
	oprtype		*adj_addr, *i, *p;
	tbp		*tripbp;
	triple		*ref0, *ref1, *ref2, *t0, *t1;
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
	if (!TREF(saw_side_effect) || ((OLD_SE == TREF(side_effect_handling)) && (GTM_BOOL == TREF(gtm_fullbool))))
	{	/* nice simple short circuit */
		assert(NULL == TREF(boolchain_ptr));
		bx_tail(t->operand[0].oprval.tref, jmp_type_one, p);
		bx_tail(t->operand[1].oprval.tref, sense, addr);
		t->opcode = OC_NOOP;
		t->operand[0].oprclass = t->operand[1].oprclass = NO_REF;
		return;
	}
	/* got a side effect and don't want them short circuited */
	/* This code violates info hiding big-time and relies on the original technique of setting up a jump ladder
	 * then it changes the jumps into stotemps and creates a new ladder using the saved evaluations
	 * for the relocated jumps to use for controlling conditional transfers, When the stotemps reference mvals,
	 * they are optimized away when possible. The most interesting part is getting the addresses for the new jump
	 * operands (targets) - see comment below. In theory we could turn this technique on and off around each side effect,
	 * but that's even more complicated, requiring additional instructions, and we don't predict the typical boolean
	 * expression has enough subexpressions to justify the extra trouble, although the potential pay-back would be to
	 * avoid unnecessary global references - again, not expecting that many in a typical boolean expresion.
	 */
	assert(TREF(shift_side_effects));
	if (expr_fini = (NULL == TREF(boolchain_ptr)))				/* NOTE assignment */
	{									/* initialize work on boolean section of the AST */
		TREF(boolchain_ptr) = &(TREF(boolchain));
		dqinit(TREF(boolchain_ptr), exorder);
		t0 = t->exorder.fl;
		if (NULL == TREF(bool_targ_ptr))
		{								/* first time - set up anchor */
			TREF(bool_targ_ptr) = &(TREF(bool_targ_anchor));	/* mcalloc won't persist over multiple complies */
			dqinit(TREF(bool_targ_ptr), que);
		} else								/* queue should be empty */
			assert((TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.fl)
				&& (TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.bl));
		/* ex_tail wraps bools that produce a value with OC_BOOLINIT (clr) and OC_BOOLFINI (set) */
		assert((OC_BOOLFINI != t0->opcode)
			|| ((OC_COMVAL == t0->exorder.fl->opcode) && (TRIP_REF == t0->operand[0].oprclass)));
	}
	for (i = t->operand; i < ARRAYTOP(t->operand); i++)
	{
		assert(NULL != TREF(boolchain_ptr));
		t1 = i->oprval.tref;
		if (&(t->operand[0]) == i)
			bx_tail(t1, jmp_type_one, p);				/* do normal transform */
		else
		{	/* operand[1] */
			bx_tail(t1, sense, addr);				/* do normal transform */
			if (!expr_fini)
				break;						/* only need to relocate last operand[1] */
		}
		if (OC_NOOP == t1->opcode)
		{	/* the technique of sprinkling noops means fishing around for the actual instruction */
			do
			{
				t1 = t1->exorder.bl;
				assert(TREF(curtchain) != t1->exorder.bl);
			} while (OC_NOOP == t1->opcode);
			if ((oc_tab[t1->opcode].octype & OCT_JUMP) && (OC_JMPTSET != t1->opcode) && (OC_JMPTCLR != t1->opcode))
				t1 = t1->exorder.bl;
			if (OC_NOOP == t1->opcode)
			{
				for (t1 = i->oprval.tref; OC_NOOP == t1->opcode; t1 = t1->exorder.fl)
					assert(TREF(curtchain) != t1->exorder.fl);
			}
		}
		assert(OC_NOOP != t1->opcode);
		assert((oc_tab[t1->exorder.fl->opcode].octype & OCT_JUMP)
			||(OC_JMPTSET != t1->exorder.fl->opcode) || (OC_JMPTCLR != t1->exorder.fl->opcode));
		ref0 = maketriple(t1->opcode);					/* copy operation for place in new ladder */
		ref1 = (TREF(boolchain_ptr))->exorder.bl;			/* common setup for above op insert */
		switch (t1->opcode)
		{								/* time to subvert original jump ladder entry */
			case OC_COBOOL:
				/* insert COBOOL and copy of following JMP in boolchain; overlay them with STOTEMP and NOOP  */
				assert(TRIP_REF == t1->operand[0].oprclass);
				dqins(ref1, exorder, ref0);
				if (oc_tab[t1->operand[0].oprval.tref->opcode].octype & OCT_MVAL)
				{						/* do we need a STOTEMP? */
					STOTEMP_IF_NEEDED(ref0, 0, t1, t1->operand[0]);
				} else
				{						/* make it an mval instead of COBOOL now */
					t1->opcode = OC_COMVAL;
					ref0->operand[0] = put_tref(t1);	/* new COBOOL points to this OC_COMVAL  */
				}
				t1 = t1->exorder.fl;
				ref0 = maketriple(t1->opcode);			/* create new jmp on result of coerce */
				ref0->operand[0] = t1->operand[0];
				t1->opcode = OC_NOOP;				/* wipe out original jmp */
				t1->operand[0].oprclass = NO_REF;
				break;
			case OC_CONTAIN:
			case OC_EQU:
			case OC_FOLLOW:
			case OC_NUMCMP:
			case OC_PATTERN:
			case OC_SORTS_AFTER:
				/* insert copies of orig OC and following JMP in boolchain & overly originals with STOTEMPs */
				assert(TRIP_REF == t1->operand[0].oprclass);
				assert(TRIP_REF == t1->operand[1].oprclass);
				dqins(ref1, exorder, ref0);
				STOTEMP_IF_NEEDED(ref0, 0, t1, t1->operand[0]);
				ref1 = t1;
				t1 = t1->exorder.fl;
				ref2 = maketriple(t1->opcode);			/* copy jmp */
				ref2->operand[0] = t1->operand[0];
				STOTEMP_IF_NEEDED(ref0, 1, t1, ref1->operand[1]);
				if (OC_NOOP == ref1->opcode)			/* does op[0] need cleanup? */
					ref1->operand[0].oprclass = ref1->operand[1].oprclass = NO_REF;
				ref0 = ref2;
				break;
			case OC_JMPTSET:
			case OC_JMPTCLR:
										/* move copy of jmp to boolchain and NOOP it */
				ref0->operand[0] = t1->operand[0];		/* new jmpt gets old target */
				ref2 = maketriple(OC_NOOP);			/* insert a NOOP in new chain inplace of COBOOL */
				dqins(ref1, exorder, ref2);
				t1->opcode = OC_NOOP;				/* wipe out original jmp */
				t1->operand[0].oprclass = NO_REF;
				break;
			default:
				assertpro(FALSE);
		}
		assert((OC_STOTEMP == t1->opcode) || (OC_NOOP == t1->opcode) || (OC_COMVAL == t1->opcode)
			  || (OC_LITC == t1->opcode));
		assert(oc_tab[ref0->opcode].octype & OCT_JUMP);
		ref1 = (TREF(boolchain_ptr))->exorder.bl;
		dqins(ref1, exorder, ref0);					/* common insert for new jmp */
	}
	assert(oc_tab[t->opcode].octype & OCT_BOOL);
	t->opcode = OC_NOOP;							/* wipe out the original boolean op */
	t->operand[0].oprclass = t->operand[1].oprclass = NO_REF;
	tripbp = &t->jmplist;							/* borrow jmplist to track jmp targets */
	assert(NULL == tripbp->bpt);
	assert((tripbp == tripbp->que.fl) && (tripbp == tripbp->que.bl));
	tripbp->bpt = jmp_to_next ? (TREF(boolchain_ptr))->exorder.bl : ref0;	/* point op triple at op[1] position or op[0] */
	dqins(TREF(bool_targ_ptr), que, tripbp);				/* queue jmplist for clean-up */
	if (!expr_fini)
		return;
	/* time to deal with new jump ladder */
	assert(NULL != TREF(boolchain_ptr));
	assert(NULL != TREF(bool_targ_ptr));
	assert(TREF(bool_targ_ptr) != (TREF(bool_targ_ptr))->que.fl);
	assert(t0->exorder.bl == t);
	assert(t0 == t->exorder.fl);
	dqadd(t, TREF(boolchain_ptr), exorder);					/* insert the new jump ladder */
	ref0 = (TREF(boolchain_ptr))->exorder.bl->exorder.fl;
	t0 = t->exorder.fl;
	if (ref0 == TREF(curtchain))
	{									/* add a safe target */
		newtriple(OC_NOOP);
		ref0 = (TREF(curtchain))->exorder.bl;
	}
	assert((OC_COBOOL == t0->opcode) ||(OC_JMPTSET != t0->opcode) || (OC_JMPTCLR != t0->opcode)) ;
	t0 = t0->exorder.fl;
	assert(oc_tab[t0->opcode].octype & OCT_JUMP);
	for (; (t0 != ref0) && oc_tab[t0->opcode].octype & OCT_JUMP; t0 = t0->exorder.fl)
	{									/* process replacement jmps */
		adj_addr = &t0->operand[0];
		assert(INDR_REF == adj_addr->oprclass);
		if (NULL != (t1 = (adj_addr = adj_addr->oprval.indr)->oprval.tref))
		{								/*  need to adjust target; NOTE assignments above */
			if (OC_BOOLFINI != t1->opcode)
			{							/* not past the end of the new chain */
				assert(TJMP_REF == adj_addr->oprclass);
				if ((t == t1) || (t1 == ref0))
					ref1 = ref0;				/* adjust to end of boolean expression */
				else
				{						/* old target should have jmplist entry */
					/* from the jmp jmplisted in the old target we move past the next
					 * test (or NOOP) and jmp which correspond to the old target and pick
					 * the subsequent test (or NOOP) and jmp which correspond to those that originally followed
					 * the logic after the old target and are therefore the appropriate new target for this jmp
					 */
					assert(OC_NOOP == t1->opcode);
					assert(&(t1->jmplist) != t1->jmplist.que.fl);
					assert(NULL != t1->jmplist.bpt);
					assert(oc_tab[t1->jmplist.bpt->opcode].octype & OCT_JUMP);
					ref1 = t1->jmplist.bpt->exorder.fl;
					assert((oc_tab[ref1->opcode].octype & OCT_BOOL) || (OC_NOOP == ref1->opcode));
					assert(oc_tab[ref1->exorder.fl->opcode].octype & OCT_JUMP);
					ref1 = ref1->exorder.fl->exorder.fl;
					assert((oc_tab[ref1->opcode].octype & OCT_BOOL) || (OC_BOOLFINI == ref1->opcode)
						|| ((OC_NOOP == ref1->opcode) && ((OC_JMPTCLR == ref1->exorder.fl->opcode)
						|| (OC_JMPTSET == ref1->exorder.fl->opcode)
						|| (TREF(curtchain) == ref1->exorder.fl))));
				}
				t0->operand[0] = put_tjmp(ref1);		/* no indrection simplifies later interations */
			}
		}
		t0 = t0->exorder.fl;
		if ((OC_BOOLFINI == t0->opcode) || (TREF(curtchain) == t0->exorder.fl))
			break;
		assert((oc_tab[t0->opcode].octype & OCT_BOOL)
			|| (OC_JMPTSET == t0->exorder.fl->opcode) || (OC_JMPTCLR == t0->exorder.fl->opcode));
	}
	dqloop(TREF(bool_targ_ptr), que, tripbp)				/* clean up borrowed jmplist entries */
	{
		dqdel(tripbp, que);
		tripbp->bpt = NULL;
	}
	assert((TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.fl)
		&& (TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.bl));
	TREF(boolchain_ptr) = NULL;
	if (TREF(expr_start) != TREF(expr_start_orig))
	{									/* inocculate against an unwanted GVRECTARG */
		ref0 = maketriple(OC_NOOP);
		dqins(TREF(expr_start), exorder, ref0);
		TREF(expr_start) = ref0;
	}
	return;
}
