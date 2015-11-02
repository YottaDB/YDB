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

#include "mdef.h"

#include "gtm_string.h"		/* for memset */

#include "copy.h"
#include "patcode.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

/* see corresponding GBLDEFs in gbldefs.c for comments on the caching mechanism */
GBLREF	int4		curalt_depth;					/* depth of alternation nesting */
GBLREF	int4		do_patalt_calls[PTE_MAX_CURALT_DEPTH];		/* number of calls to do_patalt() */
GBLREF	int4		do_patalt_hits[PTE_MAX_CURALT_DEPTH];		/* number of pte_csh hits in do_patalt() */
GBLREF	int4		do_patalt_maxed_out[PTE_MAX_CURALT_DEPTH];	/* no. of pte_csh misses after maxing on allocation size */
GBLREF	pte_csh		*pte_csh_array[PTE_MAX_CURALT_DEPTH];		/* pte_csh array (per curalt_depth) */
GBLREF	int4		pte_csh_cur_size[PTE_MAX_CURALT_DEPTH];		/* current pte_csh size (per curalt_depth) */
GBLREF	int4		pte_csh_alloc_size[PTE_MAX_CURALT_DEPTH];	/* current allocated pte_csh size (per curalt_depth) */
GBLREF	int4		pte_csh_entries_per_len[PTE_MAX_CURALT_DEPTH];	/* current number of entries per len */
GBLREF	int4		pte_csh_tail_count[PTE_MAX_CURALT_DEPTH];	/* count of non 1-1 corresponding pte_csh_array members */
GBLREF	pte_csh		*cur_pte_csh_array;			/* copy of pte_csh_array corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_size;			/* copy of pte_csh_cur_size corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_entries_per_len;		/* copy of pte_csh_entries_per_len corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_tail_count;			/* copy of pte_csh_tail_count corresponding to curalt_depth */
GBLREF	boolean_t	gtm_utf8_mode;

/* Example compiled pattern for an alternation pattern
 *	Pattern = P0_P1
 *	----------------
 *	P0 = 1.3(.N,2"-",.2A)
 *	P1 = 1" "
 *
 *	Compiled Pattern
 *	-----------------
 *	0x00000000	<-- fixed (1 if fixed length, 0 if not fixed length)
 *	0x00000027	<-- length of pattern stream (inclusive of itself)
 *
 *	0x02000000	P0	<-- pattern_mask[0] => alternation
 *	0x00000001	P0	<-- alt_rep_min[0]
 *	0x00000003	P0	<-- alt_rep_max[0]
 *	0x00000009	P0	<-- length of alternation pattern's choice[0] pattern (exclusive of itself)
 *	0x00000000	P0		<-- fixed
 *	0x00000002	P0		<-- length of pattern stream (inclusive of itself)
 *	0x40000001	P0		<-- pattern_mask[0] => DFABIT | PATM_N
 *	0x00000001	P0		<-- count
 *	0x00000000	P0		<-- tot_min
 *	0x00007fff	P0		<-- tot_max
 *	0x00000000	P0		<-- min[0]
 *	0x00007fff	P0		<-- max[0]
 *	0x00000001	P0		<-- size[0]
 *	0x0000000a	P0	<-- length of alternation pattern's choice[1] pattern (exclusive of itself)
 *	0x00000001	P0		<-- fixed
 *	0x00000004	P0		<-- length of pattern stream (inclusive of itself)
 *	0x00000082	P0		<-- pattern_mask[0] = PATM_STR | PATM_P
 *	0x00000001	P0		<-- length of PATM_STR (exclusive of itself)
 *	0x0000002d	P0		<-- PATM_STR[0] = '-'
 *	0x00000001	P0		<-- count
 *	0x00000002	P0		<-- tot_min
 *	0x00000002	P0		<-- tot_max
 *	0x00000002	P0		<-- min[0]	// Note for fixed length, max[] array is absent //
 *	0x00000001	P0		<-- size[0]
 *	0x00000000	P0	<-- End of alternation pattern's choices ('\0')
 *
 *	0x00000082	P1	<-- pattern_mask[1] => PATM_STR | PATM_P (' ')
 *	0x00000001	P1	<-- length of PATM_STR (exclusive of itself)
 *	0x00000020	P1	<-- PATM_STR[0] = ' '
 *
 *	0x00000002	<-- count
 *	0x00000001	<-- total_min
 *	0x00007fff	<-- total_max
 *	0x00000000	<-- min[0]	<-- Begin of min[2] array
 *	0x00000001	<-- min[1]
 *	0x00007fff	<-- max[0]	<-- Begin of max[2] array
 *	0x00000001	<-- max[1]
 *	0x00000001	<-- size[0]	<-- Begin of size[2] array
 *	0x00000001	<-- size[1]
 */

