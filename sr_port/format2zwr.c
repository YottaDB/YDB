/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#ifdef UNICODE_SUPPORTED
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
	int		fastate = 0, ncommas, dstlen, chlen;
	boolean_t	isctl, isill;
	uchar_ptr_t	srctop, strnext, tmpptr;

	dstlen = *des_len = 0;

	if (src_len > 0)
	{
		srctop = src + src_len;
		fastate = 0;
		/* deals with the other characters */
		for (cp = src; cp < srctop; cp += chlen)
		{
			if (!gtm_utf8_mode)
			{
		        	ch = *cp;
				isctl = ((pattern_typemask[ch] & PATM_C) != 0);
				isill = FALSE;
				chlen = 1;
			}
#ifdef UNICODE_SUPPORTED
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
			        } else if (isctl)
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
				{ /* graphic characters */
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
				}
				break;
			case 2: /* subsequent characters following a non-graphic character in the
				   form of $CHAR(x,) */
				if (isill)
				{
					MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARZCH);
					dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARZCH);
					I2A(des, dstlen, ch);
					fastate = 3;
				} else if(isctl)
				{
					ncommas++;
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
				}
				break;
			case 3: /* subsequent characters following an illegal character in the form of $ZCHAR(x,) */
				if(isill)
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
				} else if (isctl)
				{
					MEMCPY_LIT(des + dstlen, CLOSE_PAREN_DOLLARCH);
					dstlen += STR_LIT_LEN(CLOSE_PAREN_DOLLARCH);
					I2A(des, dstlen, ch);
					fastate = 2;
				} else
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
		        des[dstlen++] = '"';
			break;
		case 2:
		case 3:
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
	*des_len = dstlen;
	return 0;
}

