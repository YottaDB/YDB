/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc       *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "gtmimagename.h"
#include "pipeint_stats.h"

GBLREF	int			process_id;

/* Print stats on how many times pipe processing was interrupted by mupip interrupt. Note
   that this routine stands alone so that inclusion of it in mupip, lke, etc by print_exit_stats()
   doesn't pull in all the pipe code plus its dependencies..
*/
void pipeint_stats(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	if (IS_GTM_IMAGE)	/* Only give these stats for GTM/MUMPS image */
		FPRINTF(stderr, "Pipe/Fifo read for process %d was interrupted %d times\n", process_id, TREF(pipefifo_interrupt));
}