/* returns index in cur_pte_csh_array that holds the desired <patstr, strptr, charlen, repcnt> tuple..
 * return PTE_NOT_FOUND otherwise.
 */
static	int	pte_csh_present(char *patptr, char *strptr, int4 charlen, int repcnt)
{
	int4		index;
	pte_csh		*tmp_pte, *pte_top;

	assert(PTE_MAX_CURALT_DEPTH > curalt_depth);
	index = ((PTE_STRLEN_CUTOFF > charlen) ? charlen : PTE_STRLEN_CUTOFF) * cur_pte_csh_entries_per_len;
	assert(cur_pte_csh_size > index);
	tmp_pte = cur_pte_csh_array + index;
	pte_top = tmp_pte + ((PTE_STRLEN_CUTOFF > charlen) ? cur_pte_csh_entries_per_len : cur_pte_csh_tail_count);
	assert(pte_top <= (cur_pte_csh_array + cur_pte_csh_size));
	for (; tmp_pte < pte_top; tmp_pte++)
	{
		if ((tmp_pte->strptr != strptr) || (tmp_pte->patptr != patptr)
				|| (tmp_pte->charlen != charlen) || (tmp_pte->repcnt != repcnt))
		{
			if (NULL != tmp_pte->strptr)
				continue;
			else
				break;	/* the first NULL value means all further entries for this "charlen" are NULL */
		}
		tmp_pte->count++;
		return (int)tmp_pte->match;
	}
	return (int)PTE_NOT_FOUND;
}

static	void	pte_csh_insert(char *patptr, char *strptr, int4 charlen, int repcnt, boolean_t match)
{
	int4		index;
	pte_csh		*tmp_pte, *pte_top, *min_pte, *free_pte;

	assert(PTE_MAX_CURALT_DEPTH > curalt_depth);
	assert(PTE_NOT_FOUND == pte_csh_present(patptr, strptr, charlen, repcnt));
	index = ((PTE_STRLEN_CUTOFF > charlen) ? charlen : PTE_STRLEN_CUTOFF) * cur_pte_csh_entries_per_len;
	assert(cur_pte_csh_size > index);
	tmp_pte = cur_pte_csh_array + index;
	pte_top = tmp_pte + ((PTE_STRLEN_CUTOFF > charlen) ? cur_pte_csh_entries_per_len : cur_pte_csh_tail_count);
	assert(pte_top <= (cur_pte_csh_array + cur_pte_csh_size));
	min_pte = tmp_pte;
	free_pte = NULL;

	for (; tmp_pte < pte_top; tmp_pte++)
	{
		if (NULL == tmp_pte->patptr)
		{
			min_pte = free_pte = tmp_pte;
			break;
		} else if (min_pte->count > tmp_pte->count)
			min_pte = tmp_pte;
	}
	if (NULL == free_pte)
	{
		for (tmp_pte = cur_pte_csh_array + index; tmp_pte < pte_top; tmp_pte++)
			tmp_pte->count = 1;	/* reset count whenever new entry is made thereby causing history refresh.
						 * i.e. permitting formerly busy but currently inactive patterns to be reused
						 */
	}
	min_pte->count = 0;	/* give little priority to the rest by setting count to 1 less than the others */
	min_pte->patptr = patptr;
	min_pte->strptr = strptr;
	min_pte->charlen = charlen;
	min_pte->repcnt = repcnt;
	min_pte->match = match;
}

