/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef HASHTAB_ADDR_H
#define HASHTAB_ADDR_H

typedef struct
{
	char		*key;		/* Note this is the actual key, not its address */
	void		*value;
} ht_ent_addr;

typedef struct hash_table_addr_struct
{
	ht_ent_addr 	*base;		/* base of array of hent_* entries */
	ht_ent_addr 	*top; 		/* top of array of hent_* entries */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
} hash_table_addr;

#define HTENT_EMPTY_ADDR(tabent, type, htvalue) (!(htvalue = (type *)(tabent)->value))
#define HTENT_VALID_ADDR(tabent, type, htvalue) ((!HTENT_EMPTY_ADDR(tabent, type, htvalue)) && (HT_DELETED_ENTRY != htvalue))

/* Do not downcast when INT8_HASH, 8 byte int, is defined */
#ifdef INT8_HASH
#define HASHTAB_UINTCAST(X) X
#else
#define HASHTAB_UINTCAST(X) UINTCAST(X)
#endif

#ifdef GTM64
#  define COMPUTE_HASH_ADDR(hkey, hash) \
	hash = HASHTAB_UINTCAST(((((gtm_uint64_t)(*hkey)) & 0xFFFFFFFF) ^ (((gtm_uint64_t)(*hkey)) >> 31)))
#else
#  define COMPUTE_HASH_ADDR(hkey, hash) hash = ((uint4)(*hkey))
#endif

/* Prototypes for addr hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_addr(hash_table_addr *table, int minsize);
void expand_hashtab_addr(hash_table_addr *table, int minsize);
boolean_t add_hashtab_addr(hash_table_addr *table, char **key, void *value,  ht_ent_addr **tabentptr);
void *lookup_hashtab_addr(hash_table_addr *table, char **key);
boolean_t delete_hashtab_addr(hash_table_addr *table, char **key);
void free_hashtab_addr(hash_table_addr *table);
void reinitialize_hashtab_addr(hash_table_addr *table);
void compact_hashtab_addr(hash_table_addr *table);

#endif /* HASHTAB_ADDR_H */
