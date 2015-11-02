/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
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
   (NOTE:  this routine is also called from mur_output_show() and the mur_extract_*() routines) */
int real_len(int length, unsigned char *str)
{
	int	clen;	/* current length */
	for (clen = length - 1;  clen >= 0  &&  (str[clen] == ' '  ||  str[clen] == '\0');  --clen)
		;
	return clen + 1;
}

