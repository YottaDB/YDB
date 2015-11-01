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

/*
 * This routine does almost the same formatting as zwrite. The reason for not
 * using zwrite directly is because zwrite is much more complex than we need
 * here.
 */

#include "mdef.h"

#include "gtm_string.h"

#include "mlkdef.h"
#include "zshow.h"
#include "patcode.h"
#include "compiler.h"	/* for CHARMAXARGS */

GBLREF uint4 *pattern_typemask;

static readonly char dollarch[] = "$C(";
static readonly char quote_dch[] = "\"_$C(";
static readonly char comma[] = ",";
static readonly char close_paren_quote[] = ")_\"";
static readonly char close_paren_dollarch[] = ")_$C(";

int format2zwr(sm_uc_ptr_t src, int src_len, unsigned char *des, int *des_len)
{
        sm_uc_ptr_t cp;
	int ch;
	int  fastate = 0, ncommas;
	bool isctl;

	*des_len = 0;

	if (src_len > 0)
	{
	        /* deals with the first character */
	        ch = *src;
		isctl = ((pattern_typemask[ch] & PATM_C) != 0);
		if (isctl)
		{
		        memcpy(des + *des_len, dollarch, sizeof(dollarch) - 1);
			*des_len += sizeof(dollarch) - 1;
		        i2a(des, des_len, ch);
			fastate = 2;
			ncommas = 0;
	        }
		else
		{
		        *(des + (*des_len)++) = '"';
			if ('"' == ch)
			        *(des + (*des_len)++) = '"';
			*(des + (*des_len)++) = ch;
			fastate = 1;
		}

		/* deals with the other characters */
		for(cp = src + 1; cp < src + src_len; cp++)
		{
		        ch = *cp;
		        isctl = ((pattern_typemask[ch] & PATM_C) != 0);
			switch(fastate)
			{
			case 1:
			        if(isctl)
			        {
					memcpy(des + *des_len, quote_dch, sizeof(quote_dch) - 1);
					*des_len += sizeof(quote_dch) - 1;
					i2a(des, des_len, ch);
					fastate = 2;
					ncommas = 0;
			        }
				else
				{
				        if ('"' == ch)
					        *(des + (*des_len)++) = '"';
					*(des + (*des_len)++) = ch;
				}
				break;
			case 2:
				if((isctl) || ('"' == ch))
				{
					ncommas++;
					if (CHARMAXARGS == ncommas)
					{
						ncommas = 0;
						memcpy(des + *des_len, close_paren_dollarch, sizeof(close_paren_dollarch) - 1);
						*des_len += sizeof(close_paren_dollarch) - 1;
					}
					else
					{
 						memcpy(des + *des_len, comma, sizeof(comma) - 1);
						*des_len += sizeof(comma) - 1;
					}
					i2a(des, des_len, ch);
				}
				else
				{
					memcpy(des + *des_len, close_paren_quote, sizeof(close_paren_quote) - 1);
					*des_len += sizeof(close_paren_quote) - 1;
					*(des + (*des_len)++) = ch;
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
		        *(des + (*des_len)++) = '"';
			break;
		case 2:
			*(des + (*des_len)++) = ')';
			break;
		default:
			assert(FALSE);
			break;
		}
	}
	else
	{
		*des++ = '"';
		*des++ = '"';
		*des_len = 2;
	}

	return 0;
}


