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

#ifndef FTOK_SEMS_INCLUDED
#define FTOK_SEMS_INCLUDED

boolean_t db_ipcs_reset(gd_region *, boolean_t);
boolean_t ftok_sem_get(gd_region *, boolean_t, int, boolean_t);
boolean_t ftok_sem_lock(gd_region *, boolean_t, boolean_t);
boolean_t ftok_sem_release(gd_region *,  boolean_t, boolean_t);

#endif /* FTOK_SEMS_INCLUDED */
