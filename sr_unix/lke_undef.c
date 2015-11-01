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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "util.h"
#include "gtcmtr_lke.h"
#include "cmi.h"
#include "gvcmz.h"
#include "mlkdef.h"
#include "lke.h"
#include "gtcmtr_lke_show.h"
#include <varargs.h>

GBLDEF bool	region_init_error;

uint4 cmi_read(struct CLB *lnk)
{
	assert(FALSE);
	return(0);
}

uint4 cmi_write(struct CLB *lnk)
{
	assert(FALSE);
	return(0);
}

void util_cm_print(va_alist)
va_dcl
{
	assert(FALSE);
}

bool gtcmtr_lke_clearreq(struct CLB *lnk, char rnum, bool all, bool interactive, int4 pid, mstr *node)
{
	assert(FALSE);
	return(FALSE);
}

void gvcmz_error(char code, uint4 status)
{
	assert(FALSE);
}

bool gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node)
{
	assert(FALSE);
	return(FALSE);
}

char gtcmtr_lke_showrep(struct CLB *lnk, show_request *sreq)
{
	assert(FALSE);
	return(FALSE);
}
