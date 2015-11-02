/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#define DQLOOP(q, n, i) for (i = (q)->n.fl; i != (q); i = (i)->n.fl)

/* Initialize an element */
#define DQINIT(q, n) ((q)->n.fl = (q)->n.bl = q)

/* Delete one element "x" from the doubly linked list */
#define DQDEL(x, n) ((x)->n.bl->n.fl = (x)->n.fl, (x)->n.fl->n.bl = (x)->n.bl)

/* Delete a doubly-linked list of elements from "x->n.fl" to "y->n.bl" (i.e. everything in between "x" and "y" excluding them) */
#define DQDELCHAIN(x, y, n) ((x)->n.fl = (y), (y)->n.bl = (x))

/* Insert one element "x" in between "q" and "q->n.fl" */
#define DQINS(q, n, x) ((x)->n.fl = (q)->n.fl, (x)->n.bl = (q), (q)->n.fl = (x), ((x)->n.fl)->n.bl = (x))

/* Insert one element "x" in between "q" and "q->n.bl" */
#define DQRINS(q, n, x) ((x)->n.bl = (q)->n.bl, (x)->n.fl = (q), (q)->n.bl = (x), ((x)->n.bl)->n.fl = (x))

/* Insert a doubly-linked list of elements from "n->q.fl" to "n->q.bl" in between "o" and "o->q.fl" */
#define DQADD(o, n, q) ((o)->q.fl->q.bl = (n)->q.bl, (n)->q.bl->q.fl = (o)->q.fl, (o)->q.fl = (n)->q.fl, (n)->q.fl->q.bl = (o))

/* Define macros actually used which if #define DEBUG_TRIPLES, adds debugging information. Since these macros are
 * used in several different queue types and since these debugging macros only work for the exorder field in triples,
 * the test to see if should do debugging is not elegant but it does allow decent debugging. Note those macros without
 * debugging are statically defined.
 */

#define dqloop(q, n, i) DQLOOP(q, n, i)
#define dqinit(q, n)	DQINIT(q, n)

/* #define DEBUG_TRIPLES / * Uncomment this to do triple debugging */
#ifndef DEBUG_TRIPLES
#  define dqdel(x, n)		DQDEL(x, n)
#  define dqdelchain(x, y, n)	DQDELCHAIN(x, y, n)
#  define dqins(q, n, x)	DQINS(q, n, x)
#  define dqrins(q, n, x)	DQRINS(q, n, x)
#  define dqadd(o, n, q)	DQADD(o, n, q)
#  define CHKTCHAIN(x)
#else
#  include "compiler.h"
#  define CHKTCHAIN(x) chktchain((triple *)(x))
#  define IFEXOCHN(n, c) if (0 == memcmp(#n, "exorder", 3)) c	/* memcmp() should be faster and for 3 chars, just as effective */
#  define dqdel(x, n)			\
{					\
	IFEXOCHN(n, CHKTCHAIN(x));	\
	DQDEL(x, n);			\
}
#  define dqdelchain(x, y, n)		\
{					\
	IFEXOCHN(n, CHKTCHAIN(x));	\
	DQDELCHAIN(x, y, n);		\
	IFEXOCHN(n, CHKTCHAIN(y));	\
}
#  define dqins(q, n, x)		\
{					\
	IFEXOCHN(n, CHKTCHAIN(q));	\
	DQINS(q, n, x);			\
	IFEXOCHN(n, CHKTCHAIN(q));	\
}
#  define dqrins(q, n, x)		\
{					\
	IFEXOCHN(n, CHKTCHAIN(q));	\
	DQRINS(q, n, x);		\
	IFEXOCHN(n, CHKTCHAIN(q));	\
}
#  define dqadd(o, n, q)		\
{					\
	IFEXOCHN(n, CHKTCHAIN(o));	\
	DQADD(o, n, q);			\
	IFEXOCHN(n, CHKTCHAIN(o));	\
}
#endif

#endif
