/****************************************************************
 *								*
 * Copyright (c) 2026 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GCOL_LIST_H_INCLUDED
#define GCOL_LIST_H_INCLUDED
#include "mdef.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "gtm_atomic.h"
#include "min_max.h"
#include "wbox_test_init.h"
#include <stdint.h>

#ifdef DEBUG_GCOL
#include "caller_id.h"
#define DEBUG_GCOL_ONLY(X) X
#else
#define DEBUG_GCOL_ONLY(X)
#endif


GBLREF spdesc			rts_stringpool, indr_stringpool;
GBLREF volatile	boolean_t 	timer_in_handler;
GBLREF int 			process_exiting;
GBLREF mval			dollar_zgbldir;
#ifdef DEBUG_GCOL
GBLREF unsigned int gcol_stack_lvl;
#endif
#define RTS_MASK 		0x1
#define INDR_MASK		0x2
#define PROTECT_ARRAY_MASK	0x4
#define SORT_ARRAY_MASK		0x8
#define ARRAY_MASK		(PROTECT_ARRAY_MASK | SORT_ARRAY_MASK)
#define STP_MASK		(RTS_MASK | INDR_MASK)

#define RTS_PROTECT_ARRAY (RTS_MASK | PROTECT_ARRAY_MASK)
#define RTS_SORT_ARRAY (RTS_MASK | SORT_ARRAY_MASK)
#define INDR_PROTECT_ARRAY (INDR_MASK | PROTECT_ARRAY_MASK)
#define INDR_SORT_ARRAY (INDR_MASK | SORT_ARRAY_MASK)

#define GCOL_SORTS_BEFORE(X, Y) ((X)->addr < (Y)->addr)

#define GET_COORD_ARRAYTYPE(IN_ARRAY) ((0xFFFFLU) & ((IN_ARRAY) >> 48LU))
#define GET_COORD_INDEX(IN_ARRAY) ((0xFFFFFFFFFFFF) & (IN_ARRAY))
#define SET_COORD_ARRAYTYPE(IN_ARRAY, ARRAYTYPE) ((IN_ARRAY) = (((IN_ARRAY) & ~((0xFFFFLU) << 48LU)) | ((ARRAYTYPE) << 48LU)))
#define SET_COORD_INDEX(IN_ARRAY, INDEX) ((IN_ARRAY) = (((IN_ARRAY) & ~0xFFFFFFFFFFFFLU) | ((INDEX) & 0xFFFFFFFFFFFFLU)))
#define SET_COORD_INDEX_AND_ARRAYTYPE(IN_ARRAY, INDEX, ARRAYTYPE)						\
	((IN_ARRAY) = (((INDEX) & 0xFFFFFFFFFFFFLU) | ((ARRAYTYPE) << 48LU)))
#define REAL_SORT_INDEX(INDEX, SORT_ARRAY) (((INDEX) + (SORT_ARRAY)->start_ele) & ((SORT_ARRAY)->size - 1))
#define REAL_SORT_INDEX_ELE_SZ(INDEX, ELE, SZ) (((INDEX) + (ELE)) & ((SZ) - 1))


static inline boolean_t glist_ptr_span_in_range(const void *ptr, size_t len, const void *low_p, const void *high_p)
{
	UINTPTR_T p = (UINTPTR_T)ptr;
	UINTPTR_T base = (UINTPTR_T)low_p;
	UINTPTR_T top = (UINTPTR_T)high_p;

	return (base <= p) && (p < top) && ((UINTPTR_T)len <= (top - p));
}

static inline uint_least64_t glist_get_arraytype(uint_least64_t in_array)
{
	return GET_COORD_ARRAYTYPE(in_array);
}

static inline uint_least64_t glist_get_index(uint_least64_t in_array)
{
	return GET_COORD_INDEX(in_array);
}

static inline uint_least64_t glist_set_arraytype(uint_least64_t *in_array_p, uint_least64_t arraytype)
{
	return SET_COORD_ARRAYTYPE(*in_array_p, arraytype);
}

static inline uint_least64_t glist_set_index(uint_least64_t *in_array_p, uint_least64_t index)
{
	return SET_COORD_INDEX(*in_array_p, index);
}

static inline boolean_t glist_protect_element_inbounds(
	mstr_protect_array *protect_array_p, mstr_protect_array_element *protect_ele_p)
{
	return ((protect_ele_p >= &protect_array_p->flist) && (protect_ele_p < &protect_array_p->array[protect_array_p->size]));
}

static inline boolean_t glist_protect_element_in_flist(
	mstr_protect_array *protect_array_p, mstr_protect_array_element *protect_ele_p)
{
	assert(glist_protect_element_inbounds(protect_array_p, protect_ele_p));
	return glist_protect_element_inbounds(protect_array_p, protect_ele_p->next_ele_p);
}

static inline mstr_protect_array_element *glist_get_protect_element_from_flist(mstr_protect_array *protect_array_p)
{
	mstr_protect_array_element *result;

	if (!protect_array_p->fcount)
	{
		assert(!protect_array_p->flist.next_ele_p);
		return NULL;
	}
	assert(protect_array_p->flist.next_ele_p);
	result = protect_array_p->flist.next_ele_p;
	protect_array_p->flist.next_ele_p = protect_array_p->flist.next_ele_p->next_ele_p;
	protect_array_p->fcount--;
	result->str_p = NULL;
	return result;
}

static inline void glist_add_protect_element_to_flist(mstr_protect_array *protect_array_p,
						      mstr_protect_array_element *protect_ele_p)
{
	assert(!protect_ele_p->str_p);
	protect_ele_p->next_ele_p = protect_array_p->flist.next_ele_p;
	protect_array_p->flist.next_ele_p = protect_ele_p;
	protect_array_p->fcount++;
	assert(protect_array_p->flist.next_ele_p);
	assert(protect_array_p->fcount <= protect_array_p->count);
}

static inline uint_least64_t glist_get_protect_coords(mstr_protect_array *protect_array_p,
						      mstr_protect_array_element *protect_array_ele_p)
{
	uint_least64_t result = 0;

	assert(protect_array_p->mask == INDR_PROTECT_ARRAY || protect_array_p->mask == RTS_PROTECT_ARRAY);
	glist_set_arraytype(&result, protect_array_p->mask);
	assert((protect_array_ele_p >= protect_array_p->array)
		&& (protect_array_ele_p < &protect_array_p->array[protect_array_p->size]));;
	if (protect_array_ele_p >= protect_array_p->array && protect_array_ele_p < &protect_array_p->array[protect_array_p->size])
		glist_set_index(&result, protect_array_ele_p - protect_array_p->array);
	else
	{
		assert(FALSE);
		glist_set_arraytype(&result, 0);
	}
	return result;

}

static inline boolean_t glist_inbounds(uint_least64_t arraytype, uint_least64_t index)
{
	switch (arraytype)
	{
		case RTS_PROTECT_ARRAY:
			if ((*rts_stringpool.protect_array_pp)->mask != arraytype)
				return false;
			if (index >= (*rts_stringpool.protect_array_pp)->count)
				return false;
			break;
		case RTS_SORT_ARRAY:
			if ((*rts_stringpool.sort_array_pp)->mask != arraytype)
				return false;
			if (index >= (*rts_stringpool.sort_array_pp)->count)
				return false;
			break;
		case INDR_PROTECT_ARRAY:
			if ((*indr_stringpool.protect_array_pp)->mask != arraytype)
				return false;
			if (index >= (*indr_stringpool.protect_array_pp)->count)
				return false;
			break;
		case INDR_SORT_ARRAY:
			if ((*indr_stringpool.sort_array_pp)->mask != arraytype)
				return false;
			if (index >= (*indr_stringpool.sort_array_pp)->count)
				return false;
			break;
		default:
			return false;
	}
	return true;
}

static inline boolean_t glist_good_coords(const mstr *str)
{
	uint_least64_t protected = str->in_array;

	if (!protected)
		return true;
	if (!glist_inbounds(glist_get_arraytype(protected), glist_get_index(protected)))
		return false;
	return true;
}

static inline spdesc *glist_arraytype_get_stringpool(uint_least64_t arraytype)
{
	switch (arraytype & STP_MASK)
	{
		case INDR_MASK:
			return &indr_stringpool;
			break;
		case RTS_MASK:
			return &rts_stringpool;
			break;
		default:
			return NULL;
			break;
	}
	return NULL;
}

static inline spdesc *glist_str_get_stringpool(const mstr *str)
{
	switch (glist_get_arraytype(str->in_array) & STP_MASK)
	{
		case INDR_MASK:
			return &indr_stringpool;
			break;
		case RTS_MASK:
			return &rts_stringpool;
			break;
		default:
			return NULL;
			break;
	}
	return NULL;
}

static inline mstr **glist_get_str_pp(uint_least64_t arraytype, uint_least64_t index)
{
	spdesc *spool_p;

	assert(glist_inbounds(arraytype, index));
	spool_p = glist_arraytype_get_stringpool(arraytype);
	if (!spool_p)
		return NULL;
	switch (arraytype & ARRAY_MASK)
	{
		case PROTECT_ARRAY_MASK:
			if (!(*spool_p->protect_array_pp))
				return NULL;
			if (index >= (*spool_p->protect_array_pp)->count)
				return NULL;
			assert(!glist_protect_element_in_flist((*spool_p->protect_array_pp),
							      &(*spool_p->protect_array_pp)->array[index]));
			return &(*spool_p->protect_array_pp)->array[index].str_p;
			break;
		case SORT_ARRAY_MASK:
			assert(0 == ((*spool_p->sort_array_pp)->size & ((*spool_p->sort_array_pp)->size - 1)));
			if (!(*spool_p->sort_array_pp))
				return NULL;
			if (index >= (*spool_p->sort_array_pp)->count)
				return NULL;
			return &(*spool_p->sort_array_pp)->array[REAL_SORT_INDEX(index, (*spool_p->sort_array_pp))].str_p;
			break;
		default:
			return NULL;
			break;
	}
	return NULL;
}

static inline mstr **glist_str_get_str_pp(const mstr *str)
{
	uint_least64_t protected = str->in_array;

	if (!protected)
		return NULL;
	assert(glist_good_coords(str));
	return glist_get_str_pp(glist_get_arraytype(protected), glist_get_index(protected));
}

static inline mstr_protect_array_element *glist_get_protect_array_element(uint_least64_t arraytype, uint_least64_t index)
{
	spdesc *spool_p;

	assert(glist_inbounds(arraytype, index));
	spool_p = glist_arraytype_get_stringpool(arraytype);
	if (!spool_p)
		return NULL;
	assert((arraytype & ARRAY_MASK) == PROTECT_ARRAY_MASK);
	switch ((arraytype & ARRAY_MASK))
	{
		case PROTECT_ARRAY_MASK:
			if (!(*spool_p->protect_array_pp))
				return NULL;
			if (index >= (*spool_p->protect_array_pp)->count)
				return NULL;
			assert(!glist_protect_element_in_flist((*spool_p->protect_array_pp),
							      &(*spool_p->protect_array_pp)->array[index]));
			return &(*spool_p->protect_array_pp)->array[index];
			break;
		case SORT_ARRAY_MASK:
		default:
			return NULL;
			break;
	}
	return NULL;
}

static inline mstr_sort_array_element *glist_get_sort_array_element(uint_least64_t arraytype, uint_least64_t index)
{
	spdesc *spool_p;

	assert(glist_inbounds(arraytype, index));
	spool_p = glist_arraytype_get_stringpool(arraytype);
	if (!spool_p)
		return NULL;
	assert((arraytype & ARRAY_MASK) == SORT_ARRAY_MASK);
	switch ((arraytype & ARRAY_MASK))
	{
		case PROTECT_ARRAY_MASK:
			return NULL;
		case SORT_ARRAY_MASK:
			assert(0 == ((*spool_p->sort_array_pp)->size & ((*spool_p->sort_array_pp)->size - 1)));
			if (!(*spool_p->sort_array_pp))
				return NULL;
			if (index >= (*spool_p->sort_array_pp)->count)
				return NULL;
			return &(*spool_p->sort_array_pp)->array[REAL_SORT_INDEX(index, (*spool_p->sort_array_pp))];
		default:
			return NULL;
	}
	return NULL;
}

static inline mstr_protect_array_element *glist_str_get_protect_array_element(const mstr *str)
{
	const uint_least64_t protected = str->in_array;

	if (!protected)
		return NULL;
	assert(glist_good_coords(str));
	return glist_get_protect_array_element(glist_get_arraytype(protected), glist_get_index(protected));
}

static inline mstr_sort_array_element *glist_str_get_sort_array_element(const mstr *str)
{
	const uint_least64_t protected = str->in_array;

	if (!protected)
		return NULL;
	assert(glist_good_coords(str));
	return glist_get_sort_array_element(glist_get_arraytype(protected), glist_get_index(protected));
}

static inline boolean_t glist_str_in_a_stringpool(const mstr *str, spdesc *spool)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	return (str->len && glist_ptr_span_in_range(str->addr, str->len, spool->base, spool->top));
}

static inline boolean_t glist_str_in_stringpool(const mstr *str)
{
	return (str->len && glist_ptr_span_in_range(str->addr, str->len, stringpool.base, stringpool.top));
}

static inline boolean_t glist_umstr_in_stringpool(const unmanaged_mstr *str)
{
	return (str->len && glist_ptr_span_in_range(str->addr, str->len, stringpool.base, stringpool.top));
}

static inline boolean_t glist_str_in_range(const mstr *str, const char *low_p, const char *high_p)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	return (str->len && glist_ptr_span_in_range(str->addr, str->len, low_p, high_p));
}

static inline boolean_t glist_sort_element_active(mstr_sort_array_element *sort_ele_p)
{
	if (sort_ele_p->str_p && (glist_str_get_sort_array_element(sort_ele_p->str_p) == sort_ele_p))
		return true;
	return false;
}

static inline boolean_t glist_protect_element_active(mstr_protect_array *protect_array_p, mstr_protect_array_element *protect_ele_p)
{
	assert(!protect_array_p->fcount);
	assert(!protect_ele_p->str_p
		|| (!glist_protect_element_in_flist(protect_array_p, protect_ele_p)
		    && (glist_str_get_protect_array_element(protect_ele_p->str_p) == protect_ele_p)));
	return !!protect_ele_p->str_p;
}

static inline boolean_t glist_str_in_list(const mstr *str)
{
	const uint_least64_t protected = str->in_array;
	const uint_least64_t arraytype = glist_get_arraytype(protected);
	const uint_least64_t index = glist_get_index(protected);
	spdesc *spool_p;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (!str->in_array)
		return FALSE;
	assert(glist_good_coords(str));
	switch (arraytype & (STP_MASK))
	{
		case RTS_MASK:
			spool_p = &rts_stringpool;
			break;
		case INDR_MASK:
			spool_p = &indr_stringpool;
			break;
		default:
			assert(FALSE);
			return FALSE;
	}
	switch (arraytype & (ARRAY_MASK))
	{
		case PROTECT_ARRAY_MASK:
			if (index >= (*spool_p->protect_array_pp)->count)
			{
				assert(FALSE);
				return FALSE;
			}
			if ((*spool_p->protect_array_pp)->array[index].str_p != str)
			{
				assert(FALSE);
				return FALSE;
			}
			break;
		case SORT_ARRAY_MASK:
			if (index >= (*spool_p->sort_array_pp)->count)
			{
				assert(FALSE);
				return FALSE;
			}
			if ((*spool_p->sort_array_pp)->array[REAL_SORT_INDEX(index, (*spool_p->sort_array_pp))].str_p != str)
			{
				assert(FALSE);
				return FALSE;
			}
			break;
		default:
			assert(FALSE);
			return FALSE;
	}
	return TRUE;
}

static inline boolean_t glist_str_protected(const mstr *str)
{
	if (__builtin_expect(timer_in_handler, 0))
		return !!str->in_array;
	return glist_str_in_list(str);
}

static inline boolean_t glist_mval_in_sync_nointeg(mval *v)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	/* (p iff q) == ((p -> q) && (q -> p)) == ((p && q) || (!p && !q)). non-null str mval iff protected */
	if (v->str.in_array)
		return MV_IS_STRING(v) && (v->str.addr && v->str.len);
	return !MV_IS_STRING(v) || !v->str.addr || !v->str.len;
}

