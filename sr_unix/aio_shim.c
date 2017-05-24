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
#include "mdef.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "dpgbldir.h"
#include "gbldirnam.h"
#include "sleep_cnt.h"
#include "mutex.h"
#include "wcs_wt.h"
#include "interlock.h"
#include "performcaslatchcheck.h"
#include "gtm_rel_quant.h"
#include "wcs_sleep.h"
#include "aio_shim.h"
#include "gtm_signal.h"

#ifdef USE_LIBAIO
#include "gtm_socket.h"
#include <sys/eventfd.h>
#include "gtm_time.h"
#include <stdint.h>
#include "gtm_poll.h"
#include "gtm_stdlib.h"

/* aio_shim.c: serves as a "shim" between both POSIX AIO and Linux AIO
 * interfaces. Because POSIX AIO is truly asynchronous from the client's
 * perspective, whereas Linux AIO requires io_getevents() to be called
 * at appropriate intervals, we have the following approach:
 *
 * * A multiplexing thread  will service a single Linux kernel context
 *   and exactly one global directory.
 *   * Each global directory has one context and one thread.
 *   * The first global directory to claim the region (via udi->owning_gd)
 *     will be the one to service the IO.
 *   * If a global directory leaves, and another global directory has control
 *     over the same region, we clean up everything so that the next write
 *     into that region will set up the kernel context for the second global
 *     directory.
 */

GBLREF  char 		*aio_shim_errstr;
GBLREF	char		io_setup_errstr[];
GBLREF	int		num_additional_processors;
GBLREF	uint4		process_id;
GBLREF	boolean_t	multi_thread_in_use;
#ifdef 	DEBUG
GBLREF	boolean_t	gtm_jvm_process;
GBLREF	int		process_exiting;
#endif
GBLREF	mstr		extnam_str;
GBLREF	mval		dollar_zgbldir;
GBLREF  boolean_t	blocksig_initialized;
GBLREF  sigset_t 	block_worker;

#define	MAX_EVENTS 	100	/* An optimization to batch requests -- the
				 * maximum number of completed IO's that come
				 * back to us from io_getevents() at a time.
				 */
#define EVENTFD_SZ	8	/* The number of bytes needed to trigger an eventfd */
#define MAX_WIP_TRIES	3	/* Number of times to try to clear the WIP queue in
				 * event of a wtfini_in_prog
				 */

error_def(ERR_DBFILERR);
error_def(ERR_SYSCALL);

/* Used by the multiplexing thread to keep track of state and the pipes to
 * communicate with the main process.
 */
enum
{
	EXIT_EFD,
	LAIO_EFD
};

/* checks that aiocb and iocb are equivalent */
#define CHECK_OFFSETOF_FLD(x) ((offsetof(struct aiocb, x) == offsetof(struct iocb, x))			\
				&& (SIZEOF(((struct aiocb *)0)->x) == SIZEOF(((struct iocb *)0)->x)))
#define CHECK_STRUCT_AIOCB					\
MBSTART {							\
	assert(CHECK_OFFSETOF_FLD(aio_data) &&			\
	       CHECK_OFFSETOF_FLD(aio_key) &&			\
	       CHECK_OFFSETOF_FLD(aio_lio_opcode) &&		\
	       CHECK_OFFSETOF_FLD(aio_reqprio) &&		\
	       CHECK_OFFSETOF_FLD(aio_fildes) &&		\
	       CHECK_OFFSETOF_FLD(aio_buf) &&			\
	       CHECK_OFFSETOF_FLD(aio_nbytes) &&		\
	       CHECK_OFFSETOF_FLD(aio_offset) &&		\
	       CHECK_OFFSETOF_FLD(aio_reserved2) &&		\
	       CHECK_OFFSETOF_FLD(aio_flags) &&			\
	       CHECK_OFFSETOF_FLD(aio_resfd));			\
} MBEND

