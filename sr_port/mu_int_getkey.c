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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_ctype.h"
#include "util.h"
#include "mupint.h"
#include "mvalconv.h"

GBLDEF	gv_key		*muint_start_key;
GBLDEF	gv_key		*muint_end_key;
GBLDEF	int		muint_start_keyend;
GBLDEF	int		muint_end_keyend;

GBLREF  boolean_t	muint_key;
GBLREF	boolean_t	muint_subsc;

int mu_int_getkey(unsigned char *key_buff, int keylen)
{
	int4		keysize;
	mval		tmpmval;
	unsigned char	*top, *startsrc, *src, *dest, slit[MAX_KEY_SZ + 1], *tmp;
	int		iter;
	gv_key		*muint_tmpkey;

	src = key_buff;
	if ('"' == key_buff[keylen - 1])
		keylen--;
	if ('"' == key_buff[0])
	{
		keylen--;
		src++;
	}
	if (0 == keylen)
	{	/* null subscript specified. signal an error */
		UNIX_ONLY(assert(FALSE));	/* Unix should not reach here at all. cli_parse() would have errored out */
		util_out_print("%GTM-E-CLIERR, Unrecognized option : SUBSCRIPT, value expected but not found", TRUE);
		return FALSE;
	}
	keysize = (MAX_KEY_SZ + MAX_NUM_SUBSC_LEN + 4) & (-4);			/* same calculation as done in targ_alloc() */
	top = src + keylen;
	startsrc = src;
	for (iter = 0, top = src + keylen; (iter < 2) && (src < top); iter++)
	{
		muint_tmpkey = (gv_key *)malloc(sizeof(gv_key) -1 + keysize); /* same calculation as done in gv_init_reg() */
		muint_tmpkey->top = keysize;
		muint_tmpkey->prev = 0;
		dest = muint_tmpkey->base;
		if ('^' != *src++)
		{
			util_out_print("Error in SUBSCRIPT qualifier : Key does not begin with '^' at offset !UL in !AD",
					TRUE, src - 1 - startsrc, top - startsrc, startsrc);
			return FALSE;
		}
		if (ISALPHA(*src) || ('%' == *src))
			*dest++ = *src++;
		else
		{
			util_out_print("Error in SUBSCRIPT qualifier : Global variable name does not begin with an alphabet"
					" or % at offset !UL in !AD", TRUE, src - startsrc, top - startsrc, startsrc);
			return FALSE;
		}
		for ( ; ('(' != *src) && (src < top); )
		{
			if (':' == *src)
				break;
			if (ISALNUM(*src))
				*dest++ = *src++;
			else
			{
				util_out_print("Error in SUBSCRIPT qualifier : Global variable name contains non-alphanumeric "
					"characters at offset !UL in !AD", TRUE, src - startsrc, top - startsrc, startsrc);
				return FALSE;
			}
		}
		*dest++ = 0;
		*dest = 0;
		muint_tmpkey->end = dest - muint_tmpkey->base;
		if (!iter)
		{
			muint_start_key = muint_tmpkey;
			muint_start_keyend = muint_start_key->end;
		} else
		{
			muint_end_key = muint_tmpkey;
			muint_end_keyend = muint_end_key->end;
		}
		if ('(' == *src)
		{
			muint_subsc = TRUE;
			src++;
			for ( ; src < top; )
			{
				tmpmval.mvtype = MV_STR;
				if ('\"' != *src)
				{
					for (tmpmval.str.addr = (char *)src; (src < top) && (')' != *src) && (',' != *src); src++)
					{
						if ((*src < '0' || *src > '9') && ('-' != *src) && ('.' != *src))
						{
							util_out_print("Error in SUBSCRIPT qualifier : Non-string subscript "
									"contains non-numerical characters at offset !UL in !AD",
									TRUE, src - startsrc, top - startsrc, startsrc);
							return FALSE;
						}
					}
					tmpmval.str.len = src - (unsigned char*)tmpmval.str.addr;
					if (!tmpmval.str.len)
					{
						util_out_print("Error in SUBSCRIPT qualifier : Empty subscript specified at "
								"offset !UL in !AD",
								TRUE, src - startsrc, top - startsrc, startsrc);
						return FALSE;
					}
					s2n(&tmpmval);
					tmpmval.mvtype &= MV_NUM_MASK;
				} else
				{
					src++;
					tmp = slit;
					for (;;)
					{
						if (src >= top)
						{
							util_out_print("Error in SUBSCRIPT qualifier : String subscript does not "
									"terminate with double-quote (\") character at offset !UL "
									"in !AD", TRUE, src - startsrc, top - startsrc, startsrc);
							return FALSE;
						}
						if ('\"' == *src)
							if ('\"' != *++src)
								break;
						*tmp++ = *src++;
					}
					tmpmval.str.addr = (char*)slit;
					tmpmval.str.len = tmp - slit;
				}
				mval2subsc(&tmpmval, muint_tmpkey);
				if ((src >= top) || (',' != *src))
					break;
				src++;
			}
			if (src >= top)
			{
				assert(src == top);
				util_out_print("Error in SUBSCRIPT qualifier : Empty/Incomplete subscript specified at "
						"offset !UL in !AD",
						TRUE, src - startsrc, top - startsrc, startsrc);
				return FALSE;
			}
			if (')' != *src++)
			{
				util_out_print("Error in SUBSCRIPT qualifier : Subscript terminating right parentheses not found "
						"at offset !UL in !AD", TRUE, src - 1 - startsrc, top - startsrc, startsrc);
				return FALSE;
			}
		}
		if (':' == *src)
			src++;
		muint_key = TRUE;
	}
	if (src < top)
	{
		assert(iter == 2);
		util_out_print("Error: Subscript qualifier keyrange not clear. More than two keys specified at offset !UL in !AD",
				TRUE, src - startsrc, top - startsrc, startsrc);
		return FALSE;
	}
	return TRUE;
}
