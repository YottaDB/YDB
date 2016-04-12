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
#include "rc_iscan.h"

#define RC_MAX_LEN 15

bool	rc_iscan (mval *v)
{	bool	can, dot ;
	char 	*c, *eos, *c1;

	c = v->str.addr ;
	eos = v->str.addr + v->str.len ;
	if ( v->str.len == 0 )
	{
		return FALSE ;
	}
	else if ( v->str.len == 1 && *c == '0' )
	{
		return TRUE ;
	}
	if ( *c == '-' )
		c++ ;
	if (v->str.len > RC_MAX_LEN)
	{	for (c1 = eos - 1; *c1 == '0'; c1--)
			;
		if (c1 - c > RC_MAX_LEN)
			return FALSE;
	}
	if ( c != eos && *c <= '9' && *c > '0' )
	{
		while ( c != eos && *c <= '9' && *c >= '0' ) c++ ;
		dot = c != eos && *c == '.' ;
		if ( dot )
			c++ ;
	}
	else if ( c != eos && *c == '.' )
	{
		dot = TRUE ; c++ ;
		while ( c != eos && *c <= '9' && *c >= '0' ) c++ ;
	}
	else
	{
		return FALSE ;
	}
	while ( c != eos && *c <= '9' && *c >= '0' ) c++ ;
	if ( c != eos || (*(c-1) == '0' && dot) || *(c-1) == '.' )
	{
		return FALSE ;
	}
	return TRUE ;
}
