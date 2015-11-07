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

#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "fnorder.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "mvalconv.h"
#include "fullbool.h"
#include "glvn_pool.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;
GBLREF	short int	source_column;

error_def(ERR_ORDER2);
error_def(ERR_SIDEEFFECTEVAL);
error_def(ERR_VAREXPECTED);

LITDEF	opctype order_opc[LAST_OBJECT][LAST_DIRECTION] =
{
	/* FORWARD	BACKWARD	TBD */
	{ OC_GVORDER,	OC_ZPREVIOUS,	OC_GVO2		},	/* GLOBAL */
	{ OC_FNORDER,	OC_FNZPREVIOUS, OC_FNO2		},	/* LOCAL */
	{ OC_FNLVNAME,	OC_FNLVPRVNAME, OC_FNLVNAMEO2	},	/* LOCAL_NAME */
	{ OC_INDFUN,	OC_INDFUN,	OC_INDO2	}	/* INDIRECT */
};

int f_order(oprtype *a, opctype op)
{
	boolean_t	ok, used_glvn_slot;
	enum order_dir	direction;
	enum order_obj	object;
	int4		intval;
	opctype		gv_oc;
	oprtype		control_slot, dir_opr, *dir_oprptr, *next_oprptr;
	short int	column;
	triple		*oldchain, *r, *sav_dirref, *sav_gv1, *sav_gvn, *sav_lvn, *sav_ref, *share, *triptr;
	triple		*chain2, *obp, tmpchain2;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	oldchain = sav_dirref = NULL;			/* default to no direction and no shifting indirection */
	used_glvn_slot = FALSE;
	sav_gv1 = TREF(curtchain);
	r = maketriple(OC_NOOP);			/* We'll fill in the opcode later, when we figure out what it is */
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (TK_LPAREN == TREF(director_token))
		{
			object = LOCAL;
			ok = lvn(&r->operand[0], OC_SRCHINDX, r);	/* 2nd arg causes us to mess below with return from lvn */
		} else
		{
			object = LOCAL_NAME;
			ok = TRUE;
			r->operand[0] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
			advancewindow();
		}
		next_oprptr = &r->operand[1];
		break;
	case TK_CIRCUMFLEX:
		object = GLOBAL;
		ok = gvn();
		sav_gvn = (TREF(curtchain))->exorder.bl;
		next_oprptr = &r->operand[0];
		break;
	case TK_ATSIGN:
		object = INDIRECT;
		if (SHIFT_SIDE_EFFECTS)
			START_GVBIND_CHAIN(&save_state, oldchain);
		ok = indirection(&r->operand[0]);
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
		DISABLE_SIDE_EFFECT_AT_DEPTH;		/* doing this here let's us know specifically if direction had SE threat */
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
				stx_error(ERR_ORDER2);
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
				break;
			case LOCAL:		/* Additionally need to move srchindx triple to after potential side effect */
				triptr = newtriple(OC_PARAMETER);
				triptr->operand[0] = *next_oprptr;
				triptr->operand[1] = *(&dir_opr);
				*next_oprptr = put_tref(triptr);
				sav_lvn = r->operand[0].oprval.tref;
				assert((OC_SRCHINDX == sav_lvn->opcode) || (OC_VAR == sav_lvn->opcode));
				if (OC_SRCHINDX == sav_lvn->opcode)
				{
					dqdel(sav_lvn, exorder);
					ins_triple(sav_lvn);
					TREF(temp_subs) = TRUE;
					create_temporaries(sav_lvn, OC_SRCHINDX);
				}
				assert(&r->operand[1] == next_oprptr);
				assert(TRIP_REF == next_oprptr->oprclass);
				assert(OC_PARAMETER == next_oprptr->oprval.tref->opcode);
				assert(TRIP_REF == next_oprptr->oprval.tref->operand[0].oprclass);
				sav_lvn = next_oprptr->oprval.tref->operand[0].oprval.tref;
				if ((OC_VAR == sav_lvn->opcode) || (OC_GETINDX == sav_lvn->opcode))
				{	/* lvn excludes the last subscript from srchindx and attaches it to the "parent"
					 * now we find it is an lvn and needs protection too
					 */
					triptr = maketriple(OC_STOTEMP);
					triptr->operand[0] = put_tref(sav_lvn);
					dqins(sav_lvn, exorder, triptr);		/* NOTE: violation of info hiding */
					next_oprptr->oprval.tref->operand[0].oprval.tref = triptr;
				}
				break;
			case INDIRECT:		/* Save and restore the variable lookup for true left-to-right evaluation */
				*next_oprptr = *dir_oprptr;
				used_glvn_slot = TRUE;
				dqinit(&tmpchain2, exorder);
				chain2 = setcurtchain(&tmpchain2);
				INSERT_INDSAVGLVN(control_slot, r->operand[0], ANY_SLOT, 1);
				setcurtchain(chain2);
				obp = sav_ref->exorder.bl;	/* insert before second arg */
				dqadd(obp, &tmpchain2, exorder);
				r->operand[0] = control_slot;
				break;
			case LOCAL_NAME:	/* left argument is a string - side effect can't screw it up */
				*next_oprptr = *dir_oprptr;
				break;
			default:
				assert(FALSE);
			}
			ins_triple(r);
			if (used_glvn_slot)
			{
				triptr = newtriple(OC_GLVNPOP);
				triptr->operand[0] = control_slot;
			}
			if (SE_WARN_ON && (TREF(side_effect_base))[TREF(expr_depth)])
				ISSUE_SIDEEFFECTEVAL_WARNING(column - 1);
			DISABLE_SIDE_EFFECT_AT_DEPTH;		/* usual side effect processing doesn't work for $ORDER() */
		}
	}
	if (TBD != direction)
		ins_triple(r);
	if (NULL != sav_dirref)
	{
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(sav_dirref);
	}
	r->opcode = order_opc[object][direction];		/* finally - the op code */
	if (NULL != oldchain)
		PLACE_GVBIND_CHAIN(&save_state, oldchain); 	/* shift chain back to "expr_start" */
	if (OC_FNLVNAME == r->opcode)
		*next_oprptr = put_ilit(0);			/* Flag not to return aliases with no value */
	if (OC_INDFUN == r->opcode)
		*next_oprptr = put_ilit((mint)((FORWARD == direction) ? indir_fnorder1 : indir_fnzprevious));
	*a = put_tref(r);
	return TRUE;
}
