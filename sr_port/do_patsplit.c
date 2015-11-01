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

#include "patcode.h"
#include "copy.h"
#include "min_max.h"
#include "gtm_string.h"

/* This routine tries to split the pattern-string P into substrings LFR where F is a fixed length pattern-atom of P
 * 	and L and R are the pattern substrings in P on the left and right side of F.
 * For pattern-strings that have more than one fixed-length pattern-atom, the one closest to the median is selected.
 * Once F is determined, do_patfixed() is invoked to determine all possible matches of F with the input string.
 * do_patsplit() has additional optimizations to try match the fixed length pattern in a restricted subset of the
 * 	input string taking into account the min and max of the left and the right pattern.
 * For each such match, an attempt is made to match L and R with the left and right side of the input string.
 *	This is done using do_patfixed() or do_pattern() appropriately.
 * If any such attempt succeeds, we return TRUE.
 * If no attempt succeeds, we return FALSE.
 * If the pattern-string P doesn't contain any fixed-length pattern-atom, we return DO_PATSPLIT_FAIL
 */

int do_patsplit(mval *str, mval *pat)
{
	int4		count, total_min, total_max;
	int4		min[MAX_PATTERN_ATOMS], max[MAX_PATTERN_ATOMS], size[MAX_PATTERN_ATOMS];
	int4		len, length, fixed_len;
	int4		alt_rep_min, alt_rep_max;
	int4		alt;
	uint4		tempuint;
	uint4		code;
	uint4		*patptr, *patptr_start, *patptr_end, *fixed_patptr, *right_patptr, *tmp_patptr;
	ptstr		left_ptstr, right_ptstr, fixed_ptstr;
	mval		left_pat, right_pat, fixed_pat, left_str, right_str, fixed_str;
	int4		index, fixed_index;	/* index of our current fixed-length pattern-atom  */
	boolean_t	right;		/* 0 indicates we are processing left side, 1 indicates right side */
	boolean_t	fixed[2];	/* fixed[0] is for the left, fixed[1] is for the right */
	int4		tot_min[2], tot_max[2], cnt[2];	/* index 0 is for left, index 1 is for right */
	int4		offset;
	char		*strptr, *strtop;
	boolean_t	match;		/* match status of input pattern with input string */

	MV_FORCE_STR(str);
	patptr = (uint4 *)pat->str.addr;
	DEBUG_ONLY(
		GET_ULONG(tempuint, patptr);
		assert(!tempuint);
	)
	patptr++;
	patptr_start = patptr + 1;
	GET_ULONG(tempuint, patptr);
	patptr += tempuint;
	patptr_end = patptr;
	GET_LONG(count, patptr);
	patptr++;
	GET_LONG(total_min, patptr);
	patptr++;
	GET_LONG(total_max, patptr);
	patptr++;
	length = str->str.len;
	assert(length >= total_min && length <= total_max);
	assert(count <= MAX_PATTERN_ATOMS);
	memcpy(&min[0], patptr, sizeof(*patptr) * count);
	patptr += count;
	memcpy(&max[0], patptr, sizeof(*patptr) * count);
	patptr += count;
	memcpy(&size[0], patptr, sizeof(*patptr) * count);
	patptr = patptr_start;
	right = FALSE;	/* start with left side */
	fixed[right] = TRUE;
	tot_min[right] = tot_max[right] = 0;
	fixed_patptr = right_patptr = NULL;
	fixed_index = -1;

	for (index = 0; index < count; index++)
	{
		GET_ULONG(code, patptr);
		tmp_patptr = patptr;
		patptr++;
		if (code & PATM_ALT)
		{	/*  skip to the next pattern-atom */
			GET_LONG(alt_rep_min, patptr);
			patptr++;
			GET_LONG(alt_rep_max, patptr);
			patptr++;
			GET_LONG(alt, patptr);
			patptr++;
			while(alt)
			{
				patptr += alt;
				GET_LONG(alt, patptr);
				patptr++;
			}
			fixed[right] = FALSE;
			/* patptr now points to the next patcode after the alternation */
		} else if (code == PATM_DFA)
		{	/* Discrete Finite Automaton pat atom */
			assert(min[index] != max[index]);	/* DFA should never be fixed length */
			GET_LONG(len, patptr);
			patptr++;
			patptr += len;
			fixed[right] = FALSE;
		} else
		{
			if (code & PATM_STRLIT)
			{	/* STRLIT pat atom */
				GET_LONG(len, patptr);
				patptr++;
				patptr += DIVIDE_ROUND_UP(len, sizeof(*patptr));
			}
			if ((min[index] == max[index]) && (min[index] * size[index]))
			{	/* fixed_length */
				if (ABS(index - (count / 2)) < ABS(fixed_index - (count / 2)))
				{	/* non-zero fixed length pattern with a fixed_index closer to the median of the array */
					if (right)
					{	/* update left's tot_min and tot_max to reflect the new fixed_index */
						tot_min[0] = MIN(PAT_MAX_REPEAT,
							tot_min[0] + tot_min[right] + (min[fixed_index] * size[fixed_index]));
						tot_max[0] = MIN(PAT_MAX_REPEAT,
							tot_max[0] + tot_max[right] + (max[fixed_index] * size[fixed_index]));
						fixed[0] &= fixed[right];
					}
					fixed_index = index;
					right = TRUE;
					fixed[right] = TRUE;
					tot_min[right] = tot_max[right] = 0;
					fixed_patptr = tmp_patptr;
					right_patptr = patptr;
					continue;
				}
			} else
				fixed[right] = FALSE;
		}
		tot_min[right] = MIN(PAT_MAX_REPEAT, tot_min[right] + (min[index] * size[index]));
		tot_max[right] = MIN(PAT_MAX_REPEAT, tot_max[right] + (max[index] * size[index]));
	}
	assert(index == count);
	if (-1 == fixed_index)
		return DO_PATSPLIT_FAIL;
	assert(fixed_index < count);
	assert((total_min == (tot_min[0] + tot_min[1] + min[fixed_index] * size[fixed_index])) || (PAT_MAX_REPEAT == total_min));
	assert((total_max == (tot_max[0] + tot_max[1] + max[fixed_index] * size[fixed_index])) || (PAT_MAX_REPEAT == total_max));

	cnt[0] = fixed_index;
	if (cnt[0])
	{	/* left section has at least one pattern atom. create its compilation string */
		patptr = left_ptstr.buff;
		*patptr++ = fixed[0];
		*patptr++ = fixed_patptr - patptr_start + 1;
		memcpy(patptr, patptr_start, (char *)fixed_patptr - (char *)patptr_start);
		patptr += fixed_patptr - patptr_start;
		*patptr++ = cnt[0];
		*patptr++ = tot_min[0];
		*patptr++ = tot_max[0];
		for (index = 0; index < cnt[0]; index++)
			*patptr++ = min[index];
		if (!fixed[0])
		{
			for (index = 0; index < cnt[0]; index++)
				*patptr++ = max[index];
		}
		for (index = 0; index < cnt[0]; index++)
			*patptr++ = size[index];
		left_pat.mvtype = MV_STR;
		left_pat.str.len = (char *)patptr - (char *)&left_ptstr.buff[0];
		left_pat.str.addr = (char *)&left_ptstr.buff[0];
	}

	/* create fixed length pattern atom's compilation string */
	patptr = fixed_ptstr.buff;
	*patptr++ = TRUE;	/* fixed length pattern */
	*patptr++ = right_patptr - fixed_patptr + 1;
	memcpy(patptr, fixed_patptr, (char *)right_patptr - (char *)fixed_patptr);
	patptr += right_patptr - fixed_patptr;
	*patptr++ = 1;						/* count */
	fixed_len = min[fixed_index] * size[fixed_index];	/* tot_min and tot_max */
	*patptr++ = fixed_len;
	*patptr++ = fixed_len;
	*patptr++ = min[fixed_index];				/* min[0] */
	*patptr++ = size[fixed_index];				/* size[0] */
	fixed_pat.mvtype = MV_STR;
	fixed_pat.str.len = (char *)patptr - (char *)&fixed_ptstr.buff[0];
	fixed_pat.str.addr = (char *)&fixed_ptstr.buff[0];

	cnt[1] = count - fixed_index - 1;
	if (cnt[1])
	{	/* right section has at least one pattern atom. create its compilation string */
		patptr = right_ptstr.buff;
		*patptr++ = fixed[1];
		*patptr++ = patptr_end - right_patptr + 1;
		memcpy(patptr, right_patptr, (char *)patptr_end - (char *)right_patptr);
		patptr += patptr_end - right_patptr;
		*patptr++ = cnt[1];
		*patptr++ = tot_min[1];
		*patptr++ = tot_max[1];
		for (index = fixed_index + 1; index < count; index++)
			*patptr++ = min[index];
		if (!fixed[1])
		{
			for (index = fixed_index + 1; index < count; index++)
				*patptr++ = max[index];
		}
		for (index = fixed_index + 1; index < count; index++)
			*patptr++ = size[index];
		right_pat.mvtype = MV_STR;
		right_pat.str.len = (char *)patptr - (char *)&right_ptstr.buff[0];
		right_pat.str.addr = (char *)&right_ptstr.buff[0];
	}
	left_str.mvtype = MV_STR;
	left_str.str.addr = str->str.addr;
	fixed_str.mvtype = MV_STR;
	fixed_str.str.len = fixed_len;
	right_str.mvtype = MV_STR;

	if (str->str.len > (tot_min[0] + tot_max[1] + fixed_len))
		strptr = str->str.addr + str->str.len - tot_max[1] - fixed_len;
	else
		strptr = str->str.addr + tot_min[0];
	if (str->str.len > (tot_min[1] + tot_max[0] + fixed_len))
		strtop = str->str.addr + tot_max[0];
	else
		strtop = str->str.addr + str->str.len - tot_min[1] - fixed_len;
	/* Try to match the fixed pattern string and for each match, try matching the left and right input strings */
	for (match = FALSE; !match && (strptr <= strtop); strptr++)
	{
		fixed_str.str.addr = strptr;
		if (!do_patfixed(&fixed_str, &fixed_pat))
			continue;
		assert(cnt[0] || cnt[1]); /* fixed_pat takes only one pattern atom and non-zero rest are in cnt[0] and cnt[1] */
		if (cnt[0])
		{
			left_str.str.len = strptr - str->str.addr;
			match = fixed[0] ? do_patfixed(&left_str, &left_pat) : do_pattern(&left_str, &left_pat);
			if (!match)
				continue;
		}
		if (cnt[1])
		{
			right_str.str.addr = strptr + fixed_len;
			right_str.str.len = str->str.addr + str->str.len - right_str.str.addr;
			match = (fixed[1] ? do_patfixed(&right_str, &right_pat) : do_pattern(&right_str, &right_pat));
		}
	}
	return match;
}
