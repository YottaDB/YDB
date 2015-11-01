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

/* mpath2mval.c		parse mpath specification into a series of mvals.
 *
 * 	int mpath2mval(char *inbuff, int maxelem, mval *mvarray)
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
 *	<mpatharg> ::= ^ <name> [ ( <subscript> [ , <subscript> ]* ) ]
 *
 */

#include "mdef.h"
#include "stringpool.h"
#include "mvalsub.h"

#define STP_PREALLOC(X)    ( stringpool.free + (X) <= stringpool.top \
			      ? stringpool.free \
			      : (stp_gcol(X), stringpool.free) )

GBLREF spdesc	stringpool ;

int mpath2mval(unsigned char *inbuff, int maxelem, mval *mvarray)
{
    unsigned char *name, *subsc;
    unsigned char *dest;
    int	  mvcount;

    dest = STP_PREALLOC(MAX_STRLEN);

    /* parse off ^ <name> */
    if (*inbuff++ != '^')
	return 0;

    /* <name> ::= { % | <alpha> } [ alpha | digit ]* */
    name = inbuff;
    if (*inbuff == '%' || ISALPHA(*inbuff))
    {
	*dest++ = *inbuff++;
	while (ISALPHA(*inbuff) || ISDIGIT(*inbuff))
	    *dest++ = *inbuff++;
	mvarray[0].mvtype = MV_STR;
	mvarray[0].str.addr = (char *) stringpool.free;
	mvarray[0].str.len = dest - stringpool.free;
	*dest++ = '\0';
	stringpool.free = dest;
    }
    else
	return 0;

    mvcount = 1;
    if (*inbuff != '(')
	return mvcount;

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
