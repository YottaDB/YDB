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

/* hashtab.c - generic routines to manage a hash table.
 *
 * Interface routines:
 * void init_hashtab(hashtab **h, int minsiz)
 *      creates and initializes a hash table big enough to hold minsiz
 *	elements.
 *
 * void reinit_hashtab(hashtab **h)
 *      re-initializes a hash table
 *
 * void free_hashtab(hashtab **h)
 *	frees memory used by a hash table.
 *
 * void expand_hashtab(hashtab **h, int minsiz)
 *      expands a pre-existing hash table to hold at least minsiz elements
 *
 * bool add_hashtab_ent(hashtab **h, void *key, void *v)
 *      Adds v to the hash table if it is not already there.  Returns:
 *		TRUE - element added to table, no duplicate value exists.
 *		FALSE - element not added to table (duplicate value of v found)
 *
 * boolean_t del_hashtab_ent(hashtab **h, void *key, void *v)
 *      deletes entry with 'key' from the hash table. Returns:
 *		TRUE - element deleted from the table,
 *		FALSE - element not found in the table
 *
 * void *lookup_hastab_ent(hashtab *h, void *key, void *v)
 *      attempts to find v in the hashtab.  Returns:
 *		hashtab_ent->v if found
 *		else null
 */

/* A bucket is considered empty if hashtab_ent->v and hashtab_ent->next_chain are NULL
 * A bucket is considered a deleted entry if hashtab_ent->v is NULL and hashtab_ent->next_chain is non NULL
 * Deletion just marks the hashtab_ent->v to be NULL without reshuffling the other pointers and entries.
 * For the purposes of adding an entry, the deleted entry is considered empty but for searching purposes
 * the chain is followed even through deleted entries.
 */

#include "mdef.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "hashtab.h"
#include "error.h"
#include "longset.h"

/* The size by which the hashtable is incremented after its size exceeds the table of primes */
#define HASHTABINCRSIZE 22932
#define hash(v, nelem)          (((unsigned int)v) % nelem)

/* the following variable hash_entry is modified as a side-affect of a call to add_hashtab_ent.
 * this holds the pointer to the hash element that was created or found (depending on whether a
 * lookup failed or not). This is necessary currently for the tp_hist routine to know what the
 * added hash element was. It may be needed for other routines also. it was considered not changing
 * the interface. In case, this becomes needed for a lot of other routines, it is better to change
 * the interface appropriately instead of having a global variable
 */

GBLDEF	void	*hash_entry;
GBLREF	bool	view_debug1;

bool add_hashtab_ent(hashtab **h, void *key, void *v)
{
	hashtab		*hptr;
	hashtab_ent 	*h_ent, *prev = NULL, *deleted_entry = NULL, *any_deleted_entry;
	int 		cnt;
	unsigned int	htbl_size;
	boolean_t 	need_restruct = FALSE;

	error_def(ERR_TEXT);

	hptr = *h;
	htbl_size = hptr->size;
	if (hptr->count > htbl_size / 2)
	{
		expand_hashtab(h, htbl_size + 1);
		htbl_size = hptr->size;
	}
	if (HASH_KEY_INVALID == key)
		key = v;
	h_ent = &hptr->tbl[hash(key, htbl_size)];
	if (h_ent->v || h_ent->next_chain)
	{
		do {
			if (!h_ent->v)                      		/* empty from a previous delete */
                        {
				if (key == h_ent->key)			/* deleted entry held the same key */
				{
					deleted_entry = h_ent;
					break;
				} else if (!deleted_entry)
				{
					/* if it is not the same key, store the first empty entry for reuse.
					 * Dont use it immediately, since there may be a duplicate entry
					 */
					deleted_entry = h_ent;
				}
			} else if (key == h_ent->key)
			{
				DEBUG_FINDDUPS1
				hash_entry = h_ent->v;
				return FALSE;
			}
			prev = h_ent;
			h_ent = h_ent->next_chain;
		} while (h_ent);
		if (!h_ent)
		{
			assert(prev);
			h_ent = prev;
		}
		if (!deleted_entry)
		{
			/* not found in chain, proceed to next empty element, wrapping
			 * around to the beginning as necessary
			 */
			any_deleted_entry = NULL;
			for (cnt = htbl_size;  cnt;  cnt--)
			{
				if (0 == h_ent->v)
				{			/* if it is a deleted entry, it must not be from the
							 * chain of entries that we traversed in the above  do-while loop
							 */
					assert (!key || key != h_ent->key);
					if (!h_ent->next_chain)		/* make sure that there is no cyclic linking */
						break;
					else if (!any_deleted_entry)
						any_deleted_entry = h_ent;
				}
				if (++h_ent - hptr->tbl >= htbl_size)
					h_ent = hptr->tbl;
			}
			if (0 == cnt)
			{
				if (any_deleted_entry)			/* To start with, we had more than half the entries free
									 * in the hashtable, But now we are in a situation, where
									 * we could not find even one entry that is purely free
									 * (excludes 'deleted' frees). So, more than half the
									 * entries are 'deleted' entries, restructure the hashtable
									 */
				{
					assert(!key || key != any_deleted_entry->key);
					h_ent = any_deleted_entry;
					need_restruct = TRUE;
				} else			/* table is really full - we should have accounted for this */
				{
					assert(FALSE);
					rts_error(VARLSTCNT(4) ERR_TEXT, 2,
						strlen("add_hashtab_ent:  hash table full"),
						"add_hashtab_ent:  hash table full");
					GTMASSERT;
				}
			}
		} else
			h_ent = deleted_entry;
	}
	/* no duplicate element found, h_ent points to empty or previously deleted slot */
	h_ent->v = v;
	h_ent->key = key;
	DEBUG_FINDDUPS2
	hptr->count++;
	if(!deleted_entry && !need_restruct)
	{
		assert(!h_ent->next_chain);
		if (prev)
			prev->next_chain = h_ent;
		h_ent->next = NULL;
		if (hptr->last)
		{
			hptr->last->next = h_ent;
			hptr->last = h_ent;
		}
		else /* empty table */
			hptr->first = hptr->last = h_ent;
	} else if (need_restruct)
		expand_hashtab(h, htbl_size);

	return TRUE;
}

