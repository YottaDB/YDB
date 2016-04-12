/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_OBJCODE_H
#define HASHTAB_OBJCODE_H

#include "hashtab.h"	/* needed for STR_HASH (in COMPUTE_HASH_MNAME) */

typedef struct
{
	icode_str	key;
	void		*value;
} ht_ent_objcode;

typedef struct hash_table_objcode_struct
{
	ht_ent_objcode 	*base;		/* base of array of hent_* entries */
	ht_ent_objcode 	*top; 		/* top of array of hent_* entries */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	initial_size;	/* Hash table initial size */
	ht_ent_objcode 	*spare_base;	/* spare array of hent_* entries */
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
} hash_table_objcode;

#define HTENT_EMPTY_OBJCODE(tabent, type, htvalue) (!(htvalue = (type *)(tabent)->value) && !(tabent)->key.str.len)
#define HTENT_MARK_EMPTY_OBJCODE(tabent)				\
{									\
	(tabent)->value = NULL; 					\
	(tabent)->key.str.len = 0;					\
}
#define HTENT_VALID_OBJCODE(tabent, type, htvalue) ((!HTENT_EMPTY_OBJCODE(tabent, type, htvalue)) && (HT_DELETED_ENTRY != htvalue))
#define COMPUTE_HASH_OBJCODE(hkey, hash) 						\
{											\
	char *sptr;									\
	sptr = (hkey)->str.addr;							\
	if ((hkey)->str.len < SIZEOF(mident_fixed))					\
	{										\
		STR_HASH((sptr), (hkey)->str.len, hash, 0);				\
	} else 										\
	{										\
		STR_HASH((sptr), (SIZEOF(mident_fixed) / 2), hash, 0);			\
		STR_HASH(((sptr) + (hkey)->str.len - (SIZEOF(mident_fixed) / 2)),	\
			 (SIZEOF(mident_fixed) / 2), hash, hash);			\
	}										\
}

/* Prototypes for objcode hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_objcode(hash_table_objcode *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table);
void expand_hashtab_objcode(hash_table_objcode *table, int minsize);
boolean_t add_hashtab_objcode(hash_table_objcode *table, icode_str *key, void *value, ht_ent_objcode **tabentptr);
void *lookup_hashtab_objcode(hash_table_objcode *table, icode_str *key);
void delete_hashtab_ent_objcode(hash_table_objcode *table, ht_ent_objcode *tabent);
boolean_t delete_hashtab_objcode(hash_table_objcode *table, icode_str *key);
void free_hashtab_objcode(hash_table_objcode *table);
void reinitialize_hashtab_objcode(hash_table_objcode *table);
void compact_hashtab_objcode(hash_table_objcode *table);

#endif
