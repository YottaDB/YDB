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

/*
Different Hash Computation Macros for Strings:
All these were experminted and result is given in the design document of V5.0-000 longname project.
For now we decided to use ELF_HASH.
Do not remove the commented out section below which has all the hash functions.
We can remove them when we are certain that ELF_HASH is the best choice for us.
*/

#define STR_HASH ELF_HASH
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

/*
#define CHAR_BITS 8
#define BITS_IN_int     (SIZEOF(int) * CHAR_BITS)
#define THREE_QUARTERS  (BITS_IN_int * 3 / 4)
#define ONE_EIGHTH      (BITS_IN_int / 8)
#define HIGH_BITS       (~((unsigned int)(~0) >> ONE_EIGHTH ))
#define PJW_HASH(sptr, len, hash, init_hashval)				\
{									\
	uint4 	tempint;						\
	char *curr, *top;						\
	hash = init_hashval;						\
	for (curr = sptr, top = sptr + len; curr < top; curr++)		\
	{								\
		hash = ( hash << ONE_EIGHTH ) + *curr;			\
		if ((tempint = hash & HIGH_BITS ) != 0 )		\
			hash = ( hash ^ ( tempint >> THREE_QUARTERS )) & ~HIGH_BITS;\
	}								\
}


#define MISC1_HASH(sptr, len, hash, init_hashval)			\
{									\
	char *curr;							\
	curr = sptr;							\
	int indx;							\
	hash = init_hashval;						\
	for (indx = 0; indx < len; indx++, curr++)			\
		hash += (*curr * (len - indx));				\
}
#define MISC2_HASH(sptr, len, hash, init_hashval)			\

{									\
	char *curr;							\
	curr = sptr;							\
	int indx;							\
	hash = init_hashval;						\
	for (indx = 0; indx < len; indx++, curr++)			\
		hash = hash*31 + *curr;					\
}

#define CURR_HASH(sptr, len, hash, init_hashval)			\
{									\
	char *ptr, tchar[MAXLEN];					\
	int	indx;							\
	uint4 temp1 = 0;						\
	uint4 temp2 = 0;						\
	memset(tchar, 0, MAXLEN);					\
	memcpy(tchar, sptr, len);					\
	ptr = &tchar[0];						\
	for ( indx = 0; indx < 4; indx++, ptr++)			\
		temp1 = temp1 * 256 + *ptr ;				\
	for ( ; indx < 8; indx++, ptr++)				\
		temp2 = temp2 * 256 + *ptr ;				\
	hash = (temp1 << 1) ^ (temp2) ;					\
}
*/

#endif
