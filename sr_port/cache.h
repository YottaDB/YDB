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

#ifndef CACHE_H
#define CACHE_H

#define CACHE_TAB_SIZE 96
#define CACHE_TAB_ENTRIES 67

typedef struct cache_ent
{
	struct
	{
		struct cache_ent *fl, *bl;	/* Link entries to a given cache table entry or temp elements */
	} linkq, linktemp;
	unsigned char	code;
	mstr		src;
	mstr		obj;
	int		real_obj_len;		/* Real length of allocated buffer */
	int		refcnt;			/* Can be in use more than once */
	boolean_t	referenced;		/* For clock reuse algorithm */
	boolean_t	temp_elem;		/* Not a perm cache entry - release when element goes inactive */
} cache_entry;

typedef struct cache_table
{
	cache_entry *fl,*bl;
} cache_tabent;

#define IF_INDR_FRAME_CLEANUP_CACHE_ENTRY(frame_pointer)						\
{													\
	void cache_cleanup(stack_frame *);								\
	/* See if unwinding an indirect frame*/								\
	if (frame_pointer->flags & SFF_INDCE)								\
		cache_cleanup(frame_pointer);								\
}

#define IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(frame_pointer)					\
{													\
	void cache_cleanup(stack_frame *);								\
	/* See if unwinding an indirect frame*/								\
	if (frame_pointer->flags & SFF_INDCE)								\
	{												\
		cache_cleanup(frame_pointer);								\
                frame_pointer->flags &= SFF_INDCE_OFF;							\
	}												\
}

void cache_del(unsigned char code, mstr *source, mstr *object);
void cache_init(void);
void cache_put(unsigned char code, mstr *source, mstr *object);
void cache_hash (unsigned char code, mstr *source);
mstr *cache_get (unsigned char code, mstr *source);
void cache_stats(void);

#endif
