/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Routines to verify a simple double-linked queue.

   If we fail any of these tests, we will gtmassert in case a call to
   this routine is made from "pro" code. No messages are produced by
   this code since they would be useful only in the context of the
   dumped data and would scare the hell out of the user anyway.

   Two versions of this test are supplied. The first will lock the queue
   header first. The second assumes the queue is already locked.

   Most(All?) calls to this routine are compiled in by specifying the compile
   flag DEBUG_QUEUE.

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

void verify_queue_lock(que_head_ptr_t qhdr)
{
	que_ent_ptr_t	qe, last_qe;
	int		i, k;
	boolean_t		got_lock;

	++fast_lock_count;

	/* Before we can play with the queue, we have to lock it
	   to prevent it from being changed out from underneath us */
	for (got_lock = FALSE, k = 0; k < QI_STARVATION; ++k)
	{
		for (i = 0; got_lock == FALSE && i < QI_RETRY; ++i)
			got_lock = GET_SWAPLOCK(&qhdr->latch);
		if (got_lock)
			break;
		if (0 != k)
			wcs_backoff(k);
	}

	if (!got_lock)			/* We gotta have our lock */
		GTMASSERT;

	/* Now run through queue. Verify the fwd and backward chain ptrs */
	last_qe = (que_ent_ptr_t)qhdr;
	for (qe = (que_ent_ptr_t)((sm_uc_ptr_t)qhdr + qhdr->fl);
	     qe != (que_ent_ptr_t)qhdr;
	     qe = (que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->fl))
	{

		if ((que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->bl) != last_qe)	/* Back pointer works good? */
		{
/*                      ASWP(&qhdr->latch, LOCK_AVAILABLE, latch); */
			GTMASSERT;
		}
		last_qe = qe;
	}

	/* Release locks */
	RELEASE_SWAPLOCK(&qhdr->latch);
	--fast_lock_count;

	return;
}


void verify_queue(que_head_ptr_t qhdr)
{
	que_ent_ptr_t	qe, last_qe;

	/* Now run through queue. Verify the fwd and backward chain ptrs */
	last_qe = (que_ent_ptr_t)qhdr;
	for (qe = (que_ent_ptr_t)((sm_uc_ptr_t)qhdr + qhdr->fl);
	     qe != (que_ent_ptr_t)qhdr;
	     qe = (que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->fl))
	{

		if ((que_ent_ptr_t)((sm_uc_ptr_t)qe + qe->bl) != last_qe)	/* Back pointer works good? */
			GTMASSERT;
		last_qe = qe;
	}

	return;
}
