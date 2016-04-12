/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Uniform Hash Implementation

Hash table code supports the following data types as key:
  a) int4
  b) int8
  c) UINTPTR_T
  d) object code
  e) variable name.
  f) local process address (supported via define using either int4 or int8 as type

Using pre-processor following four C files will expand five sets of routines.
  a) hashtab_int4.c
  b) hashtab_int8.c
  c) hashtab_addr.c
  d) hashtab_mname.c
  e) hashtab_objcode.c

Restrictions :
  We assumed that no user of hash needs to add "key, value" pair where both are null.
  We examined that GT.M does not need to have such cases.
  We can add 0 as valid data for int4 and int8, however, it must always have non-zero value.
  We can add 0 length string as "key" in objcode, however, it must always have non-zero length value.
  We know object code cannot be 0 length, so even object source (key) is of 0 length, we are fine.
  GT.M cannot have 0 length mname. So "key" for mname cannot be 0 length.
  (If we want to remove above restriction, an extra field is needed for HT_ENT)
*/

#include "mdef.h"
#include "gtm_malloc.h"		/* For raise_gtmmemory_error() definition */
#include "bit_set.h"
#include "gtmio.h"
#include "have_crit.h"

LITREF	int		ht_sizes[];

#define DEBUGHASHTABLE 0

#if defined(INT4_HASH)

#	define HT_KEY_T				uint4
#	define HT_ENT				ht_ent_int4
#	define HASH_TABLE			hash_table_int4
#	define HTENT_KEY_MATCH(tabent, hkey)	((tabent)->key == (*hkey))
#	define FIND_HASH(hkey, hash)		COMPUTE_HASH_INT4(hkey, hash)
#	define HTENT_EMPTY			HTENT_EMPTY_INT4
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_INT4
#	define HTENT_VALID			HTENT_VALID_INT4
#	define INIT_HASHTAB			init_hashtab_int4
#	define INIT_HASHTAB_INTL		init_hashtab_intl_int4
#	define EXPAND_HASHTAB			expand_hashtab_int4
#	define ADD_HASHTAB			add_hashtab_int4
#	define ADD_HASHTAB_INTL			add_hashtab_intl_int4
#	define LOOKUP_HASHTAB			lookup_hashtab_int4
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_int4
#	define DELETE_HASHTAB			delete_hashtab_int4
#	define FREE_HASHTAB			free_hashtab_int4
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_int4
#	define COMPACT_HASHTAB			compact_hashtab_int4

#elif defined(INT8_HASH)

#	define HT_KEY_T				gtm_uint64_t
#	define HT_ENT				ht_ent_int8
#	define HASH_TABLE			hash_table_int8
#	define HTENT_KEY_MATCH(tabent, hkey)	((tabent)->key == (*hkey))
#	define FIND_HASH(hkey, hash)		COMPUTE_HASH_INT8(hkey, hash)
#	define HTENT_EMPTY			HTENT_EMPTY_INT8
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_INT8
#	define HTENT_VALID			HTENT_VALID_INT8
#	define INIT_HASHTAB			init_hashtab_int8
#	define INIT_HASHTAB_INTL		init_hashtab_intl_int8
#	define EXPAND_HASHTAB			expand_hashtab_int8
#	define ADD_HASHTAB			add_hashtab_int8
#	define ADD_HASHTAB_INTL			add_hashtab_intl_int8
#	define LOOKUP_HASHTAB			lookup_hashtab_int8
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_int8
#	define DELETE_HASHTAB			delete_hashtab_int8
#	define FREE_HASHTAB			free_hashtab_int8
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_int8
#	define COMPACT_HASHTAB			compact_hashtab_int8

#elif defined(ADDR_HASH)

