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


/* AST routine entered on completion of sys$qio to write the master map, in ccp_tr_exitwm */

void ccp_exitwm1( ccp_db_header	*db)
{
	sgmnt_addrs	*cs_addrs;
	uint4	status;


	assert(lib$ast_in_prog());

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	cs_addrs = db->segment;
	if (db->last_lk_sequence < cs_addrs->ti->lock_sequence)
	{
		status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_exitwm1a, db,
				 cs_addrs->lock_addrs[0], db->glob_sec->lock_space_size, LOCK_BLOCK(db->glob_sec) + 1, 0, 0, 0);
		if ((status & 1) == 0)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
		db->last_lk_sequence = cs_addrs->ti->lock_sequence;
	}
	else
		ccp_exitwm2(db);

	return;
}
