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
#include <sys/mman.h>
#include <errno.h>
#include "mdef.h"
#include "mdefsp.h"

#ifndef STRINGPOOL_DEF_INCLUDED
#define STRINGPOOL_DEF_INCLUDED

/* The mstr_sort_array and mstr_protect_array have different requirements and therefore slightly different designs.
 * The first set of differences flows from differences in the constituent structures, namely that both mstr_array_element types
 * must support the following pseudo-ops efficiently:
 * 	- is_mstr_in_stringpool()
 * 	- delete_mstr_from_array()
 * 	- ***add_mstr_to_array()***
 * In addition, the sort_array_element must support the following virtual operation:
 * 	- has_mstr_changed_since_last_sort()
 * Which is why it caches the value of the mstr. We do not need to perform this operation on the protect array (which consists of
 * a list of mstrs which must be checked at any given garbage collection). This also allows us to conduct the sort without loading
 * the actual mstr->addrs for each comparison, preserving cache locality and saving a lot of cycles. The second set of differences
 * flows from different acccess patterns and requirements. In short: the sort array is only ever added to from stp_gcol/move, while
 * the protect array can be expanded by any code which needs to protect a string. So the protect array needs to efficiently handle
 * expansion and not fragment under conditions of expansion. The sort array needs to support efficient compaction to get rid of
 * deleted elements at stp_gcol time, and efficient in-place merging of a pre-sorted compacted segment and elements that have
 * changed since the last sort.
 * For this reason the protect array is a flat array with a free-list of deleted elements, for which it reuses the string pointer as
 * a next-element pointer in the singly-linked free-list. The sort array is a circular buffer which is compacted and divided into
 * two segments at stp_gcol_time, with already-sorted and unchanged elements at the start and to-be-sorted elements at the end,
 * either from having changed or from having been added to the protection list since the last stp_gcol and pointing into the
 * stringpool. The circular buffer allows us to do efficient sorting of the second half and efficient merging of the two halves
 * without needing to have any more than n_presorted room. The mstrs to which the array elements (in both cases) point have not
 * pointers but coordinates that identify a unique array element. The reason for this is that is makes it cheap to do array
 * expansion, which might occur at any protection. While there is still the cost of the malloc and memcpy, the coordinate scheme
 * allows us to avoid the costly fixup of pointers that would otherwise be required in all the elements.
 */
typedef struct mstr_sort_array
{
	size_t size;	/* How many elements can we fit? */
	size_t count;	/* How many elements currently? */
	size_t start_ele; /* Virtual offset of start of array */
	unsigned int mask; /* RTS or indr */
	mstr_sort_array_element array[];
} mstr_sort_array;

typedef struct mstr_protect_array
{
	size_t size;  /* How many elements can we fit? */
	size_t count; /* How many elements currently? */
	size_t fcount;	/* How many elements on the free list? */
	unsigned int mask; /* RTS or indr */
	struct mstr_protect_array_element flist;
	mstr_protect_array_element array[];
} mstr_protect_array;

typedef struct
{
	unsigned char	*base, *free, *top, *invokestpgcollevel;
	size_t		lastallocbytes;	/* stores the last mmap() allocation */
	mstr_sort_array		**sort_array_pp; /* Contains mstrs which were sorted in the last gcol, and lvTreeNodes (static) */
	mstr_protect_array	**protect_array_pp; /* Mstrs which need protection but were either not in the stringpool at last
						     * gcol or have not yet seen a gcol. Saves space since no need to check for
						     * whether an intervening change has jiggered the list, just whether the string
						     * needs protection from the current garbage collection (and placement in the
						     * sort array).
						     */
	unsigned int	gcols;		/* some optimizations need to know if the stringpool garbage collected  */
	boolean_t	pending_moves;
	unsigned int	strpllim;	/* non-zero value is user specified guard on expansion */
	boolean_t	strpllimwarned;	/* if limit is in place, has been exceeded, and not recovered from */
	unsigned char	prvprt;		/* stores memory protections used in guarding stp space */
} spdesc;

#define CHARLEN_DELETED 0x0
#define CHARLEN_NORMAL 	0x1
#define CHARLEN_LVTREE 	0x2

void	*stp_mmap(size_t size);
void	stp_expand_array(uint4 *array_size_p, mstr ***array_p, size_t element_size, size_t request);
void	stp_gcol(size_t space_needed);										/* BYPASSOK */
void	stp_move(char *from, char *to);
void	stp_init(size_t size);
void	stp_fini(unsigned char *strpool_base, size_t size);
void	s2pool(mstr *a);
void	s2pool_concat(mval *dst, mstr *a);	/* concatenates strings "dst->str" + "a" and stores result in "dst->str" */

#ifdef DEBUG
void		stp_vfy_mval(void);
boolean_t	is_stp_space_available(ssize_t space_needed);
#endif

#ifdef DEBUG
#define	IS_STP_SPACE_AVAILABLE(SPC)	is_stp_space_available((ssize_t)(SPC))
#else
#define	IS_STP_SPACE_AVAILABLE(SPC)	IS_STP_SPACE_AVAILABLE_PRO((ssize_t)(SPC))
#endif

