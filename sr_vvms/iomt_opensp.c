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
#include "iottdef.h"
#include "iomtdef.h"
#include <descrip.h>
#include <ssdef.h>

uint4  iomt_opensp(
io_log_name  *dev_name,
d_mt_struct *mtdef)
{
	uint4  status, channel;
	$DESCRIPTOR(file_name,"");

	file_name.dsc$a_pointer = dev_name->dollar_io;
	file_name.dsc$w_length = (unsigned short) dev_name->len;
	if ((status = sys$assign(&file_name,&channel,0,0)) == SS$_DEVALLOC
			|| status == SS$_INSFMEM || status == SS$_NOIOCHAN)
	{	status = MT_BUSY;
	}
	else if (status != SS$_NORMAL)
	{	mtdef->access_id = status;
		status = MT_TAPERROR;
	}
	else
		mtdef->access_id = channel;
	return status;
}

