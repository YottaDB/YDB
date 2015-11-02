/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "lke_getcli.h"

/* This routine performs the necessary transformation of the LOCK keys passed in
 * from the CLI layer and produces a canonical formatted key. This routine
 *
 * 	validates if the key is indeed in the correct syntax.
 *
 * 	adds quotes to the string subscripts (which were removed by the CLI
 * 	layer on UNIX)
 *
 * 	removes the redundant quotes in the key (which were passed intact by
 * 	CLI layer on VMS).
 *
 */
int lke_getki(char* src, int srclen, char* outbuff)
{
	char	*inptr, *nextptr, *intop, *outptr, *tmpptr;
	mval	subsc = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);
	char	one_lockbuf[MAX_ZWR_KEY_SZ + 1];

	if (srclen > 1 && '"' == src[0] && '"' == src[srclen - 1])
	{
		outptr = one_lockbuf;
		for (inptr = ++src, intop = src + srclen - 2; inptr < intop; *outptr++ = *inptr++)
		{
			if ('"' == *inptr && (++inptr >= intop || *inptr != '"'))
				return -1;	/* invalid (unescaped) quote within a quoted key */
		}
		src = one_lockbuf;
		srclen = (int)(outptr - one_lockbuf);
	}
	inptr = memchr(src, '(', srclen);
	if (NULL == inptr)
	{
		memcpy(outbuff, src, srclen);
		return srclen;
	}
	++inptr;
	outptr = outbuff;
	memcpy(outptr, src, inptr - src);
	outptr += inptr - src;
	for (intop = src + srclen; inptr < intop; inptr = nextptr)
	{
		if ('$' == *inptr)
		{ /* the entire subscript is within $C() or $ZCH() */
			*outptr++ = *inptr++;
			nextptr = memchr(inptr, ')', intop - inptr);
			if (NULL == nextptr)
				return -1;
			++nextptr;
			memcpy(outptr, inptr, nextptr - inptr);
			outptr += nextptr - inptr;
		} else
		{ /* unquoted string or a number */
			nextptr = memchr(inptr, ',', intop - inptr);
			if (NULL == nextptr)
			{
				nextptr = intop - 1;;
				if (')' != *nextptr)
					return -1;
			}
			subsc.str.len = INTCAST(nextptr - inptr);
			subsc.str.addr = inptr;
			if (val_iscan(&subsc))
			{
				memcpy(outptr, subsc.str.addr, subsc.str.len);
				outptr += subsc.str.len;
			} else
			{
				if (nextptr - 1  > inptr  && '"' == *inptr && '"' == *(nextptr - 1))
				{ /* The string is already enclosed by a pair of quotes */
					*outptr++ = *inptr++;	/* initial quote */
					for (; inptr < nextptr - 1; *outptr++ = *inptr++)
					{
						if ('"' == *inptr && (++inptr >= nextptr - 1 || *inptr != '"'))
							return -1;	/* invalid (unescaped) quote within a quoted string */
					}
					*outptr++ = *inptr++;	/* final quote */
				} else
				{ /* unquoted string: add quotes */
					*outptr++ = '"';
					for (tmpptr = inptr; tmpptr < nextptr; ++tmpptr)
					{
						*outptr++ = *tmpptr;
						if ('"' == *tmpptr)
							*outptr++ = '"';
					}
					*outptr++ = '"';
				}
			}
		}
		if (',' != *nextptr && ')' != *nextptr)
			return -1;
		*outptr++ = *nextptr++;
	}
	return (int)(outptr - outbuff);
}
