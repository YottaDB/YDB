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

#include "gtm_string.h"

#include "patcode.h"
#include "copy.h"
#include "min_max.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"	/* needed by *TYPEMASK* macros defined in gtm_utf8.h */
#include "gtm_utf8.h"
#endif

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
GBLREF	boolean_t	gtm_utf8_mode;

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
	int4		bytelen, charlen, length, pbytelen, strbytelen;
	boolean_t	success, attempt, pvalid, strvalid;
	int		atom, unit, idx, index, hasfixed;
	uint4		z_diff, *rpt, *rtop, rept;
	uint4		repeat[MAX_PATTERN_ATOMS];
	uint4		*patidx[MAX_PATTERN_ATOMS];
	unsigned char	*stridx[MAX_PATTERN_ATOMS];
	unsigned char	*strptr, *strtop, *strnext, *pstr, *ptop, *pnext;
	uint4		code, tempuint;
	uint4		*dfa_ptr, dfa_val;
	uint4		*patptr;
	uint4		mbit, flags;
	int4		*min, *max, *size;
	int4		mintmp, maxtmp, sizetmp;
	int		alt, bit;
	char		buf[CHAR_CLASSES];
	boolean_t	pte_csh_init;
	boolean_t	match;
	UNICODE_ONLY(
	wint_t		utf8_codepoint;
	)

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
	strtop = stridx[0] + str->str.len;
	GET_ULONG(tempuint, patptr);
	patptr += tempuint;
	GET_LONG(count, patptr);
	patptr++;
	GET_LONG(total_min, patptr);
	patptr++;
	GET_LONG(total_max, patptr);
	patptr++;
	/* "length" actually denotes character length; Get it from the appropriate field in the mstr */
	if (!gtm_utf8_mode)
		length = str->str.len;
	UNICODE_ONLY(
	else
	{
		MV_FORCE_LEN(str); /* to set str.char_len if not already done; also issues BADCHAR error if appropriate */
		length = str->str.char_len;
	}
	)
	if (length < total_min || length > total_max)
		return FALSE;
	min = (int4 *)patptr;
	patptr += count;
	max = (int4 *)patptr;
	patptr += count;
	if (MIN_SPLIT_N_MATCH_COUNT <= count)
	{
		hasfixed = FALSE;
		for (index = 0; index < count; index++)
		{
			GET_LONG(maxtmp, max + index);
			GET_LONG(mintmp, min + index);
			if (maxtmp == mintmp)
			{
				hasfixed = TRUE;
				break;
			}
		}
		if (hasfixed && (DO_PATSPLIT_FAIL != (match = do_patsplit(str, pat))))
			return match;
	}
	size = (int4 *)patptr;
	memcpy(repeat, min, count * SIZEOF(*min));
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
				rept = *rpt;
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
					if (rept)
					{
						if (FALSE == pte_csh_init)
						{
							PTE_CSH_INCR_CURALT_DEPTH(curalt_depth);
							pte_csh_init = TRUE;
						}
						if (do_patalt(patptr, strptr, strtop, alt_rep_min, alt_rep_max, rept, 1,
													min_incr, max_incr))
						{
							if (!gtm_utf8_mode)
								strptr += rept;
							UNICODE_ONLY(
							else
							{
								for (unit = 0; unit < rept; unit++)
								{
									assert(strptr < strtop); /* below macro relies on this */
									strptr = UTF8_MBNEXT(strptr, strtop);
								}
							}
							)
						} else
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
						bytelen = 0;
						for (bit = 0; bit < PAT_MAX_BITS; bit++)
						{
							mbit = (1 << bit);
							if ((mbit & code & PATM_LONGFLAGS) && !(mbit & pat_allmaskbits))
								buf[bytelen++] = codelist[patmaskseq(mbit)];
						}
						rts_error(VARLSTCNT(4) ERR_PATNOTFOUND, 2, bytelen, buf);
					}
					if (!gtm_utf8_mode)
					{
						for (unit = 0; unit < rept; unit++)
						{
							if (!(code & pattern_typemask[*strptr++]))
								goto CALC;
						}
					}
					UNICODE_ONLY(
					else
					{
						for (unit = 0; unit < rept; unit++)
						{
							assert(strptr < strtop);	/* PATTERN_TYPEMASK macro relies on this */
							if (!(code & PATTERN_TYPEMASK(strptr, strtop, strnext, utf8_codepoint)))
								goto CALC;
							strptr = strnext;
						}
					}
					)
				} else if (code == PATM_DFA)
				{	/* Discrete Finite Automaton pat atom */
					GET_LONG(bytelen, patptr);
					patptr++;
					dfa_ptr = patptr;
					for (unit = 0; unit < rept; )
					{
						GET_ULONG(dfa_val, dfa_ptr);
						if (!(dfa_val & PATM_STRLIT))
						{
							if (!gtm_utf8_mode)
							{
								success = (dfa_val & pattern_typemask[*strptr]);
								strnext = strptr + 1;
							}
							UNICODE_ONLY(
							else
							{
								success = (dfa_val &
									PATTERN_TYPEMASK(strptr, strtop, strnext, utf8_codepoint));
							}
							)
						} else
						{
							dfa_ptr++;
							GET_ULONG(dfa_val, dfa_ptr);
							/* Only ASCII characters are currently allowed for DFA STRLITs.
							 * Assert that below.
							 */
							assert(IS_ASCII(dfa_val));
							if (!gtm_utf8_mode)
							{
								success = (dfa_val == *strptr);
								strnext = strptr + 1;
							}
							UNICODE_ONLY(
							else
							{
								UTF8_VALID(strptr, strtop, strbytelen);
								success = ((1 == strbytelen) && (dfa_val == *strptr));
								strnext = strptr + strbytelen;
							}
							)
						}
						dfa_ptr++;
						if (success)
						{
							GET_ULONG(dfa_val, dfa_ptr);
							dfa_ptr = patptr + dfa_val;
							strptr = strnext;
							unit++;
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
					if (unit < rept)
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
					patptr += bytelen;
				} else
				{	/* STRLIT pat atom */
					assert(3 == PAT_STRLIT_PADDING);
					GET_LONG(bytelen, patptr);	/* get bytelen */
					patptr++;
					GET_LONG(charlen, patptr);	/* get charlen */
					patptr++;
					GET_ULONG(flags, patptr);	/* get flags */
					patptr++;
					if (bytelen == 1)
					{
						if (!gtm_utf8_mode)
						{
							for (unit = 0; unit < rept; unit++)
							{
								if (*(unsigned char *)patptr != *strptr++)
									goto CALC;
							}
						}
						UNICODE_ONLY(
						else
						{
							for (unit = 0; unit < rept; unit++)
							{
								if ((1 != (UTF8_VALID(strptr, strtop, strbytelen), strbytelen))
										|| (*(unsigned char *)patptr != *strptr++))
									goto CALC;
							}
						}
						)
						patptr++;
					} else if (bytelen > 0)
					{
						if (!gtm_utf8_mode)
						{
							ptop = (unsigned char *)patptr + bytelen;
							for (unit = 0; unit < rept; unit++)
							{
								pstr = (unsigned char *)patptr;
								while (pstr < ptop)
								{
									if (*pstr++ != *strptr++)
										goto CALC;
								}
							}
						}
						UNICODE_ONLY(
						else
						{
							pstr = (unsigned char *)patptr;
							ptop = pstr + bytelen;
							for (unit = 0; unit < rept; unit++)
							{
								pstr = (unsigned char *)patptr;
								for ( ; pstr < ptop; )
								{
									pvalid = UTF8_VALID(pstr, ptop, pbytelen);
										/* sets pbytelen */
									assert(pvalid);
									strvalid = UTF8_VALID(strptr, strtop, strbytelen);
										/* sets strbytelen */
									if (pbytelen != strbytelen)
										goto CALC;
									DEBUG_ONLY(strnext = strptr + pbytelen);
									pnext = pstr + pbytelen;
									do
									{
										if (*pstr++ != *strptr++)
											goto CALC;
									} while (pstr < pnext);
									assert(strptr == strnext);
								}
							}
						}
						)
						patptr += DIVIDE_ROUND_UP(bytelen, SIZEOF(*patptr));
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
