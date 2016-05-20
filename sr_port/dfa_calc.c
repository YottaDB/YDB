/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "copy.h"
#include "patcode.h"
#include "compiler.h"
#include "gtm_string.h"

/* the following macro checks that a 1 dimensional array reference is valid i.e. array[index] is within defined limits */
#define ASSERT_IF_1DIM_ARRAY_OVERFLOW(array, index) assert(index < ARRAYSIZE(array))

/* the following macro checks that a 2 dimensional array reference is valid i.e. array[row][col] is within defined limits */
#define ASSERT_IF_2DIM_ARRAY_OVERFLOW(array, row, col)	\
{							\
	assert((row) < ARRAYSIZE(array));		\
	assert((col) < ARRAYSIZE(array[0]));		\
}

/* Note: in various places, dfa_calc() makes a reference to the array 'typemask'.  dfa_calc() is executed at compile-time.
 * The content of the array typemask is static, but, at run-time, the pointer that is used to access the typemask array
 * 	(pattern_typemask) may change whenever a program executes the command View "PATCODE":tablename.
 * As a result, the pattern masks that the GT.M compiler uses may differ from the ones that are in operation at run-time.
 */
LITREF	uint4	typemask[PATENTS];

static	uint4	classmask[CHAR_CLASSES] =
{
	PATM_N, PATM_P, PATM_L, PATM_U, PATM_C, PATM_B, PATM_D, PATM_F, PATM_G, PATM_H, PATM_I,
	PATM_J, PATM_K, PATM_M, PATM_O, PATM_Q, PATM_R, PATM_S, PATM_T, PATM_V, PATM_W, PATM_X,
	PATM_UTF8_ALPHABET, PATM_UTF8_NONBASIC
};

/* This procedure is part of the MUMPS compiler. The function of this procedure is to build the data structures that
 * will be used to drive the DFA engine that can evaluate certain pattern matches. Note that this routine operates
 * at compile-time, and that all data structures built in this procedure are compiled at the end into a terse string
 * of values that will be passed to do_pattern (through patstr). do_pattern(), which operates at run-time will
 * interpret this string of values and do the actual DFA work (DFA = Discrete Finite Automaton).
 *
 * Refer to the section in Aho & Sehti compiler book on going directly "From a regular expression to a DFA" for
 * a description of how the below algorithm works.
 */
