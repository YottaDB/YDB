/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "interlock.h"
#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsblk.h"
#include "gdsfhead.h"

#define LOCK_TIMEOUT_SECS	(4 * 60)	/* Define timeout as being 4 mins */

GBLREF	volatile int4	fast_lock_count;

/* Lock the given queue and verify its elements are well formed before releasing the lock. Returns the count of
 * queue elements.
 */
gtm_uint64_t verify_queue_lock(que_head_ptr_t qhdr)
{
	gtm_uint64_t	i;

	++fast_lock_count;	/* grab_latch() doesn't keep fast_lock_count incremented across rel_latch() */
	if (!grab_latch(&qhdr->latch, LOCK_TIMEOUT_SECS))
	{
		fast_lock_count--;
		assert(0 <= fast_lock_count);
		assertpro(FALSE);
	}
	i = verify_queue(qhdr);
	/* Release locks */
	rel_latch(&qhdr->latch);
	--fast_lock_count;
	assert(0 <= fast_lock_count);
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
