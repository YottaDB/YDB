/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "rtnhdr.h"
#include "zbreak.h"

GBLREF z_records 	zbrk_recs;

void zr_remove(rhdtyp *rtn)
{ /* remove all breaks in rtn */
	zbrk_struct	*zb_ptr;

	for (zb_ptr = zbrk_recs.free - 1; NULL != zbrk_recs.beg && zb_ptr >= zbrk_recs.beg; zb_ptr--)
	{ /* go in the reverse order to reduce memory movement in zr_put_free() */
		if ((NULL == rtn) || (ADDR_IN_CODE(((unsigned char *)zb_ptr->mpc), rtn)))
			zr_put_free(&zbrk_recs, zb_ptr);
	}
	return;
}
