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
#include "testpt.h"

GBLDEF testpt_struct testpoint;

void set_testpoint(int4 testvalue)
{
	switch( testvalue)
	{	case 0: memset(&testpoint,0,sizeof(testpoint));
			break;
		case TSTPT_RECOVER:
			testpoint.wc_recover = TRUE;
			break;
		default:
			break;
	}
	return;
}
