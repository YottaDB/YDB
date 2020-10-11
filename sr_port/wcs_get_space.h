/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef WCS_GET_SPACE_H_INCLUDED
#define WCS_GET_SPACE_H_INCLUDED

#ifdef	UNIX
typedef struct wcs_conflict_trace_struct
{
	int4	wcs_active_lvl;
	int4	io_in_prog_pid;
	int4	fsync_in_prog_pid;
} wcs_conflict_trace_t;
#endif

#include "interlock.h"
#include "sleep.h"

boolean_t wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr);

/* Ensure that at least DB_CSH_RDPOOL_SZ buffers are not dirty by the time an update transaction completes. This guarantees
 * a minimum number of buffers are available at all times. Macro should be used whenever second arg is non-zero.
 * WBTEST_FORCE_WCS_GET_SPACE and WBTEST_FORCE_WCS_GET_SPACE_CACHEVRFY force wcs_get_space to be called, regardless of wc_in_free.
 */
static inline boolean_t WCS_GET_SPACE(gd_region *reg, int needed, cache_rec_ptr_t cr, sgmnt_addrs *csa)
{
	/* process_id is required for FREEZE_LATCH_HELD */
	GBLREF	uint4			process_id;

	boolean_t	was_latch;
	int4		wc_in_free_copy;

	wc_in_free_copy = csa->nl->wc_in_free;
	if (wc_in_free_copy < ((int4) needed + DB_CSH_RDPOOL_SZ))
	{
		was_latch = FREEZE_LATCH_HELD(csa);
		if (!was_latch)
			grab_latch(&csa->nl->freeze_latch, GRAB_LATCH_INDEFINITE_WAIT, WS_21, csa);
		wc_in_free_copy = csa->nl->wc_in_free;
		if (!was_latch)
			rel_latch(&csa->nl->freeze_latch);
	}

	if ((wc_in_free_copy < ((int4) needed + DB_CSH_RDPOOL_SZ))
#	ifdef DEBUG
		|| (gtm_white_box_test_case_enabled
			&& ((WBTEST_FORCE_WCS_GET_SPACE == gtm_white_box_test_case_number)
				|| (WBTEST_FORCE_WCS_GET_SPACE_CACHEVRFY == gtm_white_box_test_case_number)))
#	endif
	)
		return wcs_get_space(reg, needed + DB_CSH_RDPOOL_SZ, cr);
	else
		return TRUE;
}
#endif
