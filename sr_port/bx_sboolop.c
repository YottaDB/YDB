/****************************************************************
 *								*
 * Copyright (c) 2023 Fidelity National Information		*
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
#include "cmd_qlf.h"
#include "gtm_common_defs.h"
#include "mdq.h"
#include "mmemory.h"
#include "opcode.h"
#include "fullbool.h"


LITREF octabstruct		oc_tab[];
GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;

static inline void putop_in_newopr(triple *oldop, triple *newop, int opridx, boolean_t finish)
/* oldop - an operation in its original place in the exorder chain.
 * newop - the replacement operation, not necessarily yet in place.
 * opridx - the index of the operand to protect
 * finish - whether or not to zero-out the oldop after protecting the operand.
 * This function safely gives operands of an operation which obeys standard locality to one that will be placed sommewhere else
 * in the exorder chain, and whose operands may therefore need protecting from intervening side effects. The only genuine
 * case here is when the operand is an OC_VAR, in which case this function inserts an intervening stotemp to provide as the operand
 * of the new operation. The function adds an OC_LITC between newop and an OC_LIT in -DYNAMIC_LITERALS compilation mode to prevent
 * work elsewhere. Future maintainers should note that this function may be of general use when manipulating exorder, but has not
 * been validated on operations which are not themselves the operands of OC_S(N)AND/OC_S(N)OR/OC_COM - i.e. OCT_BOOLs, either one
 * of those mentioned or a COBOOL.
 */
{
	triple		*newtrip;
	oprtype 	*opr = &oldop->operand[opridx];
	assert(TRIP_REF == opr->oprclass);
	switch(opr->oprval.tref->opcode)
	{
	case OC_VAR:
		newtrip = maketriple(OC_STOTEMP);
		newtrip->operand[0] = *opr;
		dqins(opr->oprval.tref, exorder, newtrip);
		break;
	case OC_LIT:
		if (!run_time && (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))
		{
			newtrip = maketriple(OC_LITC);
			newtrip->operand[0] = *opr;
			dqins(opr->oprval.tref, exorder, newtrip);
			break;
		}
		/* WARNING - fallthrough */
	default:
		newtrip = opr->oprval.tref;
	}
	newop->operand[opridx] = put_tref(newtrip);
	if (finish)
	{
		oldop->opcode = OC_NOOP;
		oldop->operand[0].oprclass = oldop->operand[1].oprclass = NO_REF;
	}
	return;
}

