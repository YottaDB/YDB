/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "collseq.h"
#include "muextr.h"
#include "mupip_size.h"
#include "mu_getkey.h"
#include "gvt_inline.h"

GBLDEF gv_key *mu_start_key;
GBLDEF gv_key *mu_end_key;
GBLDEF int mu_start_keyend;
GBLDEF int mu_end_keyend;

GBLREF	boolean_t	mu_subsc;
GBLREF	boolean_t	mu_key;
GBLREF	int		mu_sub_idx_st;
GBLREF	int		mu_sub_idx_end;

#define	CLNUP_AND_RETURN_FALSE			\
{						\
	GVKEY_FREE_IF_NEEDED(mu_start_key);	\
	GVKEY_FREE_IF_NEEDED(mu_end_key);	\
	return FALSE;				\
}

int mu_getkey(unsigned char *key_buff, int keylen)
{
	int4		keysize;
	mval		tmpmval;
	unsigned char	*top, *startsrc, *src, *dest, slit[MAX_KEY_SZ + 1], *tmp;
	int		iter;
	gv_key		*mu_tmpkey;
	gd_region	*reg;
	boolean_t	nullsubs_seen;

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
		util_out_print("%YDB-E-CLIERR, Unrecognized option : SUBSCRIPT, value expected but not found", TRUE);
		CLNUP_AND_RETURN_FALSE;
	}
	top = src + keylen;
	startsrc = src;
	keysize = DBKEYSIZE(MAX_KEY_SZ);
	assert(MUKEY_FALSE == mu_key);
	for (iter = 0, top = src + keylen; (iter < 2) && (src < top); iter++)
	{
		mu_tmpkey = NULL;	/* GVKEY_INIT macro requires this */
		GVKEY_INIT(mu_tmpkey, keysize);
		if (!iter)
		{
			assert(NULL == mu_start_key);
			mu_start_key = mu_tmpkey;	/* used by CLNUP_AND_RETURN_FALSE macro */
		} else
		{
			assert(NULL == mu_end_key);
			mu_end_key = mu_tmpkey;
		}
		nullsubs_seen = FALSE;
		dest = mu_tmpkey->base;
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
		iter ? (mu_sub_idx_end = dest - mu_tmpkey->base) : (mu_sub_idx_st = dest - mu_tmpkey->base);
		*dest++ = 0;
		*dest = 0;
		mu_tmpkey->end = dest - mu_tmpkey->base;
		if (!iter)
			mu_start_keyend = mu_start_key->end;
		else
			mu_end_keyend = mu_end_key->end;
		if ('(' == *src)
		{
			mu_subsc = TRUE;
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
					if (!tmpmval.str.len)
						nullsubs_seen = TRUE;
				}
				/* We could be looking for this -subscript=... specification in one of many database files
				 * each with a different standard null collation setting. As MUPIP INTEG switches to different
				 * database files, it needs to recompute the subscript representation of the input subscript
				 * if there are null subscripts in it and the regions have different standard null collation
				 * properties. For now assume standard null collation is FALSE in all regions. And note down
				 * if any null subscript was seen. If so do recomputation of key as db files get switched in integ.
				 */
				mval2subsc(&tmpmval, mu_tmpkey, STD_NULL_COLL_FALSE);
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
		if (MUKEY_NULLSUBS != mu_key)
			mu_key = (nullsubs_seen ? MUKEY_NULLSUBS : MUKEY_TRUE);
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
