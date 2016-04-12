/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* rc_rundown - GT.CM global database cleanup
 * Go through each global database and reset the "dsid" field in the header
 * to zero, indicating that GT.CM is not accessing this file.  This is
 * necessary so that GT.M processes will not try to access the GT.CM
 * semaphore lock.
 */

#include "mdef.h"
#include "rc.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc_nspace.h"
#include "filestruct.h"

GBLREF	rc_dsid_list		*dsid_list;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;

void rc_rundown(void)
{
	rc_dsid_list	*fdi_ptr;
	int dsid;

	for (fdi_ptr = dsid_list; fdi_ptr; fdi_ptr = fdi_ptr->next)
	{
		if (fdi_ptr->gda)
		{
			cs_addrs = &FILE_INFO(fdi_ptr->gda->maps[1].reg.addr)->s_addrs;
			gv_cur_region = fdi_ptr->gda->maps[1].reg.addr;

			cs_data = cs_addrs->hdr;
			dsid = cs_data->dsid;

			if (fdi_ptr->dsid != RC_NSPACE_DSID)
			{
			    assert(!cs_addrs->hold_onto_crit);	/* so we can safely do unconditional grab_crit and rel_crit */
			    grab_crit(gv_cur_region);
			    if (--cs_data->rc_srv_cnt <= 0)
			    {
				cs_data->rc_node = cs_data->dsid = 0;
				cs_data->rc_srv_cnt = 0;
				cs_addrs->hdr->dsid = 0;
			    }
			    rel_crit(gv_cur_region);
			}
		}
	}
}
