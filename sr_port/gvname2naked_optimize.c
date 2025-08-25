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
	mliteral	*subs[MAX_GVSUBSCRIPTS]; /* Includes variable name, but not the final subscript */
	uint8_t		len;
};

static boolean_t gv_dataflow(triple *curtrip, DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference);

/* Iterates through all triples in the chain looking for global variables that can be optimized to naked references. */
boolean_t gvname2naked_optimize(triple *chainstart)
{
	triple		*curtrip;
	/* NOTE: although this is named dollar_reference, it is not the same as $REFERENCE, which is a runtime value.
	 * This is instead a compile-time approximation of $REFERENCE.
	 * In particular, any kind of XECUTE or DO will reset this to NULL instead of having the true runtime value. */
	struct ctvar	dollar_reference = {{0}, 0};
	oprtype		*j, *k;
	triple		*nested_trip, *tripref;
	DEBUG_ONLY(boolean_t	oc_seen[OC_LASTOPCODE] = {0};)

	COMPDBG(printf("\n\n\n**************************** Begin gvname2naked_optimize rewrite ***********************\n"););
	/* Iterate over all triples in the translation unit */
	dqloop(chainstart, exorder, curtrip) {
		gv_dataflow(curtrip, DEBUG_ONLY_COMMA(oc_seen) &dollar_reference);
		/* Iterate over all parameters of the current triple */
		for (j = curtrip->operand, nested_trip = curtrip; j < ARRAYTOP(nested_trip->operand); ) {
			k = j;
			while (INDR_REF == k->oprclass)
				k = k->oprval.indr;
			if (TRIP_REF == k->oprclass) {
				tripref = k->oprval.tref;
				gv_dataflow(tripref, DEBUG_ONLY_COMMA(oc_seen) &dollar_reference);
				if (OC_PARAMETER == tripref->opcode)
				{
					nested_trip = tripref;
					j = nested_trip->operand;
					continue;
				}
			}
			j++;
		}
	}

	return false;
}

#ifdef DEBUG
static void ctvar_print(struct ctvar *var) {
	if (!var->len)
		return;
	printf("^%.*s(", var->subs[0]->v.str.len, var->subs[0]->v.str.addr);
	for (int i = 1; i < var->len; ++i) {
		mval *v = &var->subs[i]->v;
		printf("%.*s, ", v->str.len, v->str.addr);
	}
	/* NOTE: since ctvars do not contain the trailing subscript, it is not printed. Note that. */
	printf("_)");
}
#endif

static void unset_reference(DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference, char *why, char *extra_why) {
	COMPDBG(printf("unset global reference for %s %s\n", why, extra_why);)
	DEBUG_ONLY(memset(oc_seen, false, sizeof(boolean_t) * OC_LASTOPCODE);)
	dollar_reference->len = 0;
}