void bx_sboolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr)
/* t points to the boolean operation, jmp_type_one gives the sense of the jump associated with the first operand,   B1 B2
 * jmp_to_next gives whether to supply a second jump to finish the operation, and sense gives its sense.	     |  |
 * addr points to the operand for the jump eventually used by logic back in the invocation stack to 	     	     v  v
 * fill in a target location. It may be useful to note the two types of boolean nesting. Let the parent boolean be (1&(X!$i(v))),
 * let X be some expression, let v be a mumps variable, and Bx stand for the boolean expression denoted.
 * GT.M builds boolean expressions into boolchains, which are sequences which reset an indicator value to zero, get some value,
 * check that value and jump somewhere (often past the set-to-one of the indicator value) get another value and so on, and then
 * set the indicator value to one. GT.M could conceivably build each boolchain with only two operands and execute in dependency
 * order, like B2[init,val(X),jmpnzl,val(incr),jmpzl,set]...B1[init,val(literal 1),jmpzl,val(B2 result),jmpzl,set]. Instead,
 * for booleans which take other boolean operations as direct operands - *directly nested*, like B1 and B2, GT.M inlines the
 * nested jump chain inside its parent, like B1[init,val(literal 1),jnzl,val(X),jmpnzl,val(incr),jmpzl,set] (note that not all
 * jumps target the same operand; jnzl here jumps to the final set and jzl jumps after it). This has the benefit of reducing
 * the number of instructions. Indirectly nested booleans, however, are not handled in the same way.
 * Let X be ("mumps"_(1&$i(var))) and call the interior AND B3. That interior AND needs its own boolchain, since otherwise
 * the concatenation would need to be both after and before the result, a contradiction. Inlining booleans when they are nested
 * and not doing so when they are not is the aim of the logic in this and all boolean handling functions, since inapppropriate
 * inlining has led before execution order errors in the past. On side-effect processing - the technique we use is to leave real
 * operations in place (variable reads, nonbool expressions, etc, and build the boolchain after all operations
 * have been processed in order.
 */
{
	oprtype		*p, *i;
	triple		*t0, *t1, *t2;
	size_t		j;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(OCT_SE & oc_tab[t->opcode].octype);
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(OCT_BOOL & oc_tab[t->operand[0].oprval.tref->opcode].octype);
	assert(OCT_BOOL & oc_tab[t->operand[1].oprval.tref->opcode].octype);
	if (jmp_to_next)
	{
		p = (oprtype *)mcalloc(SIZEOF(oprtype));
		*p = put_tjmp(t);
	} else
		p = addr;
	/* The following assert is commented out because it correctly detects a mishandling of shift_side_effects and
	 * saw_side_effect that occurs when a compile-time error is seen but processing is deferred until after there
	 * is a unified working chain. Under rare circumstances, those conditions can allow for tail-processing of booleans
	 * after a compile-time error has already been discoverered so as to reach the point of reuiniting the working
	 * chains and giving effect to the error.
	 */
	/* assert(TREF(shift_side_effects)); */
	/* Since we have a guarantee that no direct boolean descendents have yet been processed (if the is_boolchild ex_tail
	 * argument is handled right), we can freely move our operands. All of our operands are bools, and some of them take
	 * bools themselves as operands (let the 'pure bools' be those operations which both result in and operate on boolean
	 * values: OC_(S)(N)AND, OC_(S)(N)OR, and OC_COM ). Since this is a purebool-with-side-effect, we need to leave both
	 * our 'real' operands in front of our own or our ancestor's boolinit. But since we only operate directly on either
	 * either COBOOLS or pure/impure bool ops which will themselves undergo processing, we can move our direct operands
	 * to their place just before our original purebool. For purebool direct operands, this function will apply the logic
	 * recursively; impure boolean operands like the relops work like COBOOLS: they'll turn into cmps and add the jump right
	 * after themselves in exorder, and they count as a leaf in the recursive unified boolchain logic - the intervening COMVAL
	 * that must be between them and any boolean operands they take themselves means that those operands will be built as part
	 * of a separate boolchain - p&(q=(r&s)) is built as two boolchains - one including the cmps and jmps belonging to the
	 * outer AND and EQU, and another including those belonging to the inner AND, but if every operator were an AND or OR
	 * this logic would make them into a single chain. Placing them in the right position here is enough to get the right
	 * sequence when calling bx_tail. Complement (OC_COM) is in many ways a special case, and maintainers should be mindful
	 * that it is the only unary pure boolean. Like any pure boolean, it can serve as a link in a chain of directly nested
	 * booleans. But since it does not relocate its operand or deliver protection for the operands of its operand, we need
	 * to do that here. One additional special case involves a left-hand-operand (N)AND/(N)OR of a parent S(N)AND/S(N)OR
	 * operation. While we could build a separate triple chain for the left-hand tree, it will be easiest to instead turn
	 * any direct boolean operation descendants of a SE-type op into SE-type ops themselves. This will allow us to build
	 * one big chain (as for any other directly-nested boolean expression) at the cost of some extra stotemps under fullbool,
	 * for the benefit of code simplicity and at least as many fewer set/clears as extra stotemps.
	 * Warning: make no assumptions about exorder during the ex_tail phase of things.
	 * */
	for (i = t->operand, j = 0U; i < ARRAYTOP(t->operand); i++, j++)
	{
		t0 = maketriple(i->oprval.tref->opcode);
		if (OCT_UNARY & oc_tab[t0->opcode].octype)
			putop_in_newopr(i->oprval.tref, t0, 0, TRUE);
		else
		{
			putop_in_newopr(i->oprval.tref, t0, 0, FALSE);
			putop_in_newopr(i->oprval.tref, t0, 1, TRUE);
		}
		t->operand[j] = put_tref(t0);
		CONVERT_TO_SE(t0);
		dqrins(t, exorder, t0);
		for (t1 = t0; OC_COM == t1->opcode; t1 = t2)
		{
			assert(t1->operand[0].oprclass == TRIP_REF);
			t2 = maketriple(t1->operand[0].oprval.tref->opcode);
			if (OCT_UNARY & oc_tab[t2->opcode].octype)
				putop_in_newopr(t1->operand[0].oprval.tref, t2, 0, TRUE);
			else
			{
				putop_in_newopr(t1->operand[0].oprval.tref, t2, 0, FALSE);
				putop_in_newopr(t1->operand[0].oprval.tref, t2, 1, TRUE);
			}
			t1->operand[0] = put_tref(t2);
			CONVERT_TO_SE(t2);
			dqrins(t1, exorder, t2);
		}
		if (!j)
			bx_tail(t0, jmp_type_one, p);
		else
			bx_tail(t0, sense, addr);
	}
	t->opcode = OC_NOOP;
	t->operand[0].oprclass = t->operand[1].oprclass = NO_REF;
	if (TREF(expr_start) != TREF(expr_start_orig))
	{
		t0 = maketriple(OC_NOOP);
		DEBUG_ONLY(t0->src = t->src);
		dqins(TREF(expr_start), exorder, t0);
		TREF(expr_start) = t0;
	}
	return;
}
