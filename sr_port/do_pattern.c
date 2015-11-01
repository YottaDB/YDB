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

GBLREF	uint4		pat_allmaskbits;
GBLREF	uint4		*pattern_typemask;
GBLREF	char		codelist[];
GBLREF	int4		curalt_depth;					/* depth of alternation nesting */
GBLREF	int4		do_patalt_calls[PTE_MAX_CURALT_DEPTH];		/* number of calls to do_patalt() */
GBLREF	int4		do_patalt_hits[PTE_MAX_CURALT_DEPTH];		/* number of pte_csh hits in do_patalt() */
GBLREF	pte_csh		*pte_csh_array[PTE_MAX_CURALT_DEPTH];		/* pte_csh array (per curalt_depth) */
GBLREF	int4		pte_csh_cur_size[PTE_MAX_CURALT_DEPTH];		/* current pte_csh size (per curalt_depth) */
GBLREF	int4		pte_csh_alloc_size[PTE_MAX_CURALT_DEPTH];	/* current allocated pte_csh size (per curalt_depth) */
GBLREF	int4		pte_csh_entries_per_len[PTE_MAX_CURALT_DEPTH];	/* current number of entries per len */
GBLREF	int4		pte_csh_tail_count[PTE_MAX_CURALT_DEPTH];	/* count of non 1-1 corresponding pte_csh_array members */
GBLREF	pte_csh		*cur_pte_csh_array;			/* copy of pte_csh_array corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_size;			/* copy of pte_csh_cur_size corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_entries_per_len;		/* copy of pte_csh_entries_per_len corresponding to curalt_depth */
GBLREF	int4		cur_pte_csh_tail_count;			/* copy of pte_csh_tail_count corresponding to curalt_depth */

/* This procedure executes at "run-time". After a pattern in a MUMPS program has been compiled (by patstr and
 * 	its helper-procedures), this procedure can be called to evaluate "variable-length" patterns.
 * Variable-length patterns are of the kind 3.5N2.A.5N i.e. for at least one pattern atom,
 * 	the lower-bound is different from the upper-bound.
 * For patterns with a fixed length, procedure do_patfixed() will be called to do the evaluation.
 * Variable length input patterns will be scanned at runtime to see if they have at least one fixed length pattern atom.
 * If yes, routine do_patsplit() will be invoked to determine the fixed length sub pattern atom that is closest to the
 * 	median of the pattern atoms. The input pattern would then be split into three, left, fixed and right.
 * The fixed pattern can be matched by a linear scan in the input string. Once that is done, the left and right
 * 	pattern atom positions are pivoted relative to the input string and do_pattern() is invoked on each of them recursively.
 * If no fixed length pattern atoms can be found, then do_pattern() calculates all possible permutations (it has certain
 * 	optimizations to prune the combinatorial search tree) and tries to see for each permutation if a match occurs.
 */

