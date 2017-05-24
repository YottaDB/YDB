/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "show_source_line.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#include "gtm_icu_api.h"	/* U_ISPRINT() needs this header */
#endif

GBLREF	int		source_column;
GBLREF	spdesc		stringpool;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	boolean_t	run_time;

LITREF	char		ctypetab[NUM_CHARS];

error_def(ERR_LITNONGRAPH);
error_def(ERR_NUMOFLOW);

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
	unsigned char	*cp1, *cp2, *cp3, *cptop, *cptr;
	unsigned char	*error, errtxt[(3 + 1) UNICODE_ONLY(* GTM_MB_LEN_MAX)], x;	/* up to 3 digits/byte & a comma */
	char		*tmp;
	int		y, charlen;
#	ifdef UNICODE_SUPPORTED
	uint4		ch;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(last_source_column) = source_column;
	source_column = (TREF(lexical_ptr) - (TREF(source_buffer)).addr + 1);
	TREF(window_token) = TREF(director_token);
	TREF(window_mval) = TREF(director_mval);
	(TREF(director_mval)).mvtype = 0; /* keeps mval from being GC'd since it is not useful until re-used */
	tmp = (TREF(window_ident)).addr;  /* More efficient to swap pointers between window_ident.addr & director_ident.addr */
	TREF(window_ident) = TREF(director_ident);	/* than to copy text from director_ident to window_ident */
	(TREF(director_ident)).addr = tmp;
	x = *TREF(lexical_ptr);
	switch (y = ctypetab[x])
	{
	case TK_CR:
		y = ctypetab[*(++(TREF(lexical_ptr)))];
		if (TK_EOL != y)
		{	/* Not looking like a <CR><LF>, so it's an error */
			y = TK_ERROR;
			break;
		}	/* WARNING fall through - it is a <CR><LF>*/
	case TK_EOL:
		TREF(director_token) = TK_EOL;
		return;		/* if next character is terminator, avoid incrementing past it */
	case TK_QUOTE:
		ENSURE_STP_FREE_SPACE((TREF(source_buffer)).len);
		cptop = (unsigned char *)((TREF(source_buffer)).addr + (TREF(source_buffer)).len - 1);
		cp2 = cp3 = stringpool.free;
		for (cp1 = (unsigned char *)TREF(lexical_ptr) + 1; cp1 <= cptop;)
		{
#			ifdef UNICODE_SUPPORTED
			if (gtm_utf8_mode)
				cptr = (unsigned char *)UTF8_MBTOWC((sm_uc_ptr_t)cp1, (TREF(source_buffer)).addr
					+ (TREF(source_buffer)).len, ch);
#			endif
			x = *cp1++;
			if ((SP > x) UNICODE_ONLY(|| (gtm_utf8_mode && !(U_ISPRINT(ch)))))
			{
				TREF(last_source_column) = (cp1 - (unsigned char*)((TREF(source_buffer)).addr));
				if (!run_time)
				{
					if ('\0' == x)
					{
						TREF(director_token) = TREF(window_token) = TK_ERROR;
						return;
					}
					show_source_line(TRUE);
					if (!gtm_utf8_mode)
						charlen = 1;			/* always one character in M mode */
#					ifdef UNICODE_SUPPORTED
					else
					{
						charlen = cptr - cp1 + 1;	/* get the number of bytes in the utf8 character */
						assert(GTM_MB_LEN_MAX >= charlen);
					}
#					endif
					for (cptr = cp1 - 1; charlen--;)	/* stop when all bytes are done */
					{	/* cptr winds up back where it started due to the increments in the loop */
						error = (unsigned char*)i2asc((uchar_ptr_t)errtxt, (unsigned int)*cptr++);
						*error++ = ',';
					}
					error--;				/* do not include the last comma */
					dec_err(VARLSTCNT(4) ERR_LITNONGRAPH, 2, (error - errtxt), errtxt);
				}
			}
			if ('\"' == x)
			{
				UNICODE_ONLY(assert(!gtm_utf8_mode || (cp1 == cptr)));
				if ('\"' == *cp1)
					cp1++;
				else
					break;
			}
			*cp2++ = x;
#			ifdef UNICODE_SUPPORTED
			if (gtm_utf8_mode && (cptr > cp1))
			{
				assert(4 > (cptr - cp1));
				for (; cptr > cp1;)
					*cp2++ = *cp1++;
			}
#			endif
			assert(cp2 <= stringpool.top);
		}
		if (cp1 > cptop)
		{
			TREF(last_source_column) = (TREF(source_buffer)).len;
			TREF(director_token) = TREF(window_token) = TK_ERROR;
			return;
		}
		TREF(lexical_ptr) = (char *)cp1;
		TREF(director_token) = TK_STRLIT;
		(TREF(director_mval)).mvtype = MV_STR;
		(TREF(director_mval)).str.addr = (char *)cp3;
		(TREF(director_mval)).str.len = INTCAST(cp2 - cp3);
		CLEAR_MVAL_BITS(TADR(director_mval));
		stringpool.free = cp2;
		s2n(&(TREF(director_mval)));
#		ifdef UNICODE_SUPPORTED
		if (gtm_utf8_mode && !run_time)
		{	/* UTF8 mode and not compiling an indirect gets an optimization to set the
			 * (true) length of the string into the mval
			 */
			charlen = utf8_len_dec(&(TREF(director_mval)).str);
			if (0 > charlen)	/* got a BADCHAR error */
				TREF(director_token) = TK_ERROR;
			else
			{
				assert(charlen == (TREF(director_mval)).str.char_len);
				(TREF(director_mval)).mvtype |= MV_UTF_LEN;
			}
		}
#		endif
		return;
	case TK_LOWER:
	case TK_PERCENT:
	case TK_UPPER:
		cp2 = (unsigned char *)((TREF(director_ident)).addr);
		cp3 = cp2 + MAX_MIDENT_LEN;
		for (;;)
		{
			if (cp2 < cp3)
				*cp2++ = x;
			y = ctypetab[x = *++(TREF(lexical_ptr))];			/* note assignment */
			if ((TK_UPPER != y) && (TK_DIGIT != y) && (TK_LOWER != y))
				break;
		}
		(TREF(director_ident)).len = INTCAST(cp2 - (unsigned char*)(TREF(director_ident)).addr);
		TREF(director_token) = TK_IDENT;
		return;
	case TK_PERIOD:
		if (ctypetab[x = *(TREF(lexical_ptr) + 1)] != TK_DIGIT)		/* note assignment */
			break;
	case TK_DIGIT:
		(TREF(director_mval)).str.addr = TREF(lexical_ptr);
		(TREF(director_mval)).str.len = (TREF(source_buffer)).len;
		(TREF(director_mval)).mvtype = MV_STR;
		CLEAR_MVAL_BITS(TADR(director_mval));
		TREF(lexical_ptr) = (char *)s2n(&(TREF(director_mval)));
		if (!((TREF(director_mval)).mvtype &= MV_NUM_MASK))
		{
			stx_error(ERR_NUMOFLOW);
			TREF(director_token) = TK_ERROR;
			return;
		}
		if (TREF(s2n_intlit))
		{
			TREF(director_token) = TK_NUMLIT ;
			n2s(&(TREF(director_mval)));
		} else
		{
			TREF(director_token) = TK_INTLIT ;
			(TREF(director_mval)).str.len = INTCAST(TREF(lexical_ptr) - (TREF(director_mval)).str.addr);
			ENSURE_STP_FREE_SPACE((TREF(director_mval)).str.len);
			memcpy(stringpool.free, (TREF(director_mval)).str.addr, (TREF(director_mval)).str.len);
			assert (stringpool.free <= stringpool.top) ;
		}
		return;
	case TK_APOSTROPHE:
		if (32 <= (x = *++(TREF(lexical_ptr))))				/* note assignment */
		{
			x -= 32;
			if (x < ARRAYSIZE(apos_ok))
			{
				if (y = apos_ok[x])
				{
					if (DEL < (x = *++(TREF(lexical_ptr))))		/* note assignment */
					{
						TREF(director_token) = TK_ERROR;
						return;
					}
					if (TK_RBRACKET == ctypetab[x])
					{
						(TREF(lexical_ptr))++;
						y = TK_NSORTS_AFTER;
					}
					TREF(director_token) = y;
					return;
				}
			}
		}
		TREF(director_token) = TK_APOSTROPHE;
		return;
	case TK_GREATER:
	case TK_LESS:
		if (TK_EQUAL == ctypetab[*(TREF(lexical_ptr) + 1)])
		{
			++(TREF(lexical_ptr));
			y = ((TK_LESS == y) ? TK_NGREATER : TK_NLESS);
		}
		break;
	case TK_SEMICOLON:
		while (*++(TREF(lexical_ptr)))
			;
		assert(TK_EOL == ctypetab[*TREF(lexical_ptr)]);
		TREF(director_token) = TK_EOL;
		return;		/* if next character is terminator, avoid incrementing past it */
	case TK_ASTERISK:
		if (DEL < (x = *(TREF(lexical_ptr) + 1)))				/* note assignment */
		{
			TREF(director_token) = TK_ERROR;
			return;
		}
		if (TK_ASTERISK == ctypetab[x])
		{
			(TREF(lexical_ptr))++;
			y = TK_EXPONENT;
		}
		break;
	case TK_RBRACKET:
		if ((x = *(TREF(lexical_ptr) + 1)) > DEL)				/* note assignment */
		{
			TREF(director_token) = TK_ERROR;
			return;
		}
		if (TK_RBRACKET == ctypetab[x])
		{
			(TREF(lexical_ptr))++;
			y = TK_SORTS_AFTER;
		}
		break;
	case TK_ATSIGN:
		if (DEL < (x = *(TREF(lexical_ptr) + 1)))				/* note assignment */
		{
			TREF(director_token) = TK_ERROR;
			return;
		}
		if (TK_HASH == ctypetab[x])
		{
			(TREF(lexical_ptr))++;
			y = TK_ATHASH;
		}
	default:
		;
	}
	(TREF(lexical_ptr))++;
	TREF(director_token) = y;
	return;
}

