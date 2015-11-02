/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T */
#ifndef __MVS__
#include <sys/param.h>

#ifdef __hpux
#include <sys/pstat.h>
#else
#include "gtm_unistd.h"
#endif
#else  /* __MVS__ */
#define CVT_ADDR                       0x10  	/* -> to CVT */
#define OFFSET_IN_CVT_OF_CSD_ADDR      0x294  	/* CVT+294 -> CSD */
#define OFFSET_IN_CSD_OF_NUM_CPUS_ADDR 0x0a   	/* CSD+a #cpus online */
#define TYPE_OF_NUM_CPUS               short
#endif
#include "send_msg.h"
#include "gtmio.h"
#include "have_crit.h"
#include "eintr_wrappers.h"
#include "set_num_additional_processors.h"
#ifdef DEBUG
#include "io.h"
#include "gtm_stdio.h"
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "deferred_signal_handler.h"
#endif

GBLREF int		num_additional_processors;

error_def(ERR_NUMPROCESSORS);

void set_num_additional_processors(void)
{
	long numcpus;

#	ifdef __hpux
	struct pst_dynamic psd;
	DEFER_INTERRUPTS(INTRPT_IN_SET_NUM_ADD_PROCS);
#	  ifdef DEBUG
	if (gtm_white_box_test_case_enabled
		&& (WBTEST_SYSCONF_WRAPPER == gtm_white_box_test_case_number))
	{
		DBGFPF((stderr, "will sleep indefinitely now\n"));
		while (TRUE)
			LONG_SLEEP(60);
	}
#	  endif
	if (pstat_getdynamic(&psd, SIZEOF(psd), (size_t)1, 0) == -1)
	{
		send_msg(VARLSTCNT(1) ERR_NUMPROCESSORS);
                numcpus = 1;
        }
	else
		numcpus = psd.psd_proc_cnt;
	ENABLE_INTERRUPTS(INTRPT_IN_SET_NUM_ADD_PROCS);
#	else
#	  ifdef __MVS__
#	    ifdef DEBUG
	DEFER_INTERRUPTS(INTRPT_IN_SET_NUM_ADD_PROCS);
	if (gtm_white_box_test_case_enabled
		&& (WBTEST_SYSCONF_WRAPPER == gtm_white_box_test_case_number))
	{
		DBGFPF((stderr, "will sleep indefinitely now\n"));
		while (TRUE)
			LONG_SLEEP(60);
	}
	ENABLE_INTERRUPTS(INTRPT_IN_SET_NUM_ADD_PROCS);
#	    endif
	numcpus = *(TYPE_OF_NUM_CPUS *)((*(int *)((*(int *)CVT_ADDR) + OFFSET_IN_CVT_OF_CSD_ADDR))
			+ OFFSET_IN_CSD_OF_NUM_CPUS_ADDR);
#	  else
	SYSCONF(_SC_NPROCESSORS_ONLN, numcpus);
	if (numcpus == -1)
	{
		send_msg(VARLSTCNT(1) ERR_NUMPROCESSORS);
		numcpus = 1;
	}
#	  endif
#	endif
	num_additional_processors = (int)(numcpus - 1);
}

