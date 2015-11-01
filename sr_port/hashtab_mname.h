/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_MNAME_H
#define HASHTAB_MNAME_H

typedef struct {
	mname_entry	key;
	void		*value;
} ht_ent_mname;

typedef struct hash_table_mname_struct {
	ht_ent_mname 	*base;		/* base of array of hent_* entries */
	ht_ent_mname 	*top; 		/* top of array of hent_* entries */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
} hash_table_mname;

#define HTENT_EMPTY_MNAME(tabent, type, htvalue) (!(tabent)->key.var_name.len)
#define HTENT_VALID_MNAME(tabent, type, htvalue) ((!HTENT_EMPTY_MNAME(tabent, type, htvalue)) && \
				(HT_DELETED_ENTRY != (htvalue = (type *)(tabent)->value)))
#define COMPUTE_HASH_MNAME(hkey)									\
{												\
		assert((0 < (hkey)->var_name.len) && (MAX_MIDENT_LEN >= (hkey)->var_name.len));	\
		STR_HASH((hkey)->var_name.addr, (hkey)->var_name.len, ((hkey)->hash_code), 0);	\
}

/* Prototypes for mname hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_mname(hash_table_mname *table, int minsize);
void expand_hashtab_mname(hash_table_mname *table, int minsize);
boolean_t add_hashtab_mname(hash_table_mname *table, mname_entry *key, void *value, ht_ent_mname **tabentptr);
void *lookup_hashtab_mname(hash_table_mname *table, mname_entry *key);
boolean_t delete_hashtab_mname(hash_table_mname *table, mname_entry *key);
void free_hashtab_mname(hash_table_mname *table);
void reinitialize_hashtab_mname(hash_table_mname *table);
void compact_hashtab_mname(hash_table_mname *table);

#endif
