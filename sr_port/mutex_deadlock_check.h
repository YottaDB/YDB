/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEX_DEADLOCK_CHECK_INCLUDED
#define MUTEX_DEADLOCK_CHECK_INCLUDED

#ifdef VMS
void mutex_deadlock_check(mutex_struct_ptr_t addr);
#else
void mutex_deadlock_check(mutex_struct_ptr_t addr, sgmnt_addrs *csa);
#endif

#endif
