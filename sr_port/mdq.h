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

#ifndef MDQ_H_DEFINED
#define HDQ_H_DEFINED

/* Define basic working macros for queue management of doubly linked list is defined using elements "n.fl" and "n.bl".
 * The DSRINS insert at tail rather than head and so work FIFO rather than LIFO with DQLOOP and associated macros
 */

/* Loop through a linked list given any element as the start (q) and a var to use as loop incrementer */
#define DQLOOP(q, n, i) for (i = (q)->n.fl; (i) != (q); (i) = (i)->n.fl)

/* Initialize an element */
#define DQINIT(q, n) ((q)->n.fl = (q)->n.bl = (q))

/* Delete one element "x" from the doubly linked list */
#define DQDEL(x, n) ((x)->n.bl->n.fl = (x)->n.fl, (x)->n.fl->n.bl = (x)->n.bl)

/* Delete a doubly-linked list of elements from "q->n.fl" to "x->n.bl" (i.e. everything in between "q" and "x" excluding them) */
#define DQDELCHAIN(q, x, n) ((q)->n.fl = (x), (x)->n.bl = (q))

/* Insert one element "x" in between "q" and "q->n.fl" */
#define DQINS(q, n, x) ((x)->n.fl = (q)->n.fl, (x)->n.bl = (q), (q)->n.fl = (x), ((x)->n.fl)->n.bl = (x))

/* Insert one element "x" in between "q" and "q->n.bl" */
#define DQRINS(q, n, x) ((x)->n.bl = (q)->n.bl, (x)->n.fl = (q), (q)->n.bl = (x), ((x)->n.bl)->n.fl = (x))

/* Insert a doubly-linked list of elements from "x->n.fl" to "x->n.bl" in between "q" and "q->n.fl" */
#define DQADD(q, x, n) ((q)->n.fl->n.bl = (x)->n.bl, (x)->n.bl->n.fl = (q)->n.fl, (q)->n.fl = (x)->n.fl, (x)->n.fl->n.bl = (q))

/* Define macros actually used which if #define DEBUG_TRIPLES, adds debugging information. Since these macros are
 * used in several different queue types and since these debugging macros only work for the exorder field in triples,
 * the test to see if should do debugging is not elegant but it does allow decent debugging. Note those macros without
 * debugging are statically defined.
 */

#define dqloop(q, n, i) DQLOOP(q, n, i)
#define dqinit(q, n)	DQINIT(q, n)

/*#define DEBUG_TRIPLES / * Uncomment this to do triple debugging, which is also tied to gtmdbglvl, as of this writing: 0x4000 */
#ifndef DEBUG_TRIPLES
#  define dqdel(x, n)		DQDEL(x, n)
#  define dqdelchain(q, x, n)	DQDELCHAIN(q, x, n)
#  define dqins(q, n, x)	DQINS(q, n, x)
#  define dqrins(q, n, x)	DQRINS(q, n, x)
#  define dqadd(q, x, n)	DQADD(q, x, n)
#  define CHKTCHAIN(q, n, b)
#else
#  include "compiler.h"
#  include "gtm_string.h"
#  include "gtmdbglvl.h"
GBLREF	uint4		gtmDebugLevel;
/* q: head of queue to check; n: the name of queue; b: whether to check main exorder (from curtchain) and any expr_start queue */
#  define CHKTCHAIN(q, n, b)								\
MBSTART {										\
	triple	*c;									\
	DCL_THREADGBL_ACCESS;								\
											\
	SETUP_THREADGBL_ACCESS;							\
					/* memcmp() is fast and 3 chars sufficient */	\
 	if ((gtmDebugLevel & GDL_DebugCompiler) && (0 == memcmp(#n, "exorder", 3)))	\
	{										\
		if ((triple *)-1 != (triple *)q) /* to avoid post-checking deletes */	\
			chktchain((triple *)q);					\
		if (b)									\
		{									\
			c = TREF(curtchain);						\
			chktchain(c);		/* this might be redundant, or not! */	\
			c = TREF(expr_start);						\
			if (NULL != c)							\
				chktchain(c);	/* this extra has been rewarding */	\
		}									\
	}										\
} MBEND
#  define dqdel(x, n)			\
MBSTART {				\
	CHKTCHAIN((x), n, FALSE);	\
	DQDEL((x), n);			\
	CHKTCHAIN(-1, n, TRUE);	\
} MBEND
#  define dqdelchain(q, x, n)		\
MBSTART {				\
	CHKTCHAIN((q), n, FALSE);	\
	DQDELCHAIN((q), (x), n);		\
	CHKTCHAIN((q), n, TRUE);	\
} MBEND
#  define dqins(q, n, x)		\
MBSTART {				\
	CHKTCHAIN((q), n, FALSE);	\
	DQINS((q), n, (x));		\
	CHKTCHAIN((q), n, TRUE);	\
} MBEND
#  define dqrins(q, n, x)		\
MBSTART {				\
	CHKTCHAIN((q), n, FALSE);	\
	DQRINS((q), n, (x));		\
	CHKTCHAIN((q), n, TRUE);	\
} MBEND
#  define dqadd(q, x, n)		\
MBSTART {				\
	CHKTCHAIN((q), n, FALSE);	\
	CHKTCHAIN((x), n, FALSE);	\
	DQADD((q), (x), n);		\
	CHKTCHAIN((q), n, TRUE);	\
} MBEND
#endif

#endif
