/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "min_max.h"

GBLREF int4		rundown_os_page_size;

boolean_t probe(uint4 len, sm_uc_ptr_t addr, boolean_t write)
{
	sm_uc_ptr_t	top;
	uint4		status;

	if ((int4)len <= 0)
		return FALSE;
	top = addr + len;
	if (addr >= top)	/* in case addr + len resulted in a top that wrapped (was beyond the representable range) */
		return FALSE;
	/* the following MUST use ROUND_DOWN2 to avoid bringing in the C RTL to help with integer division;
	 if any RTL comes into GTMSECSHR, the link looks fine, but the loader doesn't and the 1st call to SYS$ ACCVIOs */
	len = MIN(ROUND_DOWN2((sm_ulong_t)addr, 2 * rundown_os_page_size) + (2 * rundown_os_page_size) - (sm_ulong_t)addr, len);
	for (; addr < top; )
	{	/* check every page from addr to addr + len - 1, at most, 2 at a time,
		 * because probe just checks the first and last bytes of the interval specified by its arguments */
		if (write)
			status = probew(len > 1 ? len - 1 : 0, addr);
		else
			status = prober(len > 1 ? len - 1 : 0, addr);
		if (!status)
			return FALSE;
		addr += len;
		len = MIN(top - addr, 2 * rundown_os_page_size);
	}
	return TRUE;
}
