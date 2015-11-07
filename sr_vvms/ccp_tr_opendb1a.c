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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "ccp.h"
#include <iodef.h>


/* Open database, first phase, step two;  entered after successful completion
   of sys$open in ccp_opendb (via ccp_opendb1a) */

void ccp_tr_opendb1a( ccp_action_record	*rec)
{
	ccp_db_header	*db;
	uint4	status;


	db = ccp_get_reg_by_fab(rec->v.fab);
	db->segment->hdr = malloc(SIZEOF(sgmnt_data));

	status = sys$qio(0, rec->v.fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_opendb1, db,
			 db->segment->hdr, SIZEOF(sgmnt_data), 1, 0, 0, 0);
	if ((status & 1) == 0)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	return;
}
