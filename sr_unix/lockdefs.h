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

#define MLK_LOGIN(x)
#define BLOCKING_PROC_ALIVE(w,x,y,z) (!is_proc_alive(w->blocked->owner, 0))
#define PENDING_PROC_ALIVE(w,x,y,z) (!is_proc_alive(w->process_id, 0))
#define PROC_ALIVE(w,x,y,z) (!is_proc_alive(w->owner, 0))