static inline boolean_t glist_mval_in_sync(mval *v)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	/* (p iff q) == ((p -> q) && (q -> p)) == ((p && q) || (!p && !q)). non-null str mval iff protected */
	if (glist_str_protected(&v->str))
		return MV_IS_STRING(v) && (v->str.addr && v->str.len);
	return !MV_IS_STRING(v) || !v->str.addr || !v->str.len;
}

static inline boolean_t glist_element_changed(mstr_sort_array_element *sort_ele_p)
{
	assert(glist_sort_element_active(sort_ele_p)); /* Caller should check */
	if (sort_ele_p->char_len == CHARLEN_LVTREE)
	{
		assert((sort_ele_p->len == sort_ele_p->str_p->len)); /* Only our addr has the address, but assert no overwrite */
		return false;
	}
	return (sort_ele_p->addr != sort_ele_p->str_p->addr) || (sort_ele_p->len != sort_ele_p->str_p->len);
}

static inline void glist_fast_swap_sort_elements(mstr_sort_array_element *sort_p1, mstr_sort_array_element *sort_p2)
{
	mstr_sort_array_element temp;

	temp = *sort_p1;
	*sort_p1 = *sort_p2;
	*sort_p2 = temp;
}

static inline void glist_fast_swap_protect_elements(mstr_protect_array_element *protect_p1, mstr_protect_array_element *protect_p2)
{
	mstr_protect_array_element temp;

	temp = *protect_p1;
	*protect_p1 = *protect_p2;
	*protect_p2 = temp;
}

