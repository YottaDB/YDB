/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Routines to verify a simple double-linked queue.
 *
 * If we fail any of these tests in pro code, we gtmassert.
 * This code produces no messages because they would be useful only in the context of the dumped data and would scare the user.
 * We have two versions of this test. The first locks the queue header. The second assumes the queue is already locked.
 * Most calls to this routine are compiled in by specifying the compile flag DEBUG_QUEUE.
 */

#include "mdef.h"


#ifdef UNIX
#include "aswp.h"
#endif
#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "lockconst.h"
#include "interlock.h"
#include "wcs_backoff.h"

#ifdef QI_STARVATION
# undef QI_STARVATION
# undef QI_RETRY
#endif

#define QI_STARVATION 1000
#define QI_RETRY 256

GBLREF	volatile int4	fast_lock_count;
GBLREF	pid_t		process_id;
VMS_ONLY(GBLREF	uint4	image_count;)	/* Needed for GET/RELEASE_SWAPLOCK */

gtm_uint64_t verify_queue_lock(que_head_ptr_t qhdr)
{
	que_ent_ptr_t	qe, last_qe;
	gtm_uint64_t	i, k;
	boolean_t	got_lock;

	++fast_lock_count;
	/* Before running this queue, must lock it to prevent it from being changed during our run */
	for (got_lock = FALSE, k = 0; k < QI_STARVATION; ++k)
	{
		for (i = 0; got_lock == FALSE && i < QI_RETRY; ++i)
			got_lock = GET_SWAPLOCK(&qhdr->latch);
		if (got_lock)
			break;
		if (0 != k)
			wcs_backoff(k);
	}
	assertpro(got_lock);			/* We gotta have our lock */
	i = verify_queue(qhdr);
	/* Release locks */
	RELEASE_SWAPLOCK(&qhdr->latch);
	--fast_lock_count;
	return i;
}

gtm_uint64_t verify_queue(que_head_ptr_t qhdr)
{
	gtm_uint64_t	i;
	que_ent_ptr_t	qe, last_qe;

	/* run through queue. Verify the fwd and backward chain ptrs */
	last_qe = (que_ent_ptr_t)qhdr;
	for (i = 0, qe = (que_ent_ptr_t)((sm_uc_ptr_t)qhdr + qhdr->fl);
	     qe != (que_ent_ptr_t)qhdr;
	     qe = (que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->fl), i++)
	{
		assertpro((que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->bl) == last_qe);	/* Back pointer works good? */
		last_qe = qe;
	}
	return i;
}