#define CLEANUP_AIO_SHIM_THREAD_INIT(GDI)				\
MBSTART {								\
	int 	ret, save_errno;					\
									\
	save_errno = errno;						\
	if (FD_INVALID != (GDI).exit_efd)				\
	{								\
		CLOSEFILE_RESET((GDI).exit_efd, ret);			\
		assert(0 == ret);					\
	}								\
	if (FD_INVALID != (GDI).laio_efd)				\
	{								\
		CLOSEFILE_RESET((GDI).laio_efd, ret);			\
		assert(0 == ret);					\
	}								\
	if (0 != (GDI).ctx)						\
	{								\
		ret = io_destroy((GDI).ctx);				\
		assert(0 == ret);					\
	}								\
	errno = save_errno;						\
} MBEND

#define io_setup(nr, ctxp) syscall(SYS_io_setup, nr, ctxp)
#define io_destroy(ctx) syscall(SYS_io_destroy, ctx)
#define io_submit(ctx, nr, iocbpp) syscall(SYS_io_submit, ctx, nr, iocbpp)
#define io_getevents(ctx, min_nr, max_nr, events, timeout) syscall(SYS_io_getevents, ctx, min_nr, max_nr, events, timeout)

#define ATOMIC_SUB_FETCH(ptr, val) INTERLOCK_ADD(ptr, NULL,-val)
#define ATOMIC_ADD_FETCH(ptr, val) INTERLOCK_ADD(ptr, NULL, val)

/* Note that ERROR_LIT MUST be a literal */
#define ISSUE_SYSCALL_RTS_ERROR_WITH_GD(GD, ERROR_LIT, SAVE_ERRNO)			\
MBSTART {										\
	mstr		*gldname;							\
	mstr 		gld_str_tmp;							\
	char		err_buffer[GTM_PATH_MAX + SIZEOF(ERROR_LIT) + 3];		\
	/* save errno in case SNPRINTF modifies errno and SAVE_ERRNO passed		\
	 * in is "errno" in caller.							\
	 */										\
	int		lcl_save_errno = SAVE_ERRNO;					\
											\
	GET_CURR_GLD_NAME(gldname);							\
	DEBUG_ONLY({									\
		get_first_gdr_name(GD, &gld_str_tmp);					\
		assert(!strncmp(gld_str_tmp.addr, gldname->addr, gldname->len));	\
	});										\
	SNPRINTF(err_buffer, ARRAYSIZE(err_buffer), "%s(%*s)", ERROR_LIT,		\
			gldname->len, gldname->addr);					\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,			\
			LEN_AND_STR(err_buffer), CALLFROM, lcl_save_errno);		\
} MBEND

STATICFNDCL void *io_getevents_multiplexer(void *arg);
STATICFNDCL int io_getevents_internal(aio_context_t ctx);
STATICFNDCL void clean_wip_queue(unix_db_info *udi);
STATICFNDCL void aio_gld_clean_wip_queue(gd_addr *input_gd, gd_addr *match_gd);
STATICFNDCL int	aio_shim_setup(aio_context_t *ctx);
STATICFNDCL int aio_shim_thread_init(gd_addr *gd);

/* Routine performed only by the multiplexing thread. It polls on all file descriptors
 * and passes messages between the caller and the multiplexing thread to manage file
 * descriptors, etc.
 */
/* #GTM_THREAD_SAFE : The below function (io_getevents_multiplexer) is thread-safe */
STATICFNDCL void *io_getevents_multiplexer(void *arg)
{
	struct gd_info		*gdi = (struct gd_info *)arg;
	struct pollfd		fds[2];
	uint64_t		dummy;
	int			ret, num_ios;

	INIT_POLLFD(fds[LAIO_EFD], gdi->laio_efd);
	INIT_POLLFD(fds[EXIT_EFD], gdi->exit_efd);
	do
	{	/* we poll on the file descriptors */
		while ((-1 == (ret = poll(fds, ARRAYSIZE(fds), -1))) && (EINTR == errno))
			;
		assert(-1 != ret);
		if (-1 == ret)
			RECORD_ERROR_IN_WORKER_THREAD_AND_EXIT(gdi, "worker_thread::poll()", errno);
		/* Service the IO's if they completed */
		if (EVENTFD_NOTIFIED(fds, LAIO_EFD))
		{	/* flush the eventfd (though we don't care about the value) */
			DOREADRC(fds[LAIO_EFD].fd, &dummy, SIZEOF(dummy), ret);
			assert(0 == ret);
			if (-1 == ret)
				RECORD_ERROR_IN_WORKER_THREAD_AND_EXIT(gdi, "worker_thread::read()", errno);
			/* we subtract from num_ios all the IOs gleaned by
			 * io_getevents_internal().
			 */
			ret = io_getevents_internal(gdi->ctx);
			if (-1 == ret)
				RECORD_ERROR_IN_WORKER_THREAD_AND_EXIT(gdi, "worker_thread::io_getevents()", errno);
			num_ios = ATOMIC_SUB_FETCH(&gdi->num_ios, ret);
			assert(num_ios >= 0);
		}
		/* Exit if we have been notified via the exit eventfd */
		if (EVENTFD_NOTIFIED(fds, EXIT_EFD))
		{
			CLOSEFILE_RESET(gdi->laio_efd, ret);
			assert(0 == ret);
			CLOSEFILE_RESET(gdi->exit_efd, ret);
			assert(0 == ret);
			return NULL;
		}
	} while (TRUE);
}

