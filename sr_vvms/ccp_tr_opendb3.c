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
#include <descrip.h>
#include <fab.h>
#include <iodef.h>
#include <ssdef.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include "ccp_opendb3a.h"

error_def(ERR_CCPJNLOPNERR);



/* Open database, third phase;  entered after completion of sys$qio
   to read transaction history in ccp_opendb2 (via ccp_opendb3) */

void	ccp_tr_opendb3(ccp_action_record *rec)
{
	ccp_db_header		*db;
	sgmnt_addrs		*csa;
	sgmnt_data		*csd;
	jnl_private_control	*jpc;
	uint4		status;


	db = rec->v.h;
	db->master_map_start_tn = 0;

	csa = db->segment;
	csd = db->glob_sec
	    = csa->hdr
	    = csa->db_addrs[0];

	adawi(1, &csa->nl->ref_cnt);	/* GT.M process that sent open request */
	csa->lock_addrs[0] = (unsigned char *)csd + (LOCK_BLOCK(csd) * DISK_BLOCK_SIZE);
	csa->lock_addrs[1] = csa->lock_addrs[0] + csd->lock_space_size;

	if (JNL_ENABLED(csd))
	{
		jpc = csa->jnl
		    = malloc(SIZEOF(jnl_private_control));
		memset(jpc, 0, SIZEOF(jnl_private_control));

		jpc->region = db->greg;
		jpc->jnl_buff = csa->lock_addrs[1] + CACHE_CONTROL_SIZE(csd) + JNL_NAME_EXP_SIZE;

		FILE_INFO(db->greg)->s_addrs.jnl->jnllsb->lockid = 0;

		if (db->wm_iosb.valblk[CCP_VALBLK_JNL_ADDR] == 0)
		{
			/* Open jnl file based on data in file header, first machine to open */
			jnl_file_open(db->greg, FALSE, ccp_closejnl_ast);
			if (jpc->channel == 0)
			{
				/* Open failed */
				ccp_close1(db);
				ccp_signal_cont(ERR_CCPJNLOPNERR);	/***** Is this reasonable? *****/
				return;
			}

			/* Write out file id for other machines */
			csd->trans_hist.ccp_jnl_filesize = jpc->jnl_buff->filesize;
			csd->ccp_jnl_before = jpc->jnl_buff->before_images;

			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_WRITEVBLK, &db->qio_iosb, ccp_opendb3a, db,
					 csd, (MM_BLOCK - 1) * OS_PAGELET_SIZE, 1, 0, 0, 0);
		}
		else
		{
/* ??? --> */		/* Open jnl file based on id in header.  Read in header to make sure get file id from first machine */

			status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_opendb3a, db,
					 csd, (MM_BLOCK - 1) * OS_PAGELET_SIZE, 1, 0, 0, 0);
		}

		if (status != SS$_NORMAL)
		{
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
			ccp_close1(db);
		}
	}
	else
		ccp_opendb3b(db);

}