#ifdef GTM_TRIGGER
/* The M standard does not allow the '#' character to appear inside mnames but in specific places, we want to allow this
 * so that triggers, which have the imbedded '#' character in their routine names, can be debugged and printed. The places
 * where this is allowed follow.
 *
 *   1. $TEXT()
 *   2. ZBREAK
 *   3. ZPRINT
 *
 * All other uses still prohibit '#' from being in an MNAME. Routines that need to allow # in a name can call this routine to
 * recombine the existing token and the look-ahead (director) token such that '#' is considered part of an mident.
 *
 * Update: Like '#', we need to allow '/' as well to allow for an overriding region name of up to 31 characters to be
 * specified (where the trigger needs to be searched. So handle that as well in this same function.
 */
void advwindw_hash_in_mname_allowed(void)
{
	unsigned char	*cp2, *cp3, x;
	unsigned char	ident_buffer[(MAX_MIDENT_LEN * 2) + 1];
	int		ident_len, ch;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_IDENT == TREF(window_token));
	assert(TK_HASH == TREF(director_token));
	/* First copy the existing token we want to expand into our safe-haven */
	memcpy(ident_buffer, (TREF(window_ident)).addr, (TREF(window_ident)).len);
	/* Now parse further until we run out of [m]ident */
	cp2 = ident_buffer + (TREF(window_ident)).len;
	cp3 = ident_buffer + ARRAYSIZE(ident_buffer);
	if (MAX_MIDENT_LEN == (TREF(window_ident)).len)
		cp2--;
	*cp2++ = '#';	/* We are only called if director token is '#' so put that char in buffer now */
	/* Start processing with the token following the '#'. Allow '#' and/or '/' in addition to usual stuff. */
	for (x = *TREF(lexical_ptr), ch = ctypetab[x];
	     ((TK_UPPER == ch) || (TK_DIGIT == ch) || (TK_LOWER == ch) || (TK_HASH == ch) || (TK_SLASH == ch));
	     x = *++(TREF(lexical_ptr)), ch = ctypetab[x])
	{
		if (cp2 < cp3)
			*cp2++ = x;
	}
	(TREF(director_ident)).len = INTCAST(cp2 - ident_buffer);
	TREF(director_token) = TK_IDENT;
	memcpy((TREF(director_ident)).addr, ident_buffer, (TREF(director_ident)).len);
	advancewindow();	/* Makes the homogenized token the current token (again) and prereads next token */
}
#endif
