/****************************************************************
 *								*
 *	Copyright 2008, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WCS_PHASE2_COMMIT_WAIT_H__
#define __WCS_PHASE2_COMMIT_WAIT_H__

/* We have found that having the default spin count at 128 causes huge CPU usage in Tru64 even though ALL the processes are
 * waiting for one process to finish its phase2 commit. Since this parameter is user controlled anyways using DSE CHANGE -FILE,
 * the default setting is kept at a very low value of 16.
 */
#	define	WCS_PHASE2_COMMIT_DEFAULT_SPINCNT	16
/* The maximum number of cache-records with disticnt PIDs that wcs_phase2_commit_wait can note down for tracing. We don't expect
 * a large number of PIDs concurrently in phase2 of the commit. If we see evidince of more PIDs in phase2 of the commit, then
 * the following number can be bumped.
 */
#	define	MAX_PHASE2_WAIT_CR_TRACE_SIZE		32

typedef struct phase2_wait_trace_struct
{
	uint4		blocking_pid;
	cache_rec_ptr_t	cr;
} phase2_wait_trace_t;

boolean_t	wcs_phase2_commit_wait(sgmnt_addrs *csa, cache_rec_ptr_t cr);

#endif
