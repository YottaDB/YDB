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

#include "mdef.h"

#include "patcode.h"
#include "add_atom.h"
#include "min_max.h"

LITREF uint4	typemask[256];

typedef	short int	short_int;
typedef	short_int	pattern_array[MAX_PATTERN_ATOMS];

bool pat_unwind(short_int *count, struct leaf *leaves, short_int leaf_num, short_int *total, short_int *total_max,
		pattern_array min, pattern_array max, pattern_array size)
{
	struct {
		unsigned char 	len;
		unsigned char	lit_buf[MAX_DFA_STRLEN];
		}  	str_lit;
	unsigned char	x;
	short int	i, j, k, l, m, atom_map;
	bool		infinite;

	k = 0;
	atom_map = *count;
	while ( k < leaf_num )
	{
		infinite = leaves->nullable[k];
		if (leaves->letter[k][0] < ADD)
		{
			m = 0;
			x = PATM_STRLIT;
			while (m < size[atom_map])
			{
				for (l = 0; leaves->letter[k][l] >= 0; l++)
				{
					str_lit.lit_buf[m + l] = leaves->letter[k][l];
					x |= typemask[leaves->letter[k][l]];
				}
				m += l;
				k++;
			}
		}
		else
		{
			x = 0;
			for (l = 0; leaves->letter[k][l] >= 0; l++)
				x |= (unsigned char) leaves->letter[k][l];
			k++;
		}
		i = min[atom_map];
		j = max[atom_map];
		k += MAX(i - 1,0) * size[atom_map];
		str_lit.len = m;
		if (*count < MAX_PATTERN_ATOMS &&
/* Achtung!  What should patcode be? */
		     add_atom(count, 0, x, &str_lit, m, infinite,
			&min[*count], &max[*count], &size[*count], total, total_max, i, j))
		{
			atom_map++;
		}
		else
		{	return FALSE;
		}
	}
	return TRUE;
}