static inline void glist_pop_protect_array(mstr_protect_array *protect_array_p)
{
	mstr_protect_array_element *ele_p;

	assert(protect_array_p->count);
	ele_p = &protect_array_p->array[protect_array_p->count - 1];
	assert(!glist_protect_element_in_flist(protect_array_p, ele_p));
	if (ele_p->str_p)
		ele_p->str_p->in_array = FALSE;
	ele_p->str_p = NULL;
	DEBUG_GCOL_ONLY(ele_p->callerid = NULL;)
	protect_array_p->count--;
}

static inline void glist_delete_protect_array_element(mstr_protect_array *protect_array_p, unsigned long index)
{
	mstr_protect_array_element *ele_p;

	assert(protect_array_p && (index < protect_array_p->count));
	if (index >= protect_array_p->count)
	{
		assert(FALSE);
		return;
	}
	ele_p = &protect_array_p->array[index];
	if (glist_protect_element_in_flist(protect_array_p, ele_p))
	{
		assert(FALSE);
		return;
	}
	if (index == protect_array_p->count - 1)
	{
		glist_pop_protect_array(protect_array_p);
		return;
	}
	assert(ele_p->str_p);
	ele_p->str_p->in_array = FALSE;
	ele_p->str_p = NULL;
	glist_add_protect_element_to_flist(protect_array_p, ele_p);
}

