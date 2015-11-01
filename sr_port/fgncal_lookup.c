/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "toktyp.h"
#include "rtnhdr.h"
#include "fgncal.h"
#include "underr.h"

LITREF	char	ctypetab[NUM_ASCII_CHARS];
GBLREF	symval	*curr_symval;

mval *fgncal_lookup(mval *x)
{
	char		new, len, y, in, *i, *i_top, *c, *c_top;
	mident		name;
	ht_entry	*q;
	mval		*ret_val;

	MV_FORCE_DEFINED(x);
	ret_val = (mval *) 0;

	len = x->str.len;
	if (len)
	{
		len = (len > sizeof(mident)) ? sizeof(mident) : len;
		i = (char *)&name;
		i_top = i + sizeof(name);
		c = x->str.addr;
		c_top = c + len;
		if ((in = *c++) > 0 && in <= 127)
		{
			y = ctypetab[*i++ = in];
			if (y == TK_UPPER || y == TK_LOWER || y == TK_PERCENT)
			{
				for ( ; c < c_top; c++,i++)
				{	if ((in = *c) <= 0 || in > 127)
						break;
					y = ctypetab[*i = in];
					if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
						break;
				}
				if (c == c_top)
				{	/* we have an ident */
					for (; i < i_top; i++)
						*i = 0;
					q = ht_put(&curr_symval->h_symtab , (mname *)&name , &new);
					if (new)
					{	lv_newname(q, curr_symval);
					}
					ret_val = (mval *) q->ptr;
				}
			}
		}
	}
	return ret_val;
}
