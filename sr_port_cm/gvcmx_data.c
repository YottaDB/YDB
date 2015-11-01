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
#include "cmidef.h"
#include "cmmdef.h"

mint gvcmx_data()
{
	mval v;
	mint ret;

	gvcmz_doop(CMMS_Q_DATA,CMMS_R_DATA,&v);
	ret = MV_FORCE_INT(&v);
	return ret;
}