#	define HT_KEY_T				char *
#	define HT_ENT				ht_ent_addr
#	define HASH_TABLE			hash_table_addr
#	define HTENT_KEY_MATCH(tabent, hkey)	((tabent)->key == (*hkey))
#	define FIND_HASH(hkey, hash)		COMPUTE_HASH_ADDR(hkey, hash)
#	define HTENT_EMPTY			HTENT_EMPTY_ADDR
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_ADDR
#	define HTENT_VALID			HTENT_VALID_ADDR
#	define INIT_HASHTAB			init_hashtab_addr
#	define INIT_HASHTAB_INTL		init_hashtab_intl_addr
#	define EXPAND_HASHTAB			expand_hashtab_addr
#	define ADD_HASHTAB			add_hashtab_addr
#	define ADD_HASHTAB_INTL			add_hashtab_intl_addr
#	define LOOKUP_HASHTAB			lookup_hashtab_addr
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_addr
#	define DELETE_HASHTAB			delete_hashtab_addr
#	define FREE_HASHTAB			free_hashtab_addr
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_addr
#	define COMPACT_HASHTAB			compact_hashtab_addr

#elif defined(MNAME_HASH)

#	define HT_KEY_T				mname_entry
#	define HT_ENT				ht_ent_mname
#	define HASH_TABLE			hash_table_mname
#	define HTENT_KEY_MATCH(tabent, hkey)									\
	(    ((tabent)->key.hash_code == (hkey)->hash_code)							\
	     && ((tabent)->key.var_name.len == (hkey)->var_name.len)						\
	     && (0 == memcmp((tabent)->key.var_name.addr, (hkey)->var_name.addr, (hkey)->var_name.len))		\
	)
#	define FIND_HASH(hkey, hash)		{assert((hkey)->hash_code); hash = (hkey)->hash_code;}
	/* Note: FIND_HASH for mname does not compute hash_code. Callers must make sure it is already computed.
	 *	 FIND_HASH for objcode or int4 or int8 computes hash code
	 *		for every function call of add or lookup or delete. */
#	define HTENT_EMPTY			HTENT_EMPTY_MNAME
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_MNAME
#	define HTENT_VALID			HTENT_VALID_MNAME
#	define INIT_HASHTAB			init_hashtab_mname
#	define INIT_HASHTAB_INTL		init_hashtab_intl_mname
#	define EXPAND_HASHTAB			expand_hashtab_mname
#	define ADD_HASHTAB			add_hashtab_mname
#	define ADD_HASHTAB_INTL			add_hashtab_intl_mname
#	define LOOKUP_HASHTAB			lookup_hashtab_mname
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_mname
#	define DELETE_HASHTAB			delete_hashtab_mnamen
#	define FREE_HASHTAB			free_hashtab_mname
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_mname
#	define COMPACT_HASHTAB			compact_hashtab_mname

#elif defined(STRING_HASH)

#	define HT_KEY_T				stringkey
#	define HT_ENT				ht_ent_str
#	define HASH_TABLE			hash_table_str
#	define HTENT_KEY_MATCH(tabent, hkey)						\
	(((tabent)->key.hash_code == (hkey)->hash_code)					\
	 && ((tabent)->key.str.len == (hkey)->str.len)					\
	 && (0 == memcmp((tabent)->key.str.addr, (hkey)->str.addr, (hkey)->str.len))	\
	)
#	define FIND_HASH(hkey, hash)		hash = (hkey)->hash_code
/* Note: FIND_HASH for str does not compute hash_code. Callers must make sure it is already computed */
#	define HTENT_EMPTY			HTENT_EMPTY_STR
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_STR
#	define HTENT_VALID			HTENT_VALID_STR
#	define INIT_HASHTAB			init_hashtab_str
#	define INIT_HASHTAB_INTL		init_hashtab_intl_str
#	define EXPAND_HASHTAB			expand_hashtab_str
#	define ADD_HASHTAB			add_hashtab_str
#	define ADD_HASHTAB_INTL			add_hashtab_intl_str
#	define LOOKUP_HASHTAB			lookup_hashtab_str
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_str
#	define DELETE_HASHTAB			delete_hashtab_str
#	define FREE_HASHTAB			free_hashtab_str
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_str
#	define COMPACT_HASHTAB			compact_hashtab_str