static inline void glist_delete_sort_array_element(mstr_sort_array *sort_array_p, unsigned long index)
{
	mstr_sort_array_element *ele_p;

	assert(sort_array_p && (index < sort_array_p->count));
	if (index >= sort_array_p->count)
		return;
	ele_p = &sort_array_p->array[REAL_SORT_INDEX(index, sort_array_p)];
	ele_p->str_p->in_array = FALSE;
	ele_p->str_p = NULL;
	DEBUG_GCOL_ONLY(ele_p->callerid = NULL;)
	ele_p->char_len = CHARLEN_DELETED;
}

static inline void glist_new_protect_array(mstr_protect_array **protect_array_pp, size_t nmemb, unsigned int mask)
{
	mstr_protect_array *gp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(protect_array_pp);
	assert(*protect_array_pp == NULL || (*protect_array_pp)->size == (*protect_array_pp)->count);
	assert(nmemb);
	gp = (mstr_protect_array *)malloc(sizeof(mstr_protect_array) + (nmemb * sizeof(gp->array[0])));
	gp->size = nmemb;
	gp->count = 0;
	gp->mask = mask;
	gp->flist.next_ele_p = &gp->flist;
	gp->fcount = 0;
	(*protect_array_pp) = gp;

}

static inline void glist_new_sort_array(mstr_sort_array **sort_array_pp, size_t nmemb, unsigned int mask)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(sort_array_pp);
	assert(*sort_array_pp == NULL || (*sort_array_pp)->size == (*sort_array_pp)->count);
	assert(nmemb);

	assert(0 == (nmemb & (nmemb - 1)));
	mstr_sort_array *gp;
	gp = (mstr_sort_array *)malloc(sizeof(mstr_sort_array) + (nmemb * sizeof(gp->array[0])));
	gp->size = nmemb;
	gp->count = 0;
	gp->mask = mask;
	gp->start_ele = 0;
	(*sort_array_pp) = gp;
}

