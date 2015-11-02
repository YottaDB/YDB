/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

GBLREF	char		window_token, director_token;
GBLREF	mident		window_ident;
GBLREF	triple		*curtchain;

/* The following are static triples used to pass information between functions "f_order" and "set_opcode" */
STATICDEF triple	*gvo2_savtarg1;	/* Save gv_currkey after processing gvn1 in $ORDER(gvn1,expr) */
STATICDEF triple	*gvo2_savtarg2;	/* Save gv_currkey after processing gvn1 and expr in $ORDER(gvn1,expr) but before executing
					 * the runtime function for $ORDER (OC_GVO2) */
STATICDEF triple	*gvo2_pre_srchindx_triple;	/* the end of the triple chain before OC_SRCHINDX got inserted */

error_def(ERR_VAREXPECTED);
error_def(ERR_ORDER2);

LITDEF	opctype order_opc[last_obj][last_dir] =
{
	/* forward	backward	undecided */
	{ OC_GVORDER,	OC_ZPREVIOUS,	OC_GVO2		},	/* global */
	{ OC_FNLVNAME,	OC_FNLVPRVNAME, OC_FNLVNAMEO2	},	/* local_name */
	{ OC_FNORDER,	OC_FNZPREVIOUS, OC_FNO2		},	/* local_sub */
	{ OC_INDFUN,	OC_INDFUN,	OC_INDO2	}	/* indir */
};

