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
#include "io.h"
#include "io_params.h"

GBLREF io_pair io_std_device;
GBLREF bool	prin_out_dev_failure;

void flush_pio(void)
{
	/* there is no eol character in the flush prototype */
	/*unsigned char	p;*/

	if (io_std_device.out && !prin_out_dev_failure)	/* Some utility pgms don't have devices to flush */
	{ /* do not flush if we've encountered an error with io_std_device already so that we give user $ZT, or EXCEPTION
	   * a chance to execute */
		/*p = (unsigned char)iop_eol;
		(io_std_device.out->disp_ptr->flush)(io_std_device.out, &p);*/
		(io_std_device.out->disp_ptr->flush)(io_std_device.out);
	}
}
