/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "toktyp.h"
#include "stringpool.h"
#include "gtm_caseconv.h"
#include "advancewindow.h"

LITREF	char		ctypetab[NUM_ASCII_CHARS];

GBLREF	unsigned char	*source_buffer;
GBLREF	short int	source_column;
GBLREF	bool		s2n_intlit;
GBLREF	short int	last_source_column;
GBLREF	char		window_token;
GBLREF	mval		window_mval;
GBLREF	mident		window_ident;
GBLREF	char		director_token;
GBLREF	mval		director_mval;
GBLREF	mident		director_ident;
GBLREF	char		*lexical_ptr;
GBLREF	spdesc		stringpool;

static readonly unsigned char apos_ok[] =
{
	0,TK_NEXCLAIMATION,0,0,0,0,TK_NAMPERSAND,0
	,0,0,0,0,0,0,0,0
	,0,0,0,0,0,0,0,0
	,0,0,0,0,TK_NLESS,TK_NEQUAL,TK_NGREATER,TK_NQUESTION
	,0,0,0,0,0,0,0,0
	,0,0,0,0,0,0,0,0
	,0,0,0,0,0,0,0,0
	,0,0,0,TK_NLBRACKET,0,TK_NRBRACKET,0,0
};

void advancewindow(void)
{
	error_def(ERR_NUMOFLOW);
	unsigned char	*cp1, *cp2, *cp3, x;
	int		y;

	last_source_column = source_column;
	source_column = (unsigned char *) lexical_ptr - source_buffer + 1;
	window_token = director_token;
	window_mval = director_mval;
	window_ident = director_ident;
	if ((x = *lexical_ptr) > DEL)
	{
		director_token = TK_ERROR;
		return;
	}
	switch (y = ctypetab[x])
	{
	case TK_QUOTE:
		if (stringpool.free + MAX_SRCLINE > stringpool.top )
			stp_gcol(MAX_SRCLINE);
		cp1 = (unsigned char *) lexical_ptr + 1;
		cp2 = cp3 = stringpool.free;
		for (;;)
		{
			if ((x = *cp1++) < SP)
			{
				director_token = TK_ERROR;
				return;
			}
			if (x == '\"')
			{	if (*cp1 == '\"')
				{	cp1++;
				}
				else
				{	break;
				}
			}
			*cp2++ = x;
			assert(cp2 <= stringpool.top);
		}
		lexical_ptr = (char *) cp1;
		director_token = TK_STRLIT;
		director_mval.mvtype = MV_STR;
		director_mval.str.addr = (char *) cp3;
		director_mval.str.len = cp2 - cp3;
		stringpool.free = cp2;
		s2n(&director_mval);
		return;
	case TK_LOWER:
	case TK_PERCENT:
	case TK_UPPER:
		cp2 = (unsigned char *) director_ident.c;
		cp3 = cp2 + sizeof(director_ident.c);
		for (;;)
		{
			if (cp2 < cp3)
			{	*cp2++ = x;
			}
			y = ctypetab[x = *++lexical_ptr];
			if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
			{	break;
			}
		}
		while (cp2 < cp3)
		{	*cp2++ = 0;
		}
		director_token = TK_IDENT;
		return;
	case TK_PERIOD:
		if (ctypetab[*(lexical_ptr + 1)] != TK_DIGIT)
			break;
	case TK_DIGIT:
		director_mval.str.addr = lexical_ptr;
		director_mval.str.len = MAX_SRCLINE;
		director_mval.mvtype = MV_STR;
		lexical_ptr = (char *) s2n(&director_mval);
		if (!(director_mval.mvtype &= MV_NUM_MASK))
		{	stx_error(ERR_NUMOFLOW);
			director_token = TK_ERROR;
			return;
		}
		if ( s2n_intlit )
		{
			director_token = TK_NUMLIT ;
			n2s(&director_mval);
		}
		else
		{	director_token = TK_INTLIT ;
			director_mval.str.len = lexical_ptr - director_mval.str.addr ;
			if (stringpool.free + director_mval.str.len > stringpool.top)
				stp_gcol(director_mval.str.len) ;
			memcpy(stringpool.free,director_mval.str.addr,director_mval.str.len) ;
			assert (stringpool.free <= stringpool.top) ;
		}
		return;
	case TK_APOSTROPHE:
		if (( x = *++lexical_ptr) >= 32)
		{	x -= 32;
			if (x < sizeof(apos_ok) / sizeof(unsigned char))
			{	if (y = apos_ok[x])
				{
					if ((x = *++lexical_ptr) > DEL)
					{
						director_token = TK_ERROR;
						return;
					}
					if (ctypetab[x] == TK_RBRACKET)
					{
						lexical_ptr++;
						y = TK_NSORTS_AFTER;
					}
					director_token = y;
					return;
				}
			}
		}
		director_token = TK_APOSTROPHE;
		return;
	case TK_SEMICOLON:
		while (*++lexical_ptr) ;
		y = TK_EOL;
		break;
	case TK_ASTERISK:
		if ((x = *(lexical_ptr + 1)) > DEL)
		{
			director_token = TK_ERROR;
			return;
		}
		if (ctypetab[x] == TK_ASTERISK)
		{	lexical_ptr++;
			y = TK_EXPONENT;
		}
		break;
	case TK_RBRACKET:
		if ((x = *(lexical_ptr + 1)) > DEL)
		{
			director_token = TK_ERROR;
			return;
		}
		if (ctypetab[x] == TK_RBRACKET)
		{
			lexical_ptr++;
			y = TK_SORTS_AFTER;
		}
		break;
	}
	lexical_ptr++;
	director_token = y;
	return;
}
