/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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


/* Start to write transaction history to disk */

void ccp_exitwm2a(ccp_db_header *db)
{
	uint4	status;


	assert(db->segment->ti->early_tn == db->segment->ti->curr_tn);

	status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_exitwm3, db,
			 &db->glob_sec->trans_hist, BT_SIZE(db->glob_sec) + SIZEOF(th_index), TH_BLOCK, 0, 0, 0);
	if ((status & 1) == 0)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	return;
}
