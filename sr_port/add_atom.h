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

#ifndef __ADD_ATOM_H__
#define __ADD_ATOM_H__

#define CHAR_CLASSES 19
#define ADD     256
#define ADD_N	257
#define ADD_P	258
#define ADD_L	260
#define ADD_U	264
#define ADD_C	272
#define ADD_B	(256+(1<<5))
#define ADD_D	(256+(1<<6))
#define ADD_F	(256+(1<<7))
#define ADD_G	(256+(1<<8))
#define ADD_H	(256+(1<<9))
#define ADD_I	(256+(1<<10))
#define ADD_J	(256+(1<<11))
#define ADD_K	(256+(1<<12))
#define ADD_M	(256+(1<<13))
#define ADD_O	(256+(1<<14))
#define ADD_Q	(256+(1<<15))
#define ADD_R	(256+(1<<16))
#define ADD_S	(256+(1<<17))
#define ADD_T	(256+(1<<18))

#define EXP_N	0
#define EXP_P	1
#define EXP_L	2
#define EXP_U	3
#define EXP_C	4
#define EXP_B	5
#define EXP_D	6
#define EXP_F	7
#define EXP_G	8
#define EXP_H	9
#define EXP_I	10
#define EXP_J	11
#define EXP_K	12
#define EXP_M	13
#define EXP_O	14
#define EXP_Q	15
#define EXP_R	16
#define EXP_S	17
#define EXP_T	18

#define MAX_SYM  16
#define FST	0
#define LST	1

#define MAX_DFA_STRLEN 6
#define MAX_DFA_REP    10

struct leaf {
		bool		nullable[MAX_SYM];
		short int	letter[MAX_SYM][MAX_DFA_STRLEN + 1];
	    };

struct node {
		bool		nullable[MAX_SYM];
		bool		last[MAX_SYM][MAX_SYM];
	    };

struct e_table {
		short int 	meta_c[CHAR_CLASSES][26],
				num_e[CHAR_CLASSES];
		};

struct st_tb {
		bool		s[2 * MAX_SYM][CHAR_CLASSES];
	     };

struct trns_tb {
		short int	t[2 * MAX_SYM][CHAR_CLASSES];
	       };

struct c_trns_tb {
			short int	c[2 * MAX_SYM];
			unsigned char	p_msk[2 * MAX_SYM][CHAR_CLASSES];
			short int 	trns[2 * MAX_SYM][CHAR_CLASSES];
		};


bool add_atom(short int *count, unsigned char patcode, uint4 patmask,
	void *strlit_buff, unsigned char strlen, bool infinite, short int *min,
	short int *max, short int *size, short int *total, short int *total_max, int lower_bound,
	int upper_bound);
short int dfa_calc(struct leaf *leaves, short int leaf_num, struct e_table *expand);
bool pat_unwind(short int *count, struct leaf *leaves, short int leaf_num, short int *total,
	short int *total_max, short int min[], short int max[], short int size[]);


#endif
