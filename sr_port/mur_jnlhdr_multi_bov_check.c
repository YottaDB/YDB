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

/* return FALSE if the half-open {time or tn}-intervals of the two journal files intersect.
 * if ordered is TRUE, then we check that prev_header's time-stamps/tns are before in time compared to header's.
 */
bool	mur_jnlhdr_multi_bov_check(jnl_file_header *prev_header, int prev_jnl_fn_len, char *prev_jnl_fn,
					jnl_file_header *header, int jnl_fn_len, char *jnl_fn, boolean_t ordered)
{
	assert(header->bov_tn > 0);
	assert(header->eov_tn > 0);
	assert(prev_header->bov_tn > 0);
	assert(prev_header->eov_tn > 0);
	if ((header->eov_timestamp > prev_header->bov_timestamp && prev_header->eov_timestamp > header->bov_timestamp)
	     || (header->eov_tn > prev_header->bov_tn && prev_header->eov_tn > header->bov_tn)
	     || (header->eov_timestamp > prev_header->bov_timestamp && header->eov_tn < prev_header->eov_tn)
	     || (prev_header->eov_timestamp > header->bov_timestamp && prev_header->eov_tn < header->eov_tn))
	{
		util_out_print("Journal files !AD and !AD", TRUE, prev_jnl_fn_len, prev_jnl_fn, jnl_fn_len, jnl_fn);
		util_out_print("  apply to the same database file, but are discontinuous", TRUE);
		util_out_print("   -->  prev_header->bov_timestamp [!UL], prev_header->eov_timestamp [!UL]",
				TRUE, prev_header->bov_timestamp, prev_header->eov_timestamp);
		util_out_print("   -->  header->bov_timestamp        [!UL], header->eov_timestamp        [!UL]",
				TRUE, header->bov_timestamp, header->eov_timestamp);
		util_out_print("   -->  prev_header->bov_tn        [!UL], prev_header->eov_tn        [!UL]",
				TRUE, prev_header->bov_tn, prev_header->eov_tn);
		util_out_print("   -->  header->bov_tn               [!UL], header->eov_tn               [!UL]",
				TRUE, header->bov_tn, header->eov_tn);
		assert(FALSE);
		return FALSE;
	}
	if (ordered && ((prev_header->bov_tn > header->bov_tn)
			|| (prev_header->bov_timestamp > header->bov_timestamp)
			|| ((FALSE == prev_header->crash)
				&& (FALSE == header->crash)
				&& (prev_header->eov_tn > header->bov_tn || prev_header->eov_timestamp > header->bov_timestamp))))
	{
		util_out_print("Journal files !AD and !AD", TRUE, prev_jnl_fn_len, prev_jnl_fn, jnl_fn_len, jnl_fn);
		util_out_print("  apply to the same database file, but are out-of-order", TRUE);
		util_out_print("The previous journal file (former) is ahead of the current journal file (latter)", TRUE);
		util_out_print("   -->  prev_header->bov_timestamp [!UL], prev_header->eov_timestamp [!UL]",
				TRUE, prev_header->bov_timestamp, prev_header->eov_timestamp);
		util_out_print("   -->  header->bov_timestamp        [!UL], header->eov_timestamp        [!UL]",
				TRUE, header->bov_timestamp, header->eov_timestamp);
		util_out_print("   -->  prev_header->bov_tn        [!UL], prev_header->eov_tn        [!UL]",
				TRUE, prev_header->bov_tn, prev_header->eov_tn);
		util_out_print("   -->  header->bov_tn               [!UL], header->eov_tn               [!UL]",
				TRUE, header->bov_tn, header->eov_tn);
		assert(FALSE);
		return FALSE;
	}
	return TRUE;
}
