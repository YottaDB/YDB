/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_STR_H
#define HASHTAB_STR_H

#include "hashtab.h"	/* needed for STR_HASH (in COMPUTE_HASH_MNAME) */

typedef struct
{
	mstr		str;
	uint4		hash_code;
	GTM64_ONLY(int4 filler;)
} stringkey;

typedef struct
{
	stringkey	key;
	void		*value;
} ht_ent_str;

typedef struct hash_table_str_struct
{
	ht_ent_str 	*base;		/* base of array of hent_* entries */
	ht_ent_str 	*top; 		/* top of array of hent_* entries */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	initial_size;	/* Hash table initial size */
	ht_ent_str 	*spare_base;	/* spare array of hent_* entries */
	unsigned int 	spare_base_size;/* size of spare array */
	boolean_t	dont_compact;	/* if set, never perform compaction */
	boolean_t	dont_keep_spare_table; /* if set, don't keep a spare table */
	boolean_t	defer_base_release; /* if set don't release base, caller will free_base...() later */
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
	sm_uc_ptr_t	entry_passed_thru;/* Bit vector used to determine whether a particular */
					/* ht_ent has been involved in a collision (meaning that */
					/* this value can't be marked empty on delete */
} hash_table_str;

#define HTENT_EMPTY_STR(tabent, type, htvalue) (!(tabent)->key.hash_code && !(tabent)->key.str.len)
#define HTENT_MARK_EMPTY_STR(tabent)							\
{											\
	(tabent)->key.hash_code = 0;							\
	(tabent)->key.str.len = 0;							\
}
#define HTENT_VALID_STR(tabent, type, htvalue) ((!HTENT_EMPTY_STR(tabent, type, htvalue)) &&			\
						(HT_DELETED_ENTRY != (htvalue = (type *)(tabent)->value)))
#define COMPUTE_HASH_STR(hkey)								\
{											\
	assert((0 <= (hkey)->str.len) && (MAX_STRLEN >= (hkey)->str.len));		\
	if (0 == (hkey)->str.len)							\
		/* Likely null string -- need nonzero hash code  */			\
		(hkey)->hash_code = 1;							\
	else										\
		STR_HASH((hkey)->str.addr, (hkey)->str.len, ((hkey)->hash_code), 0);	\
}

/* Prototypes for addr hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_str(hash_table_str *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table);
void expand_hashtab_str(hash_table_str *table, int minsize);
boolean_t add_hashtab_str(hash_table_str *table, stringkey *key, void *value,  ht_ent_str **tabentptr);
void *lookup_hashtab_str(hash_table_str *table, stringkey *key);
void delete_hashtab_ent_str(hash_table_str *table, ht_ent_str *tabent);
boolean_t delete_hashtab_str(hash_table_str *table, stringkey *key);
void free_hashtab_str(hash_table_str *table);
void reinitialize_hashtab_str(hash_table_str *table);
void compact_hashtab_str(hash_table_str *table);

#endif /* HASHTAB_STR_H */
