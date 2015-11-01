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
#include "copy.h"
#include "patcode.h"
#include "add_atom.h"

#include "compiler.h"
#include "stringpool.h"

GBLREF spdesc stringpool;
GBLREF unsigned char  *outchar;
LITREF uint4 typemask[256];

short int dfa_calc(leaves,leaf_num,expand)

struct leaf	*leaves;
short int	leaf_num;
struct e_table  *expand;

{
	unsigned char		x,*patmaskptr;
	unsigned char		patcode;
	uint4		patmask;
	short int 		offset[2 + (2 * MAX_SYM)],pos_offset[CHAR_CLASSES],
				a,i,j,k,l,m,n,
				fst[2][2],lst[2][2],
				alpha_num,state_num,node_num,sym_num;

	long int		o;
	struct node		nodes;
	struct st_tb		fpos,states;
	struct trns_tb		d_trans,pos_lis;
	struct c_trns_tb	c_trans;

	patmaskptr = outchar;
	if (leaf_num > 1)
	{
		patcode = PATM_DFA;
		patmask = 0;
		alpha_num = 0;
		node_num = 0;
		state_num = 1;

		leaves->nullable[leaf_num] = FALSE;
		leaves->letter[leaf_num][0] = ADD;
		leaves->letter[leaf_num][1] = -1;

		pos_offset[EXP_N] = 0;
		alpha_num += expand->num_e[EXP_N];
		pos_offset[EXP_P] = alpha_num;

		alpha_num += expand->num_e[EXP_P];
		pos_offset[EXP_L] = alpha_num;

		alpha_num += expand->num_e[EXP_L];
		pos_offset[EXP_U] = alpha_num;

		alpha_num += expand->num_e[EXP_U];
		pos_offset[EXP_C] = alpha_num;

		alpha_num += expand->num_e[EXP_C];
		pos_offset[EXP_B] = alpha_num;

		alpha_num += expand->num_e[EXP_B];
		pos_offset[EXP_D] = alpha_num;

		alpha_num += expand->num_e[EXP_D];
		pos_offset[EXP_F] = alpha_num;

		alpha_num += expand->num_e[EXP_F];
		pos_offset[EXP_G] = alpha_num;

		alpha_num += expand->num_e[EXP_G];
		pos_offset[EXP_H] = alpha_num;

		alpha_num += expand->num_e[EXP_H];
		pos_offset[EXP_I] = alpha_num;

		alpha_num += expand->num_e[EXP_I];
		pos_offset[EXP_J] = alpha_num;

		alpha_num += expand->num_e[EXP_J];
		pos_offset[EXP_K] = alpha_num;

		alpha_num += expand->num_e[EXP_K];
		pos_offset[EXP_M] = alpha_num;

		alpha_num += expand->num_e[EXP_M];
		pos_offset[EXP_O] = alpha_num;

		alpha_num += expand->num_e[EXP_O];
		pos_offset[EXP_Q] = alpha_num;

		alpha_num += expand->num_e[EXP_Q];
		pos_offset[EXP_R] = alpha_num;

		alpha_num += expand->num_e[EXP_R];
		pos_offset[EXP_S] = alpha_num;

		alpha_num += expand->num_e[EXP_S];
		pos_offset[EXP_T] = alpha_num;

		alpha_num += expand->num_e[EXP_T];

		memset(&nodes.nullable[0],0,sizeof(struct node));
		memset(&fpos.s[0][0],0,sizeof(struct st_tb));
		memset(&states.s[0][0],0,sizeof(struct st_tb));
		memset(&d_trans.t[0][0],128,sizeof(struct trns_tb));
		memset(&pos_lis.t[0][0],128,sizeof(struct trns_tb));
		memset(&c_trans.c[0],0,sizeof(short int) * MAX_SYM * 2);
		memset(&offset[0],0,sizeof(short int) * ((2 * MAX_SYM) + 2));
		memset(&fst[0][0], 0, sizeof(fst));
		memset(&lst[0][0], 0, sizeof(lst));

		a = 0;
		l = 0;
		m = 0;
		nodes.nullable[0] = leaves->nullable[0] & leaves->nullable[1];
		states.s[state_num][a]=TRUE;

		for (i=0;leaves->letter[0][i] >= 0;i++)
		{
			if (leaves->letter[0][i] < ADD)
			{
				fpos.s[a][a+1] = TRUE;
				lst[FST][FST] = a;
				lst[FST][LST] = a;
				assert(leaves->letter[0][i] >= 0 && leaves->letter[0][i] < sizeof(typemask));

				/* Note: the following code makes a pattern-compile-time reference to typemask.
				   This is likely to be inadequate.
				*/
				o = typemask[leaves->letter[0][i]];
				switch (o)
				{
					case PATM_N: o = EXP_N;
						     break;
					case PATM_P: o = EXP_P;
						     break;
					case PATM_L: o = EXP_L;
						     break;
					case PATM_U: o = EXP_U;
						     break;
					case PATM_C: o = EXP_C;
						     break;
					case PATM_B: o = EXP_B;
						     break;
					case PATM_D: o = EXP_D;
						     break;
					case PATM_F: o = EXP_F;
						     break;
					case PATM_G: o = EXP_G;
						     break;
					case PATM_H: o = EXP_H;
						     break;
					case PATM_I: o = EXP_I;
						     break;
					case PATM_J: o = EXP_J;
						     break;
					case PATM_K: o = EXP_K;
						     break;
					case PATM_M: o = EXP_M;
						     break;
					case PATM_O: o = EXP_O;
						     break;
					case PATM_Q: o = EXP_Q;
						     break;
					case PATM_R: o = EXP_R;
						     break;
					case PATM_S: o = EXP_S;
						     break;
					case PATM_T: o = EXP_T;
						     break;
				}
				for (j = 1;expand->meta_c[o][j] != leaves->letter[0][i];j++)
					;
				for (k = 0;pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
					;
				pos_lis.t[pos_offset[o] + j][k]=a;
				a++;
			}
			else
			{
				o = leaves->letter[0][i] - ADD;
				switch (o)
				{
					case PATM_N: o = EXP_N;
						     x = expand->num_e[EXP_N];
						     break;
					case PATM_P: o = EXP_P;
						     x = expand->num_e[EXP_P];
						     break;
					case PATM_L: o = EXP_L;
						     x = expand->num_e[EXP_L];
						     break;
					case PATM_U: o = EXP_U;
						     x = expand->num_e[EXP_U];
						     break;
					case PATM_C: o = EXP_C;
						     x = expand->num_e[EXP_C];
						     break;
					case PATM_B: o = EXP_B;
						     x = expand->num_e[EXP_B];
						     break;
					case PATM_D: o = EXP_D;
						     x = expand->num_e[EXP_D];
						     break;
					case PATM_F: o = EXP_F;
						     x = expand->num_e[EXP_F];
						     break;
					case PATM_G: o = EXP_G;
						     x = expand->num_e[EXP_G];
						     break;
					case PATM_H: o = EXP_H;
						     x = expand->num_e[EXP_H];
						     break;
					case PATM_I: o = EXP_I;
						     x = expand->num_e[EXP_I];
						     break;
					case PATM_J: o = EXP_J;
						     x = expand->num_e[EXP_J];
						     break;
					case PATM_K: o = EXP_K;
						     x = expand->num_e[EXP_K];
						     break;
					case PATM_M: o = EXP_M;
						     x = expand->num_e[EXP_M];
						     break;
					case PATM_O: o = EXP_O;
						     x = expand->num_e[EXP_O];
						     break;
					case PATM_Q: o = EXP_Q;
						     x = expand->num_e[EXP_Q];
						     break;
					case PATM_R: o = EXP_R;
						     x = expand->num_e[EXP_R];
						     break;
					case PATM_S: o = EXP_S;
						     x = expand->num_e[EXP_S];
						     break;
					case PATM_T: o = EXP_T;
						     x = expand->num_e[EXP_T];
						     break;
					default:     x = 0;
				}
				for (j = 0;j < x;j++)
				{
					states.s[state_num][a] = TRUE;
					fst[FST][LST] = a;
					lst[FST][LST] = a;
					for (k = 0; pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
						;
					pos_lis.t[pos_offset[o] + j][k] = a;
					a++;
				}
			}
		}

		fst[LST][FST] = a;
		fst[LST][LST] = a;
		lst[LST][FST] = a;

		if(!leaves->nullable[1])
		{	nodes.last[0][a] = TRUE;
			m = a;
		}

		for (i = 0;leaves->letter[1][i] >= 0;i++)
		{
			if (leaves->letter[1][i] < ADD)
			{
				fpos.s[a][a+1] = TRUE;
				lst[LST][FST] = a;
				lst[LST][LST] = a;
				assert(leaves->letter[1][i] >= 0 && leaves->letter[1][i] < sizeof(typemask));

				/* Note: the following code makes a pattern-compile-time reference to typemask.
				   This is likely to be inadequate.
				*/
				o = typemask[leaves->letter[1][i]];
				switch (o)
				{
					case PATM_N: o = EXP_N;
						     break;
					case PATM_P: o = EXP_P;
						     break;
					case PATM_L: o = EXP_L;
						     break;
					case PATM_U: o = EXP_U;
						     break;
					case PATM_C: o = EXP_C;
						     break;
					case PATM_B: o = EXP_B;
						     break;
					case PATM_D: o = EXP_D;
						     break;
					case PATM_F: o = EXP_F;
						     break;
					case PATM_G: o = EXP_G;
						     break;
					case PATM_H: o = EXP_H;
						     break;
					case PATM_I: o = EXP_I;
						     break;
					case PATM_J: o = EXP_J;
						     break;
					case PATM_K: o = EXP_K;
						     break;
					case PATM_M: o = EXP_M;
						     break;
					case PATM_O: o = EXP_O;
						     break;
					case PATM_Q: o = EXP_Q;
						     break;
					case PATM_R: o = EXP_R;
						     break;
					case PATM_S: o = EXP_S;
						     break;
					case PATM_T: o = EXP_T;
						     break;
					default:     o = 0;
				}
				for (j = 1; expand->meta_c[o][j] != leaves->letter[1][i];j++)
					;
				for (k = 0; pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
					;
				pos_lis.t[pos_offset[o] + j][k] = a;
				a++;
			}
			else
			{
				o = leaves->letter[1][i] - ADD;
				switch (o)
				{
					case PATM_N: o = EXP_N;
						     x = expand->num_e[EXP_N];
						     break;
					case PATM_P: o = EXP_P;
						     x = expand->num_e[EXP_P];
						     break;
					case PATM_L: o = EXP_L;
						     x = expand->num_e[EXP_L];
						     break;
					case PATM_U: o = EXP_U;
						     x = expand->num_e[EXP_U];
						     break;
					case PATM_C: o = EXP_C;
						     x = expand->num_e[EXP_C];
						     break;
					case PATM_B: o = EXP_B;
						     x = expand->num_e[EXP_B];
						     break;
					case PATM_D: o = EXP_D;
						     x = expand->num_e[EXP_D];
						     break;
					case PATM_F: o = EXP_F;
						     x = expand->num_e[EXP_F];
						     break;
					case PATM_G: o = EXP_G;
						     x = expand->num_e[EXP_G];
						     break;
					case PATM_H: o = EXP_H;
						     x = expand->num_e[EXP_H];
						     break;
					case PATM_I: o = EXP_I;
						     x = expand->num_e[EXP_I];
						     break;
					case PATM_J: o = EXP_J;
						     x = expand->num_e[EXP_J];
						     break;
					case PATM_K: o = EXP_K;
						     x = expand->num_e[EXP_K];
						     break;
					case PATM_M: o = EXP_M;
						     x = expand->num_e[EXP_M];
						     break;
					case PATM_O: o = EXP_O;
						     x = expand->num_e[EXP_O];
						     break;
					case PATM_Q: o = EXP_Q;
						     x = expand->num_e[EXP_Q];
						     break;
					case PATM_R: o = EXP_R;
						     x = expand->num_e[EXP_R];
						     break;
					case PATM_S: o = EXP_S;
						     x = expand->num_e[EXP_S];
						     break;
					case PATM_T: o = EXP_T;
						     x = expand->num_e[EXP_T];
						     break;
					default:     x = 0;
				}
				for (j = 0;j < x;j++)
				{
					nodes.last[0][a] = TRUE;
					fst[LST][LST] = a;
					lst[LST][LST] = a;
					for (k = 0; pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
						;
					pos_lis.t[pos_offset[o] + j][k] = a;
					a++;
				}
			}
		}

		if (leaves->nullable[0])
		{
			for(j = lst[FST][FST];j <= lst[FST][LST];j++)
			{
				for(k = fst[FST][FST];k <= fst[FST][LST];k++)
				{
					fpos.s[j][k] = TRUE;
				}
			}

			for (j = fst[LST][FST];j <= fst[LST][LST];j++)
			{
				states.s[state_num][j] = TRUE;
			}
		}

		if (leaves->nullable[1])
		{
			nodes.last[0][a - 1] = TRUE;
			for(j = lst[LST][FST]; j <= lst[LST][LST];j++)
			{
				for(k = fst[LST][FST]; k <= fst[LST][LST];k++)
				{
					fpos.s[j][k] = TRUE;
				}
			}

			for (j = lst[FST][FST]; j <=  lst[FST][LST];j++)
			{
				nodes.last[0][j] = TRUE;
			}
			m = a;
		}

		for(j = lst[FST][FST]; j <= lst[FST][LST];j++)
		{
			for(k = fst[LST][FST]; k <= fst[LST][LST];k++)
			{
				fpos.s[j][k] = TRUE;
			}
		}

		if (!leaves->nullable[1])
		{	l = lst[LST][FST];
		}

		node_num++;

		for ( ;node_num < leaf_num ;node_num++)
		{
			nodes.nullable[node_num] = nodes.nullable[node_num - 1] &
						    leaves->nullable[node_num + 1];

			if (leaves->nullable[node_num + 1])
			{
				for (i = 0;i < a;i++)
				{
					nodes.last[node_num][i] = nodes.last[node_num - 1][i];
				}
			}

			else
			{	nodes.last[node_num][a] = TRUE;
				m = a;
			}

			fst[LST][FST] = a;
			fst[LST][LST] = a;
			lst[LST][FST] = a;

			for (i = 0;leaves->letter[node_num + 1][i] >= 0;i++)
			{
				if (leaves->letter[node_num + 1][i] < ADD)
				{
					fpos.s[a][a + 1] = TRUE;
					lst[LST][FST] = a;
					lst[LST][LST] = a;
					assert(leaves->letter[node_num + 1][i] >= 0 &&
					       leaves->letter[node_num + 1][i] < sizeof(typemask));
					o = typemask[leaves->letter[node_num + 1][i]];
					switch (o)
					{
						case PATM_N: o = EXP_N;
							     break;
						case PATM_P: o = EXP_P;
							     break;
						case PATM_L: o = EXP_L;
							     break;
						case PATM_U: o = EXP_U;
							     break;
						case PATM_C: o = EXP_C;
							     break;
						case PATM_B: o = EXP_B;
							     break;
						case PATM_D: o = EXP_D;
							     break;
						case PATM_F: o = EXP_F;
							     break;
						case PATM_G: o = EXP_G;
							     break;
						case PATM_H: o = EXP_H;
							     break;
						case PATM_I: o = EXP_I;
							     break;
						case PATM_J: o = EXP_J;
							     break;
						case PATM_K: o = EXP_K;
							     break;
						case PATM_M: o = EXP_M;
							     break;
						case PATM_O: o = EXP_O;
							     break;
						case PATM_Q: o = EXP_Q;
							     break;
						case PATM_R: o = EXP_R;
							     break;
						case PATM_S: o = EXP_S;
							     break;
						case PATM_T: o = EXP_T;
							     break;
						default:     o = 0;
					}
					for (j = 1;expand->meta_c[o][j] != leaves->letter[node_num + 1][i];j++)
						;
					for (k = 0;pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
						;
					pos_lis.t[pos_offset[o] + j][k] = a;
					a++;
				}
				else
				{
					o = leaves->letter[node_num + 1][i] - ADD;
					switch (o)
					{
						case PATM_N: o = EXP_N;
							     x = expand->num_e[EXP_N];
							     break;
						case PATM_P: o = EXP_P;
							     x = expand->num_e[EXP_P];
							     break;
						case PATM_L: o = EXP_L;
							     x = expand->num_e[EXP_L];
							     break;
						case PATM_U: o = EXP_U;
							     x = expand->num_e[EXP_U];
							     break;
						case PATM_C: o = EXP_C;
							     x = expand->num_e[EXP_C];
							     break;
						case PATM_B: o = EXP_B;
							     x = expand->num_e[EXP_B];
							     break;
						case PATM_D: o = EXP_D;
							     x = expand->num_e[EXP_D];
							     break;
						case PATM_F: o = EXP_F;
							     x = expand->num_e[EXP_F];
							     break;
						case PATM_G: o = EXP_G;
							     x = expand->num_e[EXP_G];
							     break;
						case PATM_H: o = EXP_H;
							     x = expand->num_e[EXP_H];
							     break;
						case PATM_I: o = EXP_I;
							     x = expand->num_e[EXP_I];
							     break;
						case PATM_J: o = EXP_J;
							     x = expand->num_e[EXP_J];
							     break;
						case PATM_K: o = EXP_K;
							     x = expand->num_e[EXP_K];
							     break;
						case PATM_M: o = EXP_M;
							     x = expand->num_e[EXP_M];
							     break;
						case PATM_O: o = EXP_O;
							     x = expand->num_e[EXP_O];
							     break;
						case PATM_Q: o = EXP_Q;
							     x = expand->num_e[EXP_Q];
							     break;
						case PATM_R: o = EXP_R;
							     x = expand->num_e[EXP_R];
							     break;
						case PATM_S: o = EXP_S;
							     x = expand->num_e[EXP_S];
							     break;
						case PATM_T: o = EXP_T;
							     x = expand->num_e[EXP_T];
							     break;
						default:     x = 0;
					}
					for (j = 0;j < x;j++)
					{
						nodes.last[node_num][a] = TRUE;
						if (nodes.nullable[node_num - 1])
						{	states.s[state_num][a] = TRUE;
						}
						fst[LST][LST] = a;
						lst[LST][LST] = a;
						for (k = 0; pos_lis.t[pos_offset[o] + j][k] >= 0;k++)
							;
						pos_lis.t[pos_offset[o] + j][k] = a;
						a++;
					}
				}
			}

			if (nodes.nullable[node_num - 1])
			{
				for (j = fst[LST][FST];j <= fst[LST][LST];j++)
				{
				states.s[state_num][j] = TRUE;
				}
			}
			if (leaves->nullable[node_num + 1])
			{
				nodes.last[node_num][a - 1] = TRUE;
				for(j = lst[LST][FST];j <= lst[LST][LST];j++)
				{
					for(k = fst[LST][FST];k <= fst[LST][LST];k++)
					{
						fpos.s[j][k] = TRUE;
					}
				}
				m = a;
			}

			for(j = l;j < m;j++)
			{
				for(k = fst[LST][FST]; k <= fst[LST][LST];k++)
				{
					if (nodes.last[node_num - 1][j])
					{	fpos.s[j][k] = TRUE;
					}
				}
			}

			if (!leaves->nullable[node_num + 1])
			{	l = lst[LST][FST];
			}
		}

		sym_num = a;
		state_num++;

		o = 1;
		while (o < state_num)
		{
			a = 0;
			offset[o + 1]++;
			offset[o + 1] += offset[o];
			for (i = 0;i < CHAR_CLASSES;i++)
			{
				if (expand->num_e[i] > 0)
				{
					for (j = 0;j < expand->num_e[i];j++)
					{
						m = 0;
						while (pos_lis.t[a + j][m] >= 0)
						{
							n = pos_lis.t[a + j][m];
							if (states.s[o][n])
							{
								for(l = 0;l <= sym_num;l++)
								{
									states.s[state_num][l] |=  fpos.s[n][l];
								}
							}
							m++;
						}
						for (k = 0; memcmp(states.s[k],states.s[state_num],sym_num+1) && k<state_num;k++)
							;

						if (k > 0)
						{
							if (j == 0)
							{
								d_trans.t[o][a] = k;

								for (l = 0 ;l < c_trans.c[o] && c_trans.trns[o][l] != k ;l++)
									;

								if (l == c_trans.c[o])
								{
									switch (i)
									{
										case EXP_N: c_trans.p_msk[o][l] = PATM_N;
											    break;
										case EXP_P: c_trans.p_msk[o][l] = PATM_P;
											    break;
										case EXP_L: c_trans.p_msk[o][l] = PATM_L;
											    break;
										case EXP_U: c_trans.p_msk[o][l] = PATM_U;
											    break;
										case EXP_C: c_trans.p_msk[o][l] = PATM_C;
											    break;
										case EXP_B: c_trans.p_msk[o][l] = PATM_B;
											    break;
										case EXP_D: c_trans.p_msk[o][l] = PATM_D;
											    break;
										case EXP_F: c_trans.p_msk[o][l] = PATM_F;
											    break;
										case EXP_G: c_trans.p_msk[o][l] = PATM_G;
											    break;
										case EXP_H: c_trans.p_msk[o][l] = PATM_H;
											    break;
										case EXP_I: c_trans.p_msk[o][l] = PATM_I;
											    break;
										case EXP_J: c_trans.p_msk[o][l] = PATM_J;
											    break;
										case EXP_K: c_trans.p_msk[o][l] = PATM_K;
											    break;
										case EXP_M: c_trans.p_msk[o][l] = PATM_M;
											    break;
										case EXP_O: c_trans.p_msk[o][l] = PATM_O;
											    break;
										case EXP_Q: c_trans.p_msk[o][l] = PATM_Q;
											    break;
										case EXP_R: c_trans.p_msk[o][l] = PATM_R;
											    break;
										case EXP_S: c_trans.p_msk[o][l] = PATM_S;
											    break;
										case EXP_T: c_trans.p_msk[o][l] = PATM_T;
											    break;
									}
									c_trans.trns[o][l] = k;
									offset[o + 1] += 2;
									c_trans.c[o]++;
								}
								else
								{
									switch (i)
									{
										case EXP_N: c_trans.p_msk[o][l] |= PATM_N;
											    break;
										case EXP_P: c_trans.p_msk[o][l] |= PATM_P;
											    break;
										case EXP_L: c_trans.p_msk[o][l] |= PATM_L;
											    break;
										case EXP_U: c_trans.p_msk[o][l] |= PATM_U;
											    break;
										case EXP_C: c_trans.p_msk[o][l] |= PATM_C;
											    break;
										case EXP_B: c_trans.p_msk[o][l] |= PATM_B;
											    break;
										case EXP_D: c_trans.p_msk[o][l] |= PATM_D;
											    break;
										case EXP_F: c_trans.p_msk[o][l] |= PATM_F;
											    break;
										case EXP_G: c_trans.p_msk[o][l] |= PATM_G;
											    break;
										case EXP_H: c_trans.p_msk[o][l] |= PATM_H;
											    break;
										case EXP_I: c_trans.p_msk[o][l] |= PATM_I;
											    break;
										case EXP_J: c_trans.p_msk[o][l] |= PATM_J;
											    break;
										case EXP_K: c_trans.p_msk[o][l] |= PATM_K;
											    break;
										case EXP_M: c_trans.p_msk[o][l] |= PATM_M;
											    break;
										case EXP_O: c_trans.p_msk[o][l] |= PATM_O;
											    break;
										case EXP_Q: c_trans.p_msk[o][l] |= PATM_Q;
											    break;
										case EXP_R: c_trans.p_msk[o][l] |= PATM_R;
											    break;
										case EXP_S: c_trans.p_msk[o][l] |= PATM_S;
											    break;
										case EXP_T: c_trans.p_msk[o][l] |= PATM_T;
									}
								}
							}

							else if (d_trans.t[o][a] != k)
							{
								d_trans.t[o][a + j] = k;
								offset[o + 1] += 3;
							}

							if (k == state_num)
							{	state_num++;
							}
							else
							{	memset(states.s[state_num],0,sym_num + 1);
							}
						}
					}
					a += expand->num_e[i];
				}
			}

			o++;
		}

		outchar += offset[state_num] + PATSTRLIT;
		if ((outchar - stringpool.free > MAX_DFA_SPACE) ||
		    (offset[state_num] + PATSTRLIT > MAX_PATTERN_LENGTH/2))
			return -1;

		*patmaskptr++ = PATM_DFA;
		*patmaskptr++ = (unsigned char) offset[state_num];

		for (o = 1; o < state_num;o++)
		{
			a = 0;
			for (j = 0;j < CHAR_CLASSES;j++)
			{
				if (expand->num_e[j] > 1)
				{
					for (k = 1;k < expand->num_e[j];k++)
					{
						if (d_trans.t[o][a + k] >= 0)
						{
							*patmaskptr++ = PATM_STRLIT;
							*patmaskptr++ = expand->meta_c[j][k];
							*patmaskptr++ = (unsigned char) offset[d_trans.t[o][a + k]];
						}
					}
				}
				a += expand->num_e[j];
			}

			for (j = 0 ;j < c_trans.c[o] ;j++)
			{
				*patmaskptr++ = c_trans.p_msk[o][j];
				*patmaskptr++ = (unsigned char) offset[c_trans.trns[o][j]];
			}

			if (states.s[o][sym_num])
			{	*patmaskptr++ = PATM_ACS;
			}
			else
			{	*patmaskptr++ = PATM_DFA;
			}
		}
		return 1;
	}
	else
	{
		patcode = patmask = 0;
		outchar += PATSIZE;
		i = 1;

		if (leaves->letter[0][0] < ADD)
		{
			patcode = PATM_STRLIT;
			for (i = 0;leaves->letter[0][i] >= 0;i++)
				;
			outchar += i + 1;
		}
		else
		{
			for (j = 0;leaves->letter[0][j] >= 0;j++)
				patmask |= (unsigned char)leaves->letter[0][j];
		}

		if (outchar - stringpool.free > MAX_PATTERN_LENGTH)
		{
			return -1;
		}

		if (patcode < PATM_STRLIT && patmask & PATM_I18NFLAGS)
		{
			PUT_LONG(patmaskptr, patmask);
			*patmaskptr |= PATM_USRDEF;
			patmaskptr += sizeof(int4);
		}
		else
			*patmaskptr++ = patcode | patmask;

		if (patcode ==  PATM_STRLIT)
		{
			*patmaskptr++ = (unsigned char) i;
			for (j = 0;j < i;j++)
			{
				*patmaskptr++ = (unsigned char) leaves->letter[0][j];
			}
		}
		return i;
	}
}