#elif defined (OBJCODE_HASH)

#	define HT_KEY_T				icode_str
#	define HT_ENT				ht_ent_objcode
#	define HASH_TABLE			hash_table_objcode
#	define HTENT_KEY_MATCH(tabent, hkey)						\
	(((tabent)->key.code == (hkey)->code)						\
	 && ((tabent)->key.str.len == (hkey)->str.len)					\
	 && (0 == memcmp((tabent)->key.str.addr, (hkey)->str.addr, (hkey)->str.len))	\
	)
#	define FIND_HASH(hkey, hash)		COMPUTE_HASH_OBJCODE(hkey, hash)
#	define HTENT_EMPTY			HTENT_EMPTY_OBJCODE
#	define HTENT_MARK_EMPTY			HTENT_MARK_EMPTY_OBJCODE
#	define HTENT_VALID			HTENT_VALID_OBJCODE
#	define INIT_HASHTAB			init_hashtab_objcode
#	define INIT_HASHTAB_INTL		init_hashtab_intl_objcode
#	define EXPAND_HASHTAB			expand_hashtab_objcode
#	define ADD_HASHTAB			add_hashtab_objcode
#	define ADD_HASHTAB_INTL			add_hashtab_intl_objcode
#	define LOOKUP_HASHTAB			lookup_hashtab_objcode
#	define DELETE_HASHTAB_ENT		delete_hashtab_ent_objcode
#	define DELETE_HASHTAB			delete_hashtab_objcode
#	define FREE_HASHTAB			free_hashtab_objcode
#	define REINITIALIZE_HASHTAB		reinitialize_hashtab_objcode
#	define COMPACT_HASHTAB			compact_hashtab_objcode

#else
#error undefined hash
#endif

#if DEBUGHASHTABLE
/* Debug FPRINTF with pre and post requisite flushing of appropriate streams  */
#define DBGHASHTAB(x) {flush_pio(); FPRINTF x; FFLUSH(stderr);}
#else
#define DBGHASHTAB(x)
#endif

/* We use DOUBLE HASHING algorithm. After first collision we calculate
 * rehash factor (rhfact) that is a function of the hash value (even though for
 * correctness purposes any number from 1 to prime-1 should be fine) to
 * avoid primary and secondary clustering.
 * The SET_REHASH_INDEX is actually equivalent to ht_index = (ht_index + rhfact) % prime;
 */
#define SET_REHASH_FACTOR(rhfact, hash, prime)		(rhfact) = (1 + ((hash) % ((prime) - 1)))
#define SET_REHASH_INDEX(ht_index, rhfact, prime)	\
	assert((rhfact) < (prime));			\
	assert((ht_index) < (prime));			\
	(ht_index) += (rhfact);				\
	if ((ht_index) >= (prime))			\
		(ht_index) -= (prime);

#define RETURN_IF_ADDED(table, tabent, hkey, value)				\
{										\
	void	*dummy;	 							\
	if (HTENT_EMPTY(tabent, void, dummy))					\
	{									\
		if (NULL != first_del_ent)					\
			tabent = first_del_ent;					\
		INSERT_HTENT(table, tabent, hkey, value);			\
		return TRUE;							\
	}									\
	if (!HTENT_MARK_DELETED(tabent))					\
	{									\
		if (HTENT_KEY_MATCH(tabent, hkey))				\
			return FALSE; /* valid and matched */			\
	} else if (NULL == first_del_ent)					\
		first_del_ent = tabent;						\
}

#define RETURN_IF_LOOKUP_DONE(tabent, hkey)					\
{										\
	void	*dummy;								\
	if (HTENT_EMPTY(tabent, void, dummy))					\
		return NULL;							\
	if (!HTENT_MARK_DELETED(tabent) && HTENT_KEY_MATCH(tabent, hkey))	\
		return tabent; /* valid and matched */				\
}