static boolean_t optimize_gvname(triple *curtrip, DEBUG_ONLY_COMMA(boolean_t *oc_seen) struct ctvar *dollar_reference) {
	triple		*hashtrip, *lastsubscript, *literaltrip, *subscriptstrip;
	int4		names;
	oprtype		opcodes;
	triple		*litc;

	triple	*subs_param, *opcodes_param, *deletetrip;
	mval	tmp_mval = { 0 };
	int	oc_len = 0;
	char	*oc_ptr;

	assert(OC_GVNAME == curtrip->opcode);
	/* Set subscriptstrip to the subscripts triple. */
	assert(TRIP_REF == curtrip->operand[0].oprclass);
	subscriptstrip = curtrip->operand[0].oprval.tref;
	assert(OC_ILIT == subscriptstrip->opcode);
	/* Set subscripts to the number of subscripts. */
	assert(ILIT_REF == subscriptstrip->operand[0].oprclass);
	names = subscriptstrip->operand[0].oprval.ilit;
	/* This is passing the argument count, not the number of subscripts. Adjust it not to include the hash count. */
	assert(1 < names);
	--names;

	if (2 > names) {
		/* We don't have any subscripts (e.g. ^X=1). Doing a naked reference is illegal. */
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "OC_GVNAME with 0 subscripts", "");
		return false;
	}

	/* Set hashtrip to the hash parameter. */
	assert(TRIP_REF == curtrip->operand[1].oprclass);
	hashtrip = curtrip->operand[1].oprval.tref;
	assert(OC_PARAMETER == hashtrip->opcode);
	/* Set lastsubscript to the literal parameter. */
	assert(TRIP_REF == hashtrip->operand[1].oprclass);
	lastsubscript = hashtrip->operand[1].oprval.tref;

	/* Copy all our triple parameters into a local array so they aren't overwritten.
	 * NOTE: this is a shallow copy, so it's not as expensive as it looks. */
	int sub = 0, old_len = dollar_reference->len;
	boolean_t new_access = 0 == old_len;
	mliteral *current_sub;
	assert((NO_REF == lastsubscript->operand[1].oprclass) || (TRIP_REF == lastsubscript->operand[1].oprclass));
	/* This stops just before the last subscript, since it's overwritten when doing a naked reference. */
	for (; NO_REF != lastsubscript->operand[1].oprclass; ++sub, lastsubscript = lastsubscript->operand[1].oprval.tref) {
		/* Set literaltrip to the literal value triple. */
		assert(OC_PARAMETER == lastsubscript->opcode);
		assert(TRIP_REF == lastsubscript->operand[0].oprclass);
		literaltrip = lastsubscript->operand[0].oprval.tref;
		/* If this is a dynamic literal, follow the indirect reference. */
		if ((cmd_qlf.qlf & CQ_DYNAMIC_LITERALS) && OC_LITC == literaltrip->opcode) {
			assert(TRIP_REF == literaltrip->operand[0].oprclass);
			literaltrip = literaltrip->operand[0].oprval.tref;
		}
		if (OC_LIT != literaltrip->opcode) {
			/* This is not a literal parameter; for example `^X(y)`. We don't know how to optimize it, so don't try.
			 * It may be possible to extend this in the future. But it would be a lot of work.
			 * We would have to extend this dataflow analysis to all local variables, not just to $REFERENCE. */
			unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "non-literal subscript",
				oc_tab_graphic[literaltrip->opcode]);
			return false;
		}
		/* Check if the subscript is the same as the previous access, and update dollar_reference. */
		assert(MLIT_REF == literaltrip->operand[0].oprclass);
		current_sub = literaltrip->operand[0].oprval.mlit;
		new_access |= (old_len > sub && !is_equ(&dollar_reference->subs[sub]->v, &current_sub->v));
		dollar_reference->subs[sub] = current_sub;
		assert((NO_REF == lastsubscript->operand[1].oprclass) || (TRIP_REF == lastsubscript->operand[1].oprclass));
	}
	assert(sub + 1 == names);
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
	struct ctvar	new = {{0}, 0};

	DEBUG_ONLY(oc_seen[curtrip->opcode] = true;)

	if (OCT_JUMP & oc_tab[curtrip->opcode].octype) {
		/* branching elsewhere; following control flow is not immediately consecutive */
		unset_reference(DEBUG_ONLY_COMMA(oc_seen) dollar_reference, "OCT_JUMP", oc_tab_graphic[curtrip->opcode]);
		return false;
	}

	switch (curtrip->opcode) {
	case OC_GVNAME:
		optimized = optimize_gvname(curtrip, DEBUG_ONLY_COMMA(oc_seen) dollar_reference);
		break;
	case OC_GVNAKED:
	/* Naked references can themselves modify $REFERENCE.
	 * Consider for example `S ^V(1)=2,^(1,2)=3`, which results in $REFERENCE=^V(1,2).
	 * For now, we only allow naked references if they have exactly one subscript, which we know cannot cause modifications.
	 * In the future, we should update `dollar_reference` to match the new runtime value.
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
	case OC_RESTARTPC:	/* This is where M code restarts execution after a MUPIP INTRPT and so we should not
				 * optimize if this lies in between 2 OC_GVNAME opcodes.
				 * See https://gitlab.com/YottaDB/DB/YDB/-/issues/665#note_2693154599 for more details.
				 */
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
	}

	return optimized;
}
