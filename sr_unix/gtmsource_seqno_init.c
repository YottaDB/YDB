/****************************************************************
 *								*
 *	Copyright 2006, 2010 Fidelity Information Services, Inc	*
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
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "repl_msg.h"
#include "repl_shutdcode.h"
#include "gtmsource.h"
#include "jnl.h"
#include "gtmmsg.h"
#include "repl_instance.h"

GBLREF	gd_addr          	*gd_header;
GBLREF	jnlpool_addrs 		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;

/* Find the start_jnl_seqno */
void gtmsource_seqno_init(void)
{
	gd_region		*region_top, *reg;
	sgmnt_addrs		*csa, *repl_csa;
	sgmnt_data_ptr_t	csd;
	seq_num			db_seqno, replinst_seqno, zqgblmod_seqno, max_dualsite_resync_seqno;
	sm_uc_ptr_t		gld_fn;
	unix_db_info		*udi;
	int			i;

	error_def(ERR_NOREPLCTDREG);
	error_def(ERR_REPLINSTDBMATCH);

	/* Unix and VMS have different field names for now, but will both be soon changed to instfilename instead of gtmgbldir */
	UNIX_ONLY(gld_fn = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.instfilename;)
	VMS_ONLY(gld_fn = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.gtmgbldir;)
	db_seqno = 0;
	max_dualsite_resync_seqno = 0;
	zqgblmod_seqno = 0;
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{
			if (db_seqno < csd->reg_seqno)
				db_seqno = csd->reg_seqno;
			if (max_dualsite_resync_seqno < csd->dualsite_resync_seqno)
				max_dualsite_resync_seqno = csd->dualsite_resync_seqno;
			if (zqgblmod_seqno < csd->zqgblmod_seqno)
				zqgblmod_seqno = csd->zqgblmod_seqno;
		}
	}
	if (0 == db_seqno)
	{	/* No replicated region, or databases created with older * version of GTM */
		gtm_putmsg(VARLSTCNT(3) ERR_NOREPLCTDREG, 1, gld_fn);
		/* Error, has to shutdown all regions 'cos mupip needs exclusive access to turn replication on */
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	replinst_seqno = jnlpool.repl_inst_filehdr->jnl_seqno;
	/* Assert that the jnl seqno of the instance is greater than or equal to the start_seqno of the last triple in the
	 * instance file. If this was not the case, a REPLINSTSEQORD error would have been issued in "jnlpool_init"
	 */
	assert(!jnlpool_ctl->last_triple_seqno || (replinst_seqno >= jnlpool_ctl->last_triple_seqno));
	/* Check if jnl seqno in db and instance file match */
	if ((0 != replinst_seqno) && (db_seqno != replinst_seqno))
	{	/* Journal seqno from the databases does NOT match that stored in the replication instance file header. */
		udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
		gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTDBMATCH, 4, LEN_AND_STR(udi->fn), &replinst_seqno, &db_seqno);
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	/* At this point, we are guaranteed there is no other process attached to the journal pool (since our parent source
	 * server command is still waiting with the ftok lock for the pool to be initialized by this child). Even then it
	 * does not hurt to get the lock on the journal pool before updating fields in there.
	 */
	DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
	assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
	grab_lock(jnlpool.jnlpool_dummy_reg);
	jnlpool_ctl->start_jnl_seqno = db_seqno;
	jnlpool_ctl->jnl_seqno = db_seqno;
	jnlpool_ctl->max_dualsite_resync_seqno = max_dualsite_resync_seqno;
	jnlpool_ctl->max_zqgblmod_seqno = zqgblmod_seqno;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	DEBUG_ONLY(
		/* Assert that seqno fields in "gtmsrc_lcl" array are within the instance journal seqno.
		 * This is taken care of in "mur_close_files".
		 */
		for (i = 0; i < NUM_GTMSRC_LCL; i++)
		{
			if ('\0' != jnlpool.gtmsrc_lcl_array[i].secondary_instname[0])
			{
				assert(jnlpool.gtmsrc_lcl_array[i].resync_seqno <= db_seqno);
				assert(jnlpool.gtmsrc_lcl_array[i].connect_jnl_seqno <= db_seqno);
			}
		}
	)
	jnlpool.jnlpool_ctl->pool_initialized = TRUE;	/* It is only now that the journal pool is completely initialized */
}
