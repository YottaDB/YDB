/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdio.h>

#include "compiler.h"
#include "is_equ.h"
#include "mdq.h"
#include "gtmdbglvl.h"
#include "cmd_qlf.h"
#include "stringpool.h"
#include "svnames.h"
#include "cdbg_dump.h"

GBLREF octabstruct oc_tab[];
GBLREF command_qualifier	cmd_qlf;
GBLREF char 		*oc_tab_graphic[];
GBLREF uint4		ydbDebugLevel;

struct ctvar { /* compile-time variable */
	mstr		subs[MAX_GVSUBSCRIPTS]; /* Includes variable name, but not the final subscript */
	uint8_t		len;
	boolean_t	has_lvn_subscript;	/* has at least one subscript (excluding the final subscript) that is
						 * an unsubscripted local variable name.
						 */
};

static boolean_t gv_dataflow(triple *curtrip, DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference);

/* Iterates through all triples in the chain looking for global variables that can be optimized to naked references. */
boolean_t gvname2naked_optimize(triple *chainstart)
{
	triple		*curtrip;
	/* NOTE: although this is named dollar_reference, it is not the same as $REFERENCE, which is a runtime value.
	 * This is instead a compile-time approximation of $REFERENCE.
	 * In particular, any kind of XECUTE or DO will reset this to NULL instead of having the true runtime value. */
	struct ctvar	dollar_reference;
	DEBUG_ONLY(boolean_t	oc_seen[OC_LASTOPCODE] = {0};)

	dollar_reference.len = 0;
	dollar_reference.has_lvn_subscript = FALSE;
	COMPDBG(printf("\n\n\n**************************** Begin gvname2naked_optimize rewrite ***********************\n"););
	/* Iterate over all triples in the translation unit */
	dqloop(chainstart, exorder, curtrip) {
		gv_dataflow(curtrip, DEBUG_ONLY_COMMA(oc_seen) &dollar_reference);
#		ifdef DEBUG
		/* 1) Previously, we used to invoke "gv_dataflow()" on all triple parameters too. But this was considered
		 *    unnecessary because we should have already seen all parameters in the execution chain in a prior
		 *    iteration of this "dqloop()" block.
		 * 2) Additionally, as https://gitlab.com/YottaDB/DB/YDB/-/merge_requests/1782#note_2925315026 noted,
		 *    there were cases where a naked reference optimization was missed out so it was actually doing harm
		 *    even if unintentionally.
		 *
		 * Therefore the triple parameter invocation logic was removed. But to be safe, we now have logic that
		 * verifies that all triple parameters would have been already found in the execution chain in a prior
		 * iteration.
		 *
		 * We verify that by going through the "->exorder.fl" links starting from the parameter triple and expect to
		 * eventually land on the current triple "curtrip".
		 *
		 * But just in case the parameter triple does not lead to the current triple, we do not want to loop
		 * indefinitely and so use a "tortoise and hare" algorithm to stop if ever an infinite loop is detected
		 * and assert fail below. "triptmp" is the tortoise pointer and "triptmp2" is the hare pointer below.
		 */
		oprtype		*j, *k;
		triple		*nested_trip, *tripref, *triptmp, *triptmp2;
		struct ctvar	save_dollar_reference;
		int		i;

		save_dollar_reference = dollar_reference;
		/* Iterate over all parameters of the current triple */
		for (j = curtrip->operand, nested_trip = curtrip; j < ARRAYTOP(nested_trip->operand); ) {
			k = j;
			while (INDR_REF == k->oprclass)
				k = k->oprval.indr;
			if (TRIP_REF == k->oprclass) {
				tripref = k->oprval.tref;
				switch (tripref->opcode) {
				case OC_PARAMETER:
					nested_trip = tripref;
					j = nested_trip->operand;
					continue;
					break;
				case OC_ILIT:
				case OC_LIT:
					/* It is possible literal triples are not part of the execution chain (there are
					 * lots of places in the code base where we only do a "maketriple()" of these opcodes
					 * and not a "ins_triple()"). But literal triples do not affect the gvname optimization
					 * and so should not affect the "gv_dataflow()" call. So skip these while checking that
					 * all triple parameters eventually lead to the current triple in the execution chain.
					 */
					break;
				default:
					triptmp = triptmp2 = tripref;
					while (triptmp != curtrip)
					{
						triptmp = triptmp->exorder.fl;
						triptmp2 = triptmp2->exorder.fl;
						triptmp2 = triptmp2->exorder.fl;
						assert(triptmp != triptmp2);	/* == implies an infinite loop situation */
					}
				}
			}
			j++;
		}
#		endif
	}
	return false;
}