static inline void glist_ensure_sort_array_room(mstr_sort_array **sort_array_pp, size_t size)
{
	size_t asked, actual, seg_size, seg_size2;
	mstr_sort_array *sort_array_p = NULL, *sort_array_tofree_p;

	asked = (size + (*sort_array_pp)->count);
	if (asked <= (actual = (*sort_array_pp)->size)) /* WARNING - assignment */
		return;
	actual <<= 1;
	while (asked > actual)
		actual <<= 1;
	glist_new_sort_array(&sort_array_p, actual, (*sort_array_pp)->mask);
	sort_array_p->count = (*sort_array_pp)->count;
	sort_array_p->mask = (*sort_array_pp)->mask;
	seg_size = MIN(((*sort_array_pp)->size - (*sort_array_pp)->start_ele), (*sort_array_pp)->count);
	memcpy(sort_array_p->array, &(*sort_array_pp)->array[(*sort_array_pp)->start_ele],
	       seg_size * SIZEOF((*sort_array_pp)->array[0]));
	seg_size2 = (*sort_array_pp)->count;
	seg_size2 -= seg_size;
	if (seg_size2)
		memcpy(&sort_array_p->array[seg_size], (*sort_array_pp)->array, seg_size2 * SIZEOF((*sort_array_pp)->array[0]));
	sort_array_tofree_p = *sort_array_pp;
        COMPILER_FENCE(memory_order_release);
	*sort_array_pp = sort_array_p;
	/* We do this in this order so that any asynchronous observation that finds the old pointer does
	 * not observe just-freed memory.
	 */
	COMPILER_FENCE(memory_order_acq_rel);
	if (sort_array_tofree_p)
		free(sort_array_tofree_p);
}

/* Fast path that gets a new slot without compacting and moving the array. Used by anyone outside of stp_gcol. */
static inline mstr_sort_array_element *glist_new_sort_array_element(mstr_sort_array **sort_array_pp)
{
	mstr_sort_array  *sort_array_p, *sort_array_tofree_p;
	size_t	seg_size, seg_size2;
	unsigned int	  mask = 0;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(sort_array_pp);
	if (sort_array_pp == rts_stringpool.sort_array_pp)
	{
		mask = RTS_SORT_ARRAY;
	} else if (sort_array_pp == indr_stringpool.sort_array_pp)
	{
		mask = INDR_SORT_ARRAY;
	} else
		assert(FALSE);
	sort_array_p = *sort_array_pp;
	if (!(*sort_array_pp) || ((*sort_array_pp)->size <= (*sort_array_pp)->count))
	{
		assert(!(*sort_array_pp) || ((*sort_array_pp)->size == (*sort_array_pp)->count));
		glist_new_sort_array(&sort_array_p, (*sort_array_pp) ? 2 * (*sort_array_pp)->size : STP_ARRAY_STARTITEMS, mask);
		sort_array_tofree_p = *sort_array_pp;
		if (sort_array_tofree_p)
		{
			sort_array_p->count = sort_array_tofree_p->count;
			sort_array_p->mask = sort_array_tofree_p->mask;
			seg_size = MIN(((*sort_array_pp)->size - (*sort_array_pp)->start_ele), (*sort_array_pp)->count);
			memcpy(sort_array_p->array, &sort_array_tofree_p->array[sort_array_tofree_p->start_ele],
			       seg_size * SIZEOF((sort_array_p)->array[0]));
			seg_size2 = sort_array_tofree_p->count;
			seg_size2 -= seg_size;
			if (seg_size2)
				memcpy(&sort_array_p->array[seg_size], sort_array_tofree_p->array,
				       seg_size2 * SIZEOF(sort_array_tofree_p->array[0]));
		}
		COMPILER_FENCE(memory_order_release);
		*sort_array_pp = sort_array_p;
		/* We do this in this order so that any asynchronous observation that finds the old pointer does
		 * not observe just-freed memory.
		 */
		COMPILER_FENCE(memory_order_acq_rel);
		if (sort_array_tofree_p)
			free(sort_array_tofree_p);

	}
	(*sort_array_pp)->count++;
	return &(*sort_array_pp)->array[REAL_SORT_INDEX((*sort_array_pp)->count - 1, *sort_array_pp)];
}