/* Batches io_getevent() requests until there are no more for this particular context */
/* #GTM_THREAD_SAFE : The below function (io_getevents_internal) is thread-safe */
STATICFNDCL int io_getevents_internal(aio_context_t ctx)
{
	struct timespec 	timeout = { 0, 0 };
	struct io_event 	event[MAX_EVENTS];
	int 			ret, i;
	struct aiocb		*aiocbp;
	int			num_ios = 0;

	do
	{	/* Loop on EINTR. */
		while (-1 == (ret = io_getevents(ctx, 0, MAX_EVENTS, event, &timeout))
				&& (EINTR == errno))
			;
		assert(ret >= 0);
		if (-1 == ret)
			return -1;
		num_ios += ret;
		for (i = 0; i < ret; ++i)
		{
			aiocbp = (struct aiocb *)event[i].obj;
			if (0 <= event[i].res)
				AIOCBP_SET_FLDS(aiocbp, event[i].res, event[i].res2);
			/* res < 0 means LIBAIO is providing a negated errno through
			 * the "return" (res) value instead of the positive errno via res2.
			 * We place the negated value (which is the actual errno) in
			 * the third parameter of AIOCBP_SET_FLDS() which is where the
			 * errno is expected. In this situation res2 value is also
			 * returned for potential help in debugging.
			 */
			else
				AIOCBP_SET_FLDS(aiocbp, event[i].res2, -event[i].res);
		}
	} while (0 < ret);
	return num_ios;
}

/* Walks the WIP queue and "cancels" all outstanding IO's that have not yet
 * completed, and that belong to the current process.
 */