#define DELETE_HTENT(table, tabent)						\
{										\
	uint4 entry_num;							\
	sm_uc_ptr_t ptr;							\
										\
	entry_num = (uint4)((tabent) - (table)->base); 				\
	/* Compute offset into bitmap for this entry */ \
	ptr = (table)->entry_passed_thru + (entry_num / BITS_PER_UCHAR);			\
	if ((1 << (entry_num & 7)) & *ptr) 					\
	{									\
		(tabent)->value = HT_DELETED_ENTRY; 				\
		(table)->del_count++;						\
	} else											\
		HTENT_MARK_EMPTY(tabent);					\
	(table)->count--;							\
	assert(((table)->count + (table)->del_count) <= (table)->size);		\
}

#define RETURN_IF_DELETED(table, tabent, hkey)					\
{										\
	void	*dummy;								\
	if (HTENT_EMPTY(tabent, void, dummy))					\
		return FALSE;							\
	if (!HTENT_MARK_DELETED(tabent) && HTENT_KEY_MATCH(tabent, hkey))	\
	{									\
		DELETE_HTENT(table, tabent);					\
		if (COMPACT_NEEDED(table))					\
			COMPACT_HASHTAB(table);					\
		return TRUE;							\
	}									\
}

#define HT_FIELDS_COMMON_INIT(table) 							\
	table->exp_trigger_size = (double)table->size * HT_LOAD_FACTOR / 100.0;		\
	table->cmp_trigger_size = (double)table->size * HT_REHASH_FACTOR / 100.0;	\
	table->count = table->del_count = 0;

/* Prototypes */
void INIT_HASHTAB(HASH_TABLE *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table);
STATICFNDCL void INIT_HASHTAB_INTL(HASH_TABLE *table, int minsize, HASH_TABLE *old_table);
void EXPAND_HASHTAB(HASH_TABLE *table, int minsize);
boolean_t ADD_HASHTAB(HASH_TABLE *table, HT_KEY_T *key, void *value,  HT_ENT **tabentptr);
STATICFNDCL boolean_t ADD_HASHTAB_INTL(HASH_TABLE *table, HT_KEY_T *key, void *value,  HT_ENT **tabentptr,
		boolean_t changing_table_size);
void *LOOKUP_HASHTAB(HASH_TABLE *table, HT_KEY_T *key);
void DELETE_HASHTAB_ENT(HASH_TABLE *table, HT_ENT *tabent);
boolean_t DELETE_HASHTAB(HASH_TABLE *table, HT_KEY_T *key);
void FREE_HASHTAB(HASH_TABLE *table);
void REINITIALIZE_HASHTAB(HASH_TABLE *table);
void COMPACT_HASHTAB(HASH_TABLE *table);

error_def(ERR_HTOFLOW);
error_def(ERR_HTSHRINKFAIL);

/* This is used by external callers to initially setup the hash table. */
void INIT_HASHTAB(HASH_TABLE *table, int minsize, boolean_t dont_compact, boolean_t dont_keep_spare_table)
{
	int index;

	for (index = 0, table->initial_size = ht_sizes[index]; table->initial_size && table->initial_size < minsize; index++)
		table->initial_size = ht_sizes[index];
	table->initial_size = table->initial_size ? table->initial_size : minsize;
	table->dont_compact = dont_compact;
	table->dont_keep_spare_table = dont_keep_spare_table;
	table->defer_base_release = FALSE;
	INIT_HASHTAB_INTL(table, minsize, NULL);
}

