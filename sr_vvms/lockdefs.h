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

#define MLK_LOGIN(x) ( x->login_time = TAREF1(login_time, 0), x->image_count = image_count)
#define BLOCKING_PROC_ALIVE(w,x,y,z) (z = get_proc_info(w->blocked->owner,x,&y), ((z == SS$_NONEXPR) || \
	((z == SS$_NORMAL) && ((x[0] != w->blocked->login_time) || (y != w->blocked->image_count)))))
#define PROC_ALIVE(w,x,y,z) (z = get_proc_info(w->owner,x,&y), ((z == SS$_NONEXPR) || \
	((z == SS$_NORMAL) && ((x[0] != w->login_time) || (y != w->image_count)))))
#define PENDING_PROC_ALIVE(w,x,y,z) (z = get_proc_info(w->process_id,x,&y), (z == SS$_NONEXPR))
