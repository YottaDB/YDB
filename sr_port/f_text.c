/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
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
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "cmd_qlf.h"
#include "advancewindow.h"
#include "gtm_caseconv.h"
#include "gtm_string.h"
#include "gtm_common_defs.h"

static readonly mstr zero_mstr;

GBLREF boolean_t	run_time;
GBLREF command_qualifier cmd_qlf;
GBLREF mident		routine_name;

STATICDEF char 		*suppressed_values[] = {"GTM$DMOD", "GTM$CI"};

error_def(ERR_RPARENMISSING);
error_def(ERR_RTNNAME);
error_def(ERR_TEXTARG);

int f_text(oprtype *a, opctype op)
{
	char	*c;
	int	implicit_offset = 0, len;
	triple	*label, *r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	switch (TREF(window_token))
	{
	case TK_CIRCUMFLEX:
		implicit_offset = 1;
		/* CAUTION - fall-through */
	case TK_PLUS:
		r->operand[0] = put_str(zero_mstr.addr, 0);	/* Null label - top of routine */
		break;
	case TK_INTLIT:
		int_label();
		/* CAUTION - fall through */
	case TK_IDENT:
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)(TREF(window_ident)).addr, (uchar_ptr_t)(TREF(window_ident)).addr,
				(TREF(window_ident)).len);
		r->operand[0] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
		advancewindow();
		break;
	case TK_ATSIGN:
		if (!indirection(&(r->operand[0])))
			return FALSE;
		r->opcode = OC_INDTEXT;
		break;
	default:
		stx_error(ERR_TEXTARG);
		return FALSE;
	}
	/* The assert below can be useful when working on $TEXT parsing issues but causes problems in debug builds with
	 * bad syntax asserts. Hence it is normally commented out. Uncomment to re-enable for $TEXT parsing issues.
	 */
	/* assert((TK_PLUS == TREF(window_token)) || (TK_CIRCUMFLEX == TREF(window_token)) || (TK_RPAREN == TREF(window_token))
	 *	|| (TK_EOL == TREF(window_token)));
	 */
	if ((OC_INDTEXT != r->opcode) || (TK_PLUS == TREF(window_token)) || (TK_CIRCUMFLEX == TREF(window_token)))
	{	/* Need another parm chained in to deal with offset and routine name except for the case where an
		 * indirect specifies the entire argument.
		 */
		label = newtriple(OC_PARAMETER);
		r->operand[1] = put_tref(label);
	}
	if (TK_PLUS != TREF(window_token))
	{
		if ((OC_INDTEXT != r->opcode) || (TK_CIRCUMFLEX == TREF(window_token)))
			/* Set default offset (0 or 1 as computed above) when offset not specified */
			label->operand[0] = put_ilit(implicit_offset);
		else
		{	/* Fill in indirect text for case where indirect specifies entire operand */
			r->opcode = OC_INDFUN;
			r->operand[1] = put_ilit((mint)indir_fntext);
		}
	} else
	{	/* Process offset */
		advancewindow();
		if (EXPR_FAIL == expr(&(label->operand[0]), MUMPS_INT))
			return FALSE;
	}
	if (TK_CIRCUMFLEX != TREF(window_token))
	{	/* No routine specified - default to current routine */
		if (OC_INDFUN != r->opcode)
			label->operand[1] = PUT_CURRENT_RTN; /* tell op_fntext to pick up current routine version */
	} else
	{	/* Routine has been specified - pull it */
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_IDENT:
#			ifdef GTM_TRIGGER
			if (TK_HASH == TREF(director_token))
				/* Coagulate tokens as necessary (and available) to allow '#' in the routine name */
				advwindw_hash_in_mname_allowed();
#			endif
			if (TK_DOLLAR == TREF(director_token))		/* the item has a $ in it */
			{	/*  violate information hiding to special case illegal names GT.M can return from $STACK() et al */
				c = TREF(lexical_ptr) - STR_LIT_LEN("GTM$");
				advancewindow();			/* parse to $ */
				if (0 == memcmp(c, "GTM$", STR_LIT_LEN("GTM$")))
				{	/* parse past GTM$DMOD or GTM$CI to prevent RPARENMISSING error */
					advancewindow();		/* parse to end of ident */
					len = TREF(lexical_ptr) - c - (TK_EOL == TREF(director_token) ? 0 : 1);
					for (implicit_offset = 0; ARRAYSIZE(suppressed_values) > implicit_offset; implicit_offset++)
					{	 /* reuse of implicit_offset */
						if ((STRLEN(suppressed_values[implicit_offset]) == len)
								&& (0 == memcmp(c, suppressed_values[implicit_offset], len)))
						{
							label->operand[1] = put_str(suppressed_values[implicit_offset], len);
							break;
						}
					}
					if (ARRAYSIZE(suppressed_values) == implicit_offset)
						(TREF(last_source_column))--;	/* if no match (error) adjust for extra parse */
				} else
					implicit_offset = ARRAYSIZE(suppressed_values);
				if (ARRAYSIZE(suppressed_values) == implicit_offset)
				{	/* give the error that would arise had we just ignored the $ */
					stx_error(ERR_RPARENMISSING);
					return FALSE;
				}
			} else
				label->operand[1] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&label->operand[1]))
				return FALSE;
			r->opcode = OC_INDTEXT;
			break;
		default:
			stx_error(ERR_RTNNAME);
			return FALSE;
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
