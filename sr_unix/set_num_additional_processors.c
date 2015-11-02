/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "set_num_additional_processors.h"

GBLREF int	num_additional_processors;

void set_num_additional_processors(void)
{
	long numcpus;
	error_def(ERR_NUMPROCESSORS);

#ifdef __hpux
	struct pst_dynamic psd;
	if (pstat_getdynamic(&psd, SIZEOF(psd), (size_t)1, 0) == -1)
	{
		send_msg(VARLSTCNT(1) ERR_NUMPROCESSORS);
                numcpus = 1;
        }
	else
		numcpus = psd.psd_proc_cnt;
#else
#ifdef __MVS__
	numcpus=*(TYPE_OF_NUM_CPUS *)((*(int *)((*(int *)CVT_ADDR)+OFFSET_IN_CVT_OF_CSD_ADDR))+OFFSET_IN_CSD_OF_NUM_CPUS_ADDR);
#else
	if ((numcpus = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
	{
		send_msg(VARLSTCNT(1) ERR_NUMPROCESSORS);
		numcpus = 1;
	}
#endif
#endif
	num_additional_processors = (int4)(numcpus - 1);
}