static inline mstr_protect_array_element *glist_new_protect_array_element(mstr_protect_array **protect_array_pp)
{
	unsigned int mask = 0;
	mstr_protect_array *protect_array_p, *protect_array_tofree_p;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(protect_array_pp);
	if ((protect_array_pp) == rts_stringpool.protect_array_pp)
	{
		mask = RTS_PROTECT_ARRAY;
	} else if ((protect_array_pp) == indr_stringpool.protect_array_pp)
	{
		mask = INDR_PROTECT_ARRAY;
	} else if (*protect_array_pp)
	{
		assert(FALSE);
		mask = (*protect_array_pp)->mask & STP_MASK;
		if (mask != RTS_PROTECT_ARRAY && mask != INDR_PROTECT_ARRAY)
			mask = RTS_PROTECT_ARRAY;
	} else
	{
		assert(FALSE);
		mask = RTS_PROTECT_ARRAY;
	}
	protect_array_p = *protect_array_pp;
	if (protect_array_p && protect_array_p->fcount)
		return glist_get_protect_element_from_flist(protect_array_p);
	if (!protect_array_p || (protect_array_p->size <= protect_array_p->count))
	{
		assert(!protect_array_p || (protect_array_p->size == protect_array_p->count));
		glist_new_protect_array(&protect_array_p,
			(*protect_array_pp) ? 2 * (*protect_array_pp)->size : STP_ARRAY_STARTITEMS, mask);
		protect_array_tofree_p = *protect_array_pp;
		if (protect_array_tofree_p)
		{
			assert(protect_array_p->mask == protect_array_tofree_p->mask);
			protect_array_p->count = protect_array_tofree_p->count;
			memcpy(protect_array_p->array, protect_array_tofree_p->array,
			       protect_array_tofree_p->count * SIZEOF((protect_array_p)->array[0]));
		}
		COMPILER_FENCE(memory_order_release);
		*protect_array_pp = protect_array_p;
		/* We do this in this order so that any asynchronous observation that finds the old pointer does
		 * not observe just-freed memory.
		 */
		COMPILER_FENCE(memory_order_acq_rel);
		if (protect_array_tofree_p)
			free(protect_array_tofree_p);
	}
	assert(!protect_array_p->fcount);
	return &protect_array_p->array[protect_array_p->count++];
}

static inline void glist_clear_protect_flist(mstr_protect_array *protect_array_p)
{
	mstr_protect_array_element *cur_p;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	for (cur_p = protect_array_p->flist.next_ele_p;
	     protect_array_p->fcount && (cur_p != &protect_array_p->flist);
	     cur_p = protect_array_p->flist.next_ele_p)
	{
		protect_array_p->flist.next_ele_p = cur_p->next_ele_p;
		cur_p->str_p = NULL;
		protect_array_p->fcount--;
	}
	assert(!protect_array_p->fcount);
	assert(protect_array_p->flist.next_ele_p == &protect_array_p->flist);
}

static inline void glist_clear_arrays(spdesc *spool_p)
{
	size_t i;
	mstr_sort_array *sort_array_p;
	mstr_protect_array *protect_array_p;
	mstr_protect_array_element *protect_ele_p;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(spool_p->protect_array_pp == indr_stringpool.protect_array_pp);
	sort_array_p = *spool_p->sort_array_pp;
	protect_array_p = *spool_p->protect_array_pp;
	if (sort_array_p && sort_array_p->count)
	{
		for (i = 0; i < sort_array_p->count; i++)
		{
			if (glist_sort_element_active(&sort_array_p->array[REAL_SORT_INDEX(i, sort_array_p)]))
			{
				glist_delete_sort_array_element(sort_array_p, i);
			}
		}
		sort_array_p->count = 0;
		sort_array_p->start_ele = 0;
	}
	if (protect_array_p)
	{
		glist_clear_protect_flist(protect_array_p);
		while (protect_array_p->count)
			glist_pop_protect_array(protect_array_p);
	}
}

static inline void glist_add_str_to_protect_array(mstr *str, mstr_protect_array **protect_array_pp)
{
	spdesc *lcl_spool_p = NULL;
	mstr_protect_array_element *gp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (glist_str_in_a_stringpool(str, &rts_stringpool))
		lcl_spool_p = &rts_stringpool;
	else if (glist_str_in_a_stringpool(str, &indr_stringpool))
		lcl_spool_p = &indr_stringpool;
	else
		lcl_spool_p = &stringpool;
	gp = glist_new_protect_array_element(protect_array_pp);
	/* Should not already be in any list (and pointed at itself so we are sure). */
	assert(glist_good_coords(str) && !str->in_array);
	assert(protect_array_pp == lcl_spool_p->protect_array_pp);
	gp->str_p = str;
	DEBUG_GCOL_ONLY(gp->callerid = caller_id(gcol_stack_lvl););
	str->in_array = glist_get_protect_coords(*protect_array_pp, gp);
	assert(str->in_array);
}

static inline void glist_safe_delete_str(mstr *str)
{
	spdesc *spool_p;
	const uint_least64_t protected = str->in_array;
	const uint_least64_t arraytype = glist_get_arraytype(protected);
	const uint_least64_t index = glist_get_index(protected);

	assert(timer_in_handler && process_exiting);
	if (!str->in_array)
		return;
	spool_p = glist_str_get_stringpool(str);
	if (!spool_p)
	{
		str->in_array = 0;
		return;
	}
	switch (arraytype & ARRAY_MASK)
	{
		case PROTECT_ARRAY_MASK:
			if ((index < (*spool_p->protect_array_pp)->count)
				&& ((*spool_p->protect_array_pp)->array[index].str_p == str))
				glist_delete_protect_array_element((*spool_p->protect_array_pp), index);
			break;
		case SORT_ARRAY_MASK:
			if ((index < (*spool_p->sort_array_pp)->count)
				&& ((*spool_p->sort_array_pp)
				    ->array[REAL_SORT_INDEX(index, (*spool_p->sort_array_pp))].str_p == str))
				glist_delete_sort_array_element((*spool_p->sort_array_pp), index);
			break;
		default:
			str->in_array = FALSE;
			assert(FALSE);
			break;
	}
	str->in_array = FALSE;
}

