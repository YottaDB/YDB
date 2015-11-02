/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
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

boolean_t	wcs_phase2_commit_wait(sgmnt_addrs *csa, cache_rec_ptr_t cr);

#endif
