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

#define dqloop(q,n,i) for (i = (q)->n.fl ; i != (q) ; i = (i)->n.fl)

#define dqinit(q,n) ((q)->n.fl = (q)->n.bl = q)

#define dqins(q,n,x) ((x)->n.fl = (q)->n.fl, (x)->n.bl =(q), (q)->n.fl=(x), ((x)->n.fl)->n.bl=(x))

#define dqdel(x,n) ((x)->n.bl->n.fl = (x)->n.fl , (x)->n.fl->n.bl = (x)->n.bl)

#define dqdelchain(x,y,n) ((x)->n.fl = (y), (y)->n.bl = (x))

#define dqadd(o,n,q) ((o)->q.fl->q.bl=(n)->q.bl,(n)->q.bl->q.fl=(o)->q.fl,(o)->q.fl=(n)->q.fl,(n)->q.fl->q.bl=(o))