static inline void glist_delete_str(mstr *str)
{
	spdesc *spool_p;
	const uint_least64_t protected = str->in_array;
	const uint_least64_t arraytype = glist_get_arraytype(protected);
	const uint_least64_t index = glist_get_index(protected);

	assert(glist_good_coords(str));
	if (!str->in_array)
		return;
	spool_p = glist_str_get_stringpool(str);
	if (!spool_p)
	{
		assert(FALSE);
		str->in_array = FALSE;
		return;
	}
	switch (arraytype & ARRAY_MASK)
	{
		case PROTECT_ARRAY_MASK:
			assert(index < (*spool_p->protect_array_pp)->count);
			assert((*spool_p->protect_array_pp)->array[index].str_p == str);
			glist_delete_protect_array_element((*spool_p->protect_array_pp), index);
			break;
		case SORT_ARRAY_MASK:
			assert(index < (*spool_p->sort_array_pp)->count);
			assert((*spool_p->sort_array_pp)->array[REAL_SORT_INDEX(index, (*spool_p->sort_array_pp))].str_p == str);
			glist_delete_sort_array_element((*spool_p->sort_array_pp), index);
			break;
		default:
			str->in_array = FALSE;
			assert(FALSE);
			break;
	}
	assert(!str->in_array);
	str->in_array = FALSE;
}

static inline void glist_replace_str(mstr *old, mstr *new)
{
	mstr **str_pp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(glist_str_protected(old));
	assert(!glist_str_protected(new));
	assert(glist_good_coords(old));
	if (old->in_array)
	{
		str_pp = glist_str_get_str_pp(old);
		assert(str_pp && *str_pp == old);
		if (str_pp && *str_pp == old)
		{
			new->in_array = old->in_array;
			old->in_array = 0;
			*str_pp = new;
#			ifdef DEBUG_GCOL
			*(char **)(str_pp + 1) = caller_id(gcol_stack_lvl);
#			endif
		}
	}
}

/* The reasoning behind these is that it is important to encapsulate the dqinit behind stable interfaces
 * in case it ever changes, and that it is important to do this initialization to maintain list invariants
 * and make stronger invariants possible to assert.
 */
static inline void glist_first_init_str(mstr *str)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	str->len = 0;
	str->char_len = 0;
	str->addr = NULL;
	str->in_array = FALSE;
}

static inline void glist_protect_str(mstr *mstr)
{
	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (!glist_str_protected(mstr))
		glist_add_str_to_protect_array(mstr, stringpool.protect_array_pp);
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
}

static inline void glist_unprotect_str(mstr *mstr)
{
	if (!mstr->in_array)
		return;
	if (__builtin_expect(process_exiting && timer_in_handler, 0))
	{
		glist_safe_delete_str(mstr);
	} else
	{
		assert(glist_str_protected(mstr) || !mstr->in_array);
		glist_delete_str(mstr);
		assert(!glist_str_protected(mstr));
	}
}

static inline int glist_sync_temp_move(mval *v, const char *l, const char *r)
{
	int res;

	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (MV_IS_STRING(v) && v->str.len
		&& (glist_ptr_span_in_range(v->str.addr, v->str.len, stringpool.base, stringpool.top)
			|| glist_ptr_span_in_range(v->str.addr, v->str.len, l, r)))
	{
		glist_protect_str(&v->str);
		res = 1;
	} else
	{
		res = 0;
		glist_unprotect_str(&v->str);
	}
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
	return res;
}

static inline int glist_sync_temp(mval *v)
{
	int res;

	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (MV_IS_STRING(v) && glist_str_in_stringpool(&v->str))
	{
		glist_protect_str(&v->str);
		res = 1;
	} else
	{
		res = 0;
		glist_unprotect_str(&v->str);
	}
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
	return res;
}

/* The following function is for 'actively-managed' mstrs. It will protect them if they are in the stringpool
 * and unprotect them otherwise. Important to only use when modification of mstr is well-defined and cannot be triggered
 * by implicit paths (i.e. through compiled code).
 */
static inline void glist_sync_static_str(mstr *str)
{
	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (glist_str_in_stringpool(str))
		glist_protect_str(str);
	else
		glist_unprotect_str(str);
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
}

static inline boolean_t glist_static_str_in_sync(mstr *str)
{
	if (glist_str_protected(str))
		return glist_str_in_stringpool(str);
	return !glist_str_in_stringpool(str);
}

static inline void glist_sync_static_str_move(mstr *str, const char *l, const char *r)
{
	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (str->len && glist_ptr_span_in_range(str->addr, str->len, l, r))
		glist_protect_str(str);
	else
		assert(glist_static_str_in_sync(str));
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
}

static inline void glist_sync_str(mstr *str)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	if (str->addr && str->len)
		glist_protect_str(str);
	else
		glist_unprotect_str(str);
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
}

static inline boolean_t glist_str_in_sync(mstr *str)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (glist_str_protected(str))
		return str->addr && str->len;
	return !str->addr || !str->len;
}

