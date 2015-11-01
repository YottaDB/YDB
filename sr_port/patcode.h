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

/* pattern operator constants */

#define PATM_N 1
#define PATM_P 2
#define PATM_L 4
#define PATM_U 8
#define PATM_A 12
#define PATM_C 16
#define PATM_E 31
/* Bits 5, 6, and 7 are reserved for PATM_USRDEF, PATM_STRLIT, et al. */
#define PATM_B (1 << 8)
#define PATM_D (1 << 9)
#define PATM_F (1 << 10)
#define PATM_G (1 << 11)
#define PATM_H (1 << 12)
#define PATM_I (1 << 13)
#define PATM_J (1 << 14)
#define PATM_K (1 << 15)
#define PATM_M (1 << 16)
#define PATM_O (1 << 17)
#define PATM_Q (1 << 18)
#define PATM_R (1 << 19)
#define PATM_S (1 << 20)
#define PATM_T (1 << 21)

#define PATM_USRDEF 32
#define PATM_STRLIT 128
#define PATM_DFA 254
#define PATM_ACS 255
#define PATM_SHORTFLAGS	0x1F		/* original 5 flags */
#define PATM_LONGFLAGS	0x1FFF1F	/* 18 flags */
#define PATM_I18NFLAGS	0x1FFF00	/* flags introduced with I18N */

#define PAT_MAX_REPEAT 32767
#define MAX_PATTERN_LENGTH 256
#define MAX_PATOBJ_LENGTH MAX_PATTERN_LENGTH + 3*MAX_PATTERN_ATOMS*sizeof(short int) + 3*sizeof(short int)
#define MAX_DFA_SPACE 170
#define MAX_PATTERN_ATOMS 50

#define PATSIZE sizeof(unsigned char)
#define PATSTRLIT 2 * sizeof(unsigned char)

#define PATENTS 256

typedef struct pattern_struct {
	struct pattern_struct	*flink;
	uint4		        *typemask;
	short			namlen;
	char			name[2];
} pattern;

int do_pattern(mval *str, mval *pat);
int getpattabnam(mstr *outname);
int initialize_pattern_table(void);
int load_pattern_table(int name_len,char *file_name);
void  setpattab(mstr *table_name,int *result);
void genpat(mstr *input, mval *patbuf);
int do_patfixed(mval *str, mval *pat);

