/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
 * @param [out] xlate integer array of size NUM_CHARS (must be allocated by the caller) which this function fills out with the
 *  offsets in the rplc string for single-byte characters from srch
 * @return a hash table with each character mapped to the replace character offset multi-byte input characters have their offset
 *  stored in the hash table, single-byte characters have them stored in the integer xlate table
 *
 */
hash_table_int4 *create_utf8_xlate_table(mval *srch, mval *rplc, int4 *xlate)
{
	hash_table_int4 *xlate_hash = NULL;
	ht_ent_int4 *tabent;
	char *stop, *scur, *sprev, *rtop, *rcur, *rprev, *rbase;
	int scode, rcode, rmb;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memset(xlate, NO_VALUE, SIZEOF(int4) * NUM_CHARS);
	if (!((srch->mvtype & MV_UTF_LEN) && srch->str.len == srch->str.char_len))
	{	/* We don't need the hash table if it's a single byte string, but don't calculate the char_len if we don't have it
		    because it could be compile time */
		if (TREF(compile_time))
			xlate_hash = (hash_table_int4*)mcalloc(SIZEOF(hash_table_int4));
		else
			xlate_hash = (hash_table_int4*)malloc(SIZEOF(hash_table_int4));
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
			if (NO_VALUE == xlate[*sprev])
				xlate[*sprev] = (rprev - rbase);
		} else
		{
			assert(NULL != xlate_hash);
			if(!lookup_hashtab_int4(xlate_hash, (uint4*)&scode))
			{	/* Store the offset of that character in replace string */
				add_hashtab_int4(xlate_hash, (uint4*)&scode, (void*)(rprev - rbase + 1), &tabent);
			}
		}
	}
	while (scur < stop)
	{
		sprev = scur;
		scur = (char *)UTF8_MBTOWC(scur, stop, scode);
		if (1 == (scur - sprev))
		{
			if (NO_VALUE == xlate[*sprev])
				xlate[*sprev] = DELETE_VALUE;
		} else
		{
			assert(NULL != xlate_hash);
			if(!lookup_hashtab_int4(xlate_hash, (uint4*)&scode))
				add_hashtab_int4(xlate_hash, (uint4*)&scode, (void*)(MAXPOSINT4), &tabent);
		}
	}
	return xlate_hash;
}
