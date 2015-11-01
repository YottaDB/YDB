/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* hashtab.h - function prototypes for externally callable
 * routines in hashtab.c.
 */

#ifndef HASHTAB_H
#define HASHTAB_H

typedef struct hashtab_ent_t {
    mstr			*v;
    void			*key;
    struct hashtab_ent_t	*next_chain;	/* chain of entries hashing to same value */
    struct hashtab_ent_t	*next;
} hashtab_ent;

typedef struct hashtab_t {
    hashtab_ent *tbl;
    hashtab_ent *first;
    hashtab_ent *last;
    unsigned int size;
    unsigned int count;
} hashtab;

bool add_hashtab_ent(hashtab **h, void *key, void *v);
void init_hashtab(hashtab **h, int nelem);
void reinit_hashtab(hashtab **h);
void free_hashtab(hashtab **h);
void *lookup_hashtab_ent(hashtab *h, void *key, void *v);
boolean_t del_hashtab_ent(hashtab **h, void *key, void *v);
void expand_hashtab(hashtab **h, int minsiz);

#define FIRST_HASHTAB_ENT(h)	h->first
#define NEXT_HASHTAB_ENT(h,e)   e->next
#define HASHTAB_ENT_VALUE(e)	e->v

#define	HASH_KEY_INVALID	((void *)-1)

#ifdef DEBUG_FINDDUPS
#define DEBUG_FINDDUPS1							\
{									\
	PRINTF("element (0x%x) found in entry %d.\n",			\
	       v, h_ent - hptr->tbl);					\
	if (view_debug1)						\
	    assert(FALSE);						\
}
#define DEBUG_FINDDUPS2							\
{									\
	PRINTF(" element %d (0x%x) placed in slot %d\n", h_ent->v, 	\
		h_ent->v, h_ent - hptr->tbl);				\
}
#define DEBUG_FINDDUPS3							\
{									\
	PRINTF("created tbl w/%d elements.\n", hptr->size);		\
}
#else
#define DEBUG_FINDDUPS1
#define DEBUG_FINDDUPS2
#define DEBUG_FINDDUPS3
#endif

#endif