STATICFNDCL void clean_wip_queue(unix_db_info *udi)
{
	int 			spins, maxspins, retries, ret;
	int4			max_sleep_mask;
	cache_que_head_ptr_t	que_head;
	cache_state_rec_ptr_t   cstt;
	struct aiocb 		*aiocbp;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	struct gd_info		*gdi;
	node_local_ptr_t	cnl;
	int			num_ios, lcnt;
	int			num_tries;
	boolean_t		wtfini_in_prog, done;

	/* Walk through the WIP queue  and mark all processes with our process id and fd as ECANCELED.
	 * Note that this isn't strictly necessary: once we've exited another process doing "wcs_wtfini" will eventually
	 * discover that our process id is not alive anymore and reissue the write. However, doing this here avoids
	 * that salvage/reissue logic in "wcs_wtfini" from kicking in.
	 */
	gdi = udi->owning_gd->thread_gdi;
	assert(gdi->num_ios > 0); 	/* caller should have ensured this */
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	max_sleep_mask = -1;	/* initialized to -1 to defer memory reference until needed */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	que_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
	/* Grab the WIP queue lock, similarly to wcs_get_space.c where we lock the active queue header. */
	for (num_tries = 0, done = FALSE; !done && (num_tries < MAX_WIP_TRIES); num_tries++)
	{
		if (grab_latch(&que_head->latch, WT_LATCH_TIMEOUT_SEC))
		{	/* walk the WIP queue and strike all csr's that have our epid and file descriptor,
			 * and if aio_shim_error(aiocbp) == EINPROGRESS.
			 */
			for (cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)que_head + que_head->fl);
			     (cstt != (cache_state_rec_ptr_t)que_head);
			      cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.fl))
			{
				if ((cstt->epid == process_id) && ((aiocbp = &cstt->aiocb)->aio_fildes == udi->fd))
				{
					AIO_SHIM_ERROR(aiocbp, ret);
					if (EINPROGRESS == ret)
					{
						AIOCBP_SET_FLDS(aiocbp, -1, ECANCELED);
						num_ios = ATOMIC_SUB_FETCH(&gdi->num_ios, 1);
						assert(num_ios >= 0);
						if (0 == num_ios)
							break;
					}
				}
			}
			/* Make sure that a wcs_wtfini() is not currently in progress. It's possible that
			 * wcs_wtfini() has already pulled an element off the WIP queue, and in the meantime
			 * we got the swaplock. This means our view of the WIP queue is missing a cr, and we
			 * must retry to cancel that cr.
			 */
			wtfini_in_prog = cnl->wtfini_in_prog;
			rel_latch(&que_head->latch);
			if (wtfini_in_prog)
			{	/* Wait 5 seconds, or until wtfini_in_prog becomes 0. */
				for (lcnt = 1; (lcnt < SLEEP_FIVE_SEC) && (cnl->wtfini_in_prog); ++lcnt)
					wcs_sleep(1);
				if (!cnl->wtfini_in_prog)
				{	/* This forces us to retry on the swaplock, up to MAX_WIP_TRIES */
					done = FALSE;
				} else
				{ 	/* We waited 5 seconds, and in that time wtfini_in_prog stayed non-zero.
					 * We'll simply exit as our writes will be cleaned up once the process
					 * dies anyway. We don't expect this edge case to occur in testing, so
					 * we leave the assert gd->thread_gdi->num_ios == 0 in aio_shim_destroy()
					 * as-is.
					 */
					done = TRUE;
				}
			} else
				done = TRUE; /* A wtfini was not in progress, so our WIP queue is now clean. */
		} else
			done = TRUE; /* Couldn't grab the lock, but we leave as this case will be handled later. */
	}
}

/* Helper method to initialize the AIO kernel context */
STATICFNDCL int	aio_shim_setup(aio_context_t *ctx)
{
	int 	ret;
	int	nr_events;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* initialize num_requests from the environment variable or otherwise */
	nr_events = TREF(gtm_aio_nr_events);
	/* We try io_setup() in a loop, and each failed attempt we reduce the amount of space
	 * we're asking for. If there is no space to give, just return -1 EAGAIN
	 */
	while (TRUE)
	{
		*ctx = 0;
		ret = io_setup(nr_events, ctx);
		assert((0 == ret) || (EAGAIN == errno));
		if (-1 == ret)
		{
			nr_events /= 2;
			if (0 == nr_events)
			{
				aio_shim_errstr = io_setup_errstr;
				return -1;
			}
		} else
			return 0;
	}
}

