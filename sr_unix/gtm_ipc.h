/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Serivces, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_ipc.h - interlude to <ipc.h> system header file.  */
#ifndef GTM_IPCH
#define GTM_IPCH

#include <sys/ipc.h>

#ifdef __MVS__
/* For shmget with __IPC_MEGA or _LP64 */
#define MEGA_BOUND    (1024 * 1024)
#endif

#define FTOK		gtm_ftok
#define FTOK_OLD	ftok

#define JNLPOOL_SHMDT(RC, SAVE_ERRNO)				\
{								\
	jnlpool_ctl_ptr_t save_jnlpool_ctl;			\
								\
	SAVE_ERRNO = 0; /* clear any left-over value */		\
	assert(NULL != jnlpool_ctl);				\
	DEFER_INTERRUPTS(INTRPT_IN_SHMDT);			\
	save_jnlpool_ctl = jnlpool.jnlpool_ctl;			\
	jnlpool_ctl = jnlpool.jnlpool_ctl = NULL;		\
	RC = SHMDT(save_jnlpool_ctl);				\
	SAVE_ERRNO = errno;					\
	ENABLE_INTERRUPTS(INTRPT_IN_SHMDT);			\
}

key_t gtm_ftok(const char *path, int id);

#define IPC_REMOVED(ERRNO)	((EINVAL == ERRNO) || (EIDRM == ERRNO))	/* EIDRM is only on Linux */
#define	SEM_REMOVED(ERRNO)	IPC_REMOVED(ERRNO)
#define	SHM_REMOVED(ERRNO)	IPC_REMOVED(ERRNO)

#endif
