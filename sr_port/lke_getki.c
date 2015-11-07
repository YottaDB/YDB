/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_ctype.h" /* Needed for TOUPPER() */

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
	char	one_lockbuf[MAX_ZWR_KEY_SZ + 1], *one_char;
	char	*valid_char = "HAR"; /* This is used for validating characters following $ZCH and $C */

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
		if (')' == *inptr) /* Catches incomplete lists or string concatenations */
			return -1;
		else if ('$' == *inptr)
		{	/* the entire subscript is within $C() or $ZCH */
			*outptr++ = '$';
			inptr++;
			if (('z' == *inptr) || ('Z' == *inptr))
			{	/* Very likely $ZCHAR() */
				*outptr++ = 'Z';
				inptr++;
			}
			if (('c' == *inptr) || ('C' == *inptr))
			{
				*outptr++ = 'C';
				inptr++;
				if (('Z' == *(outptr - 2)) && (('h' == *inptr) || ('H' == *inptr)))
				{
					*outptr++ ='H';
					inptr++;
					one_char = valid_char + 1;
				}
				else
					one_char = valid_char;
				/* Validate/skip letters following C so that we allow C, CH, CHA, CHAR */
				while (('\0' != *one_char) && ('(' != *inptr))
					if (TOUPPER(*inptr++) != *one_char++)
						return -1;
				if ('(' != *inptr)
					return -1;
			} else	/* We don't support anything other than $C() or $ZCH in locks */
				return -1;
			nextptr = memchr(inptr, ')', intop - inptr);
			if (NULL == nextptr)
				return -1;
			++nextptr;
			memcpy(outptr, inptr, nextptr - inptr);
			outptr += nextptr - inptr;
		} else
		{
			if ('"' == *inptr) /* Is this a quoted string? */
			{	/*Process character by character because '_' or ',' can be used within the quotes. */
				for (nextptr = inptr + 1; nextptr < intop; nextptr++)
					if ('"' == *nextptr && (nextptr + 1 < intop))
					{
						nextptr++;
						if ('"' != *nextptr)
							/* This is not a two double-quote so terminate. */
							break;
					}
			} else
			{ /* Fast-forward to the next separator */
				nextptr = memchr(inptr, '_', intop - inptr);
				if (NULL == nextptr)
				{ /* Not a string concatineated with $C() or $ZCH() */
					nextptr = memchr(inptr, ',', intop - inptr);
					if (NULL == nextptr)
						nextptr = intop - 1;
				}
			}
			if (intop - 1 == nextptr)
			{ /* If it reached to the end, it had better closed the paran */
			    if (')' != *nextptr)
				    return -1;
			}
			else if ((',' != *nextptr) && ('_' != *nextptr))
				/* If we are not at the end, it must be a separator*/
				return -1;
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
					memcpy(outptr, inptr, nextptr - inptr);
					outptr += nextptr - inptr;
					inptr += nextptr - inptr;
				} else
				{ /* unquoted string: add quotes */
					*outptr++ = '"';
					for (tmpptr = inptr; tmpptr < nextptr; ++tmpptr)
					{
						*outptr++ = *tmpptr;
						if ('"' == *tmpptr)
							*outptr++ = '"';
						if ('_' == *tmpptr)
						{
							*--outptr;
							nextptr = tmpptr;
						}
					}
					*outptr++ = '"';
				}
			}
		}
		if ((',' != *nextptr) && (')' != *nextptr) && ('_' != *nextptr))
			return -1;
		*outptr++ = *nextptr++;
	}
	return (int)(outptr - outbuff);
}
