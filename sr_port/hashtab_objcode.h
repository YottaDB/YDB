/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_OBJCODE_H
#define HASHTAB_OBJCODE_H

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
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
} hash_table_objcode;

#define HTENT_EMPTY_OBJCODE(tabent, type, htvalue) (!(htvalue = (type *)(tabent)->value) && !(tabent)->key.str.len)
#define HTENT_VALID_OBJCODE(tabent, type, htvalue) ((!HTENT_EMPTY_OBJCODE(tabent, type, htvalue)) && (HT_DELETED_ENTRY != htvalue))
#define COMPUTE_HASH_OBJCODE(hkey, hash) 				\
{									\
	char *sptr;							\
	sptr = (hkey)->str.addr;					\
	if ((hkey)->str.len < sizeof(mident_fixed))			\
	{								\
		STR_HASH((sptr), (hkey)->str.len, hash, 0);		\
	}								\
	else 								\
	{								\
		STR_HASH((sptr), sizeof(mident_fixed)/2, hash, 0);		\
		STR_HASH((sptr) +  (hkey)->str.len - sizeof(mident_fixed)/2, 	\
			sizeof(mident_fixed)/2, hash, hash);			\
	}								\
}

/* Prototypes for objcode hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_objcode(hash_table_objcode *table, int minsize);
void expand_hashtab_objcode(hash_table_objcode *table, int minsize);
boolean_t add_hashtab_objcode(hash_table_objcode *table, icode_str *key, void *value, ht_ent_objcode **tabentptr);
void *lookup_hashtab_objcode(hash_table_objcode *table, icode_str *key);
boolean_t delete_hashtab_objcode(hash_table_objcode *table, icode_str *key);
void free_hashtab_objcode(hash_table_objcode *table);
void reinitialize_hashtab_objcode(hash_table_objcode *table);
void compact_hashtab_objcode(hash_table_objcode *table);

#endif
