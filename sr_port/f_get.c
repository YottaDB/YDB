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
#include "mmemory.h"
#include "mdq.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "glvn_pool.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;
GBLREF	short int	source_column;

error_def(ERR_SIDEEFFECTEVAL);
error_def(ERR_VAREXPECTED);

int f_get(oprtype *a, opctype op)
{
	boolean_t	ok, used_glvn_slot;
	oprtype		control_slot, def_opr, *def_oprptr, indir;
	short int	column;
	triple		*oldchain, *opptr, *r, *triptr;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	oldchain = NULL;
	used_glvn_slot = FALSE;
	r = maketriple(OC_NOOP);		/* We'll fill in the opcode later, when we figure out what it is */
	switch (TREF(window_token))
	{
	case TK_IDENT:
		r->opcode = OC_FNGET;
		ok = lvn(&r->operand[0], OC_SRCHINDX, 0);
		break;
	case TK_CIRCUMFLEX:
		r->opcode = OC_FNGVGET;
		ok = gvn();
		break;
	case TK_ATSIGN:
		r->opcode = OC_INDGET2;
		if (SHIFT_SIDE_EFFECTS)
			START_GVBIND_CHAIN(&save_state, oldchain);
		if (ok = indirection(&indir))	/* NOTE: assignment */
		{
			used_glvn_slot = TRUE;
			INSERT_INDSAVGLVN(control_slot, indir, ANY_SLOT, 1);
			r->operand[0] = control_slot;
		}
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
	opptr = r;
	ins_triple(r);
	if (used_glvn_slot)
	{	/* house cleaning for the indirection */
		triptr = newtriple(OC_GLVNPOP);
		triptr->operand[0] = control_slot;
	}
	if (TK_COMMA == TREF(window_token))
	{	/* two argument form with a specified default value */
		advancewindow();
		column = source_column;
		def_oprptr = (oprtype *)mcalloc(SIZEOF(oprtype));
		def_opr = put_indr(def_oprptr);
		DISABLE_SIDE_EFFECT_AT_DEPTH;		/* doing this here let's us know specifically if direction had SE threat */
		if (EXPR_FAIL == expr(def_oprptr, MUMPS_EXPR))
		{
			if (NULL != oldchain)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (SE_WARN_ON && (TREF(side_effect_base))[TREF(expr_depth)])
			ISSUE_SIDEEFFECTEVAL_WARNING(column - 1);
		if (OC_FNGET == r->opcode)
			r->opcode = OC_FNGET1;
		else if (OC_FNGVGET == r->opcode)
			r->opcode = OC_FNGVGET1;
		else
			assert(OC_INDGET2 == r->opcode);
		r = newtriple(OC_FNGET2);
		r->operand[0] = put_tref(opptr);
		r->operand[1] = def_opr;
	} else if (OC_INDGET2 == r->opcode)
	{	/* indirect always acts like a two argument form so force that along with an empty string default */
		r = newtriple(OC_FNGET2);
		r->operand[0] = put_tref(opptr);
		r->operand[1] = put_str(0, 0);
	}
	if (NULL != oldchain)
		PLACE_GVBIND_CHAIN(&save_state, oldchain); 	/* shift chain back to "expr_start" */
	*a = put_tref(r);
	return TRUE;
}
