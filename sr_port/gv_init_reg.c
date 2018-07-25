/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "iosp.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cryptdef.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "gvusr.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */

GBLREF int4		lkid;
GBLREF bool		licensed ;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region	*gv_cur_region;

void gv_init_reg (gd_region *reg)
{
#	ifdef NOLICENSE
	licensed = TRUE;
#	else
	CRYPT_CHKSYSTEM;
#	endif
	switch (reg->dyn.addr->acc_meth)
	{
		case dba_usr:
			gvusr_init (reg, &gv_cur_region, &gv_currkey, &gv_altkey);
			break;
			/* we may be left in dba_cm state for gt_cm, if we have rundown the db and again accessed
			   the db without quitting out of gtm */
		case dba_cm:
		case dba_mm:
		case dba_bg:
			if (!reg->open)
				gvcst_init(reg);
			break;
		default:
		assertpro(reg->dyn.addr->acc_meth != reg->dyn.addr->acc_meth);
	}
	assert(reg->open);
	return;
}
