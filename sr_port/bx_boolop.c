/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
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
#include "gtm_string.h"
#include "cmd_qlf.h"
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include <emit_code.h>
#include "fullbool.h"
#include "stringpool.h"

LITREF octabstruct	oc_tab[];

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;

#define STOTEMP_IF_NEEDED(REF0, I, T1, OPND)											\
MBSTART {	/* Input:													\
		 * --- REF0:	a boolean triple, which may have either 1 input (OC_COBOOL) or 2 (other opcodes)		\
		 * --- I:	whichever operand of REF0 we are STOTEMPing							\
		 * --- T1:	STOTEMP triple. NOOPed if not needed								\
		 * --- OPND:	operand referring to value we need need to pass as input into boolean operation			\
		 * If OPND refers to a variable (OC_VAR), we need to STOTEMP it to protect it from subsequent side effects.	\
		 * If it refers to a literal, and dynamic literals are enabled, we need to insert an OC_LITC anyway. Doing it	\
		 * here in bx_boolop is convenient and ensures the OC_LITC is not skipped at run time.				\
		 */														\
	assert(TRIP_REF == OPND.oprclass);											\
	switch (OPND.oprval.tref->opcode)											\
	{															\
		case OC_VAR:													\
			T1->opcode = OC_STOTEMP;										\
			T1->operand[0] = OPND;											\
			/* Clear operand[1] in case it was set for previous opcode */						\
			T1->operand[1].oprclass = NO_REF;									\
			REF0->operand[I] = put_tref(T1);									\
			break;													\
		case OC_LIT:													\
			if (!run_time && (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))							\
			{													\
				T1->opcode = OC_LITC;										\
				T1->operand[0] = OPND;										\
				/* Clear operand[1] in case it was set for previous opcode */					\
				T1->operand[1].oprclass = NO_REF;								\
				REF0->operand[I] = put_tref(T1);								\
				break;												\
			}													\
		default:													\
			T1->opcode = OC_NOOP;											\
			T1->operand[0].oprclass = NO_REF;									\
			/* Since opcode has been reset to OC_NOOP, no need to clear operand[1] in this case */			\
			REF0->operand[I] = put_tref(OPND.oprval.tref);								\
			break;													\
	}															\
} MBEND

void bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr,
		int depth, opctype andor_opcode, boolean_t caller_is_bool_expr, int jmp_depth, boolean_t is_last_bool_operand)