GBLREF	spdesc		stringpool;

#define	IS_STP_SPACE_AVAILABLE_PRO(SPC)	((stringpool.free + SPC) <= stringpool.invokestpgcollevel)
#define	IS_IN_STRINGPOOL(PTR, LEN) (((UINTPTR_T)stringpool.base <= (UINTPTR_T)PTR) && ((UINTPTR_T)PTR < (UINTPTR_T)stringpool.top) \
		&& ((UINTPTR_T)LEN <= ((UINTPTR_T)stringpool.top - (UINTPTR_T)PTR)))
#define	IS_AT_END_OF_STRINGPOOL(PTR, LEN)		(((unsigned char *)PTR + (int)(LEN)) == stringpool.free)
#define	INVOKE_STP_GCOL(SPC)		stp_gcol(SPC);								/* BYPASSOK */

#ifdef DEBUG
GBLREF	boolean_t	stringpool_unusable;
GBLREF	boolean_t	stringpool_unexpandable;
#define	DBG_MARK_STRINGPOOL_USABLE		{ assert(stringpool_unusable); stringpool_unusable = FALSE; }
#define	DBG_MARK_STRINGPOOL_UNUSABLE		{ assert(!stringpool_unusable); stringpool_unusable = TRUE; }
#define	DBG_MARK_STRINGPOOL_EXPANDABLE		{ assert(stringpool_unexpandable); stringpool_unexpandable = FALSE; }
#define	DBG_MARK_STRINGPOOL_UNEXPANDABLE	{ assert(!stringpool_unexpandable); stringpool_unexpandable = TRUE; }
#define DBG_START_NO_GCOLS(CNT)			{ (CNT) = stringpool.gcols; }
#define DBG_END_NO_GCOLS(CNT)			{ assert((CNT) == stringpool.gcols); }
#else
#define	DBG_MARK_STRINGPOOL_USABLE
#define	DBG_MARK_STRINGPOOL_UNUSABLE
#define	DBG_MARK_STRINGPOOL_EXPANDABLE
#define	DBG_MARK_STRINGPOOL_UNEXPANDABLE
#define DBG_START_NO_GCOLS(CNT)
#define DBG_END_NO_GCOLS(CNT)
#endif

error_def(ERR_SYSCALL);

#define	ENSURE_STP_FREE_SPACE(SPC)						\
{										\
	uint4	lcl_spc_needed;							\
	/* Note down space needed in local to avoid multiple computations */	\
	lcl_spc_needed = (uint4)SPC;						\
	if (!IS_STP_SPACE_AVAILABLE(lcl_spc_needed))				\
		INVOKE_STP_GCOL(lcl_spc_needed);				\
	assert(IS_STP_SPACE_AVAILABLE(lcl_spc_needed));				\
}

#define	ADD_TO_STPARRAY(PTR, PTRARRAY, PTRARRAYCUR, PTRARRAYTOP, TYPE)					\
{													\
	GBLREF mstr	**stp_array;									\
	GBLREF uint4	stp_array_size;									\
	int     	save_errno;									\
													\
	if (NULL == PTRARRAY)										\
	{												\
		if (NULL == stp_array)									\
		{											\
			/* Same initialization as is in stp_gcol_src.h */				\
			stp_array_size = STP_MAXITEMS;							\
			stp_array = (mstr **)malloc(stp_array_size * SIZEOF(mstr *));			\
		}											\
		PTRARRAYCUR = PTRARRAY = (TYPE **)stp_array;						\
		PTRARRAYTOP = PTRARRAYCUR + stp_array_size;						\
	} else												\
	{												\
		assert(PTRARRAYCUR);									\
		assert(PTRARRAYTOP);									\
		if (PTRARRAYCUR >= PTRARRAYTOP)								\
		{											\
			stp_expand_array(&stp_array_size, &stp_array, sizeof(*stp_array), 0);		\
			PTRARRAYCUR = (TYPE **)stp_array + (PTRARRAYCUR - PTRARRAY);			\
			PTRARRAY = (TYPE **)stp_array;							\
			PTRARRAYTOP = PTRARRAY + stp_array_size;					\
		}											\
	}												\
	*PTRARRAYCUR++ = PTR;										\
}

#define	COPY_ARG_TO_STRINGPOOL(DST, KEYEND, KEYSTART)				\
MBSTART {									\
	int	keylen;								\
										\
	keylen = (unsigned char *)(KEYEND) - (unsigned char *)(KEYSTART);	\
	ENSURE_STP_FREE_SPACE(keylen);						\
	assert(stringpool.top - stringpool.free >= keylen);			\
	memcpy(stringpool.free, (KEYSTART), keylen);				\
	(DST)->mvtype = (MV_STR);						\
	(DST)->str.len = keylen;						\
	(DST)->str.addr = (char *)stringpool.free;				\
	stringpool.free += keylen;						\
} MBEND

#endif /* STRINGPOOL_DEF_INCLUDED */
