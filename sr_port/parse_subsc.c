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

/* parse_subsc.c	validate and parse MUMPS subscript into an mval.
 *
 * 	unsigned char *parse_subsc(unsigned char *inbuff, mval *v)
 *
 * 	inbuff:		buffer containing string to be parsed.
 * 	v:		pointer to mval to be assigned subscript value.
 *
 *
 * Returns:
 *	non-zero	inbuff + n, where n is the number of characters
 *			parsed to obtain the subscript.
 *
 *	zero		invalid subscript
 *
 * Parses:
 *      <subscript> ::= { <sublit> | [ + | - ] <numlit>  }
 *
 * NOTE: This routine assumes that stringpool.free points to an area
 *       which contains enough space to hold the largest possible subscript.
 *       The routines that call this one (mpath2mval() and msubpath2mval() )
 *	 ensure that this is the case.
 */

#include "mdef.h"
#include "stringpool.h"
#include "mvalsub.h"

GBLREF spdesc	stringpool ;

unsigned char *parse_subsc(unsigned char *inbuff,mval *v)
{
    unsigned char *dest;

    dest = stringpool.free;
    /* <subscript> ::= { <sublit> | [ + | - ] <numlit>  } */

    /* <sublit> */
    if (*inbuff == '"')
    {
	inbuff++;
	for(;*inbuff;)
	{
	    if (*inbuff == '"')
	    {
		inbuff++;
		if (*inbuff == '"')
		    *dest++ = *inbuff++;
		else
		    break;
	    }
	    else
		*dest++ = *inbuff++;
	}
    }
    else /* [ + | - ] <numlit> */
    {
	if (*inbuff == '+' || *inbuff == '-')
	    *dest++ = *inbuff++;

	/* <numlit> ::= <mant> [ <exp> ] */

	/* <mant> ::= . <intlit>  | <intlit> [ . <intlit> ] */
	if (*inbuff == '.')
	{
	    *dest++ = *inbuff++;
	    if (!ISDIGIT(*inbuff))
		return NULL;

	    while(ISDIGIT(*inbuff))
		*dest++ = *inbuff++;
	}
	else
	{
	    if (!ISDIGIT(*inbuff))
		return NULL;

	    while(ISDIGIT(*inbuff))
		*dest++ = *inbuff++;

	    if (*inbuff == '.')
	    {
		*dest++ = *inbuff++;
		if (!ISDIGIT(*inbuff))
		    return NULL;

		while(ISDIGIT(*inbuff))
		    *dest++ = *inbuff++;
	    }
	}

	/* <exp> ::= [ + | - ] <intlit> */
	if (*inbuff == 'E')
	{
	    *dest++ = *inbuff++;

	    if (*inbuff == '+' || *inbuff == '-')
		*dest++ = *inbuff++;

	    if (!ISDIGIT(*inbuff))
		return NULL;

	    while(ISDIGIT(*inbuff))
		*dest++ = *inbuff++;
	}
    }

    v->mvtype = MV_STR;
    v->str.addr = (char *) stringpool.free;
    v->str.len = dest - stringpool.free;
    *dest++ = '\0';
    stringpool.free = dest;

    return inbuff;
}
