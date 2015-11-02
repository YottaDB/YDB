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

#ifndef FTOK_SEMS_INCLUDED
#define FTOK_SEMS_INCLUDED

boolean_t db_ipcs_reset(gd_region *reg);
boolean_t ftok_sem_get(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate);
boolean_t ftok_sem_lock(gd_region *reg, boolean_t incr_cnt, boolean_t immediate);
boolean_t ftok_sem_incrcnt(gd_region *reg);
boolean_t ftok_sem_release(gd_region *reg, boolean_t decr_cnt, boolean_t immediate);

#endif /* FTOK_SEMS_INCLUDED */
