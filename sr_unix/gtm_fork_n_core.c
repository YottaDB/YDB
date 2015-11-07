/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

#include <errno.h>
#include <signal.h>
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
#endif
#include "gtmmsg.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "dpgbldir.h"
#include "fork_init.h"

GBLREF boolean_t	created_core;		/* core file was created */
GBLREF unsigned int	core_in_progress;
GBLREF int4		exi_condition;
GBLREF sigset_t		blockalrm;

error_def(ERR_COREINPROGRESS);
error_def(ERR_NOFORKCORE);

#define MM_MALLOC_ALREADY_TRIED	(sgmnt_data_ptr_t)-1

#ifdef AIX_SYSTRACE_ENABLE
#	ifndef _AIX
#	  error "Unsupported platform for SYSTRACE_ENABLE"
#	endif
/* Maximum number of trace files to save/rename if SYSTRACE_ENABLE is specified */
#define SYSTRACE_MAX 10
#endif

void gtm_fork_n_core(void)
{
	struct sigaction	act, intr;
	pid_t			childid, waitrc;
	int			status, save_errno;
#ifdef AIX_SYSTRACE_ENABLE
        struct stat             fs1;
        char                    oldname[1024], newname[1024], *trcpath, *trcsuffix;
	unsigned char		*p;
#endif
	sigset_t		savemask;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd, tmp_csd;
	gd_region		*reg, *r_top;
	gd_addr			*addr_ptr;
DEBUG_ONLY( struct rlimit rlim;)

	DEBUG_ONLY(
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
	)

#ifdef AIX_SYSTRACE_ENABLE
	/* Stop the current trace (aix), rename trace file and restart it.
	   To use this facility:
	   1. The $gtm_trace environment variable must be set to the directory where the
	      trace files are to reside.
	   2. The system trace must be started with the following command PRIOR to execution
	      of the mumps executable:

	   trace -a -L 8000000 -o $gtm_trace/systrace

	   This will generate an 8Mb file for each of SYSTRACE_MAX system trace files (total files
	   is SYSTRACE_MAX + 1). Make sure the directory has the room for it. In its present
	   incarnation, we only stop/rename the trace file on a sig-11.

	   Note this code has not been modified to use politically correct wrapper macros since
	   it is intended ONLY for use on AIX.
	*/
	if (SIGSEGV == exi_condition)
	{
		system("trcstop");
		trcpath = getenv("gtm_trace");
		if (trcpath)
		{
			strcpy(oldname, trcpath);	/* copy path name */
			strcat(oldname, "/systrace");	/* add file name */
			strcpy(newname, oldname);	/* copy 'to' file */

			/* Verify that the trace file we think was just created actually was */
			status = stat(oldname, &fs1);
			if (0 == status)
			{	/* We have the file. See if we can rename it */
				trcsuffix = newname + strlen(newname);	/* point to null at end of line */
				status = -1;
				for (suffix = 1; 0 != status && suffix <= SYSTRACE_MAX; ++suffix)
				{
					p = i2asc(trcsuffix, suffix);
					*p = 0;

					status = stat(newname, &fs1);		/* This file exist ? */
					if (0 != status)
						status = RENAME(oldname, newname); /* No, attempt the rename */
					else
						status = -1;			/* Yes, reset status for another iteration */
				}
			}
			strcpy(newname, "trace -a -L 8000000 -o ");
			strcat(newname, oldname);
			system(newname);
		}
	}
#endif
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
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);

	FORK(childid);	/* BYPASSOK: we exit immediately, no FORK_CLEAN needed */
	if (childid)
	{
		if (-1 == childid)
		{	/* restore interrupt handler */
			sigaction(SIGINT, &intr, 0);
			sigprocmask(SIG_SETMASK, &savemask, NULL);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_NOFORKCORE, 0, errno);
			return;		/* Fork failed, no core done */
		}
		WAITPID(childid, &status, 0, waitrc);
		save_errno = errno;
		/* restore interrupt handler */
		sigaction(SIGINT, &intr, 0);
		sigprocmask(SIG_SETMASK, &savemask, NULL);
		--core_in_progress;
		if (-1 == waitrc)
		{	/* If got error from waitpid, core may or may not have been taken. Assume worst & don't set flag */
			errno = save_errno;
			PERROR("GTM-E-FORKCOREWAIT");
		} else
			created_core = TRUE;
	} else
	{
		DUMP_CORE;	/* This will (should) not return */
		_exit(-1);	/* Protection to kill fork'd process with no rundown by exit handler(s) */
	}
}


