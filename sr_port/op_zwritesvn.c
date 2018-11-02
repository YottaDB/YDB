/****************************************************************
 *								*
 * Copyright 2010 Fidelity Information Services, Inc		*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "zshow.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gtm_maxstr.h"
#include "op.h"

void op_zwritesvn(int svn)
{
  	zshow_out	output;
	MAXSTR_BUFF_DECL(buff);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memset(&output, 0, SIZEOF(output));
	MAXSTR_BUFF_INIT;
	output.type = ZSHOW_DEVICE;
	output.buff = output.ptr = buff;
	output.size = SIZEOF(buff);
	zshow_svn(&output, svn);
	output.code = 0;
	output.flush = TRUE;
	zshow_output(&output,0);
	MAXSTR_BUFF_FINI;
}
