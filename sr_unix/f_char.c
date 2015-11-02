/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
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
#include "toktyp.h"
#include "stringpool.h"
#include "advancewindow.h"
#include "gtm_utf8.h"

GBLREF	spdesc		stringpool;
GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;

error_def(ERR_FCHARMAXARGS);
error_def(ERR_INVDLRCVAL);

int f_char(oprtype *a, opctype op)
{
	boolean_t 	all_lits;
	unsigned char 	*base, *outptr, *tmpptr;
	int 		argc, ch, char_len, size;
	mval		v;
	oprtype 	*argp, argv[CHARMAXARGS];
	triple 		*curr, *last, *root;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If we are not in UTF8 mode, we need to reroute to the $ZCHAR function to handle things correctly */
	if (!gtm_utf8_mode)
		return f_zchar(a, op);
	all_lits = TRUE;
	argp = &argv[0];
	argc = 0;
	for (;;)
	{
		if (EXPR_FAIL == expr(argp, MUMPS_INT))
			return FALSE;
		assert(TRIP_REF == argp->oprclass);
		if (OC_ILIT != argp->oprval.tref->opcode)
			all_lits = FALSE;
		argc++;
		argp++;
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		if (CHARMAXARGS <= argc)
		{
			stx_error(ERR_FCHARMAXARGS);
			return FALSE;
		}
	}
	if (all_lits)
	{	/* All literals, build the function inline */
		size = argc * GTM_MB_LEN_MAX;
		ENSURE_STP_FREE_SPACE(size);
		base = stringpool.free;
		argp = &argv[0];
		for (outptr = base, char_len = 0; argc > 0; --argc, argp++)
		{	/* For each wide char value, convert to unicode chars in stringpool buffer */
			ch = argp->oprval.tref->operand[0].oprval.ilit;
			if (0 <= ch)
			{ /* As per the M standard, negative code points should map to no characters */
				tmpptr = UTF8_WCTOMB(ch, outptr);
				assert(tmpptr - outptr <= 4);
				if (tmpptr != outptr)
					++char_len; /* yet another valid character. update the character length */
				else if (!badchar_inhibit)
					stx_error(ERR_INVDLRCVAL, 1, ch);
				outptr = tmpptr;
			}
		}
		stringpool.free = outptr;
		MV_INIT_STRING(&v, outptr - base, base);
		v.str.char_len = char_len;
		v.mvtype |= MV_UTF_LEN;
		s2n(&v);
		*a = put_lit(&v);
		return TRUE;
	}
	root = maketriple(op);
	root->operand[0] = put_ilit(argc + 1);
	last = root;
	argp = &argv[0];
	for (; argc > 0 ;argc--, argp++)
	{
		curr = newtriple(OC_PARAMETER);
		curr->operand[0] = *argp;
		last->operand[1] = put_tref(curr);
		last = curr;
	}
	ins_triple(root);
	*a = put_tref(root);
	return TRUE;
}