int do_pattern(mval *str, mval *pat)
{
	int4		count, total_min, total_max;
	int4		alt_rep_min, alt_rep_max, min_incr, max_incr;
	int4		len, length;
	unsigned char	*ptop;
	boolean_t	success, attempt;
	int		atom, unit, idx, index, fixed_count;
	uint4		z_diff, *rpt, *rtop;
	uint4		repeat[MAX_PATTERN_ATOMS];
	uint4		*patidx[MAX_PATTERN_ATOMS];
	unsigned char	*stridx[MAX_PATTERN_ATOMS];
	unsigned char	*strptr, *pstr, *strbot, *strtop;
	uint4		code, tempuint;
	uint4		*dfa_ptr, dfa_val;
	uint4		*patptr;
	uint4		mbit;
	int4		*min, *max, *size;
	int4		mintmp, maxtmp, sizetmp;
	int		alt, bit;
	char		buf[CHAR_CLASSES];
	boolean_t	pte_csh_init;
	boolean_t	match;

	error_def(ERR_PATNOTFOUND);

	/* set up information */
	MV_FORCE_STR(str);
	patptr = (uint4 *) pat->str.addr;
	GET_ULONG(tempuint, patptr);
	if (tempuint)
	{	/* tempuint non-zero implies fixed length pattern string. this in turn implies we are not called from op_pattern.s
		 * but instead called from op_fnzsearch(), gvzwr_fini(), gvzwr_var(), lvzwr_fini(), lvzwr_var() etc.
		 * in this case, call do_patfixed() as the code below and code in do_patsplit() assumes we are dealing with a
		 * variable length pattern string. changing all the callers to call do_patfixed() directly instead of this extra
		 * redirection was considered, but not felt worth it since the call to do_pattern() is not easily macroizable
		 * due to the expression-like usage in those places.
		 */
		return do_patfixed(str, pat);
	}
	patptr++;
	patidx[0] = patptr + 1;
	stridx[0] = (unsigned char *)str->str.addr;
	GET_ULONG(tempuint, patptr);
	patptr += tempuint;
	GET_LONG(count, patptr);
	patptr++;
	GET_LONG(total_min, patptr);
	patptr++;
	GET_LONG(total_max, patptr);
	patptr++;
	length = str->str.len;
	if (length < total_min || length > total_max)
		return FALSE;

	min = (int4 *)patptr;
	patptr += count;
	max = (int4 *)patptr;
	patptr += count;
	if (MIN_SPLIT_N_MATCH_COUNT <= count)
	{
		fixed_count = 0;	/* this is an approximate indicator of the number of patterns processable by do_patfixed
					 * approximate because alternations of fixed length cannot be processed by do_patfixed */
		for (index = 0; index < count; index++)
		{
			GET_LONG(maxtmp, max + index);
			GET_LONG(mintmp, min + index);
			fixed_count += ((maxtmp == mintmp) ? TRUE : FALSE);
		}
		if (fixed_count && (DO_PATSPLIT_FAIL != (match = do_patsplit(str, pat))))
			return match;
	}
	size = (int4 *)patptr;
	memcpy(repeat, min, count * sizeof(*min));
	rtop = &repeat[0] + count;
	count--;
	attempt = FALSE;
	idx = 0;
	pte_csh_init = FALSE;
	/* proceed to check string */
	for (;;)
	{
		if (total_min == length)
		{	/* attempt a match */
			attempt = TRUE;
			strptr = stridx[idx];
			patptr = patidx[idx];
			rpt = &repeat[idx];
			for (; rpt < rtop; rpt++)
			{
				GET_ULONG(code, patptr);
				patptr++;
				if (code & PATM_ALT)
				{	/* pattern alternation */
					GET_LONG(alt_rep_min, patptr);
					patptr++;
					GET_LONG(alt_rep_max, patptr);
					patptr++;
					GET_LONG(maxtmp, max + idx);
					GET_LONG(mintmp, min + idx);
					assert(alt_rep_min || !mintmp);
					assert(alt_rep_max || !maxtmp);
					min_incr = mintmp ? mintmp / alt_rep_min : 0;
					max_incr = maxtmp ? maxtmp / alt_rep_max : 0;
					if (*rpt)
					{
						if (FALSE == pte_csh_init)
						{
							PTE_CSH_INCR_CURALT_DEPTH(curalt_depth);
							pte_csh_init = TRUE;
						}
						if (do_patalt(patptr, strptr, alt_rep_min, alt_rep_max, *rpt, 1,
													min_incr, max_incr))
							strptr += *rpt;
						else
							goto CALC;
					}
					/* make sure that patptr points to the next patcode after the alternation */
					GET_LONG(alt, patptr);
					patptr++;
					while(alt)
					{
						patptr += alt;
						GET_LONG(alt, patptr);
						patptr++;
					}
				} else if (!(code & PATM_STRLIT))
				{	/* meta character pat atom */
					if (!(code & pat_allmaskbits))
					{	/* current table has no characters with this pattern code */
						len = 0;
						for (bit = 0; bit < 32; bit++)
						{
							mbit = (1 << bit);
							if ((mbit & code & PATM_LONGFLAGS) && !(mbit & pat_allmaskbits))
								buf[len++] = codelist[patmaskseq(mbit)];
						}
						rts_error(VARLSTCNT(4) ERR_PATNOTFOUND, 2, len, buf);
					}
					for (unit = 0; unit < *rpt; unit++)
					{
						if (!(code & pattern_typemask[*strptr++]))
							goto CALC;
					}
				} else if (code == PATM_DFA)
				{	/* Discrete Finite Automaton pat atom */
					GET_LONG(len, patptr);
					patptr++;
					dfa_ptr = patptr;
					strbot = strtop = strptr;
					strtop += *rpt;
					while (strptr < strtop)
					{
						GET_ULONG(dfa_val, dfa_ptr);
						if (!(dfa_val & PATM_STRLIT))
						{
							success = (dfa_val & pattern_typemask[*strptr]);
							dfa_ptr++;
						} else
						{
							dfa_ptr++;
							GET_ULONG(dfa_val, dfa_ptr);
							dfa_ptr++;
							success = (dfa_val == *strptr);
						}
						if (success)
						{
							GET_ULONG(dfa_val, dfa_ptr);
							dfa_ptr = patptr + dfa_val;
							strptr++;
							GET_ULONG(dfa_val, dfa_ptr);
							if (dfa_val == PATM_ACS)
								break;
						} else
						{
							dfa_ptr++;
							GET_ULONG(dfa_val, dfa_ptr);
							if ((dfa_val & PATM_DFA) == PATM_DFA)
								break;
						}
					}
					if (strptr < strtop)
						goto CALC;
					else
 					{
						GET_ULONG(dfa_val, dfa_ptr);
						while (dfa_val < PATM_DFA)
						{
							if (dfa_val & PATM_STRLIT)
								dfa_ptr += 3;
							else
								dfa_ptr += 2;
							GET_ULONG(dfa_val, dfa_ptr);
						}
						if (dfa_val != PATM_ACS)
							goto CALC;
					}
					patptr += len;
				} else
				{	/* STRLIT pat atom */
					GET_LONG(len, patptr);
					patptr++;
					if (len == 1)
					{
						for (unit = 0; unit < *rpt; unit++)
						{
							if (*(unsigned char *)patptr != *strptr++)
								goto CALC;
						}
						patptr++;
					} else if (len > 0)
					{
						ptop = (unsigned char *)patptr + len;
						for (unit = 0; unit < *rpt; unit++)
						{
							pstr = (unsigned char *)patptr;
							while (pstr < ptop)
							{
								if (*pstr++ != *strptr++)
									goto CALC;
							}
						}
						patptr += DIVIDE_ROUND_UP(len, sizeof(*patptr));
					}
				}
				idx++;
				stridx[idx] = strptr;
				patidx[idx] = patptr;
			}
			if (pte_csh_init)
			{	/* surrounded by braces since the following is a multi-line macro */
				PTE_CSH_DECR_CURALT_DEPTH(curalt_depth);
			}
			return TRUE;
		} else
		{	/* calculate permutations */
			attempt = FALSE;
			GET_LONG(maxtmp, max + count);
			GET_LONG(mintmp, min + count);
			GET_LONG(sizetmp, size + count);
			if (repeat[count] < maxtmp)
			{
				atom = unit = length - total_min;
				z_diff = maxtmp - mintmp;
				if (sizetmp > 1)
				{
					unit /= sizetmp;
					atom = unit * sizetmp;
					z_diff *= sizetmp;
				}
				if (atom > 0)
				{
					total_min += MIN(atom, z_diff);
					repeat[count] = MIN(repeat[count] + unit, maxtmp);
					if (total_min == length)
						continue;
				}
			}
		}

CALC:		unit = count;
		GET_LONG(sizetmp, size + unit);
		for ( ; ; )
		{
			GET_LONG(mintmp, min + unit);
			total_min -= (repeat[unit] - mintmp) * sizetmp;
			repeat[unit] = mintmp;
			unit--;
			if (unit < 0)
			{
				if (pte_csh_init)
				{	/* surrounded by braces since the following is a multi-line macro */
					PTE_CSH_DECR_CURALT_DEPTH(curalt_depth);
				}
				return FALSE;
			}
			GET_LONG(maxtmp, max + unit);
			GET_LONG(sizetmp, size + unit);
			if (repeat[unit] < maxtmp)
			{
				total_min += sizetmp;
				repeat[unit]++;
				if (total_min <= length)
				{
					if (unit <= idx)
					{
						idx = unit;
						break;
					}
					if (!attempt)
						break;
				}
			}
		}
	}
}
