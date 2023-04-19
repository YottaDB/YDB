/****************************************************************
 *								*
 * Copyright (c) 2012-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * This file contains functions related to Linux HugeTLB support.
 * Supported Huge Page functionality requires the following prerequisites:
 *	Linux kernel support of Huge Pages
 *	x86_64 or i386 architecture
 *	Availability of Huge Pages through setting value to /proc/sys/vm/nr_hugepages or hugepages=<n> kernel parameter or
 *		/proc/sys/vm/nr_overcommit_hugepages
 *	In order to use shmget with Huge Pages, either the process gid should be in /proc/sys/vm/hugetlb_shm_group or the
 *		process should have CAP_IPC_LOCK
 *	In order to remap .text/.data/.bss sections, a file system of type hugetlbfs should be mounted
 *	Appropriate environmental variables should be set (gtm_hugetlb_shm) to enable/disable Huge Pages
 */
#include "mdef.h"
#include "gtm_ipc.h"
#include <sys/shm.h>
#include <errno.h>
#include "gtm_unistd.h"

#include "hugetlbfs_overrides.h"
#undef shmget
#include "send_msg.h"
#include "error.h"

GBLREF	bool	pin_shared_memory;
GBLREF	bool	hugetlb_shm_enabled;

error_def(ERR_SHMHUGETLB);
error_def(ERR_SHMLOCK);
error_def(ERR_TEXT);

/* The below array should have an entry corresponding to each type in enum shmget_caller in mdef.h */
static readonly char *errstr[] =
{
	"for lock file",		/* LOCK_FILE */
	"for snapshot file",		/* SNAPSHOT_FILE */
	"for relink file",		/* RELINK */
	"for journal pool file",	/* JOURNAL_POOL */
	"for database file",		/* DATABASE_FILE */
	"for rc cpt path",		/* RC_CPT */
	"for gtm_multi_proc",		/* GTM_MULTI_PROC_FREEZE */
	"for gtm_multi_proc"		/* GTM_MULTI_PROC_RECOVER */
};

static char err_string[1024];

int gtm_shmget (key_t key, size_t size, int shmflg, bool lock, enum shmget_caller caller, char *errinfostr)
{
#	ifdef HUGETLB_SUPPORTED
	int			shmid;
	struct	shmid_ds	shmstat;
	bool			native_shmget = FALSE;

	if (hugetlb_shm_enabled)
	{
		shmid = shmget(key, size, shmflg | SHM_HUGETLB);
		if ((-1 == shmid) && ((EPERM == errno) || (ENOMEM == errno)))
		{	/* retry without huge pages */
			assert(N_SHMGET_CALLERS > caller && NULL != errinfostr);
			SNPRINTF(err_string, SIZEOF(err_string), "%s %s", errstr[caller], errinfostr);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SHMHUGETLB, 2, RTS_ERROR_TEXT(err_string), errno);
			shmid = shmget(key, size, shmflg);
			native_shmget = TRUE;
		}
	} else
	{
		shmid = shmget(key, size, shmflg);
		native_shmget = TRUE;
	}
	if ((-1 != shmid) && native_shmget && lock && pin_shared_memory)
	{	/* shared memory segment successfully allocated, huge pages not in use, and locking requested *
		 * lock shared memory so it won't be swapped */
		if (-1 == shmctl(shmid, SHM_LOCK, &shmstat))
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SHMLOCK, 0, errno);
		}
	}
	return shmid;
#	else
	return shmget(key, size, shmflg); /* if huge pages are not supported, then neither is locking shm */
#	endif
}
