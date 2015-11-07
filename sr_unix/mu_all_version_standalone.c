/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/sem.h>
#include <sys/shm.h>
#include "errno.h"
#include "gtm_ipc.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_fcntl.h"

#include "error.h"
#include "send_msg.h"
#include "gtmio.h"
#include "iosp.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblk.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "mu_all_version_standalone.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

static	int	ftok_ids[FTOK_ID_CNT] = {1, 43, 43};
static	int	ftok_ver[FTOK_ID_CNT] = {0, 0, 1};

error_def(ERR_MUSTANDALONE);
error_def(ERR_DBOPNERR);
error_def(ERR_FTOKKEY);
error_def(ERR_SEMID);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
ZOS_ONLY(error_def(ERR_BADTAG);)

/* Aquire semaphores that on on all V4.x releases are the access control semaphores. In pre V4.2 releases
   they were based on an FTOK of the database name with an ID of '1'. In V4.2 and later, they are based on
   the FTOK of the database name with an ID of '43'. Since we do not know which flavor of database we are
   dealing with, we must create and acquire both flavors of semaphore and hold them for the duration of
   the phase 2 run. But just holding these semaphore is not sufficient to guarrantee standalone access. We
   also must attempt to attach to the shared memory for the segment. If it is found, standalone access
   is not achieved. Early V4 versions (prior to V4.2) created the shared memory with the same FTOK id as the
   semaphore. Later versions would have had the key of the created private section in the file-header. Use
   both approaches and fail our attempt if either succeeds.
*/
void mu_all_version_get_standalone(char_ptr_t db_fn, sem_info *sem_inf)
{
	int		i, rc, save_errno, fd;
	struct sembuf	sop[4];
	int		shmid;
	ZOS_ONLY(int	realfiletag;)

	v15_sgmnt_data	v15_csd;

	/* Both semaphores must have value 0 and then all will be
	   incremented to completely lockout any other potential users
	   until we are done
	*/
	sop[0].sem_num = 0; sop[0].sem_op = 0;		/* First check all semaphore have 0 value */
	sop[1].sem_num = 1; sop[1].sem_op = 0;
	sop[2].sem_num = 0; sop[2].sem_op = 1; 		/* Increment all semaphores */
	sop[3].sem_num = 1; sop[3].sem_op = 1;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = SEM_UNDO | IPC_NOWAIT;
	memset(sem_inf, 0, (SIZEOF(sem_inf) * FTOK_ID_CNT));	/* Zero all fields so we know what to clean up */

	for (i = 0; FTOK_ID_CNT > i; ++i)
	{	/* Once through for both ftok key'd semaphores, and both FTOKs for the new semaphore */
		if (ftok_ver[i] == 0)
			sem_inf[i].ftok_key = FTOK_OLD(db_fn, ftok_ids[i]);
		else
			sem_inf[i].ftok_key = FTOK(db_fn, ftok_ids[i]);
		if (-1 == sem_inf[i].ftok_key)
		{
			save_errno = errno;
			mu_all_version_release_standalone(sem_inf);
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("ftok()"), CALLFROM, save_errno);
		}
		sem_inf[i].sem_id = semget(sem_inf[i].ftok_key, 3, RWDALL | IPC_CREAT | IPC_EXCL);
		if (-1 == sem_inf[i].sem_id)
		{
			save_errno = errno;
			mu_all_version_release_standalone(sem_inf);
			/* Different platforms seem to make different checks in different order here so if the
			   semaphore exists but is locked, on some platforms we get EAGAIN, if the semaphore exists
			   but has fewer semaphores in the set that we are requesting we get EINVAL, but if all that
			   is ok and the semaphore just already exists, we get EEXIST which is all we wanted to
			   check anyway..
			*/
			if (EEXIST == save_errno || EAGAIN == save_errno || EINVAL == save_errno)
				/* Semaphore already exists and/or is locked-- likely rundown needed */
				rts_error(VARLSTCNT(9) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR), 2, RTS_ERROR_TEXT(db_fn),
						save_errno, 0, ERR_FTOKKEY, 1, sem_inf[i].ftok_key);
			else
				rts_error(VARLSTCNT(12) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno, 0,
						ERR_FTOKKEY, 1, sem_inf[i].ftok_key);
		}
		SEMOP(sem_inf[i].sem_id, sop, 4, rc, NO_WAIT);
		if (-1 == rc)
		{
			save_errno = errno;
			mu_all_version_release_standalone(sem_inf);
			if (EAGAIN == save_errno)
				rts_error(VARLSTCNT(12) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR), 2, RTS_ERROR_TEXT(db_fn),
						save_errno, 0, ERR_FTOKKEY, 1, sem_inf[i].ftok_key,
						ERR_SEMID, 1, sem_inf[i].sem_id);
			else
				rts_error(VARLSTCNT(15) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno, 0,
						ERR_FTOKKEY, 1, sem_inf[i].ftok_key, ERR_SEMID, 1, sem_inf[i].sem_id);
		}
	}

	/* First try to access shared memory based on the database FTOK id. Only need to do the ftok returned by the first
	   FTOK done above as it was the only method where the shared memory and semaphore had the same key.

	   Detailed description: The first semaphore (for project id 1) is for early V4 versions. In these versions, the
	   database had the same shmid as the semaphore did. Given that we can try to attach to it and if it is found, we
	   assume we do not have a standalone lock. The other option is this is a later database so we open it up and pull
	   the shmid field out of the file-header (it was in the same place in all versions that had the shmid). Whatever
	   that field is (regardless of version), if we are able to attach to shared memory, then we consider standalone
	   a failure. This is not a 100% valid check but it is good enough for the few times this will actually be run
	   (upgrade, downgrade and dbcertify).
	*/
	shmid = shmget(sem_inf[0].ftok_key, 0, RWDALL);
	if (-1 == shmid)
	{	/* That failed, second check is if shmid stored in file-header (if any) exists */
		fd = OPEN(db_fn, O_RDONLY);
		if (FD_INVALID == fd)
		{
			save_errno = errno;
			mu_all_version_release_standalone(sem_inf);
			rts_error(VARLSTCNT(5) ERR_DBOPNERR, 2, RTS_ERROR_TEXT(db_fn), save_errno);
		}
#		ifdef __MVS__
		if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
			TAG_POLICY_GTM_PUTMSG(db_fn, errno, realfiletag, TAG_BINARY);
#		endif
		LSEEKREAD(fd, 0, &v15_csd, SIZEOF(v15_csd), rc);
		if (0 != rc)
		{
			mu_all_version_release_standalone(sem_inf);
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("LSEEKREAD()"), CALLFROM, rc);
		}
		CLOSEFILE_RESET(fd, rc);	/* resets "fd" to FD_INVALID */
		if (0 != v15_csd.shmid && INVALID_SHMID != v15_csd.shmid)
			shmid = shmget(v15_csd.shmid, 0, RWDALL);
	}
	if (-1 != shmid)
	{
		mu_all_version_release_standalone(sem_inf);
		rts_error(VARLSTCNT(9) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR), 2, RTS_ERROR_TEXT(db_fn),
				save_errno, 0, ERR_FTOKKEY, 1, sem_inf[i].ftok_key);
	}
}

/* The input array (filled out by mu_all_version_get_standalone()) should tell us which resources we specifically
   allocated so we can then free those resources (and no other).
*/
void mu_all_version_release_standalone(sem_info *sem_inf)
{
	int		i, rc, save_errno;

	/* Note that we ignore most errors in this routine as we may get called with the alleged semaphores in
	   just about any state.
	*/
	for (i = 0; FTOK_ID_CNT > i; ++i)
	{	/* release/delete any held semaphores in this set */
		if (sem_inf[i].sem_id && -1 != sem_inf[i].sem_id)
		{
			rc = semctl(sem_inf[i].sem_id, 0, IPC_RMID);
			if (-1 == rc && EIDRM != errno)
			{	/* Don't care if semaphore already removed */
				save_errno = errno;
				send_msg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl(remid)"), CALLFROM, save_errno);
			}
			sem_inf[i].sem_id = 0;	/* Clear so we don't repeat if redriven */
		}
	}
}
