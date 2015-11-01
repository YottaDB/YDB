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
#include "util.h"
#include "mupipbckup.h"

GBLREF gd_addr *gd_header;

gd_region *mubfndreg(unsigned char *regbuf,unsigned short len)
{
gd_region *reg;
unsigned short i;
bool found;

found = FALSE;
for (i = 0, reg = gd_header->regions; i < gd_header->n_regions ;i++, reg++)
{	if (len == reg->rname_len && (memcmp(&reg->rname[0],&(regbuf[0]),len) == 0))
	{  	found = TRUE;
		break;
	}
}
if (!found)
{ 	util_out_print("REGION !AD not found.", TRUE, len, regbuf);
 	return FALSE;
}
if (reg->dyn.addr->acc_meth == dba_usr)
{
	util_out_print("REGION !AD maps to a non-GTC database.  Specified function does not apply to a non-GTC database.",
		TRUE, len, regbuf);
	return FALSE;
}
return reg;
}
