/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "fullbool.h"
#include "fnorder.h"
#include "advancewindow.h"
#include "glvn_pool.h"
#include "mvalconv.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;
GBLREF	int		source_column;

error_def(ERR_QUERY2);
error_def(ERR_SIDEEFFECTEVAL);
error_def(ERR_VAREXPECTED);

int f_query(oprtype *a, opctype op)
{
	boolean_t	ok;
	enum order_dir	direction;
	enum order_obj	object;
	int4		intval;
	opctype		gv_oc;
	oprtype		*tmpa, *tmpa0, *tmpa1, tmpa2;
	oprtype		control_slot, dir_opr, *dir_oprptr, *next_oprptr;
	save_se		save_state;
	short int	column;
	triple		*chain2, *obp, tmpchain2;
	triple		*oldchain, *r, *r0, *r1, *r2, *sav_dirref, *triptr;
	triple		*sav_gv1, *sav_gvn, *sav_lvn, *sav_ref;
	static opctype	query_opc[LAST_OBJECT][LAST_DIRECTION] =
	{
		/* FORWARD	BACKWARD		TBD */
		{ OC_GVQUERY,	OC_GVREVERSEQUERY,	OC_GVQ2		},	/* GLOBAL */
		{ OC_FNQUERY,	OC_FNREVERSEQUERY,	OC_FNQ2		},	/* LOCAL */
		{ 0,		0,			0		},	/* LOCAL_NAME */
		{ OC_INDFUN,	OC_INDFUN,		OC_INDQ2	}	/* INDIRECT */
	};
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	oldchain = sav_dirref = NULL;			/* default to no direction and no shifting indirection */
	r = maketriple(OC_NOOP);			/* We'll fill in the opcode later, when we figure out what it is */
	switch (TREF(window_token))
	{
	case TK_IDENT:
		object = LOCAL;
		tmpa = &r->operand[0];
		ok = lvn(tmpa, OC_FNQUERY, 0);
		if (!ok)
			break;
		assert(TRIP_REF == tmpa->oprclass);
		tmpa0 = &tmpa->oprval.tref->operand[0];
		if (OC_FNQUERY == tmpa->oprval.tref->opcode)
		{	/* $query on subscripted lvn */
			assert(TRIP_REF == tmpa0->oprclass);
			assert(OC_ILIT == tmpa0->oprval.tref->opcode);
			assert(ILIT_REF == tmpa0->oprval.tref->operand[0].oprclass);
			assert(0 < tmpa0->oprval.tref->operand[0].oprval.ilit);
			/* If the compiled code is say $query(x(1,2)), the triple chain after the "lvn" call above would look as
			 *	OC_FNQUERY
			 *	OC_ILIT (holding a count of 3 == 2 subscripts + 1 base-variable)
			 *	OC_VAR (corresponding to base variable name "x")
			 *	OC_LIT (corresponding to subscript 1)
			 *	OC_LIT (corresponding to subscript 2)
			 * But op_fnquery & op_fnreversequery (the run-time functions that implement the $query operation) need
			 * to be passed 2 more parameters
			 *	a) the total # of parameters that follow (including this total parameter) and
			 *	b) the unsubscripted local variable name.
			 * Therefore bump the OC_ILIT count by 2 more below. And insert the variable name in between the
			 *	OC_ILIT and OC_VAR opcode.
			 */
			tmpa0->oprval.tref->operand[0].oprval.ilit += 2;
			tmpa1 = &tmpa->oprval.tref->operand[1];
			assert(TRIP_REF == tmpa1->oprclass);
			assert(OC_PARAMETER == tmpa1->oprval.tref->opcode);
			assert(TRIP_REF == tmpa1->oprval.tref->operand[0].oprclass);
			r0 = tmpa1->oprval.tref->operand[0].oprval.tref;
			assert(OC_VAR == r0->opcode);
			assert(MVAR_REF == r0->operand[0].oprclass);
			r1 = maketriple(OC_PARAMETER);
			r1->operand[0] = put_str(r0->operand[0].oprval.vref->mvname.addr, r0->operand[0].oprval.vref->mvname.len);
			r1->operand[1] = *tmpa1;
			*tmpa1 = put_tref(r1);
			dqins(tmpa->oprval.tref->exorder.fl, exorder, r1);
		} else
		{	/* $query on unsubscripted lvn */
			assert(OC_VAR == tmpa->oprval.tref->opcode);
			r0 = newtriple(OC_FNQUERY);
			r0->operand[0] = put_ilit(3);
			r0->operand[1] = put_tref(newtriple(OC_PARAMETER));
			r0->operand[1].oprval.tref->operand[0] = put_str(tmpa0->oprval.vref->mvname.addr,
										tmpa0->oprval.vref->mvname.len);
			r1 = r0->operand[1].oprval.tref;
			r1->operand[1] = *tmpa;
			*tmpa = put_tref(r0);
			tmpa0 = &tmpa->oprval.tref->operand[0];
			tmpa1 = &tmpa->oprval.tref->operand[1];
		}
		assert(OC_FNQUERY == tmpa->oprval.tref->opcode);
		next_oprptr = &r->operand[1];
		break;
	case TK_CIRCUMFLEX:
		object = GLOBAL;
		sav_gv1 = TREF(curtchain);
		ok = gvn();
		sav_gvn = (TREF(curtchain))->exorder.bl;
		if (OC_GVRECTARG == sav_gvn->opcode)
		{	/* because of shifting if we need to find it, look in the expr_start chain */
			assert(TREF(shift_side_effects));
			assert(((sav_gvn->operand[0].oprval.tref) == TREF(expr_start)) && (NULL != TREF(expr_start_orig)));
			sav_gv1 = TREF(expr_start_orig);
			sav_gvn = TREF(expr_start);
		}
		next_oprptr = &r->operand[0];
		break;
	case TK_ATSIGN:
		object = INDIRECT;
		if (SHIFT_SIDE_EFFECTS)
			START_GVBIND_CHAIN(&save_state, oldchain);
		ok = indirection(&(r->operand[0]));
		next_oprptr = &r->operand[1];
		break;
	default:
		ok = FALSE;
		break;
	}
	if (!ok)
	{
		if (NULL != oldchain)
			setcurtchain(oldchain);
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	if (TK_COMMA != TREF(window_token))
		direction = FORWARD;	/* default direction */
	else
	{	/* two argument form: ugly logic for direction */
		advancewindow();
		column = source_column;
		dir_oprptr = (oprtype *)mcalloc(SIZEOF(oprtype));
		dir_opr = put_indr(dir_oprptr);
		sav_ref = newtriple(OC_GVSAVTARG);
		DISABLE_SIDE_EFFECT_AT_DEPTH;		/* doing this here lets us know specifically if direction had SE threat */
		if (EXPR_FAIL == expr(dir_oprptr, MUMPS_EXPR))
		{
			if (NULL != oldchain)
				setcurtchain(oldchain);
			return FALSE;
		}
		assert(TRIP_REF == dir_oprptr->oprclass);
		triptr = dir_oprptr->oprval.tref;
		if (OC_LIT == triptr->opcode)
		{	/* if direction is a literal - pick it up and stop flailing about */
			if (MV_IS_TRUEINT(&triptr->operand[0].oprval.mlit->v, &intval) && (1 == intval || -1 == intval))
			{
				direction = (1 == intval) ? FORWARD : BACKWARD;
				sav_ref->opcode = OC_NOOP;
				sav_ref = NULL;
			} else
			{	/* bad direction */
				if (NULL != oldchain)
					setcurtchain(oldchain);
				stx_error(ERR_QUERY2);
				return FALSE;
			}
		} else
		{
			direction = TBD;
			sav_dirref = newtriple(OC_GVSAVTARG);		/* $R reflects direction eval even if we revisit 1st arg */
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(sav_ref);
			switch (object)
			{
			case GLOBAL:		/* The direction may have had a side effect, so take copies of subscripts */
				*next_oprptr = *dir_oprptr;
				for (; sav_gvn != sav_gv1; sav_gvn = sav_gvn->exorder.bl)
				{	/* hunt down the gv opcode */
					gv_oc = sav_gvn->opcode;
					if ((OC_GVNAME == gv_oc) || (OC_GVNAKED == gv_oc) || (OC_GVEXTNAM == gv_oc))
						break;
				}
				assert((OC_GVNAME == gv_oc) || (OC_GVNAKED == gv_oc) || (OC_GVEXTNAM == gv_oc));
				TREF(temp_subs) = TRUE;
				create_temporaries(sav_gvn, gv_oc);
				ins_triple(r);
				break;
			case LOCAL:		/* Additionally need to move OC_FNQUERY triple to after potential side effect */
				sav_lvn = r->operand[0].oprval.tref;
				assert(OC_FNQUERY == sav_lvn->opcode);
				dqdel(sav_lvn, exorder);
				ins_triple(sav_lvn);
				assert(TRIP_REF == tmpa0->oprclass);
				assert(OC_ILIT == tmpa0->oprval.tref->opcode);
				assert(ILIT_REF == tmpa0->oprval.tref->operand[0].oprclass);
				assert(3 <= tmpa0->oprval.tref->operand[0].oprval.ilit);
				if (3 != tmpa0->oprval.tref->operand[0].oprval.ilit)
				{	/* sav_lvn has subscripts that we need to create temporaries for */
					TREF(temp_subs) = TRUE;
					create_temporaries(sav_lvn, sav_lvn->opcode);
				}
				/* else: sav_lvn is unsubscripted. No need to create temporaries */
				/* Insert direction in OC_FNQUERY triple to eventually go to OC_FNQ2 */
				r2 = maketriple(OC_PARAMETER);
				r2->operand[0] = dir_opr;
				r2->operand[1] = *tmpa1;
				*tmpa1 = put_tref(r2);
				dqins(sav_lvn, exorder, r2);
				/* Since the "lvn" call above happened with "0" as the 3rd ("parent") parameter, r->operand[1]
				 * would not be filled in like it would in case of "f_order" so this part of the code is different
				 * from that of "f_order" (there we need to account for temporaries for r->operand[1]).
				 */
				assert(&r->operand[1] == next_oprptr);
				break;
			case INDIRECT:		/* Save and restore the variable lookup for true left-to-right evaluation */
				*next_oprptr = *dir_oprptr;
				dqinit(&tmpchain2, exorder);
				chain2 = setcurtchain(&tmpchain2);
				INSERT_INDSAVGLVN(control_slot, r->operand[0], ANY_SLOT, 1);
				setcurtchain(chain2);
				obp = sav_ref->exorder.bl;	/* insert before second arg */
				dqadd(obp, &tmpchain2, exorder);
				r->operand[0] = control_slot;
				ins_triple(r);
				triptr = newtriple(OC_GLVNPOP);
				triptr->operand[0] = control_slot;
				break;
			default:
				assert(FALSE);
			}
			if (SE_WARN_ON && (TREF(side_effect_base))[TREF(expr_depth)])
				ISSUE_SIDEEFFECTEVAL_WARNING(column - 1);
			DISABLE_SIDE_EFFECT_AT_DEPTH;		/* usual side effect processing doesn't work for $QUERY() */
		}
	}
	if (LOCAL != object)
	{
		if (TBD != direction)
			ins_triple(r);
	} else
	{
		tmpa->oprval.tref->opcode = query_opc[object][direction];
		*a = *tmpa;
	}
	if (NULL != sav_dirref)
	{
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(sav_dirref);
	}
	if (LOCAL != object)
	{
		r->opcode = query_opc[object][direction];		/* finally - the op code */
		if (NULL != oldchain)
			PLACE_GVBIND_CHAIN(&save_state, oldchain); 	/* shift chain back to "expr_start" */
		if (OC_INDFUN == r->opcode)
		{	/* If the direction is known as FORWARD, then use indir_fnquery (which would in turn come
			 * back to "f_query" as FORWARD is the default direction). If direction is BACKWARD, need to
			 * use a separate opcode indir_fnreversequery1 which in turn would invoke "f_reversequery1".
			 */
			*next_oprptr = put_ilit((mint)((FORWARD == direction) ? indir_fnquery : indir_fnreversequery1));
		}
		*a = put_tref(r);
	}
	return TRUE;
}
