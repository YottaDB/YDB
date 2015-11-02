/****************************************************************
 *                                                              *
 *      Copyright 2007, 2011 Fidelity Information Services, Inc	*
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
#include "sockint_stats.h"

GBLREF	int	process_id;
GBLREF  int     socketus_interruptus;

/* Print stats on how many times socket processing was interrupted by mupip interrupt. Note
 * that this routine stands alone so that inclusion of it in mupip, lke, etc by print_exit_stats()
 * doesn't pull in all the socket code plus its dependencies..
 */
void sockint_stats(void)
{
	if (IS_GTM_IMAGE)	/* Only give these stats for GTM/MUMPS image */
		FPRINTF(stderr, "Socket read/wait for process %d was interrupted %d times\n", process_id, socketus_interruptus);
}
