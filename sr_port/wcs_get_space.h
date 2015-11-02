/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WCS_GET_SPACE_H__
#define __WCS_GET_SPACE_H__

#ifdef	UNIX
typedef struct wcs_conflict_trace_struct
{
	int4	wcs_active_lvl;
	int4	io_in_prog_pid;
	int4	fsync_in_prog_pid;
} wcs_conflict_trace_t;
#endif

bool wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr);

#endif
