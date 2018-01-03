/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "patcode.h"
#include "toktyp.h"
#include "copy.h"
#include "min_max.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"	/* needed for UTF8_MBNEXT macro */
#endif

GBLREF	uint4		mapbit[];
GBLREF	boolean_t	gtm_utf8_mode;

LITREF	char		ctypetab[NUM_CHARS];
LITREF	uint4		typemask[PATENTS];

error_def(ERR_COMMAORRPAREXP);
error_def(ERR_PATCLASS);
error_def(ERR_PATCODE);
error_def(ERR_PATLIT);
error_def(ERR_PATMAXLEN);
error_def(ERR_PATUPPERLIM);

typedef struct
{
	unsigned char	*next;
	ptstr		altpat;
} alternation;

#define	TERMINATE_DFA(PATMASKPTR, OUTCHAR, DFA_FIXED_LEN, LV_PTR, LEAF_NUM, EXP_PTR, FSTCHAR,			\
			TOPCHAR, MIN, MAX, SIZE, TOTAL_MIN, TOTAL_MAX, COUNT, LASTPATPTR, LAST_INFINITE,	\
			MIN_DFA, ALTMIN, ALTMAX, INSTR, INCHAR, DFA)						\
{														\
	gtm_uint64_t		bound;										\
	int			cursize, i;									\
														\
	PATMASKPTR = OUTCHAR;											\
	/* If this is a fixed pattern, generate code to invoke do_patfixed as that takes up less space in	\
	 * the object code and has same runtime performance as the DFA. Else go through DFA.			\
	 */													\
	if (!DFA_FIXED_LEN)											\
	{													\
		assert(OUTCHAR <= TOPCHAR);									\
		cursize = dfa_calc(LV_PTR, LEAF_NUM, EXP_PTR, &FSTCHAR, &OUTCHAR);				\
	} else													\
		cursize = -1;											\
	if (cursize >= 0)											\
	{													\
		assert(OUTCHAR <= TOPCHAR);									\
		MIN[COUNT] = MIN_DFA;										\
		MAX[COUNT] = PAT_MAX_REPEAT;									\
		SIZE[COUNT] = cursize;										\
		TOTAL_MIN += BOUND_MULTIPLY(MIN[COUNT], SIZE[COUNT], bound);					\
		if (TOTAL_MIN > PAT_MAX_REPEAT)									\
			TOTAL_MIN = PAT_MAX_REPEAT;								\
		TOTAL_MAX += BOUND_MULTIPLY(MAX[COUNT], SIZE[COUNT], bound);					\
		if (TOTAL_MAX > PAT_MAX_REPEAT)									\
			TOTAL_MAX = PAT_MAX_REPEAT;								\
		LASTPATPTR = PATMASKPTR;									\
		LAST_INFINITE = TRUE;										\
		COUNT++;											\
	} else													\
	{													\
		OUTCHAR = PATMASKPTR;										\
		if (!pat_unwind(&COUNT, LV_PTR, LEAF_NUM, &TOTAL_MIN, &TOTAL_MAX,				\
			&MIN[0], &MAX[0], &SIZE[0], ALTMIN, ALTMAX,						\
			&LAST_INFINITE, &FSTCHAR, &OUTCHAR, &LASTPATPTR))					\
		{												\
			INSTR->addr = (char *)INCHAR;								\
			return ERR_PATMAXLEN;									\
		}												\
		assert(OUTCHAR <= TOPCHAR);									\
	}													\
	DFA = FALSE;												\
}

/*  This procedure is part of the MUMPS compiler.  The function of this procedure is to parse a pattern specification
 *  and compile it into a data structure that will be used by the run-time engine to actually attempt to match the pattern.
 *
 *  The data structure that is built in 'obj' to describe the pattern is stored in units of [unsigned int4]s.
 *  However it is treated as an mstr with a character count by all but the specifically pattern match modules.
 *
 *  The contents of the table looks like:
 *
 *  [0]		flag: non-zero if the pattern is "fixed" (3n1"abc"5a is "fixed", 3.5n is not)
 *  [1]		counter: amount of space used for pattern masks and string-buffers (in units of cell-size)
 *  [...]	space for additional buffers. If the pattern contains strings, the text for those strings are stored in this space.
 *			If the pattern contains "alternations", the specifications of these are stored in this area.
 *			Each "alternation" specifier is a (recursive) instance of this table.
 *  [n]		counter: number of pattern specifications
 *  [n+1]	total number of characters in specified patterns
 *  [n+2]	maximum number of characters in specified patterns
 *  [n+3]	min[0]: first element of array containing the minimum numbers for the repeat-counts
 *	...
 *  [n+3+count-1]
 *
 *    *** only if pattern is not "fixed" ***
 *               [m] = [n+3+count]
 *  [m]          max[0]: first element of array containing the maximum numbers for the repeat-counts
 *	...
 *  [m+count-1]
 *
 *    *** always ***
 *               [p] = [n+3+count+count] or [n+3+count]
 *  [p]          size[0]: first element of array containing sizes
 *	...
 *  [p+count-1]
 *
 *======================================================================
 *
 * Pattern specifications are compiled by this procedure (patstr).
 * Run-time evaluation occurs through one of three possible evaluators:
 *   1. For "fixed" patterns: do_patfixed ("Fixed" patterns look like 3a2n1p)
 *   2. Other patterns go through do_pattern (These are patterns like 1.3a2.5n1.6p)
 *   3. A special case are patterns that have more than 1 pattern code with an indeterminate upper bound
 *      Those patterns are processed using the DFA algorithm.
 */

