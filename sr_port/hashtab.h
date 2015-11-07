/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_COMMON_H
#define HASHTAB_COMMON_H

#define HASHTAB_COMPACT	FALSE
#define HASHTAB_NO_COMPACT TRUE

#define HASHTAB_SPARE_TABLE FALSE
#define HASHTAB_NO_SPARE_TABLE TRUE

#define HT_VALUE_DUMMY ((void *) 1L)
	/* It is required not to have 0 or -1 for this macro.
	 * This is passed when hashtable callers want to use hash table just for the keys.
	 * Say, database code wants to create a list of blocks being read for a transactions.
	 * There is no corresponding value associated. For that case HT_VALUE_DUMMY will be passed as "value". */
#define HT_DELETED_ENTRY ((void *) -1L)
		/* Note: We may need to change the above during 64-bit port */
#define HTENT_MARK_DELETED(tabent) (HT_DELETED_ENTRY == (tabent)->value)
#define HT_LOAD_FACTOR 	50
#define HT_REHASH_FACTOR (HT_LOAD_FACTOR/2)
#define HT_REHASH_TABLE_SIZE(table) 	MIN(table->size, table->count * 4)
#define INSERT_HTENT(table, tabent, hkey, value)				\
{										\
	if (HT_DELETED_ENTRY == (tabent)->value)				\
		(table)->del_count--; 						\
	(tabent)->key = *hkey; 							\
	(tabent)->value = value;						\
	(table)->count++;							\
}

#define COMPACT_NEEDED(table) ((!(table)->dont_compact) && (((table)->del_count > (table)->cmp_trigger_size) || \
	(((table)->initial_size < (table)->size ) && ((table)->count < ((table)->cmp_trigger_size / 2)))))

/*
 * This macro is used by callers outside of the hash table implementation to indicate
 * whether they will request the free of the hash table base at a later point in time or
 * if it should be released by the hash table implementation during an expansion/compaction.
 * They must call FREE_BASE_HASHTAB() later to release the base otherwise the memory
 * will be leaked.
 */
#define DEFER_BASE_REL_HASHTAB(table, deferit)					\
{										\
	(table)->defer_base_release = deferit;					\
}

/*
 * This macro is used by callers outside of the hash table implementation to indicate
 * that they are no longer using the hash table base. This function only provides a "hint" to the
 * hash table implementation, i.e., the base can now be freed when appropriate. This can
 * mean when this function is called if we are not keeping spare bases or at a potentially
 * much later time if we are using a spare base.
 */
#define FREE_BASE_HASHTAB(table, base)						\
{										\
	if ((table)->dont_keep_spare_table)					\
		free(base);							\
}

/* For string hashing, ELF hash was found to be the best during the V5.0-000 longnames project.
 * During V6.2-001, Murmur3 hash was found to be much better than ELF in terms of # of collisions.
 * So we are going with MMR hash for now. In addition, the 32-bit murmur3 hash reference implementation
 * we used gives different values for the same input on different endian machines which would not work
 * at least for triggers since we expect the trigger definition M code to hash to the same value on
 * different endian machines (this is needed so mupip endiancvt does not need to worry about changing
 * ^#t(.*TRHASH.*) nodes. Therefore we came up with a modified 32-bit murmur3 hash
 * implementation that is endian independent (gtmmrhash_32). See mmrhash.c for details.
 */
#ifdef UNIX
#include "mmrhash.h"
#define STR_HASH(KEY, LEN, HASH, SEED) gtmmrhash_32(KEY, LEN, SEED, (uint4 *)&HASH)
/* The STR_PHASH* macros are the progressive variants of the STR_HASH macro. */
#define	STR_PHASH_INIT(STATE, TOTLEN)			HASH128_STATE_INIT(STATE, 0); TOTLEN = 0
#define	STR_PHASH_PROCESS(STATE, TOTLEN, KEY, LEN)	gtmmrhash_128_ingest(&STATE, KEY, LEN); TOTLEN += LEN
#define	STR_PHASH_RESULT(STATE, TOTLEN, OUT4)			\
{								\
	gtm_uint16	out16;					\
								\
	gtmmrhash_128_result(&STATE, TOTLEN, &out16);		\
	OUT4 = (uint4)out16.one;				\
}
#else
#define STR_HASH ELF_HASH
#endif

#define ELF_HASH(sptr, len, hash, init_hashval)						\
{											\
	uint4	tempint;								\
	char	*curr, *top;								\
	uint4	hcode;									\
	for (hcode = init_hashval, curr = sptr, top = sptr + len; curr < top; curr++)	\
	{										\
		hcode = (hcode << 4) + *curr;						\
		if (tempint = (hcode & 0xF0000000))					\
			hcode ^= tempint >> 24;						\
		hcode &= ~tempint;							\
	}										\
	hash = hcode;									\
}

#endif
