/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <iodef.h>
#include <rms.h>
#include <ssdef.h>

#include "gdsroot.h"
#include "ccp.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp_writedb3.h"


/* AST routine entered on completion of sys$qio to read header in ccp_writedb2 */

void ccp_writedb3(ccp_db_header	*db)
{
	uint4		status;
	sgmnt_addrs		*cs_addrs;


	assert(lib$ast_in_prog());

	cs_addrs = db->segment;

	if (cs_addrs == NULL  ||  cs_addrs->nl->ccp_state == CCST_CLOSED)
		return;

	if ((db->qio_iosb.cond & 1) == 0)
		ccp_signal_cont(db->qio_iosb.cond);	/***** Is this reasonable? *****/

	cs_addrs->nl->in_wtstart = 0;
	cs_addrs->nl->wc_in_free = cs_addrs->hdr->n_bts;

	if (db->master_map_start_tn < cs_addrs->ti->mm_tn)
	{
		status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_writedb4, db,
				 MM_ADDR(db->glob_sec), MASTER_MAP_SIZE(db->glob_sec), MM_BLOCK, 0, 0, 0);
		if (status == SS$_IVCHAN)		/* database has been closed out, section deleted */
			return;
		if ((status & 1) == 0)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
		db->master_map_start_tn = cs_addrs->ti->mm_tn;
	}
	else
		if (db->last_lk_sequence < cs_addrs->ti->lock_sequence)
		{
			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_writedb4a, db,
					 cs_addrs->lock_addrs[0], db->glob_sec->lock_space_size, LOCK_BLOCK(db->glob_sec) + 1,
					 0, 0, 0);
			if (status == SS$_IVCHAN)		/* database has been closed out, section deleted */
				return;
			if ((status & 1) == 0)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			db->last_lk_sequence = cs_addrs->ti->lock_sequence;
		}
		else
			ccp_writedb5(db);

	return;
}
