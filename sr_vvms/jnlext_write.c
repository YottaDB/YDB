/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <fab.h>
#include <rab.h>
#include <rmsdef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"

void	jnlext_write(fi_type *file_info, char *buffer, int length)
{
	uint4		status;

	error_def	(ERR_JNLEXTR);

	file_info->rab->rab$w_rsz = length - 1;
	file_info->rab->rab$l_rbf = buffer;
	status = sys$put(file_info->rab);
	if (status != RMS$_NORMAL)
 		rts_error(VARLSTCNT(5) ERR_JNLEXTR, 2, file_info->fab->fab$b_fns, file_info->fab->fab$l_fna, status);
}
