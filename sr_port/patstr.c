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

#include "compiler.h"
#include "patcode.h"
#include "add_atom.h"
#include "stringpool.h"
#include "toktyp.h"
#include "copy.h"
#include "min_max.h"
#include "patstr.h"

LITREF char	ctypetab[128];
GBLREF spdesc	stringpool;

LITREF uint4    	typemask[256];
GBLDEF unsigned char	*outchar, *lastpatptr;
GBLDEF bool		last_infinite;

int patstr(mstr *instr, mval *obj)
{
	/* this routine contains a hack to redirect the scanner.  repair requires
		redefining the scanner/environment interaction */

	struct {
			unsigned char	len;
			unsigned char	buff[MAX_PATTERN_LENGTH - 2];
		} 		strlit;
	bool			infinite, split_atom, done, dfa, fixed_len;
	int4	        	lower_bound, upper_bound;
	unsigned char		str_ptr, *patmaskptr, patcode, *inchar, ch, x, y_max, last_leaf_code;
	uint4			patmask, last_leaf_mask;
	short int		atom_map, count, total, total_max;
	short int		min[MAX_PATTERN_ATOMS], max[MAX_PATTERN_ATOMS], size[MAX_PATTERN_ATOMS];
	struct leaf		leaves, *lv_ptr;
	struct e_table		expand, *exp_ptr;
	short int		exp_temp[CHAR_CLASSES];
	short int		leaf_num, curr_leaf_num, min_dfa, curr_min_dfa, sym_num;
	short int 		i, j, l, m;
	long int		k;

	error_def(ERR_PATCODE);
	error_def(ERR_PATUPPERLIM);
	error_def(ERR_PATCLASS);
	error_def(ERR_PATLIT);
	error_def(ERR_PATMAXLEN);

	if (stringpool.top - stringpool.free < MAX_PATOBJ_LENGTH)
		stp_gcol(MAX_PATOBJ_LENGTH);

	outchar = stringpool.free + 2 * sizeof(char);
	last_leaf_code = *outchar = 0;
	last_leaf_mask = 0;
	patmaskptr = lastpatptr = outchar;

	infinite = last_infinite = FALSE;
	dfa = split_atom = FALSE;
	fixed_len = TRUE;

	count = total = total_max = atom_map = 0;
	lv_ptr = &leaves;
	exp_ptr = &expand;

	inchar = (unsigned char *)instr->addr;
	ch = *inchar++;

	for (;;)
	{
		instr->addr = (char*)inchar;
		switch(ctypetab[ch])
		{
		case TK_PERIOD:
			lower_bound = 0;
			fixed_len = FALSE;
			break;
		case TK_DIGIT:
			lower_bound = ch - '0';
			while (ctypetab[ch = *inchar++] == TK_DIGIT)
				if ((lower_bound = lower_bound * 10 + (ch - '0'))
					> PAT_MAX_REPEAT)
				{
					lower_bound = PAT_MAX_REPEAT;
				}
			infinite = FALSE;
			break;
		default:
			if (dfa)
			{
				patmaskptr = outchar;
				m = dfa_calc(lv_ptr, leaf_num, exp_ptr);
				if (m >= 0)
				{
					min[count] = min_dfa;
					max[count] = PAT_MAX_REPEAT;
					size[count] = m;
					total = MIN((total + (min[count] * size[count])),PAT_MAX_REPEAT);
					total_max = MIN((total_max + (max[count] * size[count])),PAT_MAX_REPEAT);
					lastpatptr = patmaskptr;
					last_infinite = TRUE;
					count++;
				}
				else
				{
					outchar = patmaskptr;
					if (!pat_unwind(&count, lv_ptr, leaf_num, &total, &total_max,
						&min[0], &max[0], &size[0]))
					{
						return ERR_PATMAXLEN;
					}
				}
			}
			if (outchar == stringpool.free + 2 * sizeof(char))
			{
				return ERR_PATCODE;
			}
			patmaskptr = stringpool.free;
			*patmaskptr++ = fixed_len;
			*patmaskptr = outchar - patmaskptr;
			PUT_SHORT(outchar, count);
			outchar += sizeof(short int);
			PUT_SHORT(outchar, total);
			outchar += sizeof(short int);
			PUT_SHORT(outchar, total_max);
			outchar += sizeof(short int);

			i = count * sizeof(short int);
			memcpy(outchar,min,i);
			outchar += i;

			if (!fixed_len)
			{
				memcpy(outchar,max,i);
				outchar += i;
			}

			memcpy(outchar,size,i);
			outchar += i;

			obj->mvtype = MV_STR;
			obj->str.addr = (char *) stringpool.free;
			obj->str.len = outchar - stringpool.free;
			stringpool.free = outchar;
			instr->addr = (char *)inchar;
			return 0;
		}
		if (ch != '.')
		{	upper_bound = lower_bound;
		}
		else
		{
			fixed_len = FALSE;
			if (ctypetab[ch = *inchar++] != TK_DIGIT)
			{
				if (lower_bound > 0)
				{ split_atom = TRUE;
				}
				else
				{ infinite = TRUE;
				  upper_bound = PAT_MAX_REPEAT;
				}
			}
			else
			{
				instr->addr = (char *)inchar;
				upper_bound = ch - '0';
				while (ctypetab[ch = *inchar++] == TK_DIGIT)
				{	if ((upper_bound = upper_bound * 10 + (ch - '0'))
						> PAT_MAX_REPEAT)
					{
						upper_bound = PAT_MAX_REPEAT;
					}
				}
				if (upper_bound < lower_bound)
				{
					return ERR_PATUPPERLIM;
				}
			}
		}
		instr->addr = (char *)inchar;

		if (count >= MAX_PATTERN_ATOMS)
		{
			return ERR_PATMAXLEN;
		}
		if (ch == '"')
		{
			patcode = PATM_STRLIT;
			patmask = 0;
			str_ptr = 0;
			for (;;)
			{
				if ((ch = *inchar++) < SP || ch >= DEL)
				{
					instr->addr = (char *)inchar;
					return ERR_PATLIT;
				}
				if (ch == '\"')
				{	if ((ch = *inchar++) != '\"')
					{	break;
					}
				}
				if (str_ptr >= MAX_PATTERN_LENGTH - 2)
				{
					instr->addr = (char *)inchar;
					return ERR_PATMAXLEN;
				}
				strlit.buff[str_ptr++] = ch;
				patcode |= typemask[ch];
			}
				strlit.len =  str_ptr;
		}
		else
		{
			patcode = 0;
			for (patmask = 0 ;; ch = *inchar++)
			{
				switch (ch)
				{
				case 'A':
				case 'a':
					patmask |= PATM_A;
					continue;
				case 'C':
				case 'c':
					patmask |= PATM_C;
					continue;
				case 'E':
				case 'e':
					patmask |= PATM_E;
					continue;
				case 'L':
				case 'l':
					patmask |= PATM_L;
					continue;
				case 'N':
				case 'n':
					patmask |= PATM_N;
					continue;
				case 'P':
				case 'p':
					patmask |= PATM_P;
					continue;
				case 'U':
				case 'u':
					patmask |= PATM_U;
					continue;
				case 'B':
				case 'b':
					patmask |= PATM_B;
					continue;
				case 'D':
				case 'd':
					patmask |= PATM_D;
					continue;
				case 'F':
				case 'f':
					patmask |= PATM_F;
					continue;
				case 'G':
				case 'g':
					patmask |= PATM_G;
					continue;
				case 'H':
				case 'h':
					patmask |= PATM_H;
					continue;
				case 'I':
				case 'i':
					patmask |= PATM_I;
					continue;
				case 'J':
				case 'j':
					patmask |= PATM_J;
					continue;
				case 'K':
				case 'k':
					patmask |= PATM_K;
					continue;
				case 'M':
				case 'm':
					patmask |= PATM_M;
					continue;
				case 'O':
				case 'o':
					patmask |= PATM_O;
					continue;
				case 'Q':
				case 'q':
					patmask |= PATM_Q;
					continue;
				case 'R':
				case 'r':
					patmask |= PATM_R;
					continue;
				case 'S':
				case 's':
					patmask |= PATM_S;
					continue;
				case 'T':
				case 't':
					patmask |= PATM_T;
					continue;
				default:
					if (ctypetab[ch] == TK_UPPER || ctypetab[ch] == TK_LOWER)
					{
						instr->addr = (char *)inchar;
						return ERR_PATCLASS;
					}
					break;
				}
				break;
			}
			if (patmask == 0)
			{
				instr->addr = (char *)inchar;
				return ERR_PATCLASS;
			}
		}
		if (split_atom)
		{
			upper_bound = lower_bound;
		}

		done = FALSE;
		while(!done)
		{
			done = TRUE;

			if (infinite && !dfa && outchar - stringpool.free <= MAX_PATTERN_LENGTH / 2)
			{	dfa = TRUE;
				last_leaf_code = 0;
				last_leaf_mask = 0;
				leaf_num = 0;
				sym_num = 0;
				min_dfa = 0;
				atom_map = count;
				memset(expand.num_e,0,(CHAR_CLASSES)*sizeof(short int));
			}

			if (!dfa)
			{
				if (count >= MAX_PATTERN_ATOMS ||
				     !add_atom(&count, patcode, patmask, &strlit, strlit.len, infinite,
						&min[count], &max[count], &size[count],
						&total, &total_max, lower_bound, upper_bound))
				{
					return ERR_PATMAXLEN;
				}
			}

			else
			{
				l = j = MAX(lower_bound,1);
				if (patcode >= PATM_STRLIT)
				{	j *= strlit.len;
					l = MAX(j,l);
				}

				if (lower_bound > MAX_DFA_REP ||
				   (!infinite && lower_bound != upper_bound) ||
				    leaf_num + l >= MAX_SYM - 1 ||
				    j > MAX_DFA_STRLEN)
				{
					patmaskptr = outchar;
 					m = dfa_calc(lv_ptr, leaf_num, exp_ptr);
					if (m >= 0)
					{
						min[count] = min_dfa;
						max[count] = PAT_MAX_REPEAT;
						size[count] = m;
						total = MIN((total + (min[count] * size[count])),PAT_MAX_REPEAT);
						total_max = MIN((total_max + (max[count] * size[count])),PAT_MAX_REPEAT);
						lastpatptr = patmaskptr;
						last_infinite = TRUE;
						count++;
					}
					else
					{
						outchar = patmaskptr;
						if (!pat_unwind(&count, lv_ptr, leaf_num, &total, &total_max,
							&min[0], &max[0], &size[0]))
						{
							return ERR_PATMAXLEN;
						}
					}
					dfa = FALSE;
					done = FALSE;
				}
				else
				{
					curr_min_dfa = min_dfa;
					curr_leaf_num = leaf_num;
					if (patcode >= PATM_STRLIT)
					{
						memset(&exp_temp[0],0,sizeof(exp_temp));
						min[atom_map] = lower_bound;
						max[atom_map] = upper_bound;
						size[atom_map] = strlit.len;
						atom_map++;
						min_dfa += lower_bound * strlit.len;
						m = MAX(lower_bound,1);
						for (i = 0;i < m;i++)
						{
							for (j = 0;j < strlit.len;j++)
							{
								x = strlit.buff[j];
								k = typemask[x];
								switch (k)
								{
									case PATM_N: k = EXP_N;
										     break;
									case PATM_P: k = EXP_P;
										     break;
									case PATM_L: k = EXP_L;
										     break;
									case PATM_U: k = EXP_U;
										     break;
									case PATM_C: k = EXP_C;
										     break;
									case PATM_B: k = EXP_B;
										     break;
									case PATM_D: k = EXP_D;
										     break;
									case PATM_F: k = EXP_F;
										     break;
									case PATM_G: k = EXP_G;
										     break;
									case PATM_H: k = EXP_H;
										     break;
									case PATM_I: k = EXP_I;
										     break;
									case PATM_J: k = EXP_J;
										     break;
									case PATM_K: k = EXP_K;
										     break;
									case PATM_M: k = EXP_M;
										     break;
									case PATM_O: k = EXP_O;
										     break;
									case PATM_Q: k = EXP_Q;
										     break;
									case PATM_R: k = EXP_R;
										     break;
									case PATM_S: k = EXP_S;
										     break;
									case PATM_T: k = EXP_T;
										     break;
								}
								if ( expand.num_e[k] + exp_temp[k] == 0)
								{	exp_temp[k]++;
								}

								for (l = 1;l < expand.num_e[k] + exp_temp[k] &&
									expand.meta_c[k][l] != x;l++)
									;
								if (l == expand.num_e[k] + exp_temp[k])
								{	exp_temp[k]++;
								}
								expand.meta_c[k][l] = x;
								if (!infinite)
								{
									leaves.letter[leaf_num][0] = x;
									leaves.letter[leaf_num][1] = -1;
									leaves.nullable[leaf_num++] = FALSE;
								}
								else
									leaves.letter[leaf_num][j] = x;
							}
							if (infinite)
							{
								leaves.letter[leaf_num][j] = -1;
								leaves.nullable[leaf_num++] = infinite;
							}
						}
						last_leaf_code = PATM_STRLIT;
						last_leaf_mask = 0;
						last_infinite = infinite;

						sym_num = 0;
						for (l = 0; l < leaf_num;l++)
						{
							for (j = 0; leaves.letter[l][j] >= 0; j++)
							{
								if (leaves.letter[l][j] < ADD)
								{	sym_num++;
								}
								else
								{
									k = leaves.letter[l][j] - ADD;
									switch (k)
									{
										case PATM_N: k = EXP_N;
											     break;
										case PATM_P: k = EXP_P;
											     break;
										case PATM_L: k = EXP_L;
											     break;
										case PATM_U: k = EXP_U;
											     break;
										case PATM_C: k = EXP_C;
											     break;
										case PATM_B: k = EXP_B;
											     break;
										case PATM_D: k = EXP_D;
											     break;
										case PATM_F: k = EXP_F;
											     break;
										case PATM_G: k = EXP_G;
											     break;
										case PATM_H: k = EXP_H;
											     break;
										case PATM_I: k = EXP_I;
											     break;
										case PATM_J: k = EXP_J;
											     break;
										case PATM_K: k = EXP_K;
											     break;
										case PATM_M: k = EXP_M;
											     break;
										case PATM_O: k = EXP_O;
											     break;
										case PATM_Q: k = EXP_Q;
											     break;
										case PATM_R: k = EXP_R;
											     break;
										case PATM_S: k = EXP_S;
											     break;
										case PATM_T: k = EXP_T;
											     break;
									}
									sym_num += expand.num_e[k] + exp_temp[k];
								}
							}
						}
					}

					else
					{
						if ((last_leaf_code < PATM_STRLIT) && infinite && last_infinite)
						{
							y_max = MAX(patmask,last_leaf_mask);
							if ((last_leaf_mask & patmask) &&
					 		   ((last_leaf_mask | patmask) == y_max))
							{
								if (last_leaf_mask == y_max)
								{
									continue;
								}
								leaf_num--;
								atom_map--;
							}
						}

						min[atom_map] = lower_bound;
						max[atom_map] = upper_bound;
						size[atom_map] = 1;
						atom_map++;
						min_dfa += lower_bound;
						j = MAX(lower_bound,1);

						last_infinite = infinite;

						for (i = 0;i < j;i++)
						{
							k = 0;
							last_leaf_code = patcode;
							last_leaf_mask = patmask;
							leaves.nullable[leaf_num] = infinite;

							if (patmask & PATM_N)
							{
								if (expand.num_e[EXP_N] == 0)
								{	expand.num_e[EXP_N]++;
								}
								sym_num += expand.num_e[EXP_N];
								leaves.letter[leaf_num][k++] = ADD_N;
							}

							if (patmask & PATM_P)
							{
								if (expand.num_e[EXP_P] == 0)
								{	expand.num_e[EXP_P]++;
								}
								sym_num += expand.num_e[EXP_P];
								leaves.letter[leaf_num][k++] = ADD_P;
							}

							if (patmask & PATM_L)
							{
								if (expand.num_e[EXP_L] == 0)
								{	expand.num_e[EXP_L]++;
								}
								sym_num += expand.num_e[EXP_L];
								leaves.letter[leaf_num][k++] = ADD_L;
							}

							if (patmask & PATM_U)
							{
								if (expand.num_e[EXP_U] == 0)
								{	expand.num_e[EXP_U]++;
								}
								sym_num += expand.num_e[EXP_U];
								leaves.letter[leaf_num][k++] = ADD_U;
							}

							if (patmask & PATM_C)
							{
								if (expand.num_e[EXP_C] == 0)
								{	expand.num_e[EXP_C]++;
								}
								sym_num += expand.num_e[EXP_C];
								leaves.letter[leaf_num][k++] = ADD_C;
							}

							if (patmask & PATM_I18NFLAGS)
							{
								if (patmask & PATM_B)
								{
									if (expand.num_e[EXP_B] == 0)
									{	expand.num_e[EXP_B]++;
									}
									sym_num += expand.num_e[EXP_B];
									leaves.letter[leaf_num][k++] = ADD_B;
								}

								if (patmask & PATM_D)
								{
									if (expand.num_e[EXP_D] == 0)
									{	expand.num_e[EXP_D]++;
									}
									sym_num += expand.num_e[EXP_D];
									leaves.letter[leaf_num][k++] = ADD_D;
								}

								if (patmask & PATM_F)
								{
									if (expand.num_e[EXP_F] == 0)
									{	expand.num_e[EXP_F]++;
									}
									sym_num += expand.num_e[EXP_F];
									leaves.letter[leaf_num][k++] = ADD_F;
								}

								if (patmask & PATM_G)
								{
									if (expand.num_e[EXP_G] == 0)
									{	expand.num_e[EXP_G]++;
									}
									sym_num += expand.num_e[EXP_G];
									leaves.letter[leaf_num][k++] = ADD_G;
								}

								if (patmask & PATM_H)
								{
									if (expand.num_e[EXP_H] == 0)
									{	expand.num_e[EXP_H]++;
									}
									sym_num += expand.num_e[EXP_H];
									leaves.letter[leaf_num][k++] = ADD_H;
								}

								if (patmask & PATM_I)
								{
									if (expand.num_e[EXP_I] == 0)
									{	expand.num_e[EXP_I]++;
									}
									sym_num += expand.num_e[EXP_I];
									leaves.letter[leaf_num][k++] = ADD_I;
								}

								if (patmask & PATM_J)
								{
									if (expand.num_e[EXP_J] == 0)
									{	expand.num_e[EXP_J]++;
									}
									sym_num += expand.num_e[EXP_J];
									leaves.letter[leaf_num][k++] = ADD_J;
								}

								if (patmask & PATM_K)
								{
									if (expand.num_e[EXP_K] == 0)
									{	expand.num_e[EXP_K]++;
									}
									sym_num += expand.num_e[EXP_K];
									leaves.letter[leaf_num][k++] = ADD_K;
								}

								if (patmask & PATM_M)
								{
									if (expand.num_e[EXP_M] == 0)
									{	expand.num_e[EXP_M]++;
									}
									sym_num += expand.num_e[EXP_M];
									leaves.letter[leaf_num][k++] = ADD_M;
								}

								if (patmask & PATM_O)
								{
									if (expand.num_e[EXP_O] == 0)
									{	expand.num_e[EXP_O]++;
									}
									sym_num += expand.num_e[EXP_O];
									leaves.letter[leaf_num][k++] = ADD_O;
								}

								if (patmask & PATM_Q)
								{
									if (expand.num_e[EXP_Q] == 0)
									{	expand.num_e[EXP_Q]++;
									}
									sym_num += expand.num_e[EXP_Q];
									leaves.letter[leaf_num][k++] = ADD_Q;
								}

								if (patmask & PATM_R)
								{
									if (expand.num_e[EXP_R] == 0)
									{	expand.num_e[EXP_R]++;
									}
									sym_num += expand.num_e[EXP_R];
									leaves.letter[leaf_num][k++] = ADD_R;
								}

								if (patmask & PATM_S)
								{
									if (expand.num_e[EXP_S] == 0)
									{	expand.num_e[EXP_S]++;
									}
									sym_num += expand.num_e[EXP_S];
									leaves.letter[leaf_num][k++] = ADD_S;
								}

								if (patmask & PATM_T)
								{
									if (expand.num_e[EXP_T] == 0)
									{	expand.num_e[EXP_T]++;
									}
									sym_num += expand.num_e[EXP_T];
									leaves.letter[leaf_num][k++] = ADD_T;
								}
							}
							leaves.letter[leaf_num][k] = -1;
							leaf_num++;
						}
					}
				}
				if (sym_num >= MAX_SYM - 1)
				{
					patmaskptr = outchar;
					m = dfa_calc(lv_ptr, curr_leaf_num, exp_ptr);
					if (m >= 0)
					{
						min[count] = curr_min_dfa;
						max[count] = PAT_MAX_REPEAT;
						size[count] = m;
						total = MIN((total + (min[count] * size[count])),PAT_MAX_REPEAT);
						total_max = MIN((total_max + (max[count] * size[count])),PAT_MAX_REPEAT);
						lastpatptr = patmaskptr;
						last_infinite = TRUE;
						count++;
					}
					else
					{
						outchar = patmaskptr;
						if (!pat_unwind(&count, lv_ptr, curr_leaf_num, &total, &total_max,
							&min[0], &max[0], &size[0]))
						{
							return ERR_PATMAXLEN;
						}
					}
					dfa = FALSE;
					done = FALSE;
					continue;
				}
				else
				{
					if (last_leaf_code == PATM_STRLIT)
						for (i=0;i < CHAR_CLASSES;i++)
							expand.num_e[i] += exp_temp[i];
				}
			}

			if (split_atom)
			{
				lower_bound = 0;
				upper_bound = PAT_MAX_REPEAT;
				infinite = TRUE;
				split_atom = FALSE;
				done = FALSE;
			}
		}
	}
}