/* Initializes the multiplexing thread and the shutdown eventfd. */
STATICFNDCL int aio_shim_thread_init(gd_addr *gd)
{ 	/* Sets up the AIO context, two eventfd's and the multiplexing thread */
	int		ret, ret2;
	struct gd_info	*gdi, tmp_gdi;
	sigset_t	savemask;

	CHECK_STRUCT_AIOCB;
	DEBUG_ONLY(aio_shim_errstr = NULL;)
	/* initialize fields of tmp_gdi */
	tmp_gdi.exit_efd = FD_INVALID;
	tmp_gdi.laio_efd = FD_INVALID;
	tmp_gdi.ctx = 0;
	tmp_gdi.num_ios = 0;
	tmp_gdi.err_syscall = NULL;
	tmp_gdi.save_errno = 0;
	/* Sets up the eventfd which notifies the multiplexing thread that it must exit.  */
	if (-1 != (ret = eventfd(0, 0)))
		tmp_gdi.exit_efd = ret;
	else
	{
		assert(FALSE);
		aio_shim_errstr = "eventfd(EXIT_EFD)";
		return -1;
	}
	/* Sets up the eventfd which notifies the multiplexing thread that an AIO completed. */
	if (-1 != (ret = eventfd(0, 0)))
		tmp_gdi.laio_efd = ret;
	else
	{
		assert(FALSE);
		CLEANUP_AIO_SHIM_THREAD_INIT(tmp_gdi);
		aio_shim_errstr = "eventfd(LAIO_EFD)";
		return -1;
	}
	/* Sets up the AIO context */
	if (-1 == (ret = aio_shim_setup(&tmp_gdi.ctx)))
	{	/* The only "allowed" error is EAGAIN. The errstr should have been set by
		 * aio_shim_setup().
		 */
		assert(NULL != aio_shim_errstr);
		assert(EAGAIN == errno);
		CLEANUP_AIO_SHIM_THREAD_INIT(tmp_gdi);
		return -1;
	}
	gdi = gtm_malloc(SIZEOF(struct gd_info));
	*gdi = tmp_gdi;
	/* Temporarily block external signals for the worker thread. This enforces that
	 * the multiplexing worker thread will not invoke signal handlers (e.g. timer_handler())
	 * and manipulate global variables while the main process/thread is concurrently running
	 * and using them.
	 * Note that SIGPROCMASK relies on multi_thread_in_use and so we must set it.
	 * TODO: this code should ideally be merged with gtm_multi_thread.c When gtm_multi_thread()
	 * is enhanced to create a thread in the background. Right now it returns only when the
	 * thread completes.
	 */
	multi_thread_in_use = TRUE;
	assert(TRUE == blocksig_initialized);
	/* We block all signals, except those which could be generated from within the worker thread.
	 * Every other signal must be externally generated and should drive the signal handler from
	 * the main process and not this worker thread.
	 */
	SIGPROCMASK(SIG_BLOCK, &block_worker, &savemask, ret);
	ret = pthread_create(&gdi->pt, NULL, io_getevents_multiplexer, gdi);
	if (0 != ret)
	{
		/* We don't want to clobber ret so we use ret2. */
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, ret2);
		multi_thread_in_use = FALSE;
		assert(EAGAIN == ret);
		CLEANUP_AIO_SHIM_THREAD_INIT(tmp_gdi);
		gtm_free(gdi);
		aio_shim_errstr = "pthread_create()";
		errno = ret;	/* pthread_create() returns errno in ret. */
		return -1;
	}
	SIGPROCMASK(SIG_SETMASK, &savemask, NULL, ret);
	multi_thread_in_use = FALSE;
	gd->thread_gdi = gdi;
	return 0;
}

/* Similar to aio_cancel(), cancels all outstanding IO's by destroying the kernel context
 * associated with the region. Also destroys the multiplexing thread to clean resources.
 */
void aio_shim_destroy(gd_addr *gd)
{
	struct gd_info 	*gdi;
	int		ret;
	char 		*eventfd_str = "GTMROCKS";
	mstr		*gldname;
	gd_addr		*addr_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gdi = gd->thread_gdi;
	if (NULL == gdi)
	{	/* A write didn't happen. */
		return;
	}
	/* We notify the thread to exit; note we only need to write 8 bytes (exactly) to the fd. */
	assert(EVENTFD_SZ == STRLEN(eventfd_str));
	DOWRITERC(gdi->exit_efd, eventfd_str, EVENTFD_SZ, ret);
	assert(0 == ret);
	if (-1 == ret)
		ISSUE_SYSCALL_RTS_ERROR_WITH_GD(gd, "aio_shim_destroy::write", errno);
	/* Wait on the thread exit */
	ret = pthread_join(gdi->pt, NULL);
	assert(0 == ret);
	if (0 != ret)
		ISSUE_SYSCALL_RTS_ERROR_WITH_GD(gd, "aio_shim_destroy::pthread_join", errno);
	/* Destroy the kernel context */
	ret = io_destroy(gdi->ctx);
	assert(0 == ret);
	if (-1 == ret)
		ISSUE_SYSCALL_RTS_ERROR_WITH_GD(gd, "aio_shim_destroy::io_destroy", errno);
	/* If there was at least one region with reg->was_open = TRUE, then it is possible regions in other glds
	 * (different from "gd" have a "udi" with "udi->owning_gd" == "gd". So we would need to look at all regions
	 * across all glds opened by this process. If no was_open region was ever seen by this process, then it is
	 * enough to look at regions in just the current gld ("gd").
	 */
	if (TREF(was_open_reg_seen))
	{
		/* Iterate over all the regions in the global directory and clean their WIP queues */
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			aio_gld_clean_wip_queue(addr_ptr, gd);
	} else
		aio_gld_clean_wip_queue(gd, gd);
	/* By this point, we must have no more outstanding IOs */
	assert(0 == gdi->num_ios);
	/* Delete the thread_gdi to leave us in a state consistent with no thread existing. */
	gtm_free(gdi);
	gd->thread_gdi = NULL;
}

