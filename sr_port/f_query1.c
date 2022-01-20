/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
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

/* This function is basically a 1-argument $query call where the direction (2nd argument) is known to be forward query.
 * This is basically a copy of "f_query.c" except some code removed because we know there is no 2nd argument to parse.
 * Any changes here might need to be made in "f_query.c" and vice versa.
 */
int f_query1(oprtype *a, opctype op)
{
	boolean_t	ok;
	enum order_dir	direction;
	enum order_obj	object;
	oprtype		*tmpa, *tmpa0, *tmpa1;
	oprtype		*next_oprptr;
	save_se		save_state;
	triple		*oldchain, *r, *r0, *r1, *sav_dirref, *triptr;
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
		}
		assert(OC_FNQUERY == tmpa->oprval.tref->opcode);
		next_oprptr = &r->operand[1];
		break;
	case TK_CIRCUMFLEX:
		object = GLOBAL;
		ok = gvn();
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
	direction = FORWARD;	/* default direction */
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
		{	/* If the direction is known as FORWARD, then use indir_fnquery1 (which would in turn come
			 * back to "f_query1" as FORWARD is the default direction). If direction is BACKWARD, need to
			 * use a separate opcode indir_fnreversequery1 which in turn would invoke "f_reversequery1".
			 */
			*next_oprptr = put_ilit((mint)((FORWARD == direction) ? indir_fnquery1 : indir_fnreversequery1));
		}
		*a = put_tref(r);
	}
	return TRUE;
}
