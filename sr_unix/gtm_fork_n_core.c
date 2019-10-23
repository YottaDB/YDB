/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* This routine may do a malloc() to get space for the MM file header which usually will not be
 * available in a core file. There is no benefit to running gtm_malloc() here instead of the
 * system malloc and doing so would actually produce potential error handling implications so
 * use the system malloc() here (since the process is micro-milli-bleems from dying with a
 * core anyway to avoid error handling issues. Use system malloc by un-defining the redefinition.
 */
#undef malloc

#include "gtm_signal.h"

#include <errno.h>
#include <sys/wait.h>

#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_string.h"

#include "error.h"
#include "eintr_wrappers.h"
#ifdef DEBUG
#include <sys/resource.h>
#include <sys/time.h>
#include "gtmio.h"
#endif
#include "gtmmsg.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "dpgbldir.h"

GBLREF boolean_t	created_core;		/* core file was created */
GBLREF unsigned int	core_in_progress;
GBLREF int4		exi_condition;
GBLREF sigset_t		blockalrm;
#ifdef DEBUG
GBLREF sgmnt_addrs	*cs_addrs;
#endif

error_def(ERR_COREINPROGRESS);
error_def(ERR_NOFORKCORE);

#define MM_MALLOC_ALREADY_TRIED	(sgmnt_data_ptr_t)-1

void gtm_fork_n_core(void)
{
	struct sigaction	act, intr;
	pid_t			childid, waitrc;
	int			rc, status, save_errno;
	sigset_t		savemask;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd, tmp_csd;
	gd_region		*reg, *r_top;
	intrpt_state_t		prev_intrpt_state;
	DEBUG_ONLY(struct rlimit rlim;)

#	ifdef DEBUG
	if (simpleThreadAPI_active || multi_thread_in_use)
	{	/* Either of the conditions in the "if" check imply this process has more than one thread active.
		 * If we do a "fork_n_core", we would only get the C-stack of the current thread (since a "fork"
		 * does not inherit the C-stack of all active threads). Therefore in debug builds at least,
		 * dump a core right away so we have more information for debugging.
		 *
		 * Note that this means a user directly invoking "ydb_fork_n_core" through the SimpleThreadAPI
		 * in a debug build of YottaDB will get a core dump that terminates the process (with the C-stack
		 * of all threads) whereas the same invocation in a pro build of YottaDB will create a core file
		 * (with just the C-stack of the current thread) and the process will continue from where it left off.
		 * This difference in behavior is considered acceptable since, in debug builds, the focus is more on
		 * getting the first point of failure with the most information (C-stack of all threads).
		 */
		DUMP_CORE;	/* will not return */
	}
	getrlimit(RLIMIT_CORE, &rlim);
	if ( rlim.rlim_cur != rlim.rlim_max)
	{
		if (RLIM_INFINITY == rlim.rlim_max)
			rlim.rlim_cur = RLIM_INFINITY;
		else
		{
			if (rlim.rlim_cur < rlim.rlim_max)
			{
				rlim.rlim_cur = rlim.rlim_max;
			}
		}
		setrlimit(RLIMIT_CORE, &rlim);
	}
#	endif
	if (core_in_progress++)
	{
		if (1 == core_in_progress)
		{	/* only report once */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_COREINPROGRESS, 0);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_COREINPROGRESS, 0);
		}
		return;
	}
	/* ignore interrupts */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, &intr);

	/* block SIGALRM signal */
	SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);

	/* FORK() clears timers in the child, which shouldn't be necessary here since we have SIGALRM blocked,
	 * and it disrupts diagnosis of any timer related issues, so inline the parts we want here.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
	childid = fork();							/* BYPASSOK */
	/* Only ENABLE_INTERRUPTS() in the parent, below, as we don't want any deferred handlers firing in the child. */
	if (childid)
	{
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		if (-1 == childid)
		{	/* restore interrupt handler */
			sigaction(SIGINT, &intr, 0);
			SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_NOFORKCORE, 0, errno);
			return;		/* Fork failed, no core done */
		}

		if (NULL != cs_addrs && NULL != cs_addrs->nl)
		{
			DBG_PRINT_BLOCK_INFOS(cs_addrs->nl);
		}

		WAITPID(childid, &status, 0, waitrc);
		save_errno = errno;
		/* restore interrupt handler */
		sigaction(SIGINT, &intr, 0);
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
		--core_in_progress;
		if (-1 == waitrc)
		{	/* If got error from waitpid, core may or may not have been taken. Assume worst & don't set flag */
			errno = save_errno;
			PERROR("YDB-E-FORKCOREWAIT");
		} else
			created_core = TRUE;
	} else
	{
		intrpt_ok_state = prev_intrpt_state;	/* Restore from DEFER_INTERRUPTS */
		DUMP_CORE;	/* This will (should) not return */
		UNDERSCORE_EXIT(-1);	/* Protection to kill fork'd process with no rundown by exit handler(s) */
	}
}
