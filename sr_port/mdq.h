/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDQ_H_DEFINED
#define MDQ_H_DEFINED

/* Define basic working macros for queue management of doubly linked list is defined using elements "N.fl" and "N.bl".
 * The DSRINS insert at tail rather than head and so work FIFO rather than LIFO with DQLOOP and associated macros
 */

/* Loop through a linked list given any element as the start (Q) and a var to use as loop incrementer */
#define DQLOOP(Q, N, I) for (I = (Q)->N.fl; (I) != (Q); (I) = (I)->N.fl)

/* Initialize an element */
#define DQINIT(Q, N) ((Q)->N.fl = (Q)->N.bl = (Q))

/* Delete one element "X" from the doubly linked list */
#define DQDEL(X, N) ((X)->N.bl->N.fl = (X)->N.fl, (X)->N.fl->N.bl = (X)->N.bl)

/* Delete a doubly-linked list of elements from "Q->N.fl" to "X->N.bl" (i.e. everything in between "Q" and "X" excluding them) */
#define DQDELCHAIN(Q, X, N) ((Q)->N.fl = (X), (X)->N.bl = (Q))

/* Insert one element "X" in between "Q" and "Q->N.fl" */
#define DQINS(Q, N, X) ((X)->N.fl = (Q)->N.fl, (X)->N.bl = (Q), (Q)->N.fl = (X), ((X)->N.fl)->N.bl = (X))

/* Insert one element "X" in between "Q" and "Q->N.bl" */
#define DQRINS(Q, N, X) ((X)->N.bl = (Q)->N.bl, (X)->N.fl = (Q), (Q)->N.bl = (X), ((X)->N.bl)->N.fl = (X))

/* Insert a doubly-linked list of elements from "X->N.fl" to "X->N.bl" in between "Q" and "Q->N.fl" */
#define DQADD(Q, X, N) ((Q)->N.fl->N.bl = (X)->N.bl, (X)->N.bl->N.fl = (Q)->N.fl, (Q)->N.fl = (X)->N.fl, (X)->N.fl->N.bl = (Q))

/* Define macros actually used which if #define DEBUG_TRIPLES, adds debugging information. Since these macros are
 * used in several different queue types and since these debugging macros only work for the exorder field in triples,
 * the test to see if should do debugging is not elegant but it does allow decent debugging. Note those macros without
 * debugging are statically defined.
 */

#define dqloop(Q, N, I) DQLOOP(Q, N, I)
#define dqinit(Q, N)	DQINIT(Q, N)

<<<<<<< HEAD
/*#define DEBUG_TRIPLES / * Uncomment this to do triple debugging, which is also tied to ydb_dbglvl, as of this writing: 0x4000 */
=======
/* #define DEBUG_TRIPLES / * Uncomment this to do triple debugging, which is also tied to gtmdbglvl, as of this writing: 0x4000 */
>>>>>>> 91552df2... GT.M V6.3-009
#ifndef DEBUG_TRIPLES
#  define dqdel(X, N)		DQDEL(X, N)
#  define dqdelchain(Q, X, N)	DQDELCHAIN(Q, X, N)
#  define dqins(Q, N, X)	DQINS(Q, N, X)
#  define dqrins(Q, N, X)	DQRINS(Q, N, X)
#  define dqadd(Q, X, N)	DQADD(Q, X, N)
#  define CHKTCHAIN(Q, N, B)
#else
#  include "compiler.h"
#  include "gtm_string.h"
#  include "gtmdbglvl.h"
<<<<<<< HEAD
GBLREF	uint4		ydbDebugLevel;
/* q: head of queue to check; n: the name of queue; b: whether to check main exorder (from curtchain) and any expr_start queue */
#  define CHKTCHAIN(q, n, b)								\
=======
GBLREF	uint4		gtmDebugLevel;
/* Q: head of queue to check; N: the name of queue; B: whether to check main exorder (from curtchain) and any expr_start queue */
#  define CHKTCHAIN(Q, N, B)								\
>>>>>>> 91552df2... GT.M V6.3-009
MBSTART {										\
	triple	*c;									\
	DCL_THREADGBL_ACCESS;								\
											\
	SETUP_THREADGBL_ACCESS;								\
					/* memcmp() is fast and 3 chars sufficient */	\
<<<<<<< HEAD
 	if ((ydbDebugLevel & GDL_DebugCompiler) && (0 == memcmp(#n, "exorder", 3)))	\
	{										\
		if ((triple *)-1 != (triple *)q) /* to avoid post-checking deletes */	\
			chktchain((triple *)q);						\
		if (b)									\
=======
 	if ((gtmDebugLevel & GDL_DebugCompiler) && (0 == memcmp(#N, "exorder", 3)))	\
	{										\
		if ((triple *)-1 != (triple *)(Q)) /* to avoid post-checking deletes */	\
			chktchain((triple *)(Q));					\
		if (B)									\
>>>>>>> 91552df2... GT.M V6.3-009
		{									\
			c = TREF(curtchain);						\
			chktchain(c);		/* this might be redundant, or not! */	\
			c = TREF(expr_start_orig);					\
			if ((NULL != c) && (c != TREF(expr_start)))			\
				chktchain(c);	/* this extra has been rewarding */	\
		}									\
	}										\
} MBEND
<<<<<<< HEAD
#  define dqdel(x, n)			\
MBSTART {				\
	CHKTCHAIN((x), n, FALSE);	\
	DQDEL((x), n);			\
	CHKTCHAIN(-1, n, TRUE);		\
} MBEND
#  define dqdelchain(q, x, n)		\
MBSTART {				\
	CHKTCHAIN((q), n, FALSE);	\
	DQDELCHAIN((q), (x), n);	\
	CHKTCHAIN((q), n, TRUE);	\
=======
#  define dqdel(X, N)		\
MBSTART {			\
	CHKTCHAIN(X, N, FALSE);	\
	DQDEL(X, N);		\
	CHKTCHAIN(-1, N, TRUE);	\
} MBEND
#  define dqdelchain(Q, X, N)	\
MBSTART {			\
	CHKTCHAIN(Q, N, FALSE);	\
	DQDELCHAIN(Q, X, N);	\
	CHKTCHAIN(Q, N, TRUE);	\
>>>>>>> 91552df2... GT.M V6.3-009
} MBEND
#  define dqins(Q, N, X)	\
MBSTART {			\
	CHKTCHAIN(Q, N, FALSE);	\
	DQINS(Q, N, X);		\
	CHKTCHAIN(Q, N, TRUE);	\
} MBEND
#  define dqrins(Q, N, X)	\
MBSTART {			\
	CHKTCHAIN(Q, N, FALSE);	\
	DQRINS(Q, N, X);	\
	CHKTCHAIN(Q, N, TRUE);	\
} MBEND
#  define dqadd(Q, X, N)	\
MBSTART {			\
	CHKTCHAIN(Q, N, FALSE);	\
	CHKTCHAIN(X, N, FALSE);	\
	DQADD(Q, X, N);		\
	CHKTCHAIN(Q, N, TRUE);	\
} MBEND
#endif

#endif
