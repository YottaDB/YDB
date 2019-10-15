/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "zshow.h"
#include "patcode.h"
#include "compiler.h"		/* for CHARMAXARGS */

#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"	/* U_ISPRINT() needs this header */
#include "gtm_utf8.h"
#endif

GBLREF	uint4		*pattern_typemask;
GBLREF	boolean_t	gtm_utf8_mode;

/* Routine to convert a string to ZWRITE format. Used by the utiltities.
 * NOTE: this routine does almost the same formatting as mval_write(). The reason
 * for not using mval_write() is because it is much more complex than we need
 * here. Moreover, this version is more efficient due to the availability of
 * pre-allocated destination buffer */
int format2zwr(sm_uc_ptr_t src, int src_len, unsigned char *des, int *des_len)
{
        sm_uc_ptr_t	cp;
	uint4		ch;
	int		fastate = 0, ncommas = 0, dstlen, chlen, max_len;
	boolean_t	isctl, isill, nospace;
	uchar_ptr_t	srctop, strnext, tmpptr;

	max_len = *des_len;
	assert(0 < max_len);
	dstlen = *des_len = 0;
	nospace = FALSE;
	if (src_len > 0)
	{
		srctop = src + src_len;
		fastate = 0;
		max_len--; /* Make space for the trailing quote or paren */
		/* deals with the other characters */
		for (cp = src; (cp < srctop) && (max_len > dstlen); cp += chlen)
		{
			if (!gtm_utf8_mode)
			{
		        	ch = *cp;
				isctl = ((pattern_typemask[ch] & PATM_C) != 0);
				isill = FALSE;
				chlen = 1;
			}
#ifdef UTF8_SUPPORTED
			else {
				strnext = UTF8_MBTOWC(cp, srctop, ch);
				isill = (WEOF == ch) ? (ch = *cp, TRUE) : FALSE;
				if (!isill)
					isctl = !U_ISPRINT(ch);
				chlen = (int)(strnext - cp);
			}
#endif
			switch(fastate)
			{
			case 0:	/* beginning of the string */
			case 1: /* beginning of a new substring followed by a graphic character */
				if (isill)
			        {
					if (max_len > (dstlen + STR_LIT_LEN(DOLLARZCH)
								+ MAX_ZWR_ZCHAR_DIGITS + ((dstlen) ? 2 : 0)))
					{
						if (dstlen > 0)
						{
							des[dstlen++] = '"';
							des[dstlen++] = '_';
						}
						MEMCPY_LIT(des + dstlen, DOLLARZCH);
						dstlen += STR_LIT_LEN(DOLLARZCH);
						I2A(des, dstlen, ch);
						fastate = 3;
						ncommas = 0;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
					break;
			        } else if (isctl)
			        {
					if (max_len > (dstlen + STR_LIT_LEN(DOLLARCH)
								+ MAX_ZWR_DCHAR_DIGITS + ((dstlen) ? 2 : 0)))
					{
						if (dstlen > 0)
						{ /* close previous string with quote and prepare for concatenation */
							des[dstlen++] = '"';
							des[dstlen++] = '_';
						}
						MEMCPY_LIT(des + dstlen, DOLLARCH);
						dstlen += STR_LIT_LEN(DOLLARCH);
						I2A(des, dstlen, ch);
						fastate = 2;
						ncommas = 0;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
			        } else
				{ /* graphic characters */
					if (max_len > (dstlen + 2 + ((!gtm_utf8_mode) ? 1 : chlen)))
					{
						if (0 == fastate) /* the initial quote in the beginning */
						{
							des[dstlen++] = '"';
							fastate = 1;
						}
						if ('"' == ch)
							des[dstlen++] = '"';
						if (!gtm_utf8_mode)
							des[dstlen++] = ch;
						else {
							memcpy(&des[dstlen], cp, chlen);
							dstlen += chlen;
						}
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				}
				break;
			case 2: /* subsequent characters following a non-graphic character in the
				   form of $CHAR(x,) */
				if (isill)
				{
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_DOLLARZCH) + MAX_ZWR_ZCHAR_DIGITS))
					{
						MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARZCH);
						dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARZCH);
						I2A(des, dstlen, ch);
						fastate = 3;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				} else if(isctl)
				{
					ncommas++;
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_DOLLARCH) + MAX_ZWR_DCHAR_DIGITS))
					{
						if (CHARMAXARGS == ncommas)
						{
							ncommas = 0;
							MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARCH);
							dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARCH);
						} else
						{
							MEMCPY_LIT(des + dstlen, COMMA);
							dstlen += STR_LIT_LEN(COMMA);
						}
						I2A(des, dstlen, ch);
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				} else
				{
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_QUOTE)
								+ 1 + ((!gtm_utf8_mode) ? 1 : chlen)))
					{
						MEMCPY_LIT(des + dstlen, CLOSE_PAREN_QUOTE);
						dstlen += STR_LIT_LEN(CLOSE_PAREN_QUOTE);
						if (!gtm_utf8_mode)
							des[dstlen++] = ch;
						else {
							memcpy(&des[dstlen], cp, chlen);
							dstlen += chlen;
						}
						if ('"' == ch)
							des[dstlen++] = '"';
						fastate = 1;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				}
				break;
			case 3: /* subsequent characters following an illegal character in the form of $ZCHAR(x,) */
				if(isill)
				{
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_DOLLARZCH) + MAX_ZWR_ZCHAR_DIGITS))
					{
						ncommas++;
						if (CHARMAXARGS == ncommas)
						{
							ncommas = 0;
							MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARZCH);
							dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARZCH);
						} else
						{
							MEMCPY_LIT(des + dstlen, COMMA);
							++dstlen;
						}
						I2A(des, dstlen, ch);
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				} else if (isctl)
				{
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_DOLLARCH) + MAX_ZWR_DCHAR_DIGITS))
					{
						MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARCH);
						dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARCH);
						I2A(des, dstlen, ch);
						fastate = 2;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				} else
				{
					if (max_len > (dstlen + STR_LIT_LEN(CLOSE_PAREN_QUOTE)
								+ ((!gtm_utf8_mode) ? 1 : chlen) + ('"' == ch)))
					{
						MEMCPY_LIT(des + dstlen, CLOSE_PAREN_QUOTE);
						dstlen += STR_LIT_LEN(CLOSE_PAREN_QUOTE);
						if (!gtm_utf8_mode)
							des[dstlen++] = ch;
						else {
							memcpy(&des[dstlen], cp, chlen);
							dstlen += chlen;
						}
						if ('"' == ch)
							des[dstlen++] = '"';
						fastate = 1;
					} else
					{
						cp = srctop;	/* Not enough space, terminate the loop */
						nospace = TRUE;
					}
				}
				break;
			default:
				assert(FALSE);
				break;
			}
		}

		/* close up */
		switch(fastate)
		{
		case 1:
			if (!nospace)
				des[dstlen++] = '"';
			break;
		case 2:
		case 3:
			if (!nospace)
				des[dstlen++] = ')';
			break;
		default:
			assert(FALSE);
			break;
		}
	} else
	{
		des[0] = des[1] = '"';
		dstlen = 2;
	}
	assertpro(max_len > dstlen);
	*des_len = dstlen;
	return nospace;
}
