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
#include "patcode.h"
#include "copy.h"

GBLREF uint4    *pattern_typemask;

int do_patfixed(mval *str, mval *pat)
{
	unsigned short int	count, total;
	unsigned short int	*min;
	unsigned char		*p, *ptop;
	short int		i, j;
	unsigned short int	len, length, *r, *rtop, repeat;
	unsigned char		*s, *pstr;
	uint4			code;

	/***************************************************************************/
	/*			      Set up information.			   */
	/***************************************************************************/
	MV_FORCE_STR(str);
	p =  (unsigned char *)pat->str.addr;
	p++;
	p += *p;

	GET_SHORT(count, p);
	p += sizeof(short int);
	GET_SHORT(total, p);
	p += sizeof(short int);

	length = str->str.len;
	if (total != length)
		return FALSE;

	p += sizeof(short int);
	i = sizeof(short int) * count;
	min = (unsigned short int *)p;
	rtop = min + count;

	/***************************************************************************/
	/*			      attempt a match.				   */
	/***************************************************************************/
	s = (unsigned char *)str->str.addr;
	p = (unsigned char *)pat->str.addr + sizeof(short int);

	for (r = min; r < rtop ; r++)
	{
		GET_SHORT(repeat, r);
		code = *p++;
		/*******************************/
		/*  Meta character pat atom.   */
		/*******************************/
		if (code < PATM_STRLIT)
		{
			if (code < PATM_USRDEF)
			{
				/* [KMK] assert(code != 0); */
			} else
			{
				code = GET_LONG(code, (p - 1));
				p += 3;
				code &= PATM_LONGFLAGS;
			}
			for (j = 0; j < repeat ;j++)
			{
				if (!(code & pattern_typemask[*s++]))
					return FALSE;
			}
		}
		/*******************************/
		/*  STRLIT pat atom.	       */
		/*******************************/
		else
		{
			len = *p++;
			if (len == 1)
			{
				for (j=0;j < repeat;j++)
				{
					if (*p != *s++)
					{
						return FALSE;
					}
				}
				p++;
			} else if (len > 0)
			{
				ptop = p+len;
				for (j=0;j < repeat;j++)
				{
					pstr = p;
					while (pstr < ptop)
					{
						if (*pstr++ != *s++)
						{
							return FALSE;
						}
					}
				}
				p += len;
			}
		}
	}
	return TRUE;
}
