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
#include <descrip.h>

unsigned char *get_tpu_addr()
{
	int4 status;
	unsigned char *fcn;
	static readonly $DESCRIPTOR(filename,"TPUSHR");
	static readonly $DESCRIPTOR(symnam,"TPU$TPU");

	status = lib$find_image_symbol(&filename,&symnam,&fcn);
	if ((status & 1) == 0)
		rts_error(VARLSTCNT(1) status);
	return fcn;
}
