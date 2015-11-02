/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifdef DEBUG_TRIPLES
#  define CHKTCHAIN(x)	chktchain(x)
#else
#  define CHKTCHAIN(x)
#endif

/* All the following macros assume a doubly-linked list using "fl" and "bl" */

#define dqloop(q,n,i) for (i = (q)->n.fl; i != (q); i = (i)->n.fl)

#define dqinit(q,n) ((q)->n.fl = (q)->n.bl = q)

/* delete one element "x" from the doubly linked list */
#define dqdel(x,n) ((x)->n.bl->n.fl = (x)->n.fl, (x)->n.fl->n.bl = (x)->n.bl)

/* delete a doubly-linked list of elements from "x->n.fl" to "y->n.bl" (i.e. everything in between "x" and "y" excluding them) */
#define dqdelchain(x,y,n) {((x)->n.fl = (y), (y)->n.bl = (x)); CHKTCHAIN(x); CHKTCHAIN(y);}

/* Insert one element "x" in between "q" and "q->n.fl" */
#define dqins(q,n,x) ((x)->n.fl = (q)->n.fl, (x)->n.bl =(q), (q)->n.fl=(x), ((x)->n.fl)->n.bl=(x))

/* Insert a doubly-linked list of elements from "n->q.fl" to "n->q.bl" in between "o" and "o->q.fl" */
#define dqadd(o,n,q)												\
{														\
	((o)->q.fl->q.bl=(n)->q.bl, (n)->q.bl->q.fl=(o)->q.fl, (o)->q.fl=(n)->q.fl, (n)->q.fl->q.bl=(o));	\
	CHKTCHAIN(o);												\
}
