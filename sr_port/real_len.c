/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "real_len.h"

/* A utility routine to compute the length of a string, exclusive of trailing blanks or nuls
 * (NOTE:  this routine is also called from mur_output_show() and the mur_extract_*() routines)
 */
int real_len(int length, uchar_ptr_t str)
{
	int		clen;	/* current length */

	/* Find first 'nul' in string */
	for (clen = 0; (clen < length) && ('\0' != str[clen]); clen++)
		;
	/* Remove trailing blanks just before the 'nul' point */
	if (clen)
	{
		for (clen--; (clen >= 0) && (' ' == str[clen]); clen--)
			;
		clen++;
	}
	return clen;
}
