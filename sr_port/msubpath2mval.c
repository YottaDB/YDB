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

/* msubpath2mval.c	parse msubpath specification into a series of mvals.
 *
 * 	int msubpath2mval(char *inbuff, int maxelem, mval *mvarray)
 *
 *	inbuff:		buffer containing string to be parsed.
 *	maxelem:	number of elements in mvarray
 *	mvarray:	mval array where parsed values are place.
 *
 *	Returns:
 *	non-zero 	number of elements placed in mvarray.
 *	zero		an error occurred (invalid input, empty input,
 *			invalid prefix, invalid numeric literal, etc.)
 *
 *      Parse:
 *	<msubpatharg> ::= @ ( <subscript> [ , <subscript> ]* )
 *
 */

#include "mdef.h"
#include "stringpool.h"
#include "mvalsub.h"

#define STP_PREALLOC(X)    ( stringpool.free + (X) <= stringpool.top \
			      ? stringpool.free \
			      : (stp_gcol(X), stringpool.free) )

GBLREF spdesc	stringpool ;



int msubpath2mval(unsigned char *inbuff, int maxelem, mval *mvarray)
{
    unsigned char *name, *subsc;
    unsigned char *dest;
    int	  mvcount;

    dest = STP_PREALLOC(MAX_STRLEN);

    /* parse off @ */
    if (*inbuff++ != '@')
	return 0;

    mvcount = 0;
    if (*inbuff != '(')
	return 0;

    /* <subscript> ::= { <sublit> | [ + | - ] <numlit>  } */
    do
    {
	inbuff++;
	inbuff = parse_subsc(inbuff,&mvarray[mvcount]);
	if (!inbuff)
	    return 0;
	mvcount++;
    }
    while ( *inbuff == ',' && mvcount < maxelem);

    if (*inbuff != ')')
	return 0;

    return mvcount;
}
