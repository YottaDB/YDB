/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"
#include "rwformat.h"

GBLREF spdesc stringpool;
GBLREF bool devctlexp;

error_def(ERR_STRINGOFLOW);

#define STO_LLPTR(X) (llptr ? *++llptr = (X) : 0)
#define LITLST_TOP (&litlst[(SIZEOF(litlst) / SIZEOF(triple *)) - 2])

int m_write(void)
{
	char	*cp;
	int	lnx;
	mval	lit;
	mstr	*msp;
	oprtype	*oprptr, x;
	triple	*litlst[128], **llptr, **ltop, **ptx, *ref, *t1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	llptr = litlst;
	ltop = 0;
	*llptr = 0;
	for (;;)
	{
		devctlexp = FALSE;
		switch (TREF(window_token))
		{
		case TK_ASTERISK:
			advancewindow();
			if (EXPR_FAIL == expr(&x, MUMPS_INT))
				return FALSE;
			assert(TRIP_REF == x.oprclass);
			ref = newtriple(OC_WTONE);
			ref->operand[0] = x;
			STO_LLPTR((OC_ILIT == x.oprval.tref->opcode) ? ref : 0);
			break;
		case TK_QUESTION:
		case TK_EXCLAIMATION:
		case TK_HASH:
		case TK_SLASH:
			if (!rwformat())
				return FALSE;
			STO_LLPTR(0);
			break;
		default:
			switch (expr(&x, MUMPS_STR))
			{
			case EXPR_FAIL:
				return FALSE;
			case EXPR_GOOD:
				assert(TRIP_REF == x.oprclass);
				if (devctlexp)
				{
					ref = newtriple(OC_WRITE);
					ref->operand[0] = x;
					STO_LLPTR(0);
				} else if (x.oprval.tref->opcode == OC_CAT)
					wrtcatopt(x.oprval.tref, &llptr, LITLST_TOP);
				else
				{
					ref = newtriple(OC_WRITE);
					ref->operand[0] = x;
					STO_LLPTR((OC_LIT == x.oprval.tref->opcode) ? ref : 0);
				}
				break;
			case EXPR_INDR:
				make_commarg(&x, indir_write);
				STO_LLPTR(0);
				break;
			default:
				assert(FALSE);
			}
			break;
		}
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		if (LITLST_TOP <= llptr)
		{
			*++llptr = 0;
			ltop = llptr;
			llptr = 0;
		}
	}
	STO_LLPTR(0);
	if (ltop)
		llptr = ltop;
	for (ptx = litlst ; ptx < llptr ; ptx++)
	{
		if (*ptx && *(ptx + 1))
		{
			lit.mvtype = MV_STR;
			lit.str.addr = cp = (char *)stringpool.free;
			for (t1 = ref = *ptx++ ; ref ; ref = *ptx++)
			{
				if (OC_WRITE == ref->opcode)
				{
					msp = &(ref->operand[0].oprval.tref->operand[0].oprval.mlit->v.str);
					lnx = msp->len;
					ENSURE_STP_FREE_SPACE(lnx);
					memcpy(cp, msp->addr, lnx);
					cp += lnx;
				} else
				{
					assert(OC_WTONE == ref->opcode);
					ENSURE_STP_FREE_SPACE(1);
					*cp++ = ref->operand[0].oprval.tref->operand[0].oprval.ilit;
				}
				ref->operand[0].oprval.tref->opcode = OC_NOOP;
				ref->opcode = OC_NOOP;
				ref->operand[0].oprval.tref->operand[0].oprclass = OC_NOOP;
				ref->operand[0].oprclass = NO_REF;
			}
			ptx--;
			stringpool.free = (unsigned char *) cp;
			lit.str.len = INTCAST(cp - lit.str.addr);
			s2n(&lit);
			t1->opcode = OC_WRITE;
			t1->operand[0] = put_lit(&lit);
		}
	}
	return TRUE;
}
