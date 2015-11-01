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


int nm_iscan (mval *v)
{
	boolean_t	can, dot;
	unsigned char 	*c, *eos;

	MV_FORCE_STR(v);
	c = (unsigned char *)v->str.addr;
	eos = (unsigned char *)v->str.addr + v->str.len;
	if (v->str.len == 0)
		return FALSE;
	else  if (v->str.len == 1 && *c == '0')
		return TRUE;
	if (*c == '-')
		c++;
	if (c != eos && *c <= '9' && *c > '0')
	{
		while (c != eos && *c <= '9' && *c >= '0')
			c++;
		dot = c != eos && *c == '.';
		if (dot)
			c++;
	} else  if (c != eos && *c == '.')
	{
		dot = TRUE;
		c++;
		while (c != eos && *c <= '9' && *c >= '0')
			c++;
	} else
		return FALSE;
	while (c != eos && *c <= '9' && *c >= '0')
		c++;
	if (c != eos || (*(c-1) == '0' && dot) || *(c-1) == '.')
		return FALSE;
	return TRUE;
}
