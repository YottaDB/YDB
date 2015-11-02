/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF char window_token;
GBLREF spdesc stringpool;
GBLREF bool devctlexp;

#define STO_LLPTR(X) (llptr ? *++llptr = (X) : 0)
#define LITLST_TOP (&litlst[(SIZEOF(litlst) / SIZEOF(triple *)) - 2])
int m_write(void)
{
	error_def(ERR_STRINGOFLOW);
	oprtype x,*oprptr;
	mval lit;
	mstr *msp;
	int  lnx;
	char *cp;
	triple *ref, *t1;
	triple *litlst[128], **llptr, **ptx, **ltop;

	llptr = litlst;
	ltop = 0;
	*llptr = 0;
	for (;;)
	{
		devctlexp = FALSE;
		switch(window_token)
		{
		case TK_ASTERISK:
			advancewindow();
			if (!intexpr(&x))
				return FALSE;
			assert(x.oprclass == TRIP_REF);
			ref = newtriple(OC_WTONE);
			ref->operand[0] = x;
			STO_LLPTR((x.oprval.tref->opcode == OC_ILIT) ? ref : 0);
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
			switch (strexpr(&x))
			{
			case EXPR_FAIL:
				return FALSE;
			case EXPR_GOOD:
				assert(x.oprclass == TRIP_REF);
				if (devctlexp)
				{
					ref = newtriple(OC_WRITE);
					ref->operand[0] = x;
					STO_LLPTR(0);
				} else if (x.oprval.tref->opcode == OC_CAT)
				{
					wrtcatopt(x.oprval.tref,&llptr,LITLST_TOP);
				} else
				{
					ref = newtriple(OC_WRITE);
					ref->operand[0] = x;
					STO_LLPTR((x.oprval.tref->opcode == OC_LIT) ? ref : 0);
				}
				break;
			case EXPR_INDR:
				make_commarg(&x,indir_write);
				STO_LLPTR(0);
				break;
			default:
				assert(FALSE);
			}
			break;
		}
		if (window_token != TK_COMMA)
			break;
		advancewindow();
		if (llptr >= LITLST_TOP)
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
			lit.str.addr = cp = (char * ) stringpool.free;
			for (t1 = ref = *ptx++ ; ref ; ref = *ptx++)
			{
				if (ref->opcode == OC_WRITE)
				{
					msp = &(ref->operand[0].oprval.tref->operand[0].oprval.mlit->v.str);
					lnx = msp->len;
					if ( cp + lnx > (char *) stringpool.top)
					{	stx_error(ERR_STRINGOFLOW);
						return FALSE;
					}
					memcpy(cp, msp->addr, lnx);
					cp += lnx;
				}
				else
				{
					assert(ref->opcode == OC_WTONE);
					if (cp + 1 > (char *) stringpool.top)
					{	stx_error(ERR_STRINGOFLOW);
						return FALSE;
					}
					*cp++ = ref->operand[0].oprval.tref->operand[0].oprval.ilit;
				}
				ref->operand[0].oprval.tref->opcode = OC_NOOP;
				ref->opcode = OC_NOOP;
				ref->operand[0].oprval.tref->operand[0].oprclass = OC_NOOP;
				ref->operand[0].oprclass = 0;
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
