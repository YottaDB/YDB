/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <prtdef.h>
#include <psldef.h>	/* for PSL$C_USER */
#include <ssdef.h>

#include "iosp.h"		/* for SS_NORMAL */
#include "gtm_logicals.h"
#include "trans_log_name.h"	/* for trans_log_name() prototype */
#include "gtm_env_init.h"	/* for gtm_env_init_sp() prototype */
#include "gtm_stdlib.h"		/* for STRTOL macro */
#include "gtm_string.h"		/* for strlen() */

GBLREF	uint4	gtm_memory_noaccess_defined;	/* count of the number of GTM_MEMORY_NOACCESS_ADDR logicals which are defined */
GBLREF	uint4	gtm_memory_noaccess[GTM_MEMORY_NOACCESS_COUNT];

void	gtm_env_init_sp(void)
{
	mstr			val, tn;
	char			buf[1024], lognamebuf[1024];
	uint4			mem_start, status, count;

	/* Check if the logical GTM_MEMORY_NOACCESS_ADDR0 is defined. If so note the value down.
	 * Continue to do this for as many environment variables as are defined with a max. limit of
	 * 	GTM_MEMORY_NOACCESS_COUNT (currently 8) (i.e. stop at GTM_MEMORY_NOACCESS_ADDR3)
	 * gtm_expreg() will later look at these values before expanding the virtual address space.
	 */
	for (count = 0; count < GTM_MEMORY_NOACCESS_COUNT; count++)
	{
		assert(0 == gtm_memory_noaccess[count]);
		mem_start = 0;
		SPRINTF(lognamebuf, "%s%d", GTM_MEMORY_NOACCESS_ADDR, count);
		val.addr = lognamebuf;
		val.len = (int)strlen(lognamebuf);
		if (SS_NORMAL == (status = trans_log_name(&val, &tn, buf)))
			mem_start = STRTOL(buf, NULL, 16);
		if (!mem_start)
			break;	/* break if the environment variable is not defined properly */
		gtm_memory_noaccess[count] = mem_start;
	}
	gtm_memory_noaccess_defined = count;
	assert(GTM_MEMORY_NOACCESS_COUNT >= gtm_memory_noaccess_defined);
}
