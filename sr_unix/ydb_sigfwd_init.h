/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_SIGFWD_INIT_H
#define YDB_SIGFWD_INIT_H

/* Macros to check and twiddle bits in the signal forward mask. Note the bits are stored in reverse order within the byte
 * (right to left) but the bytes are stored in order (left to right). This doesn't matter so long as the query does the
 * same thing (which it does).
 */
#define ENABLE_SIGFWD_BIT(SIGNUM)			\
MBSTART {			  			\
	unsigned char	*bptr;				\
	GBLREF unsigned char sigfwdMask[];		\
	assert(0 < SIGNUM);				\
	bptr = &sigfwdMask[0] + ((SIGNUM - 1) / 8);	\
	*bptr |= 1 << ((SIGNUM - 1) & 7);		\
} MBEND

#define DISABLE_SIGFWD_BIT(SIGNUM)			\
MBSTART {			  			\
	unsigned char	*bptr;				\
	GBLREF unsigned char sigfwdMask[];		\
	assert(0 < SIGNUM);				\
	bptr = &sigfwdMask[0] + ((SIGNUM - 1) / 8);	\
	*bptr &= ~(1 << ((SIGNUM - 1) & 7));		\
} MBEND

#define QUERY_SIGFWD_BIT(SIGNUM, RETVAL)					\
MBSTART {			  						\
	unsigned char	*bptr;							\
	GBLREF unsigned char sigfwdMask[];					\
	assert(0 < SIGNUM);							\
	bptr = &sigfwdMask[0] + ((SIGNUM - 1) / 8);				\
	RETVAL = (0 != (*bptr & (1 << ((SIGNUM - 1) & 7)))) ? TRUE : FALSE;	\
} MBEND

/* Used for signal table defined in ydb_sigfwd_init.c */
typedef struct
{
	unsigned char	*signame;
	int		signameLen;
	int		sigvalue;
} signame_value;

void ydb_sigfwd_init(void);
int signal_lookup(unsigned char *signame, int signameLen);
void set_sigfwd_mask(unsigned char *siglist, int siglistLen, boolean_t enableSigs);

#endif /* ifndef YDB_SIGFWD_INIT_H */