void init_hashtab(hashtab **h, int minsiz)
{
	hashtab		*hptr;
	int		siz, index;
		/* Geometric sequence a, a*r, a*r*r,... with a = 2.5, r = 3.65 */
	static int	primes[] = {11, 37, 127, 1619, 5923, 21577, 78779, 287491, 0};
	int prevsiz = 0;
	static int prev_inc = 0;

	hptr = *h;
	if (!hptr)
	{
		hptr = *h = (void *)malloc(sizeof(hashtab));
		hptr->tbl = NULL;
	} else
		prevsiz = hptr->size;
	siz = primes[0];
	for(index = 0;  (0 != siz) && (siz < minsiz);  index++)
		siz = primes[index];
	if (0 == siz)	/* out of primes? */
	{
		prev_inc = (0 == prev_inc) ? HASHTABINCRSIZE : prev_inc + HASHTABINCRSIZE;
		siz =  prevsiz + prev_inc;
	}
	if (!hptr->tbl || hptr->size != siz)
	{
		hptr->size = siz;

		if (hptr->tbl)
			free(hptr->tbl);

		hptr->tbl = (void *)malloc(sizeof(hashtab_ent) * hptr->size);
	}
	longset((uchar_ptr_t)hptr->tbl, sizeof(hashtab_ent) * hptr->size, 0);
	hptr->first = NULL;
	hptr->last = NULL;
	hptr->count = 0;
	DEBUG_FINDDUPS3
}

void reinit_hashtab(hashtab **htab)
{
	hashtab	*hptr;

	hptr = *htab;
	assert(hptr->tbl && hptr->size);
	longset((uchar_ptr_t)hptr->tbl, sizeof(hashtab_ent) * hptr->size, 0);
	hptr->first = NULL;
	hptr->last = NULL;
	hptr->count = 0;
}

void free_hashtab(hashtab **h)
{
	hashtab	*hptr;

	hptr = *h;
	if (hptr->tbl)
	{
		free(hptr->tbl);
		hptr->tbl = NULL;
	}
	free(hptr);
	*h = NULL;
}

void expand_hashtab(hashtab **h, int minsiz)
{
	hashtab_ent 	*oldt, *h_ent;
	int 		oldsize = (*h)->size, i;
	boolean_t 	dummy;

	oldt = (*h)->tbl;
	(*h)->tbl = NULL;
	init_hashtab(h, minsiz);
	for(i=0, h_ent=oldt;  i < oldsize;  i++, h_ent++)
	{
		if (h_ent->v)
		{
			dummy = add_hashtab_ent(h, h_ent->key, h_ent->v);
			assert(TRUE == dummy);
		}
	}
	free(oldt);
}

void *lookup_hashtab_ent(hashtab *h, void *key, void *v)
{
	hashtab		*hptr;
	hashtab_ent 	*h_ent;
	int 		cnt;

	if (HASH_KEY_INVALID == key)
		key = v;
	hptr = h;
	h_ent = &hptr->tbl[hash(key, hptr->size)];
	if (h_ent->v || h_ent->next_chain)
	{
		for(;;)
		{
			if (h_ent->key == key)
				return(h_ent->v);	/* if h_ent->v == NULL (deleted), it anyway returns NULL */
			if (h_ent->next_chain)	/* not found yet, follow chain */
				h_ent = h_ent->next_chain;
			else
				break;
		}
	}
	return NULL;
}

/* This just marks a particular entry 'deleted' so that it is reusable and corresponding
 * lookup's fail. But it wont physically free up the space or adjust the pointers in the
 * hashtable.
 */

boolean_t del_hashtab_ent(hashtab **h, void *key, void *v)
{
	hashtab		*hptr;
	hashtab_ent 	*h_ent;

	if (HASH_KEY_INVALID == key)
		key = v;
	hptr = *h;
	h_ent = &hptr->tbl[hash(key, hptr->size)];

	if (h_ent->v || h_ent->next_chain)
	{
		for (; h_ent; h_ent = h_ent->next_chain)
		{
			if (h_ent->key == key)
			{
				if (h_ent->v)
				{
					h_ent->v = 0;
					hptr->count--;	/* h_ent->next_chain is left and not zeroed */
					return TRUE;
				}
				break;
			}
		}
	}
	return FALSE;
}