#ifdef DEBUG
static void ctvar_print(struct ctvar *var) {
	if (!var->len)
		return;
	printf("^%.*s(", var->subs[0].len, var->subs[0].addr);
	for (int i = 1; i < var->len; ++i) {
		mstr *ms = &var->subs[i];
		printf("%.*s, ", ms->len, ms->addr);
	}
	/* NOTE: since ctvars do not contain the trailing subscript, it is not printed. Note that. */
	printf("_)");
}
#endif

static void unset_reference(DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference, char *why, char *extra_why) {
	COMPDBG(printf("unset global reference for %s %s\n", why, extra_why);)
	DEBUG_ONLY(memset(oc_seen, false, sizeof(boolean_t) * OC_LASTOPCODE);)
	dollar_reference->len = 0;
	dollar_reference->has_lvn_subscript = FALSE;
}

static boolean_t optimize_gvname(triple *curtrip, DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference) {
	triple		*hashtrip, *lastsubscript, *cursub, *first_opr;
	int4		gvname_nparms;
	oprtype		opcodes;
	triple		*litc;
	triple		*subs_param, *opcodes_param, *deletetrip;
	mval		tmp_mval = { 0 };
	int		oc_len = 0;
	char		*oc_ptr;
	int		sub, old_len;
	boolean_t	new_access;

	assert(OC_GVNAME == curtrip->opcode);
	assert(TRIP_REF == curtrip->operand[0].oprclass);
	first_opr = curtrip->operand[0].oprval.tref;
	assert(OC_ILIT == first_opr->opcode);
	/* Set subscripts to the number of subscripts. */
	assert(ILIT_REF == first_opr->operand[0].oprclass);
	gvname_nparms = first_opr->operand[0].oprval.ilit;
	assert(1 < gvname_nparms);
	if (2 >= gvname_nparms) {
		/* We don't have any subscripts (e.g. ^X=1). Doing a naked reference is illegal. */
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "OC_GVNAME with 0 subscripts", "");
		return false;
	}

	/* Set hashtrip to the hash parameter. */
	assert(TRIP_REF == curtrip->operand[1].oprclass);
	hashtrip = curtrip->operand[1].oprval.tref;
	assert(OC_PARAMETER == hashtrip->opcode);
	/* Set lastsubscript to the global variable name at the start */
	assert(TRIP_REF == hashtrip->operand[1].oprclass);
	lastsubscript = hashtrip->operand[1].oprval.tref;

	/* Copy all our triple parameters into a local array so they aren't overwritten.
	 * NOTE: this is a shallow copy, so it's not as expensive as it looks. */
	sub = 0;
	old_len = dollar_reference->len;
	new_access = (0 == old_len);
	assert((NO_REF == lastsubscript->operand[1].oprclass) || (TRIP_REF == lastsubscript->operand[1].oprclass));
	/* This stops just before the last subscript, since it's overwritten when doing a naked reference. */
	for (; NO_REF != lastsubscript->operand[1].oprclass; ++sub, lastsubscript = lastsubscript->operand[1].oprval.tref) {
		assert(OC_PARAMETER == lastsubscript->opcode);
		assert(TRIP_REF == lastsubscript->operand[0].oprclass);
		cursub = lastsubscript->operand[0].oprval.tref;
		/* If this is a dynamic literal, follow the indirect reference. */
		if ((cmd_qlf.qlf & CQ_DYNAMIC_LITERALS) && OC_LITC == cursub->opcode) {
			assert(TRIP_REF == cursub->operand[0].oprclass);
			cursub = cursub->operand[0].oprval.tref;
		}
		/* At this point in time, we only optimize global variable references that have LITERAL or LOCAL VARIABLE
		 * subscripts. e.g. "^x(2,...)" "^x(i,...)" as these are considered to be the most frequently encountered
		 * patterns in M code. Anything else we do not optimize. e.g. "^x(i+2,...)".  These can be optimized
		 * in the future if/when a need arises.
		 */
		switch(cursub->opcode) {
		case OC_LIT:;
			/* Check if the subscript is the same as the previous access, and update dollar_reference. */
			mliteral *sub_lit;

			assert(MLIT_REF == cursub->operand[0].oprclass);
			sub_lit = cursub->operand[0].oprval.mlit;
			assert(MV_IS_STRING(&sub_lit->v));
			new_access |= ((old_len > sub) && !MSTR_EQ(&dollar_reference->subs[sub], &sub_lit->v.str));
			dollar_reference->subs[sub] = sub_lit->v.str;
			break;
		case OC_VAR:;
			/* Check if the subscript is the same as the previous access, and update dollar_reference. */
			mvar *sub_varname;

			assert(MVAR_REF == cursub->operand[0].oprclass);
			sub_varname = cursub->operand[0].oprval.vref;
			new_access |= ((old_len > sub) && !MSTR_EQ(&dollar_reference->subs[sub], &sub_varname->mvname));
			dollar_reference->subs[sub] = sub_varname->mvname;
			dollar_reference->has_lvn_subscript = TRUE;
			break;
		default:
			unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "non-literal non-local-varname subscript",
				oc_tab_graphic[cursub->opcode]);
			return false;
			break;
		}
		assert((NO_REF == lastsubscript->operand[1].oprclass) || (TRIP_REF == lastsubscript->operand[1].oprclass));
	}
	assert((sub + 2) == gvname_nparms);
	new_access |= (old_len > sub);
	dollar_reference->len = sub;

	if (new_access) {
		COMPDBG(
			printf("reset global reference to ");
			ctvar_print(dollar_reference);
			putchar('\n');
		)
		DEBUG_ONLY(memset(oc_seen, false, sizeof(boolean_t) * OC_LASTOPCODE);)
		return false;
	} else if (old_len < sub) {
		COMPDBG(puts("extend global reference");)
	}

	COMPDBG(
		printf("opt GVNAME(");
		ctvar_print(dollar_reference);
		puts(")->GVNAMENAKED");
	)
	curtrip->opcode = OC_GVNAMENAKED;
	subs_param = maketriple(OC_PARAMETER);
	opcodes_param = maketriple(OC_PARAMETER);
	opcodes_param->src = subs_param->src = curtrip->src;

	/* Store the number of subscripts OMITTED (not the number present). Does not include the variable name. */
	subs_param->operand[0] = put_ilit(old_len - 1);
	subs_param->operand[1] = put_tref(opcodes_param);
	/* Put the original triple tree, not the one after iteration. Do this now, before it gets overwritten. */
	opcodes_param->operand[1] = put_tref(hashtrip->operand[1].oprval.tref);
	hashtrip->operand[1] = put_tref(subs_param);

	/* Record all opcodes seen since the last time dollar_reference was reset. */
	tmp_mval.mvtype = MV_STR;
