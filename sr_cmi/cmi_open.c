/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <descrip.h>
#include <iodef.h>
#include <efndef.h>
#include "cmihdr.h"
#include "cmidef.h"

GBLREF struct NTD *ntd_root;
GBLREF struct dsc$descriptor_s cm_netname;

#define TASK_PREFIX "::\"0="
#define TASK_SUFFIX "\""
#define MAX_NCB_LENGTH 128

uint4 cmi_open(struct CLB *lnk)
{
	uint4 status;
	struct dsc$descriptor_s ncb;
	qio_iosb iosb;
	unsigned char *cp;
	unsigned char ncb_buffer[MAX_NCB_LENGTH];

	if (ntd_root == 0)
	{
		status = cmj_netinit();
		if ((status & 1) == 0)
			return status;
	}
	ncb.dsc$w_length = lnk->nod.dsc$w_length + lnk->tnd.dsc$w_length + SIZEOF(TASK_PREFIX) - 1 + SIZEOF(TASK_SUFFIX) - 1;
	ncb.dsc$b_dtype = DSC$K_DTYPE_T;
	ncb.dsc$b_class = DSC$K_CLASS_S;
	ncb.dsc$a_pointer = cp = ncb_buffer;
	assert(ncb.dsc$w_length < SIZEOF(ncb_buffer));
	memcpy(cp, lnk->nod.dsc$a_pointer, lnk->nod.dsc$w_length);
	cp += lnk->nod.dsc$w_length;
	memcpy(cp, TASK_PREFIX, SIZEOF(TASK_PREFIX) - 1);
	cp += SIZEOF(TASK_PREFIX) - 1;
	memcpy(cp, lnk->tnd.dsc$a_pointer, lnk->tnd.dsc$w_length);
	cp += lnk->tnd.dsc$w_length;
	memcpy(cp, TASK_SUFFIX, SIZEOF(TASK_SUFFIX) - 1);
	status = sys$assign(&cm_netname, &lnk->dch, 0, 0);
	if (status & 1)
	{
                status = sys$qiow(EFN$C_ENF, lnk->dch, IO$_ACCESS, &iosb, 0, 0, 0, &ncb, 0, 0, 0, 0);
		if (status & 1)
			status = iosb.status;
		if (status & 1)
		{	insqhi(lnk, ntd_root);
		}else
		{	sys$dassgn(lnk->dch);
		}
	}
	return status;
}
