/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
#include "toktyp.h"
#include "advancewindow.h"
#include "stringpool.h"
#include "rwformat.h"

error_def(ERR_COMMAORRPAREXP);
error_def(ERR_CTLMNEXPECTED);
error_def(ERR_NAMEEXPECTED);
error_def(ERR_RWFORMAT);

int rwformat(void)
{
	int	n;
	mval	key;
	oprtype	x;
	triple	*argcnt, *parm, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ref = 0;
	for (;;)
	{
		switch (TREF(window_token))
		{
		case TK_EXCLAIMATION:
			n = 0;
			do
			{
				n++;
				advancewindow();
			} while (TK_EXCLAIMATION == TREF(window_token));
			ref = maketriple(OC_WTEOL);
			ref->operand[0] = put_ilit(n);
			ins_triple(ref);
			break;
		case TK_HASH:
			advancewindow();
			ref = newtriple(OC_WTFF);
			break;
		case TK_QUESTION:
			advancewindow();
			if (EXPR_FAIL == expr(&x, MUMPS_INT))
				return FALSE;
			ref = newtriple(OC_WTTAB);
			ref->operand[0] = x;
			return TRUE;
		case TK_SLASH:
			advancewindow();
			if (TK_IDENT != TREF(window_token))
			{
				stx_error(ERR_CTLMNEXPECTED);
				return FALSE;
			}
			assert(0 < (TREF(window_ident)).len);
			key.mvtype = MV_STR;
			key.str.len = (TREF(window_ident)).len;
			key.str.addr = (TREF(window_ident)).addr;
			CLEAR_MVAL_BITS(&key);
			s2n(&key);
			s2pool(&(key.str));
			argcnt = parm = newtriple(OC_PARAMETER);
			parm->operand[0] = put_lit(&key);
			advancewindow();
			n = 1;
			if (TK_LPAREN == TREF(window_token))
			{
				advancewindow();
				for (;;)
				{
					if ((TK_COMMA == TREF(window_token)) || ((n > 1) && TK_RPAREN == TREF(window_token)))
					{ 	/* language extension - allow empty expr */
						ref = newtriple(OC_NULLEXP);
						x = put_tref(ref);
					} else if (TK_PERIOD == TREF(window_token))
					{ 	/* language extension - allow pass-by-reference */
						advancewindow();
						if (TK_IDENT != TREF(window_token))
						{
							stx_error(ERR_NAMEEXPECTED);
							return FALSE;
						}
						x = put_mvar(&(TREF(window_ident)));
						advancewindow();
					} else if (EXPR_FAIL == expr(&x, MUMPS_EXPR))
						return FALSE;
					n++;
					ref = newtriple(OC_PARAMETER);
					ref->operand[0] = x;
					parm->operand[1] = put_tref(ref);
					parm = ref;
					if (TK_RPAREN == TREF(window_token))
					{
						advancewindow();
						break;
					}
					if (TK_COMMA != TREF(window_token))
					{
						stx_error(ERR_COMMAORRPAREXP);
						return FALSE;
					}
					advancewindow();
				}
			}
			ref = newtriple(OC_IOCONTROL);
			ref->operand[0] = put_ilit(n);
			ref->operand[1] = put_tref(argcnt);
			return TRUE;
		default:
			if (ref)
				return TRUE;
			stx_error(ERR_RWFORMAT);
			return FALSE;
		}
	}
}
