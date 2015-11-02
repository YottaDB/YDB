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

/* Ensure that at least DB_CSH_RDPOOL_SZ buffers are not dirty by the time an update transaction completes. This guarantees
 * a minimum number of buffers are available at all times. Macro should be used whenever second arg is non-zero.
 * WBTEST_FORCE_WCS_GET_SPACE forces wcs_get_space to be called, regardless of wc_in_free.
 */
#define WCS_GET_SPACE(REG, NEEDED, CR)												\
(																\
	(															\
	 (cnl->wc_in_free >= ((int4)(NEEDED) + DB_CSH_RDPOOL_SZ))								\
	 DEBUG_ONLY( && !(gtm_white_box_test_case_enabled && (WBTEST_FORCE_WCS_GET_SPACE == gtm_white_box_test_case_number)))	\
	)															\
	|| wcs_get_space(REG, (NEEDED) + DB_CSH_RDPOOL_SZ, CR)									\
)

bool wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr);

#endif
