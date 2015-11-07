/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rms.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmmsg.h"

error_def(ERR_DBFILOPERR);

/* If a routine other than mupip integ uses this module, the interface and error reporting may need to change. */
gtm_uint64_t gds_file_size(file_control *fc)
{
	vms_gds_info	*gds_info;
	uint4		status;
	unsigned int	ret_size;
	struct FAB	fab;
	struct XABFHC	xab;

	gds_info = (vms_gds_info *)fc->file_info;
	if (gds_info->xabfhc)
	{	/* if sys$open fails, we will be returning xab$l_ebk that was prior existing and printing an error message. */
		ret_size = gds_info->xabfhc->xab$l_ebk;

		fab		= cc$rms_fab;
		xab		= cc$rms_xabfhc;
		fab.fab$l_xab	= &xab;
		fab.fab$l_fna	= gds_info->fab->fab$l_fna;
		fab.fab$b_fns	= gds_info->fab->fab$b_fns;
		fab.fab$b_fac	= FAB$M_GET;
		fab.fab$b_shr   = FAB$M_SHRPUT | FAB$M_SHRGET;
		if (RMS$_NORMAL == (status = sys$open(&fab)))
		{
			ret_size = xab.xab$l_ebk;
			status = sys$close(&fab);
		}
		if (RMS$_NORMAL != status)
			gtm_putmsg(VARLSTCNT(8) ERR_DBFILOPERR, 2, FAB_LEN_STR(&fab), 0, status);
	} else
		ret_size = 0;
	return (gtm_uint64_t)ret_size;
}
