/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "zshow.h"
#include "zbreak.h"
#include "stringpool.h"
#include "mvalconv.h"

GBLREF z_records zbrk_recs;
void zshow_zbreaks(zshow_out *output)
{
	zbrk_struct		*z;
	mval			v, temp;
	char			buff[50], *b, *c, *c_top;

	v.mvtype = MV_STR;
	if ((z = (zbrk_struct*)zbrk_recs.beg) == (zbrk_struct*)zbrk_recs.free)
	{	return;
	}
	else
	{
		for (z = (zbrk_struct*)zbrk_recs.beg ;z && z < (zbrk_struct*)zbrk_recs.free ; z++)
		{	v.str.addr = b = &buff[0];
			for (c = &z->lab.c[0], c_top = c + sizeof(mident); c < c_top && *c ; c++)
			{	*b++ = *c;
			}
			if (z->offset)
			{
				*b++ = '+';
				MV_FORCE_MVAL(&temp,z->offset) ;
				n2s(&temp);
				memcpy(b, temp.str.addr, temp.str.len);
				b += temp.str.len;
			}
			*b++ = '^';
			for (c = &z->rtn.c[0], c_top = c + sizeof(mident); c < c_top && *c ; c++)
			{	*b++ = *c;
			}
			v.str.len = b - v.str.addr;

			output->flush = TRUE;
			zshow_output(output,&v.str);
		}
	}
}
