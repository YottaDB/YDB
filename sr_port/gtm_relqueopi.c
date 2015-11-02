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

/*	gtm_relqueopi - C-callable relative queue interlocked routines
 *
 *	These routines perform interlocked operations on doubly-linked
 *	relative queues.  They are designed to emulate the VAX machine
 *	instructions (and corresponding VAX C library routines) after
 *	which they are named.
 *
 *	insqhi - insert entry into queue at head, interlocked
 *	insqti - insert entry into queue at tail, interlocked
 *	remqhi - remove entry from queue at head, interlocked
 *	remqti - remove entry from queue at tail, interlocked
 */

#include "mdef.h"

#include "relqueopi.h"

#ifdef DEBUG

/* An element that is not present in the queue should have its fl,bl fields zero.
 * An element that is     present in the queue should have its fl,bl fields non-zero.
 * Ensure/Check this property for all queue operations.
 * This will help catch a case when an element that is already present in the queue is re-added into the same queue.
 * Such operations will cause queue corruption and since the queue is in shared memory (given that the interlocked
 * queue operations are being done), could cause shared memory corruption and database damage (D9H03-002644).
 */
int gtm_insqhi(que_ent_ptr_t new, que_head_ptr_t base)
{
	int4	status;

	assert(0 == new->fl);
	assert(0 == new->bl);
	status = SYS_INSQHI(new, base);
	/* We cannot assert that new->fl and new->bl are non-zero at this point since they
	 * could be concurrently removed from the queue right after we inserted it above.
	 */
	return status;
}

int gtm_insqti(que_ent_ptr_t new, que_head_ptr_t base)
{
	int4	status;

	assert(0 == new->fl);
	assert(0 == new->bl);
	status = SYS_INSQTI(new, base);
	/* We cannot assert that new->fl and new->bl are non-zero at this point since they
	 * could be concurrently removed from the queue right after we inserted it above.
	 */
	return status;
}

void_ptr_t gtm_remqhi(que_head_ptr_t base)
{
	que_ent_ptr_t	ret;

	ret = SYS_REMQHI(base);
	if (NULL != ret)
	{
#		ifndef UNIX	/* in Unix, relqueopi.c already does the rest of fl,bl to 0 just before releasing the swap lock */
		ret->fl = 0;
		ret->bl = 0;
#		endif
		assert(0 == ret->fl);
		assert(0 == ret->bl);
	}
	return (void_ptr_t)ret;
}

void_ptr_t gtm_remqti(que_head_ptr_t base)
{
	que_ent_ptr_t	ret;

	ret = SYS_REMQTI(base);
	if (NULL != ret)
	{
#		ifndef UNIX	/* in Unix, relqueopi.c already does the rest of fl,bl to 0 just before releasing the swap lock */
		ret->fl = 0;
		ret->bl = 0;
#		endif
		assert(0 == ret->fl);
		assert(0 == ret->bl);
	}
	return (void_ptr_t)ret;
}

#endif
