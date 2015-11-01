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
#include "gtm_unistd.h"

#ifdef VMS
#include <rms.h>
#endif

#include "gtm_string.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF 	spdesc 		stringpool;
GBLREF 	tp_region 	*grlist;
GBLREF	tp_region	*halt_ptr;
GBLREF	bool		online;
GBLREF  bool            error_mupip;

void mubclnup(backup_reg_list *curr_ptr, clnup_stage stage)
{
	backup_reg_list *ptr, *next;
	uint4		status;
#ifdef VMS
	struct FAB	temp_fab;
#endif

	assert(stage >= need_to_free_space && stage < num_of_clnup_stage);

	free(stringpool.base);

	switch(stage)
	{
	case need_to_rel_crit:
		if (online)
			for (ptr = (backup_reg_list *)grlist; ptr != NULL && ptr != curr_ptr && ptr != (backup_reg_list *)halt_ptr;)
			{
				if (keep_going == ptr->not_this_time)
					rel_crit(ptr->reg);
				ptr = ptr->fPtr;
			}
		curr_ptr = (backup_reg_list *)halt_ptr;
		/* Intentional Fall Through */
	case need_to_del_tempfile:
		for(ptr = (backup_reg_list *)grlist; ptr != NULL && ptr != curr_ptr;)
		{
			if (keep_going == ptr->not_this_time || give_up_after_create_tempfile == ptr->not_this_time)
			{
				free(ptr->backup_hdr);
				if (online)
				{
					/* get rid of the temporary file */
#if defined(UNIX)
					if (ptr->backup_fd > 2)
					{
						close(ptr->backup_fd);
						UNLINK(ptr->backup_tempfile);
					}
#elif defined(VMS)
					temp_fab = cc$rms_fab;
					temp_fab.fab$b_fac = FAB$M_GET;
					temp_fab.fab$l_fna = ptr->backup_tempfile;
					temp_fab.fab$b_fns = strlen(ptr->backup_tempfile);
					if (RMS$_NORMAL == (status = sys$open(&temp_fab, NULL, NULL)))
					{
						temp_fab.fab$l_fop |= FAB$M_DLT;
						status = sys$close(&temp_fab);
					}
					if (RMS$_NORMAL != status)
					{
						util_out_print("!/Cannot delete the the temporary file !AD.",
							TRUE, temp_fab.fab$b_fns, temp_fab.fab$l_fna);
						gtm_putmsg(VARLSTCNT(1) status);
					}
#else
#error UNSUPPORTED PLATFORM
#endif
				}
				else
				{
					/* defreeze the databases */
					region_freeze(ptr->reg, FALSE, FALSE);
				}
			}
			ptr = ptr->fPtr;
		}

		/* Intentional fall through */
	case need_to_free_space:
		for (ptr = (backup_reg_list *)grlist; ptr != NULL;)
		{
			next = ptr->fPtr;
			if (keep_going != ptr->not_this_time)
				error_mupip = TRUE;
			if (NULL != ptr->backup_file.addr)
				free(ptr->backup_file.addr);
			free(ptr);
			ptr = next;
		}
	}

	return;
}
