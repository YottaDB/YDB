/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

GBLREF	bool	shift_gvrefs;
GBLREF	char	window_token, director_token;
GBLREF	triple	*expr_start;
GBLREF	mident	window_ident;

error_def(ERR_VAREXPECTED);
error_def(ERR_ORDER2);

static	opctype order_opc[last_obj][last_dir] =
		{
			/* forward	backward	undecided */
			{ OC_GVORDER,	OC_ZPREVIOUS,	OC_GVO2		},	/* global */
			{ OC_FNLVNAME,	OC_FNLVPRVNAME, OC_FNLVNAMEO2	},	/* local_name */
			{ OC_FNORDER,	OC_FNZPREVIOUS, OC_FNO2		},	/* local_sub */
			{ OC_INDFUN,	OC_INDFUN,	OC_INDO2	}	/* indir */
		};


static	bool set_opcode(triple *r, oprtype *result, oprtype *result_ptr, oprtype *second_opr, enum order_obj object)
{
	enum order_dir	direction;
	triple		*s;


	if (window_token == TK_COMMA)
	{
		advancewindow();
		if (!expr(result_ptr))
			return FALSE;

		assert(result_ptr->oprclass == TRIP_REF);
		s = result_ptr->oprval.tref;
		if (s->opcode == OC_LIT)
			if (MV_IS_INT(&s->operand[0].oprval.mlit->v)  &&
			    (s->operand[0].oprval.mlit->v.m[1] == MV_BIAS  ||  s->operand[0].oprval.mlit->v.m[1] == -MV_BIAS))
				direction = s->operand[0].oprval.mlit->v.m[1] == MV_BIAS ? forward : backward;
			else
			{
				stx_error(ERR_ORDER2);
				return FALSE;
			}
		else
			direction = undecided;
	}
	else
		direction = forward;

	switch (object)
	{
	case global:
	case local_name:
		if (direction == undecided)
			*second_opr = *result;
		break;

	case local_sub:
		if (direction == undecided)
		{
			s = newtriple(OC_PARAMETER);
			s->operand[0] = *second_opr;
			s->operand[1] = *result;
			*second_opr = put_tref(s);
		}
		break;

	case indir:
		if (direction == forward)
			*second_opr = put_ilit((mint)indir_fnorder1);
		else
			if (direction == backward)
				*second_opr = put_ilit((mint)indir_fnzprevious);
			else
				*second_opr = *result;
		break;

	default:
		assert(FALSE);
	}

	r->opcode = order_opc[object][direction];

	return TRUE;
}

int f_order(oprtype *a, opctype op)
{
	enum order_obj	object;
	oprtype		result, *result_ptr, *second_opr;
	triple		tmpchain, *oldchain, *r, *triptr;


	result_ptr = (oprtype *)mcalloc(sizeof(oprtype));
	result = put_indr(result_ptr);

	r = maketriple(OC_NOOP);	/* We'll fill in the opcode later, when we figure out what it is */

	switch (window_token)
	{
	case TK_IDENT:
		if (director_token == TK_LPAREN)
		{
			if (!lvn(&r->operand[0], OC_SRCHINDX, r))
				return FALSE;
			object = local_sub;
		}
		else
		{
			r->operand[0] = put_str(&window_ident.c[0], sizeof(mident));
			advancewindow();
			object = local_name;
		}
		second_opr = &r->operand[1];
		break;

	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		object = global;
		second_opr = &r->operand[0];
		break;

	case TK_ATSIGN:
		if (shift_gvrefs)
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&r->operand[0]))
			{
				setcurtchain(oldchain);
				return FALSE;
			}

			if (!set_opcode(r, &result, result_ptr, &r->operand[1], indir))
				return FALSE;
			ins_triple(r);

			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(expr_start, &tmpchain, exorder);
			expr_start = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
			*a = put_tref(r);
			return TRUE;
		}

		if (!indirection(&r->operand[0]))
			return FALSE;

		object = indir;
		second_opr = &r->operand[1];
		break;

	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}

	if (set_opcode(r, &result, result_ptr, second_opr, object))
	{
		ins_triple(r);
		*a = put_tref(r);

		return TRUE;
	}

	return FALSE;
}
