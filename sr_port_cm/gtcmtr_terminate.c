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
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct *curr_entry;
GBLREF int4		gtcm_users;
GBLREF short		gtcm_ast_avail;
GBLREF struct CLB	*proc_to_clb[];

bool gtcmtr_terminate(bool cm_err)
{
	void 		gtcmd_rundown(),cmi_close(),gtcmtr_terminate_free();
	unsigned char	*mbuffer;
	uint4		status;

	if (curr_entry)
	{
		VMS_ONLY(
			status = sys$cantim(curr_entry,0);
			if (!(status & 1))
				rts_error(VARLSTCNT(1) status);
		)
		gtcml_lkrundown();
		gtcmd_rundown(curr_entry, cm_err);
		if (curr_entry->clb_ptr)
		{	mbuffer = curr_entry->clb_ptr->mbf;
			cmi_close(curr_entry->clb_ptr);
			curr_entry->clb_ptr = NULL;
                        proc_to_clb[curr_entry->procnum] = NULL;
			if (mbuffer)
				free(mbuffer);
		}
		if (curr_entry->pvec)
		{
			free(curr_entry->pvec);
			curr_entry->pvec = NULL;
		}
		curr_entry->region_root = NULL;	/* make sure you can't access any regions through this entry... just in case */
		free(curr_entry);
		curr_entry = NULL;
	}
	gtcm_users--;
	gtcm_ast_avail++;
	return CM_NOOP;
}
