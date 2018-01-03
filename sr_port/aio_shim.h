/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef AIO_SHIM_H_INCLUDED
#define AIO_SHIM_H_INCLUDED

#include "gtm_libaio.h"

#ifdef USE_NOAIO
#define AIO_SHIM_WRITE(UNUSED, AIOCBP, RET)   /* no-op, N/A */
#define AIO_SHIM_RETURN(AIOCBP, RET)          /* no-op, N/A */
#define AIO_SHIM_ERROR(AIOCBP, RET)           /* no-op, N/A */
#define SIGNAL_ERROR_IN_WORKER_THREAD(gdi, err_str, errno)    /* no-op, N/A */
#define CHECK_ERROR_IN_WORKER_THREAD(reg, udi)                        /* no-op, N/A */
#elif !defined(USE_LIBAIO)    /* USE_NOAIO */
#define AIO_SHIM_WRITE(UNUSED, AIOCBP, RET)	MBSTART { RET = aio_write(AIOCBP); } MBEND
#define AIO_SHIM_RETURN(AIOCBP, RET)	 	MBSTART { RET = aio_return(AIOCBP);} MBEND
#define AIO_SHIM_ERROR(AIOCBP, RET)							\
MBSTART {										\
	intrpt_state_t		prev_intrpt_state;					\
											\
	/* Defer interrupts around aio_error() to prevent pthread mutex deadlock. */	\
	DEFER_INTERRUPTS(INTRPT_IN_AIO_ERROR, prev_intrpt_state);			\
	RET = aio_error(AIOCBP); 							\
	ENABLE_INTERRUPTS(INTRPT_IN_AIO_ERROR, prev_intrpt_state);			\
} MBEND
#define SIGNAL_ERROR_IN_WORKER_THREAD(gdi, err_str, errno)	/* no-op, N/A */
#define CHECK_ERROR_IN_WORKER_THREAD(reg, udi)			/* no-op, N/A */

#else /* USE_LIBAIO */

#include "memcoherency.h"

error_def(ERR_DBFILERR);
error_def(ERR_SYSCALL);

void 	aio_shim_destroy(gd_addr *gd);
int 	aio_shim_write(gd_region *reg, struct aiocb *aiocbp);

#define AIO_SHIM_WRITE(REG, AIOCBP, RET) 	MBSTART { RET = aio_shim_write(REG, AIOCBP); } MBEND
#define AIO_SHIM_ERROR(AIOCBP, RET) 		MBSTART { RET = (AIOCBP)->status; } MBEND
#define AIO_SHIM_RETURN(AIOCBP, RET) 		MBSTART { SHM_READ_MEMORY_BARRIER; RET = (AIOCBP)->res; } MBEND
							/* Need a memory barrier here so that we can
							 * enforce publication safety of aiocbp->res
							 * after the status has been set by the
							 * AIOCBP_SET_FLDS macro.
							 */
#define AIOCBP_SET_FLDS(AIOCBP, RES, RES2)								\
MBSTART	{												\
	/* Note the user may not call aio_return before aio_error has a non-EINPROGRESS value.		\
	 * This means as long as aiocbp->res gets set before aiocbp->status, we are correctly		\
	 * adhering to the spec.									\
	 */												\
	aiocbp->res = RES;										\
	/* to enforce publication safety here and in aio_shim_return() */				\
	SHM_WRITE_MEMORY_BARRIER;									\
	aiocbp->status = RES2;										\
} MBEND

/* We will exit the worker thread and notify the main thread that an error
 * has occurred. This is so that we don't rts_error() at the same time that
 * commit logic is occurring, crashing and possibly causing database damage.
 * The next time a process calls wcs_wtfini() or aio_shim_write() the
 * database can safely issue an error.
 */
#define RECORD_ERROR_IN_WORKER_THREAD_AND_EXIT(gdi, err_str, errno)	\
MBSTART {								\
	gdi->err_syscall = err_str;					\
	SHM_WRITE_MEMORY_BARRIER;					\
	gdi->save_errno = errno;					\
	pthread_exit(NULL);						\
} MBEND

/* If there was an error in the worker thread, we should rts_error() */
#define CHECK_ERROR_IN_WORKER_THREAD(reg, udi)										\
MBSTART {														\
	struct gd_info 	*gdi, tmp;											\
															\
	gdi = udi->owning_gd->thread_gdi;										\
	if ((NULL != gdi) && (gdi->save_errno != 0))									\
	{	/* We close everything related to the worker thread, in hopes that when it is retried later there	\
		 * will be no more error										\
		 */													\
		SHM_READ_MEMORY_BARRIER;										\
		tmp = *gdi;												\
		aio_shim_destroy(udi->owning_gd);									\
		rts_error_csa(CSA_ARG(&udi->s_addrs) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,	\
			LEN_AND_STR(tmp.err_syscall), CALLFROM, tmp.save_errno);					\
	}														\
} MBEND

struct gd_info
{
	pthread_t	pt;
	int		exit_efd;	/* eventfd notifies on thread shutdown   */
	int		laio_efd;	/* eventfd notifies on libaio completion */
	aio_context_t 	ctx;		/* kernel context associated with AIO    */
	volatile int	num_ios;	/* Number of IOs in flight 		 */

	/* Note that errno must be set before what */
	volatile char 	*err_syscall;	/* If an error occurred, what was it? 	 */
	volatile int	save_errno;
};


#endif /* USE_LIBAIO */

#endif /* AIO_SHIM_H_INCLUDED */