/* process the operations into a chain of mostly conditional jumps
 * *t points to the Boolean operation
 * jmp_type_one gives the sense of the jump associated with the first operand
 * jmp_to_next gives whether we need a second jump to complete the operation
 * sense gives the sense of the requested operation
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 * depth is the boolean expression recursion depth
 * andor_opcode tells us whether we are inside an AND or OR
 */
{
	boolean_t	expr_fini;
	opctype		new_andor_opcode;
	oprtype		*adj_addr, *i, *p;
	tbp		*tripbp;
	triple		*bfini, *binit, *ref0, *ref1, *ref2, *ref0_next, *ref_parms, *t0, *t1, *tb, *tj;
	triple		*leftmost[NUM_TRIPLE_OPERANDS];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	CHECK_AND_RETURN_IF_BOOLEXPRTOODEEP(depth);
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(((1 & sense) == sense) && ((1 & jmp_to_next) == jmp_to_next) && ((1 & jmp_type_one) == jmp_type_one));
	assert((TRIP_REF == t->operand[0].oprclass) && (TRIP_REF == t->operand[1].oprclass));
	if (jmp_to_next)
	{
		p = (oprtype *)mcalloc(SIZEOF(oprtype));
		*p = put_tjmp(t);
	} else
		p = addr;
	if (!TREF(saw_side_effect) || (YDB_BOOL == TREF(ydb_fullbool)))
	{	/* nice simple short circuit */
		assert(NULL == TREF(boolchain_ptr));
		leftmost[0] = bool_return_leftmost_triple(t->operand[0].oprval.tref);
		leftmost[1] = t->operand[0].oprval.tref->exorder.fl;
		new_andor_opcode = bx_get_andor_opcode(t->operand[0].oprval.tref->opcode, andor_opcode);
		bx_tail(t->operand[0].oprval.tref, jmp_type_one, p, depth, new_andor_opcode,
								caller_is_bool_expr, jmp_depth, IS_LAST_BOOL_OPERAND_FALSE);
		RETURN_IF_RTS_ERROR;
		new_andor_opcode = bx_get_andor_opcode(t->operand[1].oprval.tref->opcode, andor_opcode);
		bx_tail(t->operand[1].oprval.tref, sense, addr, depth, new_andor_opcode, caller_is_bool_expr,
											jmp_depth, is_last_bool_operand);
		RETURN_IF_RTS_ERROR;
		bx_insert_oc_andor(andor_opcode, depth, leftmost); /* insert OC_ANDOR triples before each OC_AND/OC_OR operand */
		t->opcode = OC_NOOP;	/* must not delete as this can be a jmp target */
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
		TREF(bool_targ_ptr) = &(TREF(bool_targ_anchor));		/* mcalloc won't persist over multiple compiles */
		dqinit(TREF(bool_targ_ptr), que);
	}
	/* Check if OC_BOOLFINI follows `t`. Note that there could be one or more number of OC_COM triples in between.
	 * In that case, we need to skip them to check for OC_BOOLFINI.
	 */
	for (bfini = t->exorder.fl; ; bfini = bfini->exorder.fl)
	{
		if (OC_BOOLFINI == bfini->opcode)
			break;
		if (OC_COM != bfini->opcode)
		{
			bfini = NULL;
			break;
		}
	}
	if (NULL != bfini)
	{	/* ex_tail wraps bools that produce a value with OC_BOOLINIT (clr) and OC_BOOLFINI (set) followed by an OC_COMVAL */
		assert((TRIP_REF == bfini->operand[0].oprclass) && (OC_BOOLINIT ==  bfini->operand[0].oprval.tref->opcode));
		assert(NO_REF == bfini->operand[0].oprval.tref->operand[0].oprclass);
		bfini->operand[0].oprval.tref->opcode = OC_NOOP;
		binit = maketriple(OC_BOOLINIT);
		DEBUG_ONLY(binit->src = bfini->operand[0].oprval.tref->src);
		ref1 = (TREF(boolchain_ptr))->exorder.bl;
		dqins(ref1, exorder, binit);
	}
	for (i = t->operand; i < ARRAYTOP(t->operand); i++)
	{
		assert(NULL != TREF(boolchain_ptr));
		tb = i->oprval.tref;
		new_andor_opcode = bx_get_andor_opcode(tb->opcode, andor_opcode);
		if (&(t->operand[0]) == i)
		{
			leftmost[0] = (TREF(boolchain_ptr))->exorder.bl;
			bx_tail(tb, jmp_type_one, p, depth, new_andor_opcode,
							caller_is_bool_expr, jmp_depth, IS_LAST_BOOL_OPERAND_FALSE);
						/* do normal transform */
			RETURN_IF_RTS_ERROR;
		} else
		{	/* operand[1] */
			leftmost[1] = (TREF(boolchain_ptr))->exorder.bl;
			bx_tail(tb, sense, addr, depth, new_andor_opcode, caller_is_bool_expr, jmp_depth, is_last_bool_operand);
						/* do normal transform */
			RETURN_IF_RTS_ERROR;
		}
		if (OC_NOOP == tb->opcode)
		{	/* the technique of sprinkling noops means fishing around for the actual instruction */
			do
			{
				tb = tb->exorder.bl;
				assert(TREF(curtchain) != tb->exorder.bl);
			} while (OC_NOOP == tb->opcode);
			if (OCT_JUMP & oc_tab[tb->opcode].octype)
			{
				switch (tb->opcode)
				{
				case OC_JMP:
					break;
				default:
					tb = tb->exorder.bl;
					if (OC_NOOP == tb->opcode)
					{
						for (tb = i->oprval.tref; OC_NOOP == tb->opcode; tb = tb->exorder.fl)
							assert(TREF(curtchain) != tb->exorder.fl);
					}
					break;
				}
			}
		}
		assert(OC_NOOP != tb->opcode);
		ref0 = maketriple(tb->opcode);					/* copy operation to place in new ladder */
		DEBUG_ONLY(ref0->src = tb->src);
		ref1 = (TREF(boolchain_ptr))->exorder.bl;			/* common setup for coming copy of this op */
		switch (tb->opcode)
		{								/* time to subvert original jump ladder entry */
			case OC_COBOOL:
				/* insert COBOOL and copy of following JMP in boolchain; overlay them with STOTEMP and NOOP  */
				assert(TRIP_REF == tb->operand[0].oprclass);
				dqins(ref1, exorder, ref0);
				ref0->operand[1] = tb->operand[1];
				for (t1 = tb->operand[0].oprval.tref; (OCT_UNARY & oc_tab[t1->opcode].octype); )
				{	/* leading unary operator no longer needed; leaving would cause problems */
					assert(TRIP_REF == t1->operand[0].oprclass);
					t1->opcode = OC_PASSTHRU;
					t1 = t1->operand[0].oprval.tref;
				}
				if (OCT_MVAL & oc_tab[t1->opcode].octype)
				{						/* do we need a STOTEMP? */
					STOTEMP_IF_NEEDED(ref0, 0, tb, tb->operand[0]);
				} else
				{						/* make it an mval instead of COBOOL now */
					tb->opcode = OC_COMVAL;
					/* OC_COBOOL has 3 parameters whereas OC_COMVAL has 2 parameters. Both of them have
					 * the same first parameter. So no need to change tb->operand[0]. But the 2nd parameter
					 * needs to be set in OC_COMVAL. Hence the set of tb->operand[1] below.
					 */
					tb->operand[1] = make_ilit((mint)INIT_GBL_BOOL_DEPTH);
					ref0->operand[0] = put_tref(tb);	/* new COBOOL points to this OC_COMVAL  */
				}
				t1 = tb->exorder.fl;
				assert(OCT_JUMP & oc_tab[t1->opcode].octype);
				tj = maketriple(t1->opcode);			/* create JMP in boolchain on result of coerce */
				DEBUG_ONLY(tj->src = t1->src);
				tj->operand[0] = t1->operand[0];
				t1->opcode = OC_NOOP;				/* wipe out original JMP */
				t1->operand[0].oprclass = NO_REF;
				break;
			case OC_BXRELOP:
				/* insert copies of orig OC and following JMP in boolchain & overlay originals with STOTEMPs */
				assert(TRIP_REF == tb->operand[0].oprclass);
				assert(TRIP_REF == tb->operand[1].oprclass);
				dqins(ref1, exorder, ref0);
				/* Copy tb->operand[1] over before STOTEMP_IF_NEEDED call as macro can clear tb->operand[1] */
				ref_parms = tb->operand[1].oprval.tref;
				assert(OC_PARAMETER == ref_parms->opcode);
				ref0->operand[1] = tb->operand[1];
				/* Now that operand[1] has been copied over, call STOTEMP_IF_NEEDED */
				STOTEMP_IF_NEEDED(ref0, 0, tb, tb->operand[0]);
				t1 = tb->exorder.fl;
				assert(OCT_JUMP & oc_tab[t1->opcode].octype);
				tj = maketriple(t1->opcode);			/* copy JMP */
				DEBUG_ONLY(tj->src = t1->src);
				tj->operand[0] = t1->operand[0];
				STOTEMP_IF_NEEDED(ref_parms, 0, t1, ref_parms->operand[0]);
				if (OC_NOOP == tb->opcode)			/* does operand[0] need cleanup? */
					tb->operand[0].oprclass = tb->operand[1].oprclass = NO_REF;
				break;
			default:
				tj = NULL;					/* toss in indicator as flag for code below */
				break;
		}
		if (NULL != tj)
		{
			TRACK_JMP_TARGET(tb, ref0);
			assert((OC_STOTEMP == t1->opcode) || (OC_NOOP == t1->opcode) || (OC_COMVAL == t1->opcode)
				|| (OC_LITC == t1->opcode) || (OC_GETTRUTH == t1->opcode));
			assert(OCT_JUMP & oc_tab[tj->opcode].octype);
			ref1 = (TREF(boolchain_ptr))->exorder.bl;
			dqins(ref1, exorder, tj);				/* common insert for new jmp */
			ref0_next = tj;
		}
		if (&(t->operand[0]) == i)
		{
			if (NULL != tj)
			{
				leftmost[0] = bool_return_leftmost_triple(ref0);
				leftmost[1] = ref0_next;
			} else
			{	/* leftmost[0] and/or leftmost[1] are already set before `bx_tail()` invocation.
				 * There is just one case that needs to be handled. And that is if the boolchain
				 * changed (i.e. triples got added in nested `bx_tail()` invocations) since we
				 * noted down leftmost[0]. If so position ourself past one triple in the chain.
				 */
				if (leftmost[0] != (TREF(boolchain_ptr))->exorder.bl)
					leftmost[0] = leftmost[0]->exorder.fl;
			}
		} else
			leftmost[1] = leftmost[1]->exorder.fl;
	}
	bx_insert_oc_andor(andor_opcode, depth, leftmost); /* insert OC_ANDOR triples before each OC_AND/OC_OR operand */
	TRACK_JMP_TARGET(t, (TREF(boolchain_ptr))->exorder.bl);			/* track the operator as well as the operands */
	if (NULL != bfini)
	{	/* if OC_BOOLINIT/OC_BOOLFINI pair, move them to the new chain */
		assert((NULL != binit) && (OC_BOOLFINI == bfini->opcode) && (OC_BOOLINIT == binit->opcode));
		ref0 = bfini->exorder.fl;			/* get a pointer to the OC_COMVAL/OC_COMINT */
		assert(((OC_COMVAL == ref0->opcode) || (OC_COMINT == ref0->opcode)) && (TRIP_REF == ref0->operand[0].oprclass));
		bfini->opcode = OC_NOOP;
		bfini->operand[0].oprclass = NO_REF;
		assert(NO_REF == binit->operand[0].oprclass);
		ref2 = maketriple(OC_BOOLFINI);					/* put the OC_BOOLFINI at the current end */
		DEBUG_ONLY(ref2->src = bfini->src);
		ref2->operand[0] = put_tref(binit);
		ref1 = (TREF(boolchain_ptr))->exorder.bl;
		dqins(ref1, exorder, ref2);
		TRACK_JMP_TARGET(bfini, ref2);
		bfini = ref2;
		ref2 = maketriple(ref0->opcode);	/* followed by the OC_COMVAL/OC_COMINT */
		DEBUG_ONLY(ref2->src = ref0->src);
		ref2->operand[0] = put_tref(binit);
		/* Copy depth (parameter 2) from original OC_COMVAL/OC_COMINT triple */
		assert(TRIP_REF == ref0->operand[1].oprclass);
		assert(OC_ILIT == ref0->operand[1].oprval.tref->opcode);
		assert(ILIT_REF == ref0->operand[1].oprval.tref->operand[0].oprclass);
		ref2->operand[1] = ref0->operand[1];
		ref1 = (TREF(boolchain_ptr))->exorder.bl;
		dqins(ref1, exorder, ref2);
		ref0->opcode = OC_PASSTHRU;				/* turn original OC_COMVAL/OC_COMINT into an OC_PASSTHRU */
		ref0->operand[0] = put_tref(ref2);
		TRACK_JMP_TARGET(ref0, ref2);					/* also track it, in case it's a jump target */
		TRACK_JMP_TARGET(ref2, ref2);					/* without this there can be a gap in jmplist */
		if ((OCT_JUMP & oc_tab[(ref1 = bfini->exorder.bl)->opcode].octype)	/* WARNING assignment */
				&& (INDR_REF == ref1->operand[0].oprclass) && (NO_REF == ref1->operand[0].oprval.indr->oprclass))
		{	/* unresolved JMP around BOOLFINI goes to OC_COMVAL/OC_COMINT */
			*ref1->operand[0].oprval.indr = put_tnxt(bfini);
		}
	}
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	t->opcode = OC_NOOP;							/* wipe out the original boolean op */
	t->operand[0].oprclass = t->operand[1].oprclass = NO_REF;
	CHKTCHAIN(TREF(boolchain_ptr), exorder, FALSE);				/* ensure no cross threading between the 2 chains */
	if (!expr_fini)
		return;								/* more to come */
	/* time to deal with new jump ladder */
	assert(NULL != TREF(boolchain_ptr));
	assert(NULL != TREF(bool_targ_ptr));
	assert(TREF(bool_targ_ptr) != (TREF(bool_targ_ptr))->que.fl);
	ref2 = maketriple(OC_NOOP);						/* add a safe target */
	ref1 = (TREF(boolchain_ptr))->exorder.bl;
	dqins(ref1, exorder, ref2);
	ref0 = TREF(boolchain_ptr);
	for (t0 = ref0->exorder.fl; (t0 != ref0); t0 = t0->exorder.fl)
	{									/* process replacement jmps */
		if (!(OCT_JUMP & oc_tab[t0->opcode].octype))
			continue;
		adj_addr = &t0->operand[0];
		if (NULL != (t1 = (adj_addr = adj_addr->oprval.indr)->oprval.tref))	/* WARNING assignment */
		{								/*  need to adjust target */
			if (TNXT_REF == adj_addr->oprclass)			/* TNXT requires a different adjustment */
			{
				for (ref1 = t1->exorder.fl; ref1 != ref0; ref1 = ref1->exorder.fl)
				{
					if (NULL != ref1->jmplist.bkptr)		/* find 1st recorded target after TNXT */
					{
						assert((OC_NOOP == ref1->opcode) || (OC_STOTEMP == ref1->opcode)
							|| (OC_COMVAL == ref1->opcode) || (OC_COMINT == ref1->opcode)
							|| (OC_PASSTHRU == ref1->opcode));
						ref1 = ref1->jmplist.bkptr;	/* should point to appropriate new target */
						assert((OCT_BOOL & oc_tab[ref1->opcode].octype)
							|| (OC_NOOP == ref1->opcode)
							|| (OC_COMVAL == ref1->opcode|| (OC_COMINT == ref1->opcode)));
						break;
					}
				}
			} else
			{
				assert(TJMP_REF == adj_addr->oprclass);
				assert(OC_NOOP == t1->opcode);
				assert(&(t1->jmplist) != t1->jmplist.que.fl);
				assert(NULL != t1->jmplist.bkptr);
				ref1 = t1->jmplist.bkptr;
				if (ref1 == ref0->exorder.bl->exorder.bl)	/* if it wants to jump to the end of the chain */
				{
					assert(OC_NOOP == ref0->exorder.bl->opcode);
					ref1 = ref0->exorder.bl;		/* no need to do more fishing */
				} else
				{	/* from the jmp jmplisted in the old target, move forward to the next COBOOL) */
					assert(OCT_JUMP & oc_tab[ref1->opcode].octype);
					ref1 = ref1->exorder.fl;
					assert((OCT_BOOL & oc_tab[ref1->opcode].octype) || (OC_BOOLFINI == ref1->opcode)
						|| (OC_BOOLINIT == ref1->opcode));
				}
			}
			t0->operand[0] = put_tjmp(ref1);			/* no indirection simplifies later compilation */
		}
	}
	dqloop(TREF(bool_targ_ptr), que, tripbp)				/* clean up borrowed jmplist entries */
	{
		dqdel(tripbp, que);
		tripbp->bkptr = NULL;
	}
	assert((TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.fl)
		&& (TREF(bool_targ_ptr) == (TREF(bool_targ_ptr))->que.bl));
	dqadd(t, TREF(boolchain_ptr), exorder);					/* insert the new JMP ladder */
	TREF(boolchain_ptr) = NULL;
	if (TREF(expr_start) != TREF(expr_start_orig))
	{									/* inocculate against an unwanted GVRECTARG */
		ref0 = maketriple(OC_NOOP);
		DEBUG_ONLY(ref0->src = t->src);
		dqins(TREF(expr_start), exorder, ref0);
		TREF(expr_start) = ref0;
	}
	return;
}