#	ifdef DEBUG
	for (int i = 0; i < OC_LASTOPCODE; i++)
		if (oc_seen[i])
			oc_len += strlen(oc_tab_graphic[i]) + 1;
	tmp_mval.str.len = oc_len;
	tmp_mval.str.char_len = oc_len;
	ENSURE_STP_FREE_SPACE(oc_len);
	oc_ptr = tmp_mval.str.addr = (char *)stringpool.free;
	stringpool.free += oc_len;
	assert(stringpool.free <= stringpool.top);
	for (int i = 0; i < OC_LASTOPCODE; i++)
		if (oc_seen[i]) {
			strcpy(oc_ptr, oc_tab_graphic[i]);
			oc_ptr += strlen(oc_tab_graphic[i]);
			*oc_ptr++ = '|';
		}
#	else
	tmp_mval.str.len = tmp_mval.str.char_len = 0;
	tmp_mval.str.addr = NULL;
#	endif
	opcodes = put_lit(&tmp_mval);
	if (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS)
	{
		triple	*tref;

		tref = opcodes.oprval.tref;
		litc = maketriple(OC_LITC);
		litc->src = tref->src;
		litc->operand[0] = put_tref(tref);
		dqrins(curtrip, exorder, litc);
		opcodes_param->operand[0] = put_tref(litc);
	} else
		opcodes_param->operand[0] = opcodes;

	dqins(hashtrip, exorder, subs_param);
	dqins(hashtrip, exorder, opcodes_param);
	resolve_tref(opcodes_param, opcodes_param->operand);
	resolve_tref(subs_param, subs_param->operand);
	return true;
}