/* This routine initializes hash table. It must be called once before hashing can be used. Note that
   the ht_sizes array is defined in mtables.c. A NULL old_table pointer means that the table is being
   setup for the first time.
*/
STATICFNDEF void INIT_HASHTAB_INTL(HASH_TABLE *table, int minsize, HASH_TABLE *old_table)
{
	unsigned int 	cur_ht_size, prior_size;
	int 		index;
	boolean_t	dont_keep_spare_table;
	DBGHASHTAB((stderr, "INIT_HASHTAB:table(%lx) minsize(%d) old_table(%lx)\n", table, minsize, old_table));

	/* If this is the first time the hash table is being initialized (old_table == NULL), then look up the
	 * actual hash table size in ht_sizes based on the requested size (minsize).
	 *
	 * We dont want the hash table to shrink too fast so if we are changing the size of an existing hash table:
	 * 1) if the requested size is not smaller than half of the previous size:
	 *		a) pick the actual size from ht_sizes based on the requested size.
	 * 2) if the requested size is smaller than half of the previous size:
	 * 		b) pick the previous entry (from the previous size (old_table->size) in ht_sizes.
	 */
	if ((NULL == old_table) || (minsize > (old_table->size / 2)))
	{
		for (index = 0, cur_ht_size = ht_sizes[index]; cur_ht_size && cur_ht_size < minsize; index++)
			cur_ht_size = ht_sizes[index];
	} else /* don't shrink too fast ! */
	{
		prior_size = ht_sizes[0];
		for (index = 1, cur_ht_size = ht_sizes[index]; cur_ht_size && cur_ht_size < old_table->size; index++)
		{
			cur_ht_size = ht_sizes[index];
			prior_size = ht_sizes[index-1];
		}
		cur_ht_size = prior_size;
		cur_ht_size = (cur_ht_size > old_table->initial_size) ? cur_ht_size : old_table->initial_size;
	}
	if (cur_ht_size)
	{
		DBGHASHTAB((stderr, "INIT_HASHTAB:table size will be (%d) for table(%lx)\n",
			cur_ht_size, old_table?old_table:table));
		table->base = NULL; table->spare_base = NULL; table->spare_base_size = 0; /* a fresh new hash table */
		/* If this is is an initialization from a caller outside of the hash table implementation then
		 * old_table == NULL since there is no previous hash table. In this case the external versions of
		 * INIT_HASHTAB will setup table with values for dont_compact and dont_keep_spare_table. Otherwise,
		 * we can use them from the old_table.
		 */
		dont_keep_spare_table = old_table ? old_table->dont_keep_spare_table:table->dont_keep_spare_table;
		if (!dont_keep_spare_table)
		{
			DBGHASHTAB((stderr, "INIT_HASHTAB: old_table(%lx)\n", old_table));
			if (NULL != old_table)
			{
				DBGHASHTAB((stderr, "INIT_HASHTAB: cur_ht_size(%d), spare_base_size(%d)\n",
					cur_ht_size, old_table->spare_base_size));
				if (old_table->spare_base_size == cur_ht_size)
				{ /* We can use the spare table since it is the size we would have malloc'd */
					table->base = old_table->spare_base;
					DBGHASHTAB((stderr, "INIT_HASHTAB: use spare table: base(%lx)\n", table->base));
					/* We no longer have a spare */
					old_table->spare_base = NULL;
					old_table->spare_base_size = 0;
				} else /* no luck on the reuse thing */
					if (NULL != old_table->spare_base) /* so free it if it exists */
					{
						DBGHASHTAB((stderr, "INIT_HASHTAB: table(%lx): free spare_base(%lx)\n",
							old_table, old_table->spare_base));
						free(old_table->spare_base);
						old_table->spare_base = NULL;
						old_table->spare_base_size = 0;
					}
			}
		}
		if (NULL == table->base)
		{
			/* Let's make sure we have a HT_ENT table. We are here either thru dont_keep_spare_table,
			   old_table == NULL, or wrong-sized spare */
			table->base = (void *)malloc((cur_ht_size * SIZEOF(HT_ENT)) + ROUND_UP(cur_ht_size, BITS_PER_UCHAR));
			DBGHASHTAB((stderr, "INIT_HASHTAB: malloc a new table: table(%lx) base(%lx)\n",
				old_table?old_table:table, table->base));
		}
		memset((char *)table->base, 0, (cur_ht_size * SIZEOF(HT_ENT)) + ROUND_UP(cur_ht_size, BITS_PER_UCHAR));
		table->size = cur_ht_size;
		if (NULL != old_table)
		{
			table->initial_size = old_table->initial_size;
			table->dont_compact = old_table->dont_compact;
			table->dont_keep_spare_table = old_table->dont_keep_spare_table;
			table->defer_base_release = old_table->defer_base_release;
		}
		table->top = table->base + cur_ht_size;
		table->entry_passed_thru = (sm_uc_ptr_t) table->top;
		DBGHASHTAB((stderr, "INIT_HASHTAB: entry_passed_thru points to (%lx)\n", table->entry_passed_thru));
		HT_FIELDS_COMMON_INIT(table);
	} else
	{
		DBGHASHTAB((stderr, "INIT_HASHTAB:HTOFLOW: minsize(%d) cur_ht_size(%d)\n", minsize, cur_ht_size));
 		send_msg(VARLSTCNT(3) ERR_HTOFLOW, 1, minsize);
 		rts_error(VARLSTCNT(3) ERR_HTOFLOW, 1, minsize);
	}
}
/* Description:
	Expand the hash table with at least minsize.
	This can do either expansion or compaction, which depends on old table size and minsize passed.
	It creates a new table and move old element to new table.
	It deallocate old table entries
*/
void EXPAND_HASHTAB(HASH_TABLE *table, int minsize)
{
	HASH_TABLE 	newtable, temptab;
	HT_ENT 		*tabent, *topent, *dummy;
	boolean_t	added;
	void		*htval;
	CONDITION_HANDLER(hashtab_rehash_ch);
	ESTABLISH(hashtab_rehash_ch);
	DBGHASHTAB((stderr, "EXPAND_HASHTAB:ENTER: table: table(%lx) base (%lx), spare_base(%lx), spare_base_size(%d), \n",
		table, table->base, table->spare_base, table->spare_base_size));
	/* The next line keeps the HP-UX Itanium compiler in pro happy. This initialization is done is INIT_HASHTAB_INTL*
	 * but this line is placed here to appease the compiler.
	 */
	newtable.dont_keep_spare_table = table->dont_keep_spare_table;
	INIT_HASHTAB_INTL(&newtable, minsize, table);
	REVERT;
	if (0 < table->count) /* if no active entries then nothing to move */
		for (tabent = table->base, topent = table->top; tabent < topent; tabent++)
		{
			if (HTENT_VALID(tabent, void, htval))
			{
				/* Place location of new ht_ent entry into value location of existing ht entry */
				added = ADD_HASHTAB_INTL(&newtable, &tabent->key, htval, (HT_ENT **)&tabent->value, TRUE);
				assert(added);
			}
		}
	if (!table->defer_base_release && table->dont_keep_spare_table)
	{
		DBGHASHTAB((stderr, "EXPAND_HASHTAB:free base (%lx) \n", table->base));
		free(table->base); 	/* Deallocate old table entries */
	}
	if (!table->dont_keep_spare_table)
	{
		temptab.spare_base = table->base; /* let's keep a spare in case we just have to clear the DELETED entries */
		temptab.spare_base_size = table->size;
	}

	*table = newtable;

	if (table->dont_keep_spare_table)
	{
		table->spare_base = NULL;
		table->spare_base_size = 0;
	} else
	{
		table->spare_base = temptab.spare_base; /* let's keep a spare in case we just have to clear the DELETED entries */
		table->spare_base_size = temptab.spare_base_size;
	}
	DBGHASHTAB((stderr, "EXPAND_HASHTAB:EXIT: table: table(%lx ) base (%lx), spare_base(%lx), spare_base_size(%d) \n",
		table, table->base, table->spare_base, table->spare_base_size));
}

