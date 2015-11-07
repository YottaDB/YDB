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

#include "mdef.h"

#include <fab.h>
#include <iodef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"


void ccp_opendb3b( ccp_db_header *db)
{
	uint4	status;


	db->glob_sec->trans_hist.early_tn = db->glob_sec->trans_hist.curr_tn;

	status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_opendb3c, db,
			 db->segment->db_addrs[0], (MM_BLOCK + 1) * 512 + MASTER_MAP_SIZE_V4, 1, 0, 0, 0);
	if ((status & 1) == 0)
	{
		ccp_close1(db);
		ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	return;
}
