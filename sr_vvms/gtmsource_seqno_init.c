/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include <descrip.h> /* Required for gtmsource.h */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "gtmmsg.h"

GBLREF	gd_addr          	*gd_header;
GBLREF	gd_region        	*gv_cur_region;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	jnlpool_addrs 		jnlpool;
GBLREF  seq_num                 seq_num_one, seq_num_zero;

error_def(ERR_NOREPLCTDREG);

void gtmsource_seqno_init(void)
{
	/* Find the start_jnl_seqno */

	gd_region		*reg, *region_top;
	seq_num			local_read_jsn, local_jsn;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		gld_fn;

	/* Unix and VMS have different field names for now, but will both be soon changed to instname instead of gtmgbldir */
	gld_fn = (sm_uc_ptr_t)jnlpool.jnlpool_ctl->jnlpool_id.gtmgbldir;
	QWASSIGN(jnlpool.jnlpool_ctl->start_jnl_seqno, seq_num_zero);
	QWASSIGN(local_read_jsn, seq_num_zero);
	QWASSIGN(local_jsn, seq_num_zero);
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{
			if (QWLT(local_read_jsn, csd->resync_seqno))
				QWASSIGN(local_read_jsn, csd->resync_seqno);
			if (QWLT(local_jsn, csd->reg_seqno))
				QWASSIGN(local_jsn, csd->reg_seqno);
			/* Copy gtmgbldir into the database shared memory.
			 * Used later to avoid updates from a different gtmgbldir to this database.
			 */
			assert(SIZEOF(csa->nl->replinstfilename) == SIZEOF(jnlpool.jnlpool_ctl->jnlpool_id.gtmgbldir));
			memcpy(csa->nl->replinstfilename, gld_fn, SIZEOF(csa->nl->replinstfilename));
		}
	}
	if (QWEQ(local_jsn, seq_num_zero))
	{
		/* No replicated region, or databases created with older version of GTM */
		gtm_putmsg(VARLSTCNT(5) ERR_NOREPLCTDREG, 3, LEN_AND_LIT("global directory"), gld_fn);
		/* Error, has to shutdown all regions 'cos mupip needs exclusive access to turn replication on */
		gtmsource_autoshutdown();
	}
	QWASSIGN(jnlpool.jnlpool_ctl->start_jnl_seqno, local_jsn);
	QWASSIGN(jnlpool.jnlpool_ctl->jnl_seqno, local_jsn);
	QWASSIGN(jnlpool.gtmsource_local->read_jnl_seqno, local_read_jsn);
}
