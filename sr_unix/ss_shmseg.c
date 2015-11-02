/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <sys/shm.h>
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "gtm_ctype.h"
#include "shmpool.h"
#include "db_snapshot.h"

GBLREF	sgmnt_addrs		*cs_addrs;

void		*ss_attach_shmseg(int size, long *ss_shmid, int *status, boolean_t do_putmsg_on_error)
{
	void		*shmaddr;
	sgmnt_addrs	*csa;

	error_def(ERR_SYSCALL);
	csa = cs_addrs;
	/* GT.M runtime should never try to create the shared storage. To identify whether the caller is a GT.M runtime or not
	 * we cannot use image_type as programs like UPDATE PROCESS, MUPIP LOAD call the runtime code but carries the image_type
	 * as MUPIP. But, runtime will never pass TRUE for do_putmsg as we don't want gtm_putmsg to happen in the runtime code.
	 * So, use do_putmsg_on_error to assert accordingly.
	 */
	assert((INVALID_SHMID != *ss_shmid) || do_putmsg_on_error);
	/* if ss_shmid is -1, then caller wanted us to create a new shared memory segment */
	if (INVALID_SHMID == *ss_shmid)
	{
		if (INVALID_SHMID == (*ss_shmid = shmget(IPC_PRIVATE, size, RWDALL | IPC_CREAT)))
		{
			*status = errno;
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmget"), CALLFROM, *status);
			return NULL;
		}
	}
	if ((void *)-1 == (shmaddr = shmat((int)*ss_shmid, NULL, 0)))
	{
		*status = errno;
		if (do_putmsg_on_error)
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmat"), CALLFROM, *status);
		else if (csa->now_crit) /* Report error only when we are in final retry (holding crit) */
			send_msg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmat"), CALLFROM, *status);
		return NULL;
	}
	*status = 0;
	return shmaddr;
}

int	ss_detach_shmseg(void *shmaddr, long ss_shmid, boolean_t remove_shmid, boolean_t do_putmsg_on_error)
{
	int		save_errno;
	sgmnt_addrs	*csa;

	error_def(ERR_SYSCALL);
	csa = cs_addrs;
	if (NULL != shmaddr)
	{
		assert(0 == ((long)shmaddr % OS_PAGE_SIZE));
		if (0 != SHMDT(shmaddr))
		{
			save_errno = errno;
			/* If detaching to a shared memory fails, then we don't want to continue as that would mean
			 * the process space will keep increasing
			 */
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmdt"), CALLFROM, save_errno);
		}
		/* GT.M runtime should never try to create the shared storage. To identify whether the caller is a GT.M runtime
		 * or not we cannot use image_type as programs like UPDATE PROCESS, MUPIP LOAD call the runtime code but carries
		 * the image_type as MUPIP. But, runtime will never pass TRUE for do_putmsg as we don't want gtm_putmsg to happen
		 * in the runtime code. So, use do_putmsg_on_error to assert accordingly.
		 */
		assert(!remove_shmid || do_putmsg_on_error);
		if (remove_shmid && (0 != shmctl((int)ss_shmid, IPC_RMID, 0)))
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("Error with shmctl IPC_RMID"), CALLFROM, save_errno);
			return save_errno;
		}
		return 0;
	}
	return -1;
}
