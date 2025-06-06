/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_MNAME_H
#define HASHTAB_MNAME_H

#include "hashtab.h"	/* needed for STR_HASH (in COMPUTE_HASH_MNAME) */

typedef struct
{
	mname_entry	key;
	void		*value;
} ht_ent_mname;

typedef struct hash_table_mname_struct
{
	ht_ent_mname 	*base;		/* base of array of hent_* entries */
	ht_ent_mname 	*top; 		/* top of array of hent_* entries */
	ht_ent_mname 	*spare_base;	/* spare array of hent_* entries */
	sm_uc_ptr_t	entry_passed_thru;/* Bit vector used to determine whether a particular */
						/* ht_ent has been involved in a collision (meaning that */
						/* this value can't be marked empty on delete */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	initial_size;	/* Hash table initial size */
	unsigned int 	spare_base_size;/* size of spare array */
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
	boolean_t	dont_compact;	/* if set, never perform compaction */
	boolean_t	dont_keep_spare_table; /* if set, don't keep a spare table */
	boolean_t	defer_base_release; /* if set don't release base, caller will free_base...() later */
	boolean_t	active;	/* indicates that the table "ready to use"; if not, we need to activate it */
} hash_table_mname;

#define HTENT_EMPTY_MNAME(tabent, type, htvalue) (!(tabent)->key.var_name.len)
#define HTENT_MARK_EMPTY_MNAME(tabent) (tabent)->key.var_name.len = 0
#define HTENT_VALID_MNAME(tabent, type, htvalue) ((!HTENT_EMPTY_MNAME(tabent, type, htvalue)) && \
				(HT_DELETED_ENTRY != (htvalue = (type *)(tabent)->value)))
#define COMPUTE_HASH_MNAME(hkey)							\
{											\
	assert((0 < (hkey)->var_name.len) && (MAX_MIDENT_LEN >= (hkey)->var_name.len));	\
	STR_HASH((hkey)->var_name.addr, (hkey)->var_name.len, ((hkey)->hash_code), 0);	\
}

/* This is the same as the COMPUTE_HASH_MNAME macro except "hkey" is of type mstr (not mname_entry)
 * and target of the computed hash_code is an input parameter "hash_code" of type "int".
 */
#define COMPUTE_HASH_MSTR(hkey, hash_code)				\
{									\
	assert((0 < (hkey).len) && (MAX_MIDENT_LEN >= (hkey).len));	\
	STR_HASH((hkey).addr, (hkey).len, hash_code, 0);		\
}

/* Prototypes for mname hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_mname(hash_table_mname *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table);
void expand_hashtab_mname(hash_table_mname *table, int minsize);
boolean_t add_hashtab_mname(hash_table_mname *table, mname_entry *key, void *value, ht_ent_mname **tabentptr);
boolean_t add_hashtab_mname_symval(hash_table_mname *table, mname_entry *key, void *value, ht_ent_mname **tabentptr);
ht_ent_mname *lookup_hashtab_mname(hash_table_mname *table, mname_entry *key);
void delete_hashtab_ent_mname(hash_table_mname *table, ht_ent_mname *tabent);
boolean_t delete_hashtab_mname(hash_table_mname *table, mname_entry *key);
void free_hashtab_mname(hash_table_mname *table);
void reinitialize_hashtab_mname(hash_table_mname *table);
void compact_hashtab_mname(hash_table_mname *table);
sm_uc_ptr_t copy_hashtab_to_buffer_mname(hash_table_mname *table,
		sm_uc_ptr_t buffer, int (*copy_entry_to_buffer)(ht_ent_mname *, sm_uc_ptr_t));
void activate_hashtab_in_buffer_mname(sm_uc_ptr_t buffer, hash_table_mname *table);

#endif