/* 	Description:
		Adds a key and corresponding value in hash table.
		If key is already present, return false.
		If key is not present, it adds the entry and returns true.
		As a side-effect tabent points to the matched entry or added entry
*/
/* This flavor is used by external caller (outside of the hash table implementation */
boolean_t ADD_HASHTAB(HASH_TABLE *table, HT_KEY_T *key, void *value,  HT_ENT **tabentptr)
{
	return ADD_HASHTAB_INTL(table, key, value, tabentptr, FALSE);
}

/* This flavor is used by internal callers, for example when adding entries during a change of hash table size. */
STATICFNDEF boolean_t ADD_HASHTAB_INTL(HASH_TABLE *table, HT_KEY_T *key, void *value,  HT_ENT **tabentptr,
		boolean_t changing_table_size)
{
#ifdef INT8_HASH
	gtm_uint64_t    hash, ht_index, save_ht_index, prime, rhfact;
#else
	uint4	 	hash, ht_index, save_ht_index, prime, rhfact;
#endif /* INT8_HASH */
	HT_ENT		*oldbase, *first_del_ent, *tabbase;
	if (!changing_table_size && (table->count >= table->exp_trigger_size))
	{
		oldbase = table->base;
		EXPAND_HASHTAB(table, table->size + 1);
		if (oldbase == table->base) /* expansion failed */
		{
			if (table->exp_trigger_size >= table->size)
				/* Note this error routine will use the memory error parameters recorded when the
				   memory error was first raised by EXPAND_HASHTAB() above so the error will appear
				   as if it had occurred during that expansion attempt.
				*/
			raise_gtmmemory_error();
			table->exp_trigger_size = table->size;
		}
	}
	first_del_ent = NULL;
	tabbase = &table->base[0];
	prime = table->size;
	FIND_HASH(key, hash);
	ht_index = (int) (hash % prime);
	*tabentptr = tabbase + ht_index;
	RETURN_IF_ADDED(table, *tabentptr, key, value);
	/* We are here because collision happened. Do collision resolution */
#	ifdef INT8_HASH
	assert(MAXUINT4 > ht_index);
#	endif
	bit_set(ht_index, table->entry_passed_thru);
	save_ht_index = ht_index;
	SET_REHASH_FACTOR(rhfact, hash, prime);
	SET_REHASH_INDEX(ht_index, rhfact, prime);
	do
	{
		*tabentptr = tabbase + ht_index;
		RETURN_IF_ADDED(table, *tabentptr, key, value);
#		ifdef INT8_HASH
		assert(MAXUINT4 > ht_index);
#		endif
		bit_set(ht_index, table->entry_passed_thru);
		SET_REHASH_INDEX(ht_index, rhfact, prime);
	} while(ht_index != save_ht_index);
	/* All entries either deleted or used. No empty frame found */
	if (NULL != first_del_ent)
	{	/* There was a deleted one we could use - reuse the deleted frame */
		*tabentptr = first_del_ent;
		INSERT_HTENT(table, *tabentptr, key, value);
		return TRUE;
	}
	GTMASSERT;
	return FALSE; /* to prevent warnings */
}