int dfa_calc(struct leaf *leaves, int leaf_num, struct e_table *expand, uint4 **fstchar_ptr, uint4 **outchar_ptr)
{
	uint4			*locoutchar;
	uint4			pattern_mask;
	unsigned char		*textstring;
	int 			offset[MAX_DFA_STATES + 2];
	int			pos_offset[CHAR_CLASSES];
	int			fst[2][2], lst[2][2];
	int4			charcls, maskcls, numexpand, count, clsnum, maxcls, clsposlis;
	int4			state_num, node_num, sym_num, expseq, seq;
	struct node		nodes;
				/* EdM: comment for reviewers:
				 * 'states' is currently defined as a boolean_t.
				 * In the original version it was a bool (== char).
				 * Since comparisons on this array are done using
				 * memcmp, and the only values assigned to elements
				 * in this array are TRUE and FALSE (1 and 0),
				 * we might consider declaring states as a 'char'
				 * array after all...
				 */
	boolean_t		states[MAX_DFA_STATES][MAX_DFA_STATES];
	boolean_t		fpos[MAX_DFA_STATES][MAX_DFA_STATES];
	int			d_trans[MAX_DFA_STATES][MAX_DFA_STATES];
	int			pos_lis[MAX_DFA_STATES][MAX_DFA_STATES];
	struct c_trns_tb	c_trans;

	locoutchar = *outchar_ptr;
	if (0 == leaf_num)
		return -1;
	if (leaf_num > 1)
	{
		pattern_mask = PATM_DFA;
		state_num = 1;
		leaves->nullable[leaf_num] = FALSE;
		leaves->letter[leaf_num][0] = DFABIT;
		leaves->letter[leaf_num][1] = -1;
		pos_offset[0] = 0;
		for (seq = 1; seq < CHAR_CLASSES; seq++)
			pos_offset[seq] = pos_offset[seq - 1] + expand->num_e[seq - 1];
		memset(&nodes, 0, SIZEOF(nodes));
		memset(&fpos[0][0], 0, SIZEOF(fpos));
		memset(&states[0][0], 0, SIZEOF(states));
		memset(&d_trans[0][0], 128, SIZEOF(d_trans));
		memset(&pos_lis[0][0], 128, SIZEOF(pos_lis));
		memset(&c_trans.c[0], 0, SIZEOF(c_trans.c));
		memset(offset, 0, SIZEOF(offset));
		memset(fst, 0, SIZEOF(fst));
		memset(lst, 0, SIZEOF(lst));
		charcls = 0;
		clsnum = 0;
		maxcls = 0;
		nodes.nullable[0] = leaves->nullable[0] & leaves->nullable[1];
		states[state_num][charcls] = TRUE;
		for (maskcls = 0; leaves->letter[0][maskcls] >= 0; maskcls++)
		{
			if (!(leaves->letter[0][maskcls] & DFABIT))
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, charcls, charcls + 1);
				fpos[charcls][charcls + 1] = TRUE;
				lst[FST][FST] = charcls;
				lst[FST][LST] = charcls;
				assert(leaves->letter[0][maskcls] >= 0 && leaves->letter[0][maskcls] < SIZEOF(typemask));
				seq = patmaskseq(typemask[leaves->letter[0][maskcls]]);
				if (seq < 0)
					seq = 0;
				for (numexpand = 1; expand->meta_c[seq][numexpand] != leaves->letter[0][maskcls]; numexpand++)
					;
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(expand->meta_c, seq, numexpand);
				ASSERT_IF_1DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand));
				for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
					;
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
				pos_lis[pos_offset[seq] + numexpand][count] = charcls;
				charcls++;
			} else
			{
				seq = patmaskseq(leaves->letter[0][maskcls]);
				if (seq < 0)
				{
					seq = 0;
					expseq = 0;
				} else
					expseq = expand->num_e[seq];
				for (numexpand = 0; numexpand < expseq; numexpand++)
				{
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(states, state_num, charcls);
					states[state_num][charcls] = TRUE;
					fst[FST][LST] = charcls;
					lst[FST][LST] = charcls;
					ASSERT_IF_1DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand));
					for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
						;
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
					pos_lis[pos_offset[seq] + numexpand][count] = charcls;
					charcls++;
				}
			}
		}
		fst[LST][FST] = charcls;
		fst[LST][LST] = charcls;
		lst[LST][FST] = charcls;
		if (!leaves->nullable[1])
		{
			nodes.last[0][charcls] = TRUE;
			maxcls = charcls;
		}
		for (maskcls = 0; leaves->letter[1][maskcls] >= 0; maskcls++)
		{
			if (!(leaves->letter[1][maskcls] & DFABIT))
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, charcls, charcls + 1);
				fpos[charcls][charcls + 1] = TRUE;
				lst[LST][FST] = charcls;
				lst[LST][LST] = charcls;
				assert(leaves->letter[1][maskcls] >= 0 && leaves->letter[1][maskcls] < SIZEOF(typemask));
				seq = patmaskseq(typemask[leaves->letter[1][maskcls]]);
				if (seq < 0)
					seq = 0;
				for (numexpand = 1; expand->meta_c[seq][numexpand] != leaves->letter[1][maskcls]; numexpand++)
					;
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(expand->meta_c, seq, numexpand);
				ASSERT_IF_1DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand));
				for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
					;
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
				pos_lis[pos_offset[seq] + numexpand][count] = charcls;
				charcls++;
			} else
			{
				seq = patmaskseq(leaves->letter[1][maskcls]);
				if (seq < 0)
				{
					seq = 0;
					expseq = 0;
				} else
					expseq = expand->num_e[seq];
				for (numexpand = 0; numexpand < expseq; numexpand++)
				{
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, 0, charcls);
					nodes.last[0][charcls] = TRUE;
					fst[LST][LST] = charcls;
					lst[LST][LST] = charcls;
					ASSERT_IF_1DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand));
					for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
						;
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
					pos_lis[pos_offset[seq] + numexpand][count] = charcls;
					charcls++;
				}
			}
		}
		if (leaves->nullable[0])
		{
			assert(MAX_DFA_STATES > lst[FST][LST]);
			assert(MAX_DFA_STATES > fst[LST][LST]);
			for (numexpand = lst[FST][FST]; numexpand <= lst[FST][LST]; numexpand++)
			{
				for (count = fst[FST][FST]; count <= fst[FST][LST]; count++)
				{
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
					fpos[numexpand][count] = TRUE;
				}
			}
			for (numexpand = fst[LST][FST]; numexpand <= fst[LST][LST]; numexpand++)
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(states, state_num, numexpand);
				states[state_num][numexpand] = TRUE;
			}
		}
		if (leaves->nullable[1])
		{
			nodes.last[0][charcls - 1] = TRUE;
			for (numexpand = lst[LST][FST]; numexpand <= lst[LST][LST]; numexpand++)
			{
				for (count = fst[LST][FST]; count <= fst[LST][LST]; count++)
				{
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
					fpos[numexpand][count] = TRUE;
				}
			}
			for (numexpand = lst[FST][FST]; numexpand <= lst[FST][LST]; numexpand++)
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, 0, numexpand);
				nodes.last[0][numexpand] = TRUE;
			}
			maxcls = charcls;
		}
		for (numexpand = lst[FST][FST]; numexpand <= lst[FST][LST]; numexpand++)
		{
			for (count = fst[LST][FST]; count <= fst[LST][LST]; count++)
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
				fpos[numexpand][count] = TRUE;
			}
		}
		if (!leaves->nullable[1])
			clsnum = lst[LST][FST];
		for (node_num = 1; node_num < leaf_num; node_num++)
		{
			nodes.nullable[node_num] = nodes.nullable[node_num - 1] & leaves->nullable[node_num + 1];
			if (leaves->nullable[node_num + 1])
			{
				for (maskcls = 0; maskcls < charcls; maskcls++)
					nodes.last[node_num][maskcls] = nodes.last[node_num - 1][maskcls];
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, node_num, maskcls);
			} else
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, node_num, charcls);
				nodes.last[node_num][charcls] = TRUE;
				maxcls = charcls;
			}
			fst[LST][FST] = charcls;
			fst[LST][LST] = charcls;
			lst[LST][FST] = charcls;
			for (maskcls = 0; leaves->letter[node_num + 1][maskcls] >= 0; maskcls++)
			{
				ASSERT_IF_1DIM_ARRAY_OVERFLOW(leaves->letter[node_num + 1], maskcls);
				if (!(leaves->letter[node_num + 1][maskcls] & DFABIT))
				{
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, charcls, charcls + 1);
					fpos[charcls][charcls + 1] = TRUE;
					lst[LST][FST] = charcls;
					lst[LST][LST] = charcls;
					assert((0 <= leaves->letter[node_num + 1][maskcls])
						&& (leaves->letter[node_num + 1][maskcls] < SIZEOF(typemask)));
					seq = patmaskseq(typemask[leaves->letter[node_num + 1][maskcls]]);
					if (seq < 0)
						seq = 0;
					for (numexpand = 1;
						expand->meta_c[seq][numexpand] != leaves->letter[node_num + 1][maskcls];
						numexpand++)
						;
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(expand->meta_c, seq, numexpand);
					ASSERT_IF_1DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand));
					for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
						;
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
					pos_lis[pos_offset[seq] + numexpand][count] = charcls;
					charcls++;
				} else
				{
					seq = patmaskseq(leaves->letter[node_num + 1][maskcls]);
					if (seq < 0)
					{
						seq = 0;
						expseq = 0;
					} else
						expseq = expand->num_e[seq];
					for (numexpand = 0; numexpand < expseq; numexpand++)
					{
						ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, node_num, charcls);
						nodes.last[node_num][charcls] = TRUE;
						if (nodes.nullable[node_num - 1])
							states[state_num][charcls] = TRUE;
						fst[LST][LST] = charcls;
						lst[LST][LST] = charcls;
						for (count = 0; pos_lis[pos_offset[seq] + numexpand][count] >= 0; count++)
							;
						ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, (pos_offset[seq] + numexpand), count);
						pos_lis[pos_offset[seq] + numexpand][count] = charcls;
						charcls++;
					}
				}
			}
			if (nodes.nullable[node_num - 1])
			{
				for (numexpand = fst[LST][FST]; numexpand <= fst[LST][LST]; numexpand++)
					states[state_num][numexpand] = TRUE;
				ASSERT_IF_1DIM_ARRAY_OVERFLOW(states[state_num], numexpand);
			}
			if (leaves->nullable[node_num + 1])
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(nodes.last, node_num, charcls - 1);
				nodes.last[node_num][charcls - 1] = TRUE;
				for (numexpand = lst[LST][FST]; numexpand <= lst[LST][LST]; numexpand++)
				{
					for (count = fst[LST][FST]; count <= fst[LST][LST]; count++)
						fpos[numexpand][count] = TRUE;
					ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
				}
				maxcls = charcls;
			}
			for (numexpand = clsnum; numexpand < maxcls; numexpand++)
			{
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
				for (count = fst[LST][FST]; count <= fst[LST][LST]; count++)
					if (nodes.last[node_num - 1][numexpand])
						fpos[numexpand][count] = TRUE;
				ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, numexpand, count);
			}
			ASSERT_IF_1DIM_ARRAY_OVERFLOW(leaves->nullable, node_num + 1);
			if (!leaves->nullable[node_num + 1])
				clsnum = lst[LST][FST];
		}
		sym_num = charcls;
		state_num++;
		for (seq = 1; seq < state_num; seq++)
		{
			charcls = 0;
			ASSERT_IF_1DIM_ARRAY_OVERFLOW(offset, seq + 1);
			offset[seq + 1]++;
			offset[seq + 1] += offset[seq];
			for (maskcls = 0; maskcls < CHAR_CLASSES; maskcls++)
			{
				if (expand->num_e[maskcls] > 0)
				{
					for (numexpand = 0; numexpand < expand->num_e[maskcls]; numexpand++)
					{
						for  (maxcls = 0; pos_lis[charcls + numexpand][maxcls] >= 0; maxcls++)
						{
							clsposlis = pos_lis[charcls + numexpand][maxcls];
							if (states[seq][clsposlis])
							{
								ASSERT_IF_2DIM_ARRAY_OVERFLOW(fpos, clsposlis, clsnum);
								for (clsnum = 0; clsnum <= sym_num; clsnum++)
									states[state_num][clsnum] |= fpos[clsposlis][clsnum];
								ASSERT_IF_2DIM_ARRAY_OVERFLOW(states, state_num, clsnum);
							}
						}
						ASSERT_IF_2DIM_ARRAY_OVERFLOW(pos_lis, charcls + numexpand, maxcls);
						ASSERT_IF_1DIM_ARRAY_OVERFLOW(states[state_num], clsnum);
						ASSERT_IF_1DIM_ARRAY_OVERFLOW(states, state_num);
						for (count = 0;
							memcmp(states[count], states[state_num], (sym_num + 1) * SIZEOF(boolean_t))
								&& (count < state_num);
							count++)
							;
						if (count > 0)
						{
							ASSERT_IF_2DIM_ARRAY_OVERFLOW(d_trans, seq, charcls);
							if (0 == numexpand)
							{
								d_trans[seq][charcls] = count;
								ASSERT_IF_1DIM_ARRAY_OVERFLOW(c_trans.c, seq);
								for (clsnum = 0;
									(clsnum < c_trans.c[seq])
										&& (c_trans.trns[seq][clsnum] != count);
									clsnum++)
									;
								ASSERT_IF_2DIM_ARRAY_OVERFLOW(c_trans.trns, seq, clsnum);
								ASSERT_IF_1DIM_ARRAY_OVERFLOW(c_trans.p_msk[seq], clsnum);
								if (clsnum == c_trans.c[seq])
								{
									c_trans.p_msk[seq][clsnum] = classmask[maskcls];
									c_trans.trns[seq][clsnum] = count;
									offset[seq + 1] += 2;
									c_trans.c[seq]++;
								} else
									c_trans.p_msk[seq][clsnum] |= classmask[maskcls];
							} else if (d_trans[seq][charcls] != count)
							{
								ASSERT_IF_2DIM_ARRAY_OVERFLOW(d_trans, seq, charcls + numexpand);
								d_trans[seq][charcls + numexpand] = count;
								offset[seq + 1] += 3;
							}
							if (count == state_num)
							{
								state_num++;
								if (state_num >= ARRAYSIZE(states))
									return -1;
							} else
								memset(states[state_num], 0, (sym_num + 1) * SIZEOF(states[0][0]));
						}
					}
					charcls += expand->num_e[maskcls];
				}
			}
		}
		*outchar_ptr += offset[state_num] + 2;
		if ((*outchar_ptr - *fstchar_ptr > MAX_DFA_SPACE) || ((offset[state_num] + 1) > (MAX_PATTERN_LENGTH / 2)))
			return -1;
		*locoutchar++ = pattern_mask;
		*locoutchar++ = offset[state_num];
		for (seq = 1; seq < state_num; seq++)
		{
			charcls = 0;
			for (numexpand = 0; numexpand < CHAR_CLASSES; numexpand++)
			{
				if (expand->num_e[numexpand] > 1)
				{
					for (count = 1; count < expand->num_e[numexpand]; count++)
					{
						ASSERT_IF_2DIM_ARRAY_OVERFLOW(d_trans, seq, charcls + count);
						if (d_trans[seq][charcls + count] >= 0)
						{
							*locoutchar++ = PATM_STRLIT;
							*locoutchar++ = expand->meta_c[numexpand][count];
							*locoutchar++ = offset[d_trans[seq][charcls + count]];
						}
					}
				}
				charcls += expand->num_e[numexpand];
			}
			for (numexpand = 0; numexpand < c_trans.c[seq]; numexpand++)
			{
				*locoutchar++ = c_trans.p_msk[seq][numexpand];
				*locoutchar++ = offset[c_trans.trns[seq][numexpand]];
			}
			*locoutchar++ = (states[seq][sym_num]) ? PATM_ACS : PATM_DFA;
		}
		assert(MAX_DFA_SPACE >= (locoutchar - *fstchar_ptr));
		return 1;
	} else
	{
		pattern_mask = 0;
		*outchar_ptr += 1;
		maskcls = 1;
		if (!(leaves->letter[0][0] & DFABIT))
		{
			pattern_mask = PATM_STRLIT;
			for (maskcls = 0; leaves->letter[0][maskcls] >= 0; maskcls++)
				;
			*outchar_ptr += PAT_STRLIT_PADDING + ((maskcls + SIZEOF(uint4) - 1) / SIZEOF(uint4));
		} else
		{
			for (numexpand = 0; leaves->letter[0][numexpand] >= 0; numexpand++)
				pattern_mask |= leaves->letter[0][numexpand];
		}
		if (*outchar_ptr - *fstchar_ptr > MAX_PATTERN_LENGTH)
			return -1;
		*locoutchar++ = pattern_mask;
		if (PATM_STRLIT & pattern_mask)
		{
			*locoutchar++ = maskcls;	/* bytelen */
 			*locoutchar++ = maskcls;	/* charlen */

			*locoutchar++ = 0;		/* both NONASCII and BADCHAR flags are absent since this is
							   indeed a valid ASCII string or else we would not have come
							   to dfa_calc (and hence there are no bad chars) */
			assert(3 == PAT_STRLIT_PADDING);
			textstring = (unsigned char *)locoutchar;	/* change pointer type */
			for (numexpand = 0; numexpand < maskcls; numexpand++)
				*textstring++ = leaves->letter[0][numexpand];
		}
		return maskcls;
	}
}