int do_patalt(uint4 *firstalt, unsigned char *strptr, unsigned char *strtop, int4 repmin, int4 repmax, int totchar, int repcnt,
											int4 min_incr, int4 max_incr)
{
	boolean_t	fixed;
	int4		alt_tot_min, alt_tot_max, new_pte_csh_size, tmp_do_patalt_calls;
	uint4		*cur_alt, tempuint;
	uint4		*patptr;
	int		match, alt_size, charlen, bytelen, pat_found;
	mval		alt_pat, alt_str;
	pte_csh		*tmp_pte;
	unsigned char	*strtmp, *strnext;

	if (PTE_MAX_CURALT_DEPTH > curalt_depth)
	{	/* try to find it in the current pattern evaluation cache (cur_pte_csh_array) itself */
		tmp_do_patalt_calls = ++do_patalt_calls[curalt_depth];
		pat_found = pte_csh_present((char *)firstalt, (char *)strptr, totchar, repcnt);
		if (PTE_NOT_FOUND != pat_found)
		{
			do_patalt_hits[curalt_depth]++;
			return pat_found;
		} else if ((tmp_do_patalt_calls > cur_pte_csh_size)
			&& ((tmp_do_patalt_calls - do_patalt_hits[curalt_depth]) > (tmp_do_patalt_calls / PTE_CSH_MISS_FACTOR)))
		{	/* lots of cache miss happening. try to increase pt_csh_array size */
			do_patalt_hits[curalt_depth] = do_patalt_calls[curalt_depth] = 1;
			new_pte_csh_size = cur_pte_csh_size;
			if (cur_pte_csh_size < pte_csh_alloc_size[curalt_depth])
			{
				new_pte_csh_size = (cur_pte_csh_size << 1);
				assert(cur_pte_csh_size <= pte_csh_alloc_size[curalt_depth]);
			} else if (PTE_MAX_ENTRIES > pte_csh_alloc_size[curalt_depth])
			{
				new_pte_csh_size = (cur_pte_csh_size << 1);
				tmp_pte = malloc(SIZEOF(pte_csh) * new_pte_csh_size);
				free(cur_pte_csh_array);
				pte_csh_alloc_size[curalt_depth] = new_pte_csh_size;
				pte_csh_array[curalt_depth] = tmp_pte;
				cur_pte_csh_array = pte_csh_array[curalt_depth];
			} else
				do_patalt_maxed_out[curalt_depth]++;
			if (new_pte_csh_size != cur_pte_csh_size)
			{
				memset(pte_csh_array[curalt_depth], 0, SIZEOF(pte_csh) * new_pte_csh_size);
				pte_csh_cur_size[curalt_depth] *= 2;
				pte_csh_entries_per_len[curalt_depth] *= 2;
				pte_csh_tail_count[curalt_depth] *= 2;
				UPDATE_CUR_PTE_CSH_MINUS_ARRAY(cur_pte_csh_size,
								cur_pte_csh_entries_per_len, cur_pte_csh_tail_count);
			}
		}
	}
	alt_pat.mvtype = MV_STR;
	alt_str.mvtype = MV_STR;
	alt_str.str.addr = (char *)strptr;
	patptr = firstalt;
	GET_LONG(alt_size, patptr);
	patptr++;
	for (match = FALSE; !match && alt_size; patptr++)
	{
		cur_alt = patptr;
		cur_alt++;
		GET_ULONG(tempuint, cur_alt);
		cur_alt++;
		cur_alt += tempuint;
		GET_LONG(alt_tot_min, cur_alt);
		cur_alt++;
		if (alt_tot_min <= totchar)
		{
			GET_LONG(tempuint, cur_alt);
			GET_LONG(fixed, patptr);
			/* Note that some patterns whose minimum and maximum length are the same need not have
			 * "fixed" field 1. This is because alternations which have choices that all evaluate
			 * to the same length (e.g. 5(2l,2e,"ab")) are currently not recognizable by do_patfixed
			 * and hence go through do_pattern.
			 */
			assert(!fixed || (alt_tot_min == tempuint));
			alt_tot_max = (tempuint < totchar) ? tempuint : totchar;
			alt_pat.str.addr = (char *)patptr;
			alt_pat.str.len = alt_size * SIZEOF(uint4);
			/* Note that the below zero min length avoiding code is actually an optimization.
			 * This is because if we start from length 0, we will end up matching the input string and in case
			 * 	the alternation pattern's max count is huge (e.g. PAT_MAX) we will end up recursing
			 * 	in do_patalt() as many times each time matching a length of 0, without realizing we are
			 * 	not progressing anywhere in the match by matching a huge number of empty strings.
			 * This will effectively cause a combinatorial explosion to occur in case there are at least 2 choices
			 * 	in the alternation pattern (which usually will be the case) since the choices that need to be
			 * 	examined are 2 ** PAT_MAX.
			 * Instead, if we start from length 1, every level of recursion we decrease the size of the problem
			 * 	by matching a non-zero length of the input string and hence we can't progress much in the
			 * 	recursion levels before starting to backtrack, thereby avoiding the explosion.
			 * Note that we do have to consider zero length in case we haven't yet exhausted our minimum count of
			 * 	the alternation pattern and we have a null input string remaining to be matched.
			 * Hence the if check below.
			 */
			if (totchar && (0 == alt_tot_min))
				alt_tot_min = 1;	/* avoid zero min length when non-zero string still needs to be matched */
			if (!gtm_utf8_mode)
			{	/* each character is 1 byte so charlen and bytelen is same */
				charlen = alt_tot_min;
				bytelen = alt_tot_min;
			}
			UNICODE_ONLY(
			else
			{	/* skip alt_tot_min characters */
				strtmp = strptr;
				for (charlen = 0; charlen < alt_tot_min; charlen++)
				{
					assert(strtmp < strtop);
					strtmp = UTF8_MBNEXT(strtmp, strtop);
				}
				bytelen = (int)(strtmp - strptr);
			}
			)
			UNICODE_ONLY(
				if (gtm_utf8_mode)
					alt_str.mvtype |= MV_UTF_LEN;	/* avoid recomputing "char_len" in do_pattern/do_patfixed */
			)
			for ( ; !match && (charlen <= alt_tot_max); charlen++)
			{
				alt_str.str.len = bytelen;
				UNICODE_ONLY(
					if (gtm_utf8_mode)
					{
						assert(utf8_len(&alt_str.str) == charlen);
						alt_str.str.char_len = charlen;	/* set "char_len" */
					}
				)
				match = charlen ? (fixed ? do_patfixed(&alt_str, &alt_pat)
							: do_pattern(&alt_str, &alt_pat))
						: TRUE;
				/* max_incr and min_incr aid us in an earlier backtracking optimization.
				 * for example, let us consider "abcdefghijklmnopqrstuvwxyz"?.13(1l,1e,1n,1u,1p,2l)
				 * say the first do_patalt() call matches a substring (the beginning of the input string) "a"
				 *	with the first alternation choice 1l
				 * say the recursive second do_patalt() call then matches a substring of the now beginning
				 *	input string "b" with the first alternation choice 1l again
				 * the recursively called third do_patalt() now can rest assured that the remaining string
				 *	can't be matched by the alternation. This is because it has only 11 chances left
				 *	(note the maximum is .13) and each time the maximum length it can match is 2 (the
				 *	maximum length of all the alternation choices which is 2l) which leaves it with a
				 *	maximum of 22 characters while there are still 24 characters left in the input-string.
				 * this optimization can cause a backtracking to occur at the 3rd level of call to do_patalt()
				 *	instead of going through the call trace 13 times and then determining at the leaf level.
				 * since at each level, the choices examined are 6, we are saving nearly (6 to the power of 11)
				 *	choice examinations (11 for the levels that we avoid with the optimization)
				 */
				if (match && ((charlen < totchar) || (repcnt < repmin)))
					match &= ((repcnt < repmax)
							&& ((totchar - charlen) <= (repmax - repcnt) * max_incr)
							&& ((totchar - charlen) >= (repmin - repcnt) * min_incr))
						? do_patalt(firstalt, &strptr[bytelen], strtop, repmin, repmax,
									totchar - charlen, repcnt + 1, min_incr, max_incr)
						: FALSE;
				if (!match)
				{	/* update "bytelen" to correspond to "charlen + 1" */
					if (!gtm_utf8_mode)
						bytelen++;
					UNICODE_ONLY(
					else
					{
						assert((strtmp < strtop) || (charlen == alt_tot_max));
						if (strtmp < strtop)
						{
							strnext = UTF8_MBNEXT(strtmp, strtop);
							assert(strnext > strtmp);
							bytelen += (int)(strnext - strtmp);
							strtmp = strnext;
						}
					}
					)
				}
			}
		}
		patptr += alt_size;
		GET_LONG(alt_size, patptr);
	}
	if (PTE_MAX_CURALT_DEPTH > curalt_depth)
		pte_csh_insert((char *)firstalt, (char *)strptr, totchar, repcnt, match);
	return match;
}