/* The following function is for 'actively-managed' mvals which shall be protected if they are non-null string types.
 * This is an equivalently-safe alternative to glist_sync_str, with the same pitfalls. It protects literalpool-pointed mvals and
 * makes them visible to the stp_move which may need to correct them on an unlink, provided that it is used under the
 * same constrints as sync_str ( that is, it should not be used for mvals (ie lv_val values) which can change
 * arbitrarily). This may be preferred to sync_str when an mval is available since the check is cheaper and mvtype
 * is a gating criterion in STPG_MVAL_PUT
 */

static inline void glist_sync_mval(mval *v)
{
	DEBUG_GCOL_ONLY(gcol_stack_lvl++;)
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (MV_IS_STRING(v))
	{
		glist_sync_str(&v->str);
	} else
		glist_unprotect_str(&v->str);
	DEBUG_GCOL_ONLY(gcol_stack_lvl--;)
}

/* Use the following function after calling unprotect_str on strings which are going to be emitted or otherwise should have
 * zeroed-out "gcol-related" parts.
 */
static inline boolean_t glist_str_null(mstr *str)
{
	return !str->in_array;
}

static inline void glist_init_str(mstr *str)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(!glist_str_protected(str));
	glist_first_init_str(str);
}

/* Use the below function when a protected str (str1) is copied to and 'consumed' by
 * str2 and can take its place in the protection list.
 */
static inline void glist_transfer_protection_to_from(mstr *tostr, mstr *fromstr)
{
	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	assert(glist_str_protected(fromstr));
	assert(!glist_str_protected(tostr));
	assert(*glist_str_get_str_pp(fromstr) != tostr); /* Weird potential corruption but assert against it */
	assert(tostr->addr == fromstr->addr);
	assert(fromstr->len == tostr->len);
	glist_replace_str(fromstr, tostr);
	assert(glist_str_protected(tostr));
	assert(!glist_str_protected(fromstr));
}

/* Ensures that either a string knows it is not in an array or knows that it is in an array.
 * Knowledge is justified true belief. glist_good_coords == TRUE is justification. agreement of the array-resident
 * pointer with this assessment makes it true or not. Returns whether the string had knowledge. Side effect of clearing
 * up any misconceptions.
 */
static inline boolean_t glist_str_make_integ(mstr *str)
{
	mstr **str_pp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (!str->in_array)
		return TRUE;
	if (!glist_good_coords(str))
	{
		str->in_array = FALSE;
		return FALSE;
	}
	if (!(str_pp = glist_str_get_str_pp(str)) || (*str_pp != str)) /* WARNING - assignment */
	{
		str->in_array = FALSE;
		return FALSE;
	}
	return TRUE;
}

static inline boolean_t glist_str_check_integ(mstr *str)
{
	mstr **str_pp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (!str->in_array)
		return TRUE;
	if (!glist_good_coords(str))
	{
		return FALSE;
	}
	if (!(str_pp = glist_str_get_str_pp(str)) || (*str_pp != str)) /* WARNING - assignment */
	{
		return FALSE;
	}
	return TRUE;
}

static inline void glist_str_before_move(mstr *str)
{
	boolean_t had_integ;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (!str->in_array)
		return;
	/* Below assert should fail or the move didn't happen */
	had_integ = glist_str_make_integ(str);
	assert(had_integ); /* This operation is conservative in pro, but if it needs to do anything that means
			      * some invariant broke elsewhere, so find and fix.
			      */
}

/* Essentially fixes integrity from the array side. Must be used with care, only on strings which had
 * glist_str_before_move() run on them in advance before the move.
 */
static inline void glist_str_after_move(mstr *str)
{
	mstr **str_pp;

	assert(!timer_in_handler || WBTEST_ENABLED(WBTEST_ABUSE_TIMERS));
	if (!str->in_array)
		return;
	/* Below assert should fail or the move didn't happen */
	assert(!glist_str_check_integ(str));
	str_pp = glist_str_get_str_pp(str);
	assert(str_pp);
	*str_pp = str;
#	ifdef DEBUG_GCOL
	*(char **)(str_pp + 1) = caller_id(gcol_stack_lvl);
#	endif
	assert(glist_str_check_integ(str));
}

static inline void glist_fixup_sort_list()
{
	size_t sort_ele_top, sort_ele_i;
	mstr_sort_array_element *sort_ele_base_p, *sort_ele_i_p;
	uint_least64_t stp_mask;

	if (stringpool.pending_moves)
	{
		sort_ele_base_p = &(*stringpool.sort_array_pp)->array[(*stringpool.sort_array_pp)->start_ele];
		stp_mask = (stringpool.sort_array_pp != rts_stringpool.sort_array_pp) ? INDR_MASK : RTS_MASK;
		sort_ele_top = (*stringpool.sort_array_pp)->count;
		for (sort_ele_i = 0, sort_ele_i_p = sort_ele_base_p;
			sort_ele_i < sort_ele_top;
			sort_ele_i++,
			sort_ele_i_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(sort_ele_i, *stringpool.sort_array_pp)])
		{
			assert(sort_ele_i_p->str_p && sort_ele_i_p->str_p->in_array);
			if (sort_ele_i_p->str_p)
				SET_COORD_INDEX_AND_ARRAYTYPE(
					sort_ele_i_p->str_p->in_array, sort_ele_i, stp_mask | SORT_ARRAY_MASK);
		}
		stringpool.pending_moves = FALSE;
	}
}
#endif
