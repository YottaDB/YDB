/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_UMNAME_H
#define HASHTAB_UMNAME_H

#include "mdef.h"
#include "hashtab.h"

typedef struct
{
	unmanaged_mname_entry	key;
	void			*value;
} ht_ent_umname;

typedef struct hash_table_umname_struct
{
	ht_ent_umname 	*base;		/* base of array of hent_* entries */
	ht_ent_umname 	*top; 		/* top of array of hent_* entries */
	ht_ent_umname 	*spare_base;	/* spare array of hent_* entries */
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
} hash_table_umname;


#define HTENT_EMPTY_UMNAME(tabent, type, htvalue) (!(tabent)->key.var_name.len)
#define HTENT_MARK_EMPTY_UMNAME(tabent) (tabent)->key.var_name.len = 0
#define HTENT_VALID_UMNAME(tabent, type, htvalue) ((!HTENT_EMPTY_UMNAME(tabent, type, htvalue)) && \
				(HT_DELETED_ENTRY != (htvalue = (type *)(tabent)->value)))

/* Prototypes for mname hash routines. See hashtab_implementation.h for detail interface and implementation
 * protos that are potentially involved with lifetime maintainence take the full mname_entry pointer, whereas
 * protos which do not add/remove hashtab entries take the unmanaged mname (or union member of the regular mname)*/
void init_hashtab_umname(hash_table_umname *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table);
void expand_hashtab_umname(hash_table_umname *table, int minsize);
boolean_t add_hashtab_umname(hash_table_umname *table, unmanaged_mname_entry *key, void *value, ht_ent_umname **tabentptr);
ht_ent_umname *lookup_hashtab_umname(hash_table_umname *table, unmanaged_mname_entry *key);
void delete_hashtab_ent_umname(hash_table_umname *table, ht_ent_umname *tabent);
boolean_t delete_hashtab_umname(hash_table_umname *table, unmanaged_mname_entry *key);
void free_hashtab_umname(hash_table_umname *table);
void reinitialize_hashtab_umname(hash_table_umname *table);
void compact_hashtab_umname(hash_table_umname *table);
sm_uc_ptr_t copy_hashtab_to_buffer_umname(hash_table_umname *table,
		sm_uc_ptr_t buffer, int (*copy_entry_to_buffer)(ht_ent_umname *, sm_uc_ptr_t));
hash_table_umname *activate_hashtab_in_buffer_umname(sm_uc_ptr_t buffer,
		int (*copy_entry_from_buffer)(ht_ent_umname *, sm_uc_ptr_t));

#endif
