/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <jpidef.h>
#include <ssdef.h>
#include <descrip.h>
#include "job.h"	/* for MAX_PRCNAM_LEN in jobsp.h */
#include "getzprocess.h"

GBLDEF mval 		dollar_zproc;
static unsigned char	proc_buf[MAX_PRCNAM_LEN + 1];

void getzprocess(void)
{
	int4 jpi_code, status;
	short int out_len;
	$DESCRIPTOR(out_string, proc_buf);
	error_def(ERR_SYSCALL);

	jpi_code = JPI$_PRCNAM;
	dollar_zproc.str.addr = proc_buf;
	dollar_zproc.mvtype = MV_STR;
	if ((status = lib$getjpi(	 &jpi_code
					,0
					,0
					,0
					,&out_string
					,&out_len  )) != SS$_NORMAL)
	{
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("LIB$GETJPI"), CALLFROM, status);
	}
	dollar_zproc.str.len = out_len;
	return;
}
