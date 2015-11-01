/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "toktyp.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "indir_enum.h"
#include "cmd_qlf.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "do_indir_do.h"

LITREF	char			ctypetab[NUM_ASCII_CHARS];
GBLREF	stack_frame		*frame_pointer;
GBLREF	command_qualifier	cmd_qlf;
GBLREF	boolean_t		is_tracing_on;

int do_indir_do(mval *v, unsigned char argcode)
{
	mval		label;
	int4		y;
	char		*i, *i_top, *c, *c_top;
	LNR_TABENT	USHBIN_ONLY(*)*addr;
	mident		ident;
	rhdtyp		*current_rhead;

	i = (char *) &ident;
	i_top = i + sizeof(ident);
	c = v->str.addr;
	c_top = c + v->str.len;
	switch (y = ctypetab[*i++ = *c++])
	{
		case TK_LOWER:
		case TK_PERCENT:
		case TK_UPPER:
		case TK_DIGIT:
			for ( ; c < c_top; c++,i++)
			{
				y = ctypetab[*i = *c];
				if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
					break;
			}
			if (c == c_top)
			{	/* we have an ident */
				if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
					lower_to_upper((uchar_ptr_t)ident.c, (uchar_ptr_t)ident.c, sizeof(mident));
				label.mvtype = MV_STR;
				label.str.len = i - (char*)&ident;
				label.str.addr = (char *) &ident;
				addr = op_labaddr(frame_pointer->rvector, &label, 0);
				if (argcode == indir_do)
				{
					if (is_tracing_on)
						exfun_frame_sp();
					else
						exfun_frame();
				}
				current_rhead = CURRENT_RHEAD_ADR(frame_pointer->rvector);
				frame_pointer->mpc = LINE_NUMBER_ADDR(current_rhead, USHBIN_ONLY(*)addr);
#if defined(__alpha) || defined(__MVS__) || defined(__s390__) || defined(_AIX)
				frame_pointer->ctxt = (unsigned char *)LINKAGE_ADR(current_rhead);
#else
				frame_pointer->ctxt = PTEXT_ADR(current_rhead);
#endif
				return TRUE;
			} else
				return FALSE;
			break;
		default:
			return FALSE;
	}
}
