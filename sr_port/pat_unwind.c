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
#include "min_max.h"

LITREF uint4	typemask[PATENTS];

/* This procedure is part of the MUMPS compiler. Under certain circumstances, procedure patstr will decide that a
 * pattern is a candidate for optimized processing using a DFA method. In such cases, procedure dfa_calc is called
 * to compile the instructions for do_pattern to execute the DFA evaluation. When, later, either dfa_calc or patstr
 * decides that the rest of the pattern invalidates the DFA strategy, this procedure is called to undo the compilation
 * for the DFA engine and build the data that would "normally" have been compiled for the pattern segment in question.
 */
boolean_t pat_unwind(
	short int		*count,
	struct leaf		*leaves,
	short int		leaf_num,
	short int		*total_min,
	short int		*total_max,
	short int		min[],
	short int		max[],
	short int		size[],
	int			altmin,
	int			altmax,
	boolean_t		*last_infinite_ptr,
	uint4			**fstchar_ptr,
	uint4			**outchar_ptr,
	uint4			**lastpatptr_ptr)
{
	struct {
		int4	 	len;
		unsigned char	lit_buf[MAX_DFA_STRLEN];
		}  	str_lit;
	uint4		pattern_mask;
	short int	minim, maxim, leaf_cnt, charpos, offset, atom_map;
	boolean_t	infinite;

	assert(MAX_SYM > leaf_num);
	for (atom_map = *count, leaf_cnt = 0; leaf_cnt < leaf_num; atom_map++)
	{
		infinite = leaves->nullable[leaf_cnt];
		if (!(leaves->letter[leaf_cnt][0] & DFABIT))
		{
			pattern_mask = PATM_STRLIT;
			for (offset = 0; offset < size[atom_map]; offset += charpos)
			{
				for (charpos = 0; leaves->letter[leaf_cnt][charpos] >= 0; charpos++)
				{
					assert(MAX_DFA_STRLEN > (offset + charpos));
					str_lit.lit_buf[offset + charpos] = leaves->letter[leaf_cnt][charpos];
					pattern_mask |= typemask[leaves->letter[leaf_cnt][charpos]];
				}
				leaf_cnt++;
			}
		} else
		{
			pattern_mask = 0;
			for (charpos = 0; leaves->letter[leaf_cnt][charpos] >= 0; charpos++)
			{
				assert(MAX_DFA_STRLEN > charpos);
				pattern_mask |= leaves->letter[leaf_cnt][charpos];
			}
			leaf_cnt++;
		}
		minim = min[atom_map];
		maxim = max[atom_map];
		leaf_cnt += MAX(minim - 1, 0) * size[atom_map];
		str_lit.len = offset;
		if ((MAX_PATTERN_ATOMS <= *count)
				|| !add_atom(count, pattern_mask, &str_lit, offset, infinite,
					&min[*count], &max[*count], &size[*count], total_min, total_max,
					minim, maxim, altmin, altmax, last_infinite_ptr, fstchar_ptr, outchar_ptr, lastpatptr_ptr))
			return FALSE;
	}
	return TRUE;
}
