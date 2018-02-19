/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_fcntl.h"  /* Needed for AIX's silly open to open64 translations (open used in JNLPOOL_CLEAR_FIELDS macro) */

#ifdef __MVS__
/* For shmget with __IPC_MEGA or _LP64 */
#define MEGA_BOUND    (1024 * 1024)
#endif

#define FTOK		gtm_ftok
#define FTOK_OLD	ftok

#define	JNLPOOL_CLEAR_FIELDS(JNLPOOL)				\
{								\
	GBLREF	int		pool_init;			\
								\
	assert(NULL != JNLPOOL);				\
	JNLPOOL->jnlpool_ctl = NULL;				\
	JNLPOOL->gtmsrc_lcl_array = NULL;			\
	JNLPOOL->gtmsource_local_array = NULL;			\
	JNLPOOL->jnldata_base = NULL;				\
	JNLPOOL->repl_inst_filehdr = NULL;			\
	JNLPOOL->jnlpool_dummy_reg->open = FALSE;		\
	JNLPOOL->gd_ptr = NULL;					\
	if (JNLPOOL->pool_init && (0 < pool_init))		\
		pool_init--;					\
	JNLPOOL->pool_init = JNLPOOL->recv_pool = FALSE;	\
}

#define JNLPOOL_SHMDT(JNLPOOL, RC, SAVE_ERRNO)			\
{								\
	jnlpool_ctl_ptr_t	save_jnlpool_ctl;		\
	intrpt_state_t		prev_intrpt_state;		\
								\
	SAVE_ERRNO = 0; /* clear any left-over value */		\
	assert(NULL != JNLPOOL);				\
	assert(NULL != JNLPOOL->jnlpool_ctl);			\
	DEFER_INTERRUPTS(INTRPT_IN_SHMDT, prev_intrpt_state);	\
	save_jnlpool_ctl = JNLPOOL->jnlpool_ctl;		\
	JNLPOOL->jnlpool_ctl = NULL;		\
	RC = SHMDT(save_jnlpool_ctl);				\
	SAVE_ERRNO = errno;					\
	JNLPOOL_CLEAR_FIELDS(JNLPOOL);				\
	ENABLE_INTERRUPTS(INTRPT_IN_SHMDT, prev_intrpt_state);	\
}

key_t gtm_ftok(const char *path, int id);

#define IPC_REMOVED(ERRNO)	((EINVAL == ERRNO) || (EIDRM == ERRNO))	/* EIDRM is only on Linux */
#define	SEM_REMOVED(ERRNO)	IPC_REMOVED(ERRNO)
#define	SHM_REMOVED(ERRNO)	IPC_REMOVED(ERRNO)

#endif
