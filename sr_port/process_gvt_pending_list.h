/****************************************************************
 *								*
 *	Copyright 2008, 2010 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef PROCESS_GVT_PENDING_LIST_DEFINED

/* Declare parms for process_gvt_pending_list.c */

boolean_t	is_gvt_in_pending_list(gv_namehead *gvt);
void 		process_gvt_pending_list(gd_region *reg, sgmnt_addrs *csa);

#define PROCESS_GVT_PENDING_LIST_DEFINED

#endif
