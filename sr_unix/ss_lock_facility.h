/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SS_LOCK_FACILITY_H
#define SS_LOCK_FACILITY_H

boolean_t 		ss_get_lock(gd_region *reg);
boolean_t 		ss_get_lock_nowait(gd_region *reg);
void			ss_release_lock(gd_region *reg);
boolean_t		ss_lock_held_by_us(gd_region *reg);


#endif