/*
 *	Returns pointer to the value corresponding to key, if found.
 *	Otherwise, it returns null.
 */
void *LOOKUP_HASHTAB(HASH_TABLE *table, HT_KEY_T *key)
{
#	ifdef INT8_HASH
	gtm_uint64_t 	hash, ht_index, save_ht_index, prime, rhfact;
#	else
	uint4 		hash, ht_index, save_ht_index, prime, rhfact;
#	endif /* INT8_HASH */
	HT_ENT		*tabent, *tabbase;

	tabbase = &table->base[0];
	prime = table->size;
	FIND_HASH(key, hash);
	ht_index = hash % prime;
	tabent = tabbase + ht_index;
	RETURN_IF_LOOKUP_DONE(tabent, key);
	/* We are here because collision happened. Do collision resolution */
	save_ht_index = ht_index;
	SET_REHASH_FACTOR(rhfact, hash, prime);
	SET_REHASH_INDEX(ht_index, rhfact, prime);
	do
	{
		tabent = tabbase + ht_index;
		RETURN_IF_LOOKUP_DONE(tabent, key);
		SET_REHASH_INDEX(ht_index, rhfact, prime);
	} while(ht_index != save_ht_index);
	return (void *)NULL;
}
/* 	Description:
	Deletes hash table entry from hash table (whether it was active or not).
	The function version is for callers outside of the hash table implementation.
*/
void DELETE_HASHTAB_ENT(HASH_TABLE *table, HT_ENT *tabent)
{
	DELETE_HTENT(table, tabent);
}
/*
 *	Returns TRUE if
 *		1) key is found and deleted successfully
 *			or
 *		2) already key was marked deleted.
 *	Otherwise, it returns FALSE
 *	Deletion is done by marking value to HT_DELETED_ENTRY.
 * 	If there are too many deleted entry, we call expand_hashtab() to do the
 * 	compaction eliminating entries marked HT_DELETED_ENTRY
 *	Compaction will save memory and also cause LOOKUP_HASHTAB to run faster.
 */
