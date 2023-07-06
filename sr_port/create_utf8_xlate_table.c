/****************************************************************
 *								*
 * Copyright (c) 2018-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_string.h"
#include "mdef.h"
#include "mmemory.h"
#include "toktyp.h"
#include "op.h"

#ifdef UTF8_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"
#endif

/**
 * Generates a translate table where each character in the search string (srch) is mapped to the corresponding character offset in
 *  the replace string (repl). Note character, not byte.
 *
 * In the resulting hash_table_int4, the (void*)value in ht_ent_int4 is  overloaded to represent the mapped character. Characters
 *  with the mapping MAXPOSINT4 are to be deleted, and characters without an entry are to be left alone.
 *
 * Note, we malloc the hashtable here, and it must be freed by the caller.
 *
 * @param [in] srch string containing characters to search for
 * @param [in] rplc string containing the characters to replace with
 * @param [out] xlate pointer to integer array which this fills out with offsets into the rplc string of utf-8 characters
 *
 * @return a hash table with each character mapped to the replace character offset
 */
hash_table_int4 *create_utf8_xlate_table(mval *srch, mval *rplc, mstr *m_xlate)
{
	char		*rbase, *rcur, *rprev, *rtop, *scur, *sprev, *stop;
	hash_table_int4	*xlate_hash = NULL;
	ht_ent_int4	*tabent;
	int		rcode, scode, xlate_len;
	int4		*xlate;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	xlate = (int4 *)m_xlate->addr;
	xlate_len = m_xlate->len;
	assertpro((xlate_len >= srch->str.char_len) && ((NUM_CHARS * SIZEOF(int4)) <= xlate_len));
	memset(xlate, NO_VALUE, xlate_len);
	if (!((srch->mvtype & MV_UTF_LEN) && srch->str.len == srch->str.char_len))
<<<<<<< HEAD
	{       /* need hash table if srch contains multi-byte UTF-8 characters (i.e. is not an ASCII string) */
=======
	{	/* need hash table if srch is a not a sting of single bytes */
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
		if (TREF(compile_time))
			xlate_hash = (hash_table_int4*)mcalloc(SIZEOF(hash_table_int4));
		else
			xlate_hash = (hash_table_int4*)malloc(SIZEOF(hash_table_int4));
		assert(0 < srch->str.len * (100.0 / HT_LOAD_FACTOR));
		init_hashtab_int4(xlate_hash, srch->str.len * (100.0 / HT_LOAD_FACTOR),
			  HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	}
	scur = srch->str.addr;
	stop = scur + srch->str.len;
	rbase = rcur = rplc->str.addr;
	rtop = rcur + rplc->str.len;
	while ((scur < stop) && (rcur < rtop))
	{
		sprev = scur;
		scur = (char *)UTF8_MBTOWC(scur, stop, scode);
		rprev = rcur;
		rcur = (char *)UTF8_MBTOWC(rcur, rtop, rcode);
		if (1 == (scur - sprev))
		{
<<<<<<< HEAD
			int	 index;

			index = *(unsigned char *)sprev;
			if (NO_VALUE == xlate[index])		/* 1st replacement rules, so ignore any stragglers */
				xlate[index] = (rprev - rbase);
=======
			if (NO_VALUE == xlate[(unsigned char)*sprev])	/* 1st replacement rules, so ignore any stragglers */
				xlate[(unsigned char)*sprev] = (rprev - rbase);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
		} else
		{
			assert(NULL != xlate_hash);
			if (!lookup_hashtab_int4(xlate_hash, (uint4*)&scode))	/* first replacement rules again */
			{	/* Store the offset of that character in replace string */
				add_hashtab_int4(xlate_hash, (uint4*)&scode, (void*)(rprev - rbase + 1), &tabent);
			}
		}
	}
	while (scur < stop)
	{ /* if replacement character length is less than search character length, drop matches to remaining search characters */
		sprev = scur;
		scur = (char *)UTF8_MBTOWC(scur, stop, scode);
		if (1 == (scur - sprev))
		{
<<<<<<< HEAD
			int	 index;

			index = *(unsigned char *)sprev;
			if (NO_VALUE == xlate[index])
				xlate[index] = DELETE_VALUE;
=======
			if (NO_VALUE == xlate[(unsigned char)*sprev])
				xlate[(unsigned char)*sprev] = DELETE_VALUE;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
		} else
		{
			assert(NULL != xlate_hash);
			if (!lookup_hashtab_int4(xlate_hash, (uint4*)&scode))
				add_hashtab_int4(xlate_hash, (uint4*)&scode, (void*)(MAXPOSINT4), &tabent);
		}
	}
	return xlate_hash;
}
