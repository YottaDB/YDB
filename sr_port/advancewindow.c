/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

LITREF	char		ctypetab[NUM_CHARS];

GBLREF	unsigned char	*source_buffer;
GBLREF	short int	source_column;
GBLREF	bool		s2n_intlit;
GBLREF	short int	last_source_column;
GBLREF	char		window_token;
GBLREF	mval		window_mval;
GBLREF	char		director_token;
GBLREF	mval		director_mval;
GBLREF	char		*lexical_ptr;
GBLREF	spdesc		stringpool;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	boolean_t	run_time;

GBLREF	mident		window_ident;	/* the current identifier */
GBLREF	mident		director_ident;	/* the look-ahead identifier */

error_def(ERR_LITNONGRAPH);

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
	char		*tmp, source_line_buff[MAX_SRCLINE + sizeof(ARROW)];
	int		y, charlen;
#ifdef UNICODE_SUPPORTED
	uint4		ch;
	unsigned char	*cptr;
#endif

	last_source_column = source_column;
	source_column = (unsigned char *)lexical_ptr - source_buffer + 1;
	window_token = director_token;
	window_mval = director_mval;

	/* It is more efficient to swich buffers between window_ident and director_ident
	 * instead of copying the text from director_ident to window_ident. This can be
	 * done by exchanging the pointers window_ident.addr and director_ident.addr */
	tmp = window_ident.addr;
	window_ident = director_ident;
	director_ident.addr = tmp;

	x = *lexical_ptr;
	switch (y = ctypetab[x])
	{
		case TK_QUOTE:
			if (stringpool.free + MAX_SRCLINE > stringpool.top )
				stp_gcol(MAX_SRCLINE);
			cp1 = (unsigned char *)lexical_ptr + 1;
			cp2 = cp3 = stringpool.free;
			for (;;)
			{

#ifdef UNICODE_SUPPORTED
				if (gtm_utf8_mode)
					cptr = (unsigned char *)UTF8_MBTOWC((sm_uc_ptr_t)cp1, source_buffer + MAX_SRCLINE, ch);
#endif
				x = *cp1++;
				if ((((SP > x) && !gtm_utf8_mode)
					UNICODE_ONLY(|| (gtm_utf8_mode && !(U_ISPRINT(ch))))) && !run_time)
				{
					last_source_column = cp1 - source_buffer;
					dec_err(VARLSTCNT(1) ERR_LITNONGRAPH);
					show_source_line(source_line_buff, TRUE);
				}
				if ('\"' == x)
				{
					UNICODE_ONLY(assert(!gtm_utf8_mode || (cp1 == cptr));)
					if ('\"' == *cp1)
						cp1++;
					else
						break;
				}
				*cp2++ = x;
#ifdef UNICODE_SUPPORTED
				if (gtm_utf8_mode && (cptr > cp1))
				{
					assert(4 > (cptr - cp1));
					for (; cptr > cp1;)
						*cp2++ = *cp1++;
				}
#endif
				assert(cp2 <= stringpool.top);
			}
			lexical_ptr = (char *)cp1;
			director_token = TK_STRLIT;
			director_mval.mvtype = MV_STR;
			director_mval.str.addr = (char *)cp3;
			director_mval.str.len = INTCAST(cp2 - cp3);
			stringpool.free = cp2;
			s2n(&director_mval);
#ifdef UNICODE_SUPPORTED
			if (gtm_utf8_mode && !run_time)
			{	/* UTF8 mode and not compiling an indirect gets an optimization to set the
				   (true) length of the string into the mval
				*/
				charlen = utf8_len_stx(&director_mval.str);
				if (0 > charlen)	/* got a BADCHAR error */
					director_token = TK_ERROR;
				else
				{
					assert(charlen == director_mval.str.char_len);
					director_mval.mvtype |= MV_UTF_LEN;
				}
			}
#endif
			return;
		case TK_LOWER:
		case TK_PERCENT:
		case TK_UPPER:
			cp2 = (unsigned char *)director_ident.addr;
			cp3 = cp2 + MAX_MIDENT_LEN;
			for (;;)
			{
				if (cp2 < cp3)
					*cp2++ = x;
				y = ctypetab[x = *++lexical_ptr];
				if ((TK_UPPER != y) && (TK_DIGIT != y) && (TK_LOWER != y))
					break;
			}
			director_ident.len = INTCAST(cp2 - (unsigned char*)director_ident.addr);
			director_token = TK_IDENT;
			return;
		case TK_PERIOD:
			if (ctypetab[x = *(lexical_ptr + 1)] != TK_DIGIT)
				break;
		case TK_DIGIT:
			director_mval.str.addr = lexical_ptr;
			director_mval.str.len = MAX_SRCLINE;
			director_mval.mvtype = MV_STR;
			lexical_ptr = (char *)s2n(&director_mval);
			if (!(director_mval.mvtype &= MV_NUM_MASK))
			{
				stx_error(ERR_NUMOFLOW);
				director_token = TK_ERROR;
				return;
			}
			if (s2n_intlit)
			{
				director_token = TK_NUMLIT ;
				n2s(&director_mval);
			} else
			{
				director_token = TK_INTLIT ;
				director_mval.str.len = INTCAST(lexical_ptr - director_mval.str.addr);
				if (stringpool.free + director_mval.str.len > stringpool.top)
					stp_gcol(director_mval.str.len);
				memcpy(stringpool.free, director_mval.str.addr, director_mval.str.len);
				assert (stringpool.free <= stringpool.top) ;
			}
			return;
		case TK_APOSTROPHE:
			if (( x = *++lexical_ptr) >= 32)
			{
				x -= 32;
				if (x < sizeof(apos_ok) / sizeof(unsigned char))
				{
					if (y = apos_ok[x])
					{
						if (DEL < (x = *++lexical_ptr))
						{
							director_token = TK_ERROR;
							return;
						}
						if (TK_RBRACKET == ctypetab[x])
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
			if (DEL < (x = *(lexical_ptr + 1)))
			{
				director_token = TK_ERROR;
				return;
			}
			if (TK_ASTERISK == ctypetab[x])
			{
				lexical_ptr++;
				y = TK_EXPONENT;
			}
			break;
		case TK_RBRACKET:
			if ((x = *(lexical_ptr + 1)) > DEL)
			{
				director_token = TK_ERROR;
				return;
			}
			if (TK_RBRACKET == ctypetab[x])
			{
				lexical_ptr++;
				y = TK_SORTS_AFTER;
			}
			break;
		default:
			;
	}
	lexical_ptr++;
	director_token = y;
	return;
}