boolean_t DELETE_HASHTAB(HASH_TABLE *table, HT_KEY_T *key)
{
#	ifdef INT8_HASH
	gtm_uint64_t	hash, ht_index, save_ht_index, prime, rhfact;
#	else
	uint4		hash, ht_index, save_ht_index, prime, rhfact;
#	endif /* INT8_HASH */
	HT_ENT		*tabent, *tabbase;

	tabbase = &table->base[0];
	prime = table->size;
	FIND_HASH(key, hash);
	ht_index = hash % prime;
	tabent = tabbase + ht_index;
	RETURN_IF_DELETED(table, tabent, key);
	/* We are here because collision happened. Do collision resolution */
	save_ht_index = ht_index;
	SET_REHASH_FACTOR(rhfact, hash, prime);
	SET_REHASH_INDEX(ht_index, rhfact, prime);
	do
	{
		tabent = tabbase + ht_index;
		RETURN_IF_DELETED(table, tabent, key);
		SET_REHASH_INDEX(ht_index, rhfact, prime);
	} while(ht_index != save_ht_index);
	return FALSE;
}

/*
 * free memory occupied by hash table.
 */
void FREE_HASHTAB(HASH_TABLE *table)
{
	if (table->base)
	{
		DBGHASHTAB((stderr, "FREE_HASHTAB:free table(%lx): base (%lx)\n", table, table->base));
		free(table->base);
	}
	table->base = NULL;
	if (table->spare_base)
	{
		DBGHASHTAB((stderr, "FREE_HASHTAB:free table(%lx): spare_base (%lx)\n", table, table->spare_base));
		free(table->spare_base);
	}
	table->spare_base = NULL;
	table->spare_base_size = 0;
}

/*
 * Returns TRUE, if key found and deleted successfully or already deleted.
 */
void REINITIALIZE_HASHTAB(HASH_TABLE *table)
{
	memset((char *)table->base, 0, (table->size * SIZEOF(HT_ENT)) + ((table->size / BITS_PER_UCHAR) + 1));
	HT_FIELDS_COMMON_INIT(table);
}

/*
 * Compact hashtable removing entries marked deleted. Note that this is necessary because
 * of the search algorithm in ADD_HASHTAB which needs to find if a key exists before it can
 * add a new entry. It keeps searching until it either finds the key, finds an empty (never
 * used) entry or until it searches the entire table. So we need to replentish the supply of
 * never used nodes.
 */
void COMPACT_HASHTAB(HASH_TABLE *table)
{
	HT_ENT	*oldbase;

	DBGHASHTAB((stderr, "COMPACT_HASHTAB: table(%lx)\n", table));
	if (!table->dont_compact)
	{
		oldbase = (table)->base;
		EXPAND_HASHTAB(table, HT_REHASH_TABLE_SIZE(table));
		if (oldbase == (table)->base) /* rehash failed */
		{	/* We will continue but performance will likely suffer */
			send_msg(VARLSTCNT(1) ERR_HTSHRINKFAIL);
			(table)->cmp_trigger_size = (table)->size;
		}
	}
}