STATICFNDEF boolean_t set_opcode(triple *r, oprtype *result, oprtype *result_ptr, oprtype *second_opr, enum order_obj object)
{
	enum order_dir	direction;
	triple		*s;
	triple		tmpchain, *oldchain, *x, *tp, *tmptriple, *gvo2_post_srchindx_triple, *t1, *t2;
	int4		dummy_intval;

	if (window_token == TK_COMMA)
	{
		advancewindow();
		if (local_sub == object)
			gvo2_post_srchindx_triple = curtchain->exorder.bl;
		if (global == object)
		{	/* Prepare for OC_GVSAVTARG/OC_GVRECTARG processing in case second argument has global references.
			 * If the first argument to $ORDER is a global variable and the second argument is a literal,
			 *	then the opcodes generated are (in that order)
			 *
			 *	OC_GVNAME, EXPR, OC_GVO2
			 *
			 * But if the first argument is a global variable and the second argument is an expression that is
			 *	not a literal, then the opcodes generated are (in that order)
			 *
			 *	OC_GVNAME, OC_SAVTARG1, EXPR, OC_SAVTARG2, OC_RECTARG1, OC_GVO2, OC_RECTARG2
			 *
			 * Note that OC_SAVTARG1 and OC_SAVTARG2 are the same opcode OC_SAVTARG but are placeholder indicators.
			 * Similarly OC_RECTARG1 and OC_RECTARG2.
			 *
			 * This opcode order ensures that OC_GVO2 is presented the right gv_currkey on entry into function
			 * "op_gvo2" as well as ensure that after the $ORDER returns, the naked indicator is set correctly.
			 */
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
		}
		if (!expr(result_ptr))
		{
			if (global == object)
				setcurtchain(oldchain);
			return FALSE;
		}
		assert(TRIP_REF == result_ptr->oprclass);
		s = result_ptr->oprval.tref;
		if (OC_LIT == s->opcode)
		{
			if (MV_IS_TRUEINT(&s->operand[0].oprval.mlit->v, &dummy_intval)
					&& ((MV_BIAS == s->operand[0].oprval.mlit->v.m[1])
						||  (-MV_BIAS == s->operand[0].oprval.mlit->v.m[1])))
				direction = (MV_BIAS == s->operand[0].oprval.mlit->v.m[1]) ? forward : backward;
			else
			{
				if (global == object)
					setcurtchain(oldchain);
				stx_error(ERR_ORDER2);
				return FALSE;
			}
			if (global == object)
			{	/* No need for OC_GVSAVTARG/OC_GVRECTARG processing as expr is a constant (no global references) */
				setcurtchain(oldchain);
				tmptriple = curtchain->exorder.bl;
				dqadd(tmptriple, &tmpchain, exorder);
			}
		} else
		{
			direction = undecided;
			if (global == object)
			{	/* Need to do OC_GVSAVTARG/OC_GVRECTARG processing as expr could contain global references */
				assert(OC_GVO2 == order_opc[object][direction]);
				setcurtchain(oldchain);
				/* Note down the value of gv_currkey at this point */
				newtriple(OC_GVSAVTARG);
				/* Add second argument triples */
				gvo2_savtarg1 = curtchain->exorder.bl;
				tmptriple = curtchain->exorder.bl;
				dqadd(tmptriple, &tmpchain, exorder);
				/* Note down the value of gv_currkey at this point */
				newtriple(OC_GVSAVTARG);
				gvo2_savtarg2 = curtchain->exorder.bl;
			}
		}
	} else
		direction = forward;

	switch (object)
	{
		case global:
			if (direction == undecided)
				*second_opr = *result;
			break;
		case local_name:
			if (direction == undecided)
				*second_opr = *result;
			else if (direction == forward)
			{	/* The op_fnlvname rtn needs an extra parm - insert it now */
				assert(OC_FNLVNAME == order_opc[object][direction]);
				*second_opr = put_ilit(0);	/* Flag not to return aliases with no value */
			}
			break;

		case local_sub:
			if (direction == undecided)
			{	/* This is $ORDER(subscripted-local-variable, expr). The normal order of evaluation would be
				 *
				 * 1) Evaluate subscripts of local variable
				 * 2) Do OC_SRCHINDX
				 * 3) Evaluate expr
				 * 4) Do OC_FNORDER
				 *
				 * But it is possible that the subscripted local-variable is defined only by an extrinsic function
				 * that is part of "expr". In that case, we should NOT do the OC_SRCHINDX before "expr" gets
				 * evaluated (as otherwise OC_SRCHINDX will not return the right lv_val structure). That is, the
				 * order of evaluation should be
				 *
				 * 1) Evaluate subscripts of local variable
				 * 2) Evaluate expr
				 * 3) Do OC_SRCHINDX
				 * 4) Do OC_FNORDER
				 *
				 * The triples need to be reordered accordingly to implement the above evaluation order.
				 * This reordering of triples is implemented below by recording the end of the triple chain
				 * just BEFORE (variable "gvo2_pre_srchindx_triple") and just AFTER (variable
				 * "gvo2_post_srchindx_triple") parsing the subscripted-local-variable first argument to
				 * $ORDER. This is done partly in the function "f_order" and partly in "set_opcode". Once these
				 * are recorded, the second argument "expr" is parsed and the triples generated. After this, we
				 * start from gvo2_post_srchindx_triple and go back the triple chain until we find the OC_SRCHINDX
				 * opcode or gvo2_pre_srchindx_triple whichever is earlier (e.g. for unsubscripted names
				 * OC_SRCHINDX triple is not generated). This portion of the triple chain (that does the
				 * OC_SRCHINDX computation) is deleted and added at the end of the current triple chain. This
				 * accomplishes the desired evaluation reordering. Note that the value of the naked indicator is
				 * not affected by this reordering (since OC_SRCHINDX does not do global references).
				 */
				for (tmptriple = gvo2_post_srchindx_triple; (OC_SRCHINDX != tmptriple->opcode);
				     tmptriple = tmptriple->exorder.bl)
				{
					if (tmptriple == gvo2_pre_srchindx_triple)
						break;
				}
				if (OC_SRCHINDX == tmptriple->opcode)
				{
					t1 = tmptriple->exorder.bl;
					t2 = gvo2_post_srchindx_triple->exorder.fl;
					dqdelchain(t1,t2,exorder);
					dqinit(&tmpchain, exorder);
					tmpchain.exorder.fl = tmptriple;
					tmpchain.exorder.bl = gvo2_post_srchindx_triple;
					gvo2_post_srchindx_triple->exorder.fl = &tmpchain;
					tmptriple->exorder.bl = &tmpchain;
					tmptriple = curtchain->exorder.bl;
					dqadd(tmptriple, &tmpchain, exorder);
				}
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	result_ptr = (oprtype *)mcalloc(SIZEOF(oprtype));
	result = put_indr(result_ptr);
	r = maketriple(OC_NOOP);	/* We'll fill in the opcode later, when we figure out what it is */
	switch (window_token)
	{
		case TK_IDENT:
			if (director_token == TK_LPAREN)
			{	/* See comment in "set_opcode" for why we maintain "gvo2_pre_srchindx_triple" here */
				gvo2_pre_srchindx_triple = curtchain->exorder.bl;
				if (!lvn(&r->operand[0], OC_SRCHINDX, r))
					return FALSE;
				object = local_sub;
			} else
			{
				r->operand[0] = put_str(window_ident.addr, window_ident.len);
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
			if (TREF(shift_side_effects))
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
				dqadd(TREF(expr_start), &tmpchain, exorder);
				TREF(expr_start) = tmpchain.exorder.bl;
				triptr = newtriple(OC_GVRECTARG);
				triptr->operand[0] = put_tref(TREF(expr_start));
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
	{	/* Restore gv_currkey of the first argument (in case the second expression contained a global reference).
		 * This will ensure op_gvo2 has gv_currkey set properly on entry */
		if (OC_GVO2 == r->opcode)
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(gvo2_savtarg1);
			ins_triple(r);
			/* Restore gv_currkey to what it was after evaluating the second argument (to preserved naked indicator) */
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(gvo2_savtarg2);
		} else
			ins_triple(r);
		*a = put_tref(r);
		return TRUE;
	}
	return FALSE;
}
