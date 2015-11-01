/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* do_shmat.c UNIX - Attach shared memory (stub)
 *   Note:  The TANDEM (NonStop/UX) requires a special version of this routine
 *          so that attaching shared memory does not interfere with malloc.
 */

#include "gtm_stdio.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#ifndef __MVS__
#include <sys/sysmacros.h>
#endif
#include "mdef.h"
#include "do_shmat.h"

void *do_shmat(int4 shmid, const void *shmaddr, int4 shmflg)
{
	return(shmat((int)shmid, shmaddr, SHMAT_ARG((int)shmflg)));
}

