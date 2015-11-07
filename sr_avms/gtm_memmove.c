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

/*
 * ---------------------------------------------------
 * memmove() - Move memory.
 *	This routine may replace any existing memcpy()
 *	calls. It correctly handles overlapping memory
 *	regions.
 *
 * Arguments:
 *	same as memcpy()
 *	pdest	- pointer to destination
 *	psrc	- pointer to source
 *	cnt	- # of bytes to copy
 * Return:
 *	pointer to destination
 * ---------------------------------------------------
 */

#include "mdef.h"

char_ptr_t gtm_memmove(char_ptr_t pdest,
		   char_ptr_t psrc,
		   int cnt)
{
	register char_ptr_t src, dst;

	assert((0 <= cnt) && (cnt <= MAXPOSINT4));	/* nothing beyond max positive int4 allowed */
	src = psrc;
	dst = pdest;
	if (cnt && dst != src)
	{
		if (src < dst && src + cnt > dst)
		{
			/* Overlapping region, downward copy, copy backwards */
			dst += cnt;
			src += cnt;
			while (cnt-- > 0)
				*--dst = *--src;
		}
		else
		{
			while (cnt-- > 0)
				*dst++ = *src++;
		}
	}
	return (pdest);
}
