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
#ifndef HASHTAB_INT4_H
#define HASHTAB_INT4_H

/* Note that hashtab_addr has #define references to all items in this header file. Changes/additions
   should be reflected there as well.
*/

typedef struct
{
	uint4		key;
	void		*value;
} ht_ent_int4;

typedef struct hash_table_int4_struct
{
	ht_ent_int4 	*base;		/* base of array of hent_* entries */
	ht_ent_int4 	*top; 		/* top of array of hent_* entries */
	unsigned int 	size;		/* Hash table size */
	unsigned int 	count;		/* Number of valid entries */
	unsigned int 	del_count;	/* Number of entries marked deleted. */
	unsigned int 	exp_trigger_size;/* When exp_trigger_size entried are used, expand table */
	unsigned int 	cmp_trigger_size;/* When cmp_trigger_size reached compact table */
} hash_table_int4;

#define HTENT_EMPTY_INT4(tabent, type, htvalue) (!(htvalue = (type *)(tabent)->value))
#define HTENT_VALID_INT4(tabent, type, htvalue) (!(HTENT_EMPTY_INT4(tabent, type, htvalue)) && (HT_DELETED_ENTRY != htvalue))
#define COMPUTE_HASH_INT4(hkey, hash) hash = (*hkey)

/* Prototypes for int4 hash routines. See hashtab_implementation.h for detail interface and implementation */
void init_hashtab_int4(hash_table_int4 *table, int minsize);
void expand_hashtab_int4(hash_table_int4 *table, int minsize);
boolean_t add_hashtab_int4(hash_table_int4 *table, uint4 *key, void *value,  ht_ent_int4 **tabentptr);
void *lookup_hashtab_int4(hash_table_int4 *table, uint4 *key);
boolean_t delete_hashtab_int4(hash_table_int4 *table, uint4 *key);
void free_hashtab_int4(hash_table_int4 *table);
void reinitialize_hashtab_int4(hash_table_int4 *table);
void compact_hashtab_int4(hash_table_int4 *table);

#endif