void	aio_gld_clean_wip_queue(gd_addr *input_gd, gd_addr *match_gd)
{
	unix_db_info	*udi;
	gd_region 	*reg, *r_top;
	struct gd_info 	*gdi;

	gdi = match_gd->thread_gdi;
	assert(NULL != gdi);
	for (reg = input_gd->regions, r_top = reg + input_gd->n_regions; reg < r_top; reg++)
	{
		assert(input_gd == reg->owning_gd);
		if (reg->open && reg->dyn.addr->asyncio && (dba_cm != reg->dyn.addr->acc_meth))
		{
			udi = FILE_INFO(reg);
			/* We don't call clean_wip_queue() if we don't have any outstanding IOs in the region.
			 * This reduces wip queue header lock contention if lots of processes exit at the same time.
			 * Note that in case multiple regions map to same db file, it is possible udi->owning_gd
			 * points to a gld different from "match_gd". In that case, skip the wip queue clean as we are
			 * interested only in cleaning up aio writes issued from "match_gd".
			 */
			if ((udi->owning_gd == match_gd) && (0 < gdi->num_ios))
				clean_wip_queue(udi);
		}
	}
}

/* Lazily loads the multiplexing thread and submits an IO. */
int aio_shim_write(gd_region *reg, struct aiocb *aiocbp)
{
	unix_db_info 	*udi;
	struct gd_info 	*gdi;
	gd_addr		*owning_gd;
	int		ret;
	struct iocb 	*iocbp;
	struct iocb 	*cb[1];

	udi = FILE_INFO(reg);
	assert(gtm_is_main_thread() || (gtm_jvm_process && process_exiting));
	owning_gd = udi->owning_gd;
	assert(NULL != owning_gd);
	if (NULL == (gdi = owning_gd->thread_gdi))
	{	/* No thread is servicing this global directory -- set it up. */
		ret = aio_shim_thread_init(owning_gd);
		assert((0 == ret) || (EAGAIN == errno));
		if (-1 == ret)
		{	/* aio_shim_thread_init() should set aio_shim_errstr */
			assert(NULL != aio_shim_errstr);
			/* Caller will handle -1 case by calling wcs_wterror() which
			 * looks at aio_shim_errstr to report error detail.
			 */
			return -1;
		}
		gdi = owning_gd->thread_gdi;
	}
	assert(NULL != gdi);
	/* submit the write */
	CHECK_ERROR_IN_WORKER_THREAD(reg, udi);
	aiocbp->status = EINPROGRESS;
	iocbp = (struct iocb *)aiocbp;
	iocbp->aio_lio_opcode = IOCB_CMD_PWRITE;
	iocbp->aio_resfd = gdi->laio_efd;
	iocbp->aio_flags = IOCB_FLAG_RESFD;
	cb[0] = iocbp;
	ATOMIC_ADD_FETCH(&gdi->num_ios, 1);
	ret = io_submit(gdi->ctx, 1, cb);
	/* the only acceptable error is EAGAIN in our case */
	assert((1 == ret) || (EAGAIN == errno));
	if (1 == ret)
		return 0;
	/* we need to rescind the write */
	ATOMIC_SUB_FETCH(&gdi->num_ios, 1);
	aio_shim_errstr = "io_submit()";
	return -1;
}

#endif