/* Given a triple, determine if and how it modifies $REFERENCE.
 *
 * This analysis is very dumb. It works linearly on the translation unit. In particular, any kind of control flow will disrupt it:
 * even something as simple as `set ^Y(1)=1  write:$data(^Y) ^Y(1)  set ^Y(2)=2` will not be optimized,
 * because we see there is a conditional jump and invalidate `dollar_reference`.
 *
 * Extending this to do proper dataflow analysis, such as determining that the conditional jump in the example above cannot modify
 * $REFERENCE, is necessary to do loop-invariant code motion (LICM).
 * But that's hard. So we don't do it right now.
 */
static boolean_t gv_dataflow(triple *curtrip, DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference) {
	boolean_t	optimized = false;
	triple 		*svtrip, *subscriptstrip;
	int4		subscripts;
	enum isvopcode	sv;

	DEBUG_ONLY(oc_seen[curtrip->opcode] = true;)

	/* If current triple branches elsewhere, reset internal $REFERENCE as first global reference after this triple
	 * can not be safely assumed to be a consecutive global reference due to the change in control flow.
	 */
	if (OCT_JUMP & oc_tab[curtrip->opcode].octype) {
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "OCT_JUMP", oc_tab_graphic[curtrip->opcode]);
		return false;
	}

	/* If current triple is a jump target of some other triple (prior or later), reset internal $REFERENCE as first
	 * global reference after this triple can not be safely assumed to be a consecutive global reference since the
	 * immediately previous global reference could be prior to the triple whose jump target is the current triple
	 * and not necessarily the previous global reference in the execution chain.
	 */
	if (curtrip->jmplist.que.fl != &curtrip->jmplist)
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "Jump Target", oc_tab_graphic[curtrip->opcode]);

	switch (curtrip->opcode) {
	case OC_GVNAME:
		optimized = optimize_gvname(curtrip, DEBUG_ONLY_COMMA(oc_seen) dollar_reference);
		break;
	case OC_GVNAKED:
		/* Naked references can themselves modify $REFERENCE.
		 * Consider for example `S ^V(1)=2,^(1,2)=3`, which results in $REFERENCE=^V(1,2).
		 * For now, we only allow naked references if they have exactly one subscript, which we know cannot
		 * cause modifications. In the future, we should update `dollar_reference` to match the new runtime value.
		 */
		/* Set subscriptstrip to the subscripts triple. */
		assert(TRIP_REF == curtrip->operand[0].oprclass);
		subscriptstrip = curtrip->operand[0].oprval.tref;
		assert(OC_ILIT == subscriptstrip->opcode);
		/* Set subscripts to the number of subscripts. */
		assert(ILIT_REF == subscriptstrip->operand[0].oprclass);
		subscripts = subscriptstrip->operand[0].oprval.ilit;
		/* This is passing the argument count, not the number of subscripts. Adjust it. */
		assert(0 < subscripts);
		subscripts -= 1;
		if (1 < subscripts) {
			/* TODO: optimize here too (https://gitlab.com/YottaDB/DB/YDB/-/issues/665#todo-list) */
			unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "naked reference with more than 1 subscript", "");
		}
		break;
	/* Opcodes that invoke code we don't know how to analyze (e.g. not in this translation unit), or directly modify $REFERENCE
	 */
	case OC_RET:        /* QUIT */
	/* OC_CALL is unreachable; has type OCT_JUMP */
	case OC_EXTCALL:    /* do f^module */
	case OC_FGNCAL:     /* do &x */
	case OC_FNFGNCAL:   /* $&x */
	/* intentionally not present because they always imply a JMP:
	 * - OC_EXCAL: do f(1)
	 * - OC_EXFUN: $$f(1)
	 */
	case OC_EXTEXFUN:   /* $$f^source. unlike EXFUN, this is not accompanied by a JMP. include it explicitly. */
	case OC_EXTEXCAL:   /* do f^source(1). not accompanied by a JMP. */
	case OC_FORCHK1:    /* start of FOR loop iteration */
	/* OC_FORLCLDO is unreachable; has type OCT_JUMP */
	case OC_GVNEXT:     /* $N(^X(1)) can modify $REFERENCE to ^X(-1) */
	case OC_GVEXTNAM:   /* ^|"dir"|x. Theoretically we could track this but it's hard and also rare. */
	case OC_INDGLVN:    /* write 1+@y (must be in an expression, not the sole argument) */
	case OC_INDSAVGLVN: /* set @y=1 */
	case OC_INDFUN:     /* $data(@y) */
	case OC_INDRZSHOW:  /* zshow "a":@"^y(3)" */
	case OC_INDINCR:    /* $incr(@z) */
	case OC_INDSET:     /* read @x:0 */
	case OC_INDDEVPARMS:/* open "file":@x */
	case OC_INDMERGE:   /* merge a=@x */
	case OC_INDMERGE2:  /* merge a=@x in context of side effects */
	case OC_INDFNNAME:  /* $name(@a) */
	case OC_INDFNNAME2: /* $name(@a) */
	case OC_TROLLBACK:  /* trollback */
	/* not present because they already imply INDSAVGLVN:
	 * INDO2: $order(@y,a)
	 * INDGET1: set $zpiece(@x,1)=y
	 * INDGET2: $get(@x,1)
	 * INDMERGE2: TODO
	 * INDQ2: $query(@x,a)
	 */
	/* @(...) indirection.
	 * NOTE: this is not *strictly* neccessary; e.g. ZWRITE @(...) is ok.
	 * But telling the difference is hard, and indirection is rare, so just mark them all as invalidating the optimization. */
	case OC_COMMARG:
	case OC_ZSHOW:	/* can modify $REFERENCE if target is a global (e.g. zshow "*":^gbl) */
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, oc_tab_graphic[curtrip->opcode], "");
		break;
	case OC_NEWINTRINSIC: /* new $ZGBLDIR */
	case OC_SVPUT:        /* set $ZGBLDIR= */
		/* If this modifies $ZGBLDIR, $REFERENCE is invalid. */
		assert(TRIP_REF == curtrip->operand[0].oprclass);
		svtrip = curtrip->operand[0].oprval.tref;
		/* Set sv to the intrinsic variable being modified. */
		assert(OC_ILIT == svtrip->opcode);
		assert(ILIT_REF == svtrip->operand[0].oprclass);
		sv = svtrip->operand[0].oprval.ilit;
		if (SV_ZGBLDIR == sv) {
			unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "$ZGBLDIR change", "");
		}
		break;
	/* Opcodes we know do not modify $REFERENCE. Note that most opcodes do not modify $REFERENCE, but these appear frequently
	 * and make the ERR_GVDBGNAKED error harder to read. */
	case OC_NOOP:
	case OC_PARAMETER:
	case OC_GVPUT:
	case OC_GVGET:
	case OC_ILIT:
	case OC_LIT:
		/* Hide these from the debug output. */
		DEBUG_ONLY(
		if (! (ydbDebugLevel & GDL_DebugCompiler)) {
			oc_seen[curtrip->opcode] = false;
		}
		)
		break;
	case OC_LINESTART: /* Optimizing across lines is ok because ZGOTO sets a runtime flag that disables the optimization */
	case OC_LINEFETCH:
	/* Line that fetches a local variable. When passed `-noline_entry`, this is only present at the start of a function. But to
	 * handle the `-line_entry` case, we need to set the `gv_namenaked_state` runtime flag anyway. So no need to disable the
	 * optimization at compile time.
	*/
	default:
		break;
	/* Below are opcodes that can modify the value of a local variable. If so, they need to disable the naked
	 * reference optimization for future identical global references if the current $reference had local variables
	 * as subscripts. For example, "set a=1 set ^x(a,1)=2 set a=2 set ^x(a,1)=3" should not use a naked reference
	 * for the "set ^x(a,1)=3" because "a" changed in value from 1 to 2 in between the 2 "^x(a,1)" references.
	 */
	case OC_STO:		/* set a=2 */
	case OC_STOLITC:	/* OC_STO can get converted into OC_STOLITC in "resolve_ref()" so treat latter like OC_STO */
	case OC_FNINCR: 	/* if $incr(a) */
	case OC_KILL:		/* kill a */
	case OC_KILLALL:	/* kill */
	case OC_XKILL:		/* kill (b) */
	case OC_LVZWITHDRAW:	/* zkill a */
	case OC_NEWVAR:		/* new a */
	case OC_XNEW:		/* new (b) */
	case OC_READ:		/* read a */
	case OC_RDONE:		/* read *a */
	case OC_READFL:		/* read a#2 */
	case OC_SETPIECE:	/* set $piece(a,"abcd",1)=2 */
	case OC_SETZP1:		/* set $piece(a,"2")=2 in M     mode */
	case OC_SETP1:		/* set $piece(a,"2")=2 in UTF-8 mode */
	case OC_SETEXTRACT:	/* set $extract(a,"2")=2 */
	case OC_SETZEXTRACT:	/* set $zextract(a,"2")=2 */
	case OC_SETZPIECE:	/* set $zpiece(a,"23")=2 */
	case OC_MERGE_LVARG:	/* merge a=^x(a,1) */
	case OC_SETALS2ALS:	/* set *a=b */
	case OC_SETALSCTIN2ALS:	/* set *a=b(1) */
	case OC_SETFNRETIN2ALS:	/* set *a=$$^b */
	case OC_KILLALIAS:	/* kill *a */
	case OC_KILLALIASALL:	/* kill * */
		if (dollar_reference->has_lvn_subscript)
		{	/* Internal reference has at least one local variable subscript (excluding the final subscript).
			 * In that case, it is possible that local variable is being modified by the OC_STO etc. opcode.
			 * Therefore reset the internal $reference to avoid naked misoptimization in the next global reference.
			 * For example, "set a=1,^x(a,1)=2,a=2,^x(a,1)=3" should not optimize the second "^x(a,1)" to a naked
			 * reference because the local variable "a" changed from 1 to 2 between the 2 references.
			 */
			unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "Local variable subscript", "");
		}
		/* Below opcodes can also modify the value of a local variable. But it is OC_STO that gets transformed to the
		 * below opcodes in "alloc_reg()" and that call has not yet happened and so we will see OC_STO first and will
		 * disable the optimization. Therefore these opcodes are currently commented out.
		 *
		 * OC_ADD:	set a=a+1`
		 * OC_SUB:	set a=a-1`
		 * OC_MUL:	set a=a*2`
		 * OC_DIV:	set a=a/0.5`
		 * OC_IDIV:	set a=a\0.5`
		 * OC_MOD:	set a=a#1`
		 * OC_NEG:	set a=-a`
		 * OC_FORCENUM:	set a=+(a-2)`
		 * OC_CAT:	set a=a_2`
		 * OC_STOLIT:	set a=2
		 */
		break;
	}

	return optimized;
}
