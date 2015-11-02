/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

#define	CLNUP_AND_RETURN_FALSE			\
{						\
	GVKEY_FREE_IF_NEEDED(muint_start_key);	\
	GVKEY_FREE_IF_NEEDED(muint_end_key);	\
	return FALSE;				\
}

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
		CLNUP_AND_RETURN_FALSE;
	}
	top = src + keylen;
	startsrc = src;
	keysize = DBKEYSIZE(MAX_KEY_SZ);
	for (iter = 0, top = src + keylen; (iter < 2) && (src < top); iter++)
	{
		muint_tmpkey = NULL;	/* GVKEY_INIT macro requires this */
		GVKEY_INIT(muint_tmpkey, keysize);
		if (!iter)
		{
			assert(NULL == muint_start_key);
			muint_start_key = muint_tmpkey;	/* used by CLNUP_AND_RETURN_FALSE macro */
		} else
		{
			assert(NULL == muint_end_key);
			muint_end_key = muint_tmpkey;	/* used by CLNUP_AND_RETURN_FALSE macro */
		}
		dest = muint_tmpkey->base;
		if ('^' != *src++)
		{
			util_out_print("Error in SUBSCRIPT qualifier : Key does not begin with '^' at offset !UL in !AD",
					TRUE, src - 1 - startsrc, top - startsrc, startsrc);
			CLNUP_AND_RETURN_FALSE;
		}
		if (ISALPHA_ASCII(*src) || ('%' == *src))
			*dest++ = *src++;
		else
		{
			util_out_print("Error in SUBSCRIPT qualifier : Global variable name does not begin with an alphabet"
					" or % at offset !UL in !AD", TRUE, src - startsrc, top - startsrc, startsrc);
			CLNUP_AND_RETURN_FALSE;
		}
		for ( ; ('(' != *src) && (src < top); )
		{
			if (':' == *src)
				break;
			if (ISALNUM_ASCII(*src))
				*dest++ = *src++;
			else
			{
				util_out_print("Error in SUBSCRIPT qualifier : Global variable name contains non-alphanumeric "
					"characters at offset !UL in !AD", TRUE, src - startsrc, top - startsrc, startsrc);
				CLNUP_AND_RETURN_FALSE;
			}
		}
		*dest++ = 0;
		*dest = 0;
		muint_tmpkey->end = dest - muint_tmpkey->base;
		if (!iter)
			muint_start_keyend = muint_start_key->end;
		else
			muint_end_keyend = muint_end_key->end;
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
							CLNUP_AND_RETURN_FALSE;
						}
					}
					tmpmval.str.len = INTCAST(src - (unsigned char*)tmpmval.str.addr);
					if (!tmpmval.str.len)
					{
						util_out_print("Error in SUBSCRIPT qualifier : Empty subscript specified at "
								"offset !UL in !AD",
								TRUE, src - startsrc, top - startsrc, startsrc);
						CLNUP_AND_RETURN_FALSE;
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
							CLNUP_AND_RETURN_FALSE;
						}
						if ('\"' == *src)
							if ('\"' != *++src)
								break;
						*tmp++ = *src++;
					}
					tmpmval.str.addr = (char*)slit;
					tmpmval.str.len = INTCAST(tmp - slit);
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
				CLNUP_AND_RETURN_FALSE;
			}
			if (')' != *src++)
			{
				util_out_print("Error in SUBSCRIPT qualifier : Subscript terminating right parentheses not found "
						"at offset !UL in !AD", TRUE, src - 1 - startsrc, top - startsrc, startsrc);
				CLNUP_AND_RETURN_FALSE;
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
		CLNUP_AND_RETURN_FALSE;
	}
	return TRUE;
}
