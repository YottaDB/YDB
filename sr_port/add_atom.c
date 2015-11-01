/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "copy.h"
#include "patcode.h"
#include "min_max.h"

/* This function is part of the MUMPS compiler. It adds one pattern atom to the string of compiled pattern atoms.
 * If the atom to be added can be "compressed" with the previous one, this function will allow compress() to do so.
 */
boolean_t add_atom(short int	*count,
	uint4			pattern_mask,
	void			*strlit_buff,
	int4			strlen,
	boolean_t		infinite,
	short int		*min,
	short int		*max,
	short int		*size,
	short int		*total_min,
	short int		*total_max,
	int			lower_bound,
	int			upper_bound,
	int			altmin,
	int			altmax,
	boolean_t		*last_infinite_ptr,
	uint4			**fstchar_ptr,
	uint4			**outchar_ptr,
	uint4			**lastpatptr_ptr)
{
	uint4			*patmaskptr;

	if ((pattern_mask & PATM_STRLIT) && !strlen && *count)
	{	/* A special case is a pattern like xxx?1N5.7""2A . Since there is an infinite number of empty strings between
		 * any two characters in a string, a pattern atom that counts repetitions of the fixed string "" can be ignored.
		 * That is, such an atom can only be ignored if it is not the only one in the pattern...
		 */
		return TRUE;
	}
	if (*count && !*(size - 1))
	{	/* If the previous atom was an n.m"", it should be removed. In such a case, the last two values
		 * in the 'outchar' array are PATM_STRLIT (pattern mask) and 0 (stringlength). */
		*outchar_ptr -= 2;
		(*count)--;
		assert(0 == *count);
		min--;
		max--;
		size--;
	}
	if (pattern_mask & PATM_ALT)
	{
		lower_bound = lower_bound * altmin;
		upper_bound = upper_bound * altmax;
	}
	if (*count && pat_compress(pattern_mask, strlit_buff, strlen, infinite, *last_infinite_ptr, *lastpatptr_ptr))
	{
		min--;
		max--;
		size--;
		*min = MIN(*min + lower_bound, PAT_MAX_REPEAT);
		*max = MIN(*max + upper_bound, PAT_MAX_REPEAT);
	} else
	{
		*min = MIN(lower_bound, PAT_MAX_REPEAT);
		*max = MIN(upper_bound, PAT_MAX_REPEAT);
		*lastpatptr_ptr = patmaskptr = *outchar_ptr;
		*last_infinite_ptr = infinite;
		(*outchar_ptr)++;
		if (*outchar_ptr - *fstchar_ptr > MAX_PATTERN_LENGTH)
			return FALSE;
		if ((pattern_mask & PATM_ALT) || !(pattern_mask & PATM_STRLIT))
		{
			*patmaskptr++ = pattern_mask;
			*size = 1;
		} else
		{
			*outchar_ptr += DIVIDE_ROUND_UP(strlen, sizeof(uint4)) + 1;
			if (*outchar_ptr - *fstchar_ptr > MAX_PATTERN_LENGTH)
				return FALSE;
			*patmaskptr++ = pattern_mask;
			memcpy(patmaskptr, strlit_buff, strlen + sizeof(uint4));
			*size = strlen;
		}
		(*count)++;
	}
	*total_min = MIN(*total_min + (*size * lower_bound), PAT_MAX_REPEAT);
	*total_max = MIN(*total_max + (*size * upper_bound), PAT_MAX_REPEAT);
	return TRUE;
}
