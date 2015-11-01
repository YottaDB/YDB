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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "util.h"


/* check bov_{timestamp,tn} and eov_{timestamp,tn} of the journals are indeed well-formed */
bool	mur_jnlhdr_bov_check(jnl_file_header *header, int jnl_fn_len, char *jnl_fn)
{
	assert(header->bov_tn > 0);
	assert(header->eov_tn > 0);
	if (header->bov_timestamp > header->eov_timestamp || header->bov_tn > header->eov_tn)
	{
		util_out_print("Journal file !AD has a malformed time-sequence", TRUE, jnl_fn_len, jnl_fn);
		util_out_print("header->bov_timestamp        [!UL], header->eov_timestamp        [!UL]",
								TRUE, header->bov_timestamp, header->eov_timestamp);
		util_out_print("header->bov_tn               [!UL], header->eov_tn               [!UL]",
								TRUE, header->bov_tn, header->eov_tn);
		return FALSE;
	}
	return TRUE;
}