int patstr(mstr *instr, ptstr *obj, unsigned char **relay)
{
	pat_strlit		strlit;
	boolean_t		dfa, done, infinite;
	boolean_t		dfa_fixed_len, fixed_len, prev_fixed_len, split_atom, start_dfa;
	int4			lower_bound, upper_bound, alloclen;
	gtm_uint64_t		bound;
	unsigned char		curchar, symbol, *inchar, *in_top, *buffptr;
	uint4			pattern_mask, last_leaf_mask, y_max, mbit;
	uint4			*patmaskptr;
	int			atom_map, count, total_min, total_max;
	int			min[MAX_PATTERN_ATOMS], max[MAX_PATTERN_ATOMS], size[MAX_PATTERN_ATOMS];
	struct leaf		leaves, *lv_ptr;
	struct e_table		expand, *exp_ptr;
	int			exp_temp[CHAR_CLASSES];
	int			leaf_num, curr_leaf_num, min_dfa, curr_min_dfa, sym_num;
	int 			seqcnt, charpos, leafcnt, cursize;
	int4			bitpos;
	alternation		init_alt;
	alternation		*cur_alt;
	mstr			alttail;
	int4			status;
	int4			altactive = 0;
	int4			altend;
	char			*saveinstr;
	int			chidx;
	int			bit;
	int			seq;
	int			altmin, altmax;
	int			saw_delimiter = 0;
	int4			altlen, bytelen;
	int4			allmask;
	boolean_t		last_infinite;
	boolean_t		done_free;
	unsigned char		*let_go;
	uint4			*fstchar, *lastpatptr, *outchar, *topchar;
	int			any_alt = FALSE;
	int			altcount, altsimplify;
	int			low_in, high_in, size_in, jump;
	boolean_t		topseen = FALSE;/* If TRUE it means we found inchar to be == in_top and so did NOT scan the NEXT
						 * byte in inchar (to be stored in curchar). Therefore from this point onwards,
						 * "curchar" should never be used in this function. This is also asserted below.
						 */

	if (0 == instr->len)		/* empty pattern string. Cant do much */
	{
		instr->addr++;	/* Return 1 byte more for compile_pattern to properly compute erroring M source column */
		return ERR_PATCODE;
	}
	memset(&leaves, 0, SIZEOF(leaves));
	memset(&expand, 0, SIZEOF(expand));
	init_alt.next = NULL;
	init_alt.altpat.len = 0;
	done_free = TRUE;
	fstchar = &obj->buff[0];
	saveinstr = (char *) &instr->addr[0];
	for (allmask = 0, chidx = 'A'; chidx <= 'X'; chidx++)
		allmask |= mapbit[chidx - 'A'];
	outchar = &obj->buff[PAT_MASK_BEGIN_OFFSET]; /* Note: offset is actually PAT_MASK_BEGIN_OFFSET * SIZEOF (uint4) bytes */
	topchar = &obj->buff[MAX_PATOBJ_LENGTH];
	last_leaf_mask = *outchar = 0;
	patmaskptr = lastpatptr = outchar;
	infinite = last_infinite = FALSE;
	dfa = split_atom = FALSE;
	fixed_len = TRUE;
	count = total_min = total_max = atom_map = 0;
	lv_ptr = &leaves;
	exp_ptr = &expand;
	inchar = (unsigned char *)instr->addr;
	in_top = (unsigned char *)&inchar[instr->len];
	assert(inchar < in_top);
	curchar = *inchar++;
	altactive = 0;
	for (;;)
	{
		assert(inchar <= in_top);
		altend = 0;
		prev_fixed_len = fixed_len;
		if ((NULL != relay) && !saw_delimiter)
		{
			if (topseen)
			{
				instr->addr = (char *)(in_top + 1);
				return ERR_COMMAORRPAREXP;
			}
			if ((',' == curchar) || (')' == curchar))
			{
				*relay = (inchar - 1);
				altend = 1;
			}
		}
		saw_delimiter = 0;
		if (!altactive || altend)
		{	/* Use DFA for patterns by default (fast at runtime even if time/space consuming at compile time).
			 * If at the end of the parse we find that the input pattern is of fixed length we will decide
			 * to compile it so (invoke do_patfixed instead of do_pattern which in turn invokes the DFA).
			 */
			start_dfa = TRUE;
			instr->addr = (char*)inchar;
			if (!topseen && (TK_PERIOD == ctypetab[curchar]))
			{
				lower_bound = 0;
				fixed_len = FALSE;
			} else if (!topseen && (TK_DIGIT == ctypetab[curchar]))
			{
				lower_bound = curchar - '0';
				for ( ; ; )
				{
					if (inchar >= in_top)
					{
						assert(inchar == in_top);
						topseen = TRUE;
						break;
					}
					curchar = *inchar++;
					if (TK_DIGIT != ctypetab[curchar])
						break;
					if (PAT_MAX_REPEAT < (lower_bound = (lower_bound * 10) + (curchar - '0')))
						lower_bound = PAT_MAX_REPEAT;
				}
				infinite = FALSE;
			} else
			{
				if (dfa)
				{	/* Note: Below macro can do a "return ERR_PATMAXLEN" if there is not enough space */
					TERMINATE_DFA(patmaskptr, outchar, dfa_fixed_len, lv_ptr, leaf_num, exp_ptr, fstchar,
						topchar, min, max, size, total_min, total_max, count, lastpatptr, last_infinite,
						min_dfa, altmin, altmax, instr, inchar, dfa);
				}
				if (outchar == &obj->buff[PAT_MASK_BEGIN_OFFSET])
				{
					instr->addr = (char *)inchar;
					return ERR_PATCODE;
				}
				patmaskptr = &obj->buff[0];
				*patmaskptr++ = fixed_len;
				assert(outchar <= topchar);
				if ((outchar + 3 + ((fixed_len ? 2 : 3) * count)) > topchar)
				{
					instr->addr = (char *)inchar;
					return ERR_PATMAXLEN;
				}
				*patmaskptr = (uint4)(outchar - patmaskptr); /* unit is SIZEOF(uint4) */
				*outchar++ = count;
				*outchar++ = total_min;
				*outchar++ = total_max;
				for (seqcnt = 0; seqcnt < count; seqcnt++)
					*outchar++ = min[seqcnt];
				if (!fixed_len)
					for (seqcnt = 0; seqcnt < count; seqcnt++)
						*outchar++ = max[seqcnt];
				for (seqcnt = 0; seqcnt < count; seqcnt++)
					*outchar++ = size[seqcnt];
				assert(outchar <= topchar);
				obj->len = (int4)(outchar - &obj->buff[0]);
				assert(!topseen || (inchar == in_top));
				assert(inchar <= in_top);
				instr->addr = (topseen ? (char *)inchar : (char *)inchar - 1);
				return 0;
			}
			if (!topseen && (curchar != '.'))
				upper_bound = lower_bound;
			else
			{
				fixed_len = FALSE;
				if (inchar >= in_top)
				{
					assert(inchar == in_top);
					instr->addr = (char *)inchar + 1;
					return ERR_PATCLASS;
				}
				assert(!topseen);
				if (ctypetab[curchar = *inchar++] != TK_DIGIT)
				{
					if (lower_bound > 0)
					{	/* A pattern atom like 5.A will be split into two atoms:
						 * 	 (i) the first will be a fixed length one (5A);
						 * 	(ii) the second one will be a completely indefinite one (.A).
						 * This split allows the run-time engine to separate out the
						 * 	fixed part from the indefinite part.
						 */
						split_atom = TRUE;
						if ((count >= (MAX_PATTERN_ATOMS - 1)) ||
								(atom_map >= (MAX_PATTERN_ATOMS -2)))
			                         	return ERR_PATMAXLEN;

					} else
					{
						infinite = TRUE;
						upper_bound = PAT_MAX_REPEAT;
					}
				} else
				{
					infinite = FALSE;
					instr->addr = (char *)inchar;
					upper_bound = curchar - '0';
					for ( ; ; )
					{
						if (inchar >= in_top)
						{
							assert(inchar == in_top);
							topseen = TRUE;
							break;
						}
						curchar = *inchar++;
						if (TK_DIGIT != ctypetab[curchar])
							break;
						if (PAT_MAX_REPEAT < (upper_bound = (upper_bound * 10) + (curchar - '0')))
							upper_bound = PAT_MAX_REPEAT;
					}
					if (upper_bound < lower_bound)
					{
						instr->addr = (char *)inchar;
						return ERR_PATUPPERLIM;
					}
				}
			}
			instr->addr = (char *)inchar;
			if (count >= MAX_PATTERN_ATOMS)
				return ERR_PATMAXLEN;
		}
		if (!altend)
		{
			if (!topseen && ('\"' == curchar))
			{
				pattern_mask = PATM_STRLIT;
				strlit.bytelen= 0;
				strlit.charlen= 0;
				strlit.flags = 0;
				alloclen = (SIZEOF(strlit.buff) / SIZEOF(strlit.buff[0]));
				buffptr = &strlit.buff[0];
				for (;;)
				{
					if (inchar >= in_top)
					{
						assert(inchar == in_top);
						instr->addr = (char *)inchar + 1;
						return ERR_PATLIT;
					}
					curchar = *inchar;
					if ('\"' == curchar)
					{
						if (++inchar >= in_top)
						{
							assert(inchar == in_top);
							topseen = TRUE;
							break;
						}
						if ((curchar = *inchar) != '\"')
						{
							inchar++;
							break;
						}
					}
					if (!gtm_utf8_mode)
						bytelen = 1;
#					ifdef UNICODE_SUPPORTED
					else
					{
						if (!UTF8_VALID(inchar, in_top, bytelen))
						{
							instr->addr = (char *)inchar;
							return ERR_PATLIT;
						}
						assert(1 <= bytelen);
					}
#					endif
					if (!IS_ASCII(curchar))
						strlit.flags |= PATM_STRLIT_NONASCII;
					strlit.bytelen += bytelen;
					if (strlit.bytelen >= alloclen)
					{
						instr->addr = (char *)inchar;
						return ERR_PATMAXLEN;
					}
					do
					{
						assert(inchar < in_top);
						*buffptr++ = *inchar++;
					} while (0 < --bytelen);
					strlit.charlen++;
				}
				assert((strlit.flags & PATM_STRLIT_NONASCII) || strlit.bytelen == strlit.charlen);
				assert((strlit.flags & PATM_STRLIT_NONASCII) || !(strlit.flags & PATM_STRLIT_BADCHAR));
				if (!strlit.charlen)
				{
					lower_bound = upper_bound = 0;
					infinite = FALSE;
					fixed_len = prev_fixed_len;
					split_atom = FALSE;
				}
			} else if (!topseen && ('(' == curchar))
			{	/* start of 'alternation' */
				if (dfa)
				{	/* Finish ongoing DFA and start compiling alternation afresh at a nested depth */
					/* Note: Below macro can do a "return ERR_PATMAXLEN" if there is not enough space */
					TERMINATE_DFA(patmaskptr, outchar, dfa_fixed_len, lv_ptr, leaf_num, exp_ptr, fstchar,
						topchar, min, max, size, total_min, total_max, count, lastpatptr, last_infinite,
						min_dfa, altmin, altmax, instr, inchar, dfa);
				}
				start_dfa = FALSE;	/* reset start_dfa for entire duration of alternation */
				if (inchar >= in_top)
				{
					assert(inchar == in_top);
					instr->addr = (char *)inchar + 1;
					return ERR_PATCODE;
				}
				pattern_mask = PATM_ALT;
				cur_alt = &init_alt;
				alttail.addr = (char *)inchar;
				alttail.len = instr->len - (int4)((char *)inchar - saveinstr);
				status = patstr(&alttail, &cur_alt->altpat, &inchar);
				if (status)
				{
					instr->addr = (char *)alttail.addr;
					return status;
				}
				saw_delimiter = 1;
				altlen = cur_alt->altpat.buff[PAT_LEN_OFFSET];
				altmin = cur_alt->altpat.buff[PAT_TOT_MIN_OFFSET(altlen)];
				altmax = cur_alt->altpat.buff[PAT_TOT_MAX_OFFSET(altlen)];
				altcount = 1;
				any_alt = TRUE;
				assert(inchar < in_top);
				curchar = *inchar++;
				altactive = 1;
				continue;
			} else if (!topseen && (',' == curchar))
			{	/* separator between alternate possibilities */
				/* The malloc that is requested here will be freed below when the alternation is
				 * added to the output data structure (just below the call to add_atom).
				 */
				if (!altactive)
				{
					instr->addr = (char *)inchar;
					return ERR_PATCLASS;
				}
				if (inchar >= in_top)
				{
					assert(inchar == in_top);
					instr->addr = (char *)inchar + 1;
					return ERR_PATCODE;
				}
				cur_alt->next = (unsigned char *)malloc(SIZEOF(alternation));
				cur_alt = (alternation *)cur_alt->next;
				cur_alt->next = NULL;
				done_free = FALSE;
				alttail.addr = (char *)inchar;
				alttail.len = instr->len - (int4)((char *)inchar - saveinstr);
				status = patstr(&alttail, &cur_alt->altpat, &inchar);
				if (status)
				{
					instr->addr = (char *)alttail.addr;
					return status;
				}
				saw_delimiter = 1;
				altlen = cur_alt->altpat.buff[PAT_LEN_OFFSET];
				if (cur_alt->altpat.buff[PAT_TOT_MIN_OFFSET(altlen)] < altmin)
					altmin = cur_alt->altpat.buff[PAT_TOT_MIN_OFFSET(altlen)];
				if (cur_alt->altpat.buff[PAT_TOT_MAX_OFFSET(altlen)] > altmax)
					altmax = cur_alt->altpat.buff[PAT_TOT_MAX_OFFSET(altlen)];
				altcount++;
				assert(inchar < in_top);
				curchar = *inchar++;
				continue;
			} else if (!topseen && (')' == curchar))
			{	/* end of 'alternation' */
				if (!altactive)
				{
					instr->addr = (char *)inchar;
					return ERR_PATCLASS;
				}
				altactive = 0;
				if (inchar < in_top)
					curchar = *inchar++;
				else
				{	/* We cannot do curchar = *inchar++ in this case since we are beyond the input bounds */
					assert(inchar == in_top);
					topseen = TRUE;
					assert(!dfa);
				}
			} else
			{
				if (topseen)
				{
					instr->addr = (char *)inchar + 1;
					return ERR_PATCLASS;
				}
				pattern_mask = 0;
				do
				{
					chidx = (curchar > 'Z') ? curchar - 'a' : curchar - 'A';
					if ((0 <= chidx) && (chidx <= 'X' - 'A'))
						pattern_mask |= mapbit[chidx];
					else if (('Y' - 'A' == chidx) || ('Z' - 'A' == chidx))
					{	/* YxxxY and ZxxxZ codes not yet implemented */
						instr->addr = (char *)inchar;
						return ERR_PATCLASS;
					} else
					{
						assert(TK_UPPER != ctypetab[curchar] && TK_LOWER != ctypetab[curchar]);
						break;
					}
					if (inchar >= in_top)
					{
						topseen = TRUE;
						assert(inchar == in_top);
						break;
					}
					curchar = *inchar++;
				} while (TRUE);
				if (0 == pattern_mask)
				{
					if (any_alt)
					{
						instr->addr = alttail.addr + 1;
						return ERR_PATCODE;
					}
					instr->addr = topseen ? (char *)inchar + 1 : (char *)inchar;
					return ERR_PATCLASS;
				}
			}
		}
		if (split_atom)
		{
			assert(FALSE == infinite);
			upper_bound = lower_bound;
		}
		done = FALSE;
		while (!done)
		{
			done = TRUE;
			/* DFAs can be used within alternations, but not at the nesting level where the alternations themselves
			 * occur. Also, strings with a length of 0 characters should not be processed within DFAs, since there
			 * are no character cells to hold the mask and flag bits that the DFA code needs. Also strings with
			 * non-ASCII UTF-8 byte sequences are currently not processed through the DFA logic.
			 */
			assert(outchar <= topchar);
			if (start_dfa && !any_alt && !dfa
				&& (!(pattern_mask & PATM_STRLIT) || (strlit.charlen && !(strlit.flags & PATM_STRLIT_NONASCII)))
					&& ((outchar - &obj->buff[0]) <= (MAX_PATTERN_LENGTH / 2)))
			{
				dfa = TRUE;
				last_leaf_mask = 0;
				leaf_num = 0;
				sym_num = 0;
				min_dfa = 0;
				/* Maintain this just like "fixed_len" but starting from current atom
				 * whereas "fixed_len" starts from beginning of input pattern string.
				 */
				dfa_fixed_len = !infinite;
				atom_map = count;
				memset(expand.num_e, 0, SIZEOF(expand.num_e));
			}
			start_dfa = FALSE;
			if (!dfa)
			{
				assert(outchar <= topchar);
				if ((MAX_PATTERN_ATOMS <= count)
					|| !add_atom(&count, pattern_mask, &strlit, infinite,
						&min[count], &max[count], &size[count],
						&total_min, &total_max, lower_bound, upper_bound, altmin, altmax,
						&last_infinite, &fstchar, &outchar, &lastpatptr))
				{
					instr->addr = (char *)inchar;
					return ERR_PATMAXLEN;
				}
				assert(outchar <= topchar);
				if (pattern_mask & PATM_ALT)
				{	/* If the alternation contains only one alternative (altcount == 1) AND
					 * that alternative contains only one pattern atom, AND that atom is not an
					 * alternation or a DFA, the alternation can be reduced to that atom.
					 * The boundaries of the compressed atom will be the products of the
					 * boundaries of the alternation and those of the atom within the alternation
					 * (lower*lower and upper*upper) E.g. 10.20(.5AN) is the same as .100AN
					 *
					 * Such a simplification can be made if:
					 *    => the inner lower limit is 0 or 1
					 *    		e.g. 2.3(0.2L) = 0.6L
					 *    			0 = 0 + 0 + 0
					 *    			1 = 0 + 0 + 1
					 *    			2 = 0 + 1 + 1
					 *    			3 = 1 + 1 + 1
					 *    			4 = 1 + 1 + 2
					 *    			5 = 1 + 2 + 2
					 *    			6 = 2 + 2 + 2
					 *    		e.g. 2.3(1.2L) = 2.6L
					 *    			2 =     1 + 1
					 *    			3 = 1 + 1 + 1
					 *    			4 = 1 + 1 + 2
					 *    			5 = 1 + 2 + 2
					 *    			6 = 2 + 2 + 2
					 *    		Note that lower limit 1 case is the same as the lower limit 0 case
					 *    			except for not counting "0" as one match.
					 *
					 *    => or the outer lower and upper limit are the same (not a range)
					 *    		e.g. 4(4.5L) = 16.20L
					 *    			16 = 4 + 4 + 4 + 4
					 *    			17 = 4 + 4 + 4 + 5
					 *    			18 = 4 + 4 + 5 + 5
					 *    			19 = 4 + 5 + 5 + 5
					 *    			20 = 5 + 5 + 5 + 5
					 */
					altsimplify = ((1 == altcount) && cur_alt) ? TRUE : FALSE;
					if (altsimplify && ((cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET] & PATM_ALT)
								|| (PATM_DFA == cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET])))
						altsimplify = FALSE;
					if (altsimplify)
					{
						jump = cur_alt->altpat.buff[PAT_LEN_OFFSET];
						if (1 != cur_alt->altpat.buff[PAT_COUNT_OFFSET(jump)])
							altsimplify = FALSE;
					}
					if (altsimplify)
					{
						assert(0 == cur_alt->altpat.buff[0] || 1 == cur_alt->altpat.buff[0]);
						size_in = cur_alt->altpat.buff[
								PAT_SIZE_BEGIN_OFFSET(cur_alt->altpat.buff[0], jump, 1)];
						high_in = cur_alt->altpat.buff[
								PAT_MAX_BEGIN_OFFSET(cur_alt->altpat.buff[0], jump, 1)];
						low_in = cur_alt->altpat.buff[
								PAT_MIN_BEGIN_OFFSET(cur_alt->altpat.buff[0], jump, 1)];
						if (!size_in)
							size_in = 1;
						if ((1 != low_in) && (0 != low_in) && (lower_bound != upper_bound))
							altsimplify = FALSE;
					}
					if (altsimplify)
					{
						size[count - 1] = size_in;
						min[count - 1] = BOUND_MULTIPLY(low_in, lower_bound, bound);
						lower_bound = min[count - 1];
						if (!cur_alt->altpat.buff[0])
							fixed_len = FALSE;
						max[count - 1] = BOUND_MULTIPLY(high_in, upper_bound, bound);
						if (!fixed_len)
							upper_bound = max[count - 1];
						outchar--;
						if ((outchar + (jump - PAT_MASK_BEGIN_OFFSET + 1)) > topchar)
						{
							instr->addr = (char *)inchar;
							return ERR_PATMAXLEN;
						}
						for (seq = PAT_MASK_BEGIN_OFFSET; seq <= jump; seq++)
							*outchar++ = cur_alt->altpat.buff[seq];
						assert(outchar <= topchar);
						pattern_mask = cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET];
						if (pattern_mask & PATM_STRLIT)
						{
							assert(3 == PAT_STRLIT_PADDING);
							strlit.bytelen = cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET + 1];
							strlit.charlen = cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET + 2];
							strlit.flags = cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET + 3];
							memcpy(strlit.buff, &cur_alt->altpat.buff[PAT_MASK_BEGIN_OFFSET
										+ PAT_STRLIT_PADDING + 1], strlit.bytelen);
						}
					} else
					{
						fixed_len = FALSE;
						if ((outchar + 3) > topchar)
						{
							instr->addr = (char *)inchar;
							return ERR_PATMAXLEN;
						}
						*outchar++ = lower_bound;
						*outchar++ = upper_bound;
						for (cur_alt = &init_alt; cur_alt; )
						{
							assert(outchar < topchar);
							if ((outchar + 2 + cur_alt->altpat.len) > topchar)
							{
								instr->addr = (char *)inchar;
								return ERR_PATMAXLEN;
							}
							*outchar++ = cur_alt->altpat.len;
							for (seq = 0; seq < cur_alt->altpat.len; seq++)
								*outchar++ = cur_alt->altpat.buff[seq];
							cur_alt = (alternation *)cur_alt->next;
						}
						*outchar++ = 0;
						assert(outchar <= topchar);
						done_free = TRUE;
					}
				}
			} else
			{
				leafcnt = charpos = MAX(lower_bound, 1);
				if ((pattern_mask & PATM_STRLIT) && !(strlit.flags & PATM_STRLIT_NONASCII))
				{
					assert(strlit.charlen == strlit.bytelen);
					charpos *= strlit.bytelen;
					leafcnt = MAX(charpos, leafcnt);
				}
				if (((pattern_mask & PATM_STRLIT) && (strlit.flags & PATM_STRLIT_NONASCII))
					|| (!infinite && ((lower_bound != upper_bound) || !lower_bound))
					|| ((leaf_num + leafcnt) >= (MAX_SYM - 1))
					|| (charpos > MAX_DFA_STRLEN))
				{
					/* Note: Below macro can do a "return ERR_PATMAXLEN" if there is not enough space */
					TERMINATE_DFA(patmaskptr, outchar, dfa_fixed_len, lv_ptr, leaf_num, exp_ptr, fstchar,
						topchar, min, max, size, total_min, total_max, count, lastpatptr, last_infinite,
						min_dfa, altmin, altmax, instr, inchar, dfa);
					start_dfa = ((MAX_DFA_STRLEN >= charpos) /* Try another DFA for remainder if possible */
							&& (infinite || (lower_bound == upper_bound) && lower_bound) && leaf_num);
					done = FALSE;
					continue;
				}
				curr_min_dfa = min_dfa;
				curr_leaf_num = leaf_num;
				if (pattern_mask & PATM_STRLIT)
				{
					memset(&exp_temp[0], 0, SIZEOF(exp_temp));
					assert(atom_map < MAX_PATTERN_ATOMS);
					min[atom_map] = lower_bound;
					max[atom_map] = upper_bound;
					size[atom_map] = strlit.bytelen;
					atom_map++;
					min_dfa += lower_bound * strlit.bytelen;
					cursize = MAX(lower_bound, 1);
					for (seqcnt = 0; seqcnt < cursize; seqcnt++)
					{
						for (charpos = 0; charpos < strlit.bytelen; charpos++)
						{
							symbol = strlit.buff[charpos];
							/* It is ok to use typemask[] below because we are guaranteed
							 * that "symbol" is a 1-byte valid ASCII character. Assert that.
							 */
							assert(!(strlit.flags & PATM_STRLIT_NONASCII) && IS_ASCII(symbol));
							bitpos = patmaskseq(typemask[symbol]);
							if (expand.num_e[bitpos] + exp_temp[bitpos] == 0)
								exp_temp[bitpos]++;
							for (leafcnt = 1;
								(leafcnt < (expand.num_e[bitpos] + exp_temp[bitpos]))
									&& (expand.meta_c[bitpos][leafcnt] != symbol);
								leafcnt++)
								;
							if (leafcnt == expand.num_e[bitpos] + exp_temp[bitpos])
								exp_temp[bitpos]++;
							expand.meta_c[bitpos][leafcnt] = symbol;
							if (!infinite)
							{
								leaves.letter[leaf_num][0] = symbol;
								leaves.letter[leaf_num][1] = -1;
								leaves.nullable[leaf_num++] = FALSE;
							} else
								leaves.letter[leaf_num][charpos] = symbol;
						}
						if (infinite)
						{
							leaves.letter[leaf_num][charpos] = -1;
							leaves.nullable[leaf_num++] = infinite;
						}
					}
					last_leaf_mask = PATM_STRLIT;
					last_infinite = infinite;
					sym_num = 0;
					for (leafcnt = 0; leafcnt < leaf_num; leafcnt++)
					{
						for (charpos = 0; leaves.letter[leafcnt][charpos] >= 0; charpos++)
						{
							if (!(leaves.letter[leafcnt][charpos] & DFABIT))
								sym_num++;
							else
							{
								bitpos = patmaskseq(leaves.letter[leafcnt][charpos]);
								sym_num += expand.num_e[bitpos] + exp_temp[bitpos];
							}
						}
					}
				} else
				{
					if (!(last_leaf_mask & PATM_STRLIT) && infinite && last_infinite)
					{
						y_max = MAX(pattern_mask, last_leaf_mask);
						if ((last_leaf_mask & pattern_mask) &&
						   ((last_leaf_mask | pattern_mask) == y_max))
						{
							if (last_leaf_mask == y_max)
								continue;
							leaf_num--;
							atom_map--;
						}
					}
					assert(atom_map < MAX_PATTERN_ATOMS);
					min[atom_map] = lower_bound;
					max[atom_map] = upper_bound;
					size[atom_map] = 1;
					atom_map++;
					min_dfa += lower_bound;
					charpos = MAX(lower_bound, 1);
					last_infinite = infinite;
					last_leaf_mask = pattern_mask;
					for (seqcnt = 0; seqcnt < charpos; seqcnt++)
					{
						bitpos = 0;
						leaves.nullable[leaf_num] = infinite;
						/* Check all PAT_MAX_BITS bits if there are flags for internationalization,
						 * otherwise, check only the original PAT_BASIC_CLASSES bits
						 * (C, L, N, P, U, 0, 1) where
						 *	0 = PATM_UTF8_ALPHABET, 1 = PATM_UTF8_NONBASIC
						 */
						if (PATM_E != pattern_mask)
						{
							chidx = (pattern_mask & PATM_I18NFLAGS)
								? PAT_MAX_BITS : PAT_BASIC_CLASSES;
						} else
							chidx = PAT_BASIC_CLASSES;
						for (bit = 0; bit < chidx; bit++)
						{
							mbit = 1 << bit;
							if ((allmask & mbit) && (pattern_mask & mbit))
							{
								seq = patmaskseq((uint4)mbit);
								if (expand.num_e[seq] == 0)
									expand.num_e[seq]++;
								sym_num += expand.num_e[seq];
								assert(MAX_DFA_STRLEN >= bitpos);
								leaves.letter[leaf_num][bitpos++] = DFABIT | mbit;
							}
						}
						assert(MAX_DFA_STRLEN >= bitpos);
						leaves.letter[leaf_num][bitpos] = -1;
						leaf_num++;
					}
				}
				if (sym_num >= MAX_SYM - 1)
				{
					leaf_num = curr_leaf_num;
					min_dfa = curr_min_dfa;
					/* Note: Below macro can do a "return ERR_PATMAXLEN" if there is not enough space */
					TERMINATE_DFA(patmaskptr, outchar, dfa_fixed_len, lv_ptr, leaf_num, exp_ptr, fstchar,
						topchar, min, max, size, total_min, total_max, count, lastpatptr, last_infinite,
						min_dfa, altmin, altmax, instr, inchar, dfa);
					start_dfa = curr_leaf_num; /* Try another DFA for remainder if possible */
					done = FALSE;
					continue;
				} else
				{
					if (last_leaf_mask & PATM_STRLIT)
						for (seqcnt = 0; seqcnt < CHAR_CLASSES; seqcnt++)
							expand.num_e[seqcnt] += exp_temp[seqcnt];
				}
			}
			if (split_atom)
			{
				lower_bound = 0;
				upper_bound = PAT_MAX_REPEAT;
				infinite = TRUE;
				split_atom = FALSE;
				done = FALSE;
				if (dfa)
					dfa_fixed_len = FALSE;
			}
		}
		if (dfa && infinite)
			dfa_fixed_len = FALSE;
		for (cur_alt = &init_alt; cur_alt; )
		{
			let_go = (cur_alt != (alternation *)&init_alt) ? (unsigned char *)cur_alt : NULL;
			cur_alt = (alternation *)cur_alt->next;
			if (let_go)
				free(let_go);
		}
		init_alt.next = NULL;
		init_alt.altpat.len = 0;
	}
}
