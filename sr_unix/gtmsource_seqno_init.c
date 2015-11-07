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
GBLREF	repl_conn_info_t	*this_side;

error_def(ERR_NOREPLCTDREG);
error_def(ERR_REPLINSTDBMATCH);
error_def(ERR_REPLINSTDBSTRM);

/* Find the start_jnl_seqno */
void gtmsource_seqno_init(boolean_t this_side_std_null_coll)
{
	boolean_t		is_supplementary;
	gd_region		*region_top, *reg;
	int4			idx;
	sgmnt_addrs		*csa, *repl_csa;
	sgmnt_data_ptr_t	csd;
	seq_num			db_seqno, replinst_seqno, strm_db_seqno[MAX_SUPPL_STRMS], strm_inst_seqno, zqgblmod_seqno;
	sm_uc_ptr_t		gld_fn;
	unix_db_info		*udi;

	/* Unix and VMS have different field names for now, but will both be soon changed to instfilename instead of gtmgbldir */
	gld_fn = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.instfilename;
	zqgblmod_seqno = 0;
	region_top = gd_header->regions + gd_header->n_regions;
	db_seqno = 0;
	replinst_seqno = jnlpool.repl_inst_filehdr->jnl_seqno;
	/* The stream specific jnl seqnos are valid only if this is a supplementary instance. */
	is_supplementary = jnlpool.repl_inst_filehdr->is_supplementary;
	if (is_supplementary)
	{	/* Since this is a supplementary instance, the 0th stream should be at least 1 (even if the db file header
		 * still says 0). The first update to that replicated database will set the strm_reg_seqno to a non-zero value.
		 * See repl_inst_create.c for similar code and comment on why this is needed for a supplementary instance.
		 * By a similar argument, streams 1 thru 15 also need to have seqno of at least 1 as that will be the seqno
		 * assigned for the next update which happens on that stream. This adjustment of seqno 0 to 1 avoids spurious
		 * ERR_REPLINSTDBSTRM errors further down in this module.
		 */
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			strm_db_seqno[idx] = 1;
	}
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{
			if (db_seqno < csd->reg_seqno)
				db_seqno = csd->reg_seqno;
			if (is_supplementary)
			{
				for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				{
					if (strm_db_seqno[idx] < csd->strm_reg_seqno[idx])
						strm_db_seqno[idx] = csd->strm_reg_seqno[idx];
				}
			}
			if (zqgblmod_seqno < csd->zqgblmod_seqno)
				zqgblmod_seqno = csd->zqgblmod_seqno;
		}
	}
	if (0 == db_seqno)
	{	/* No replicated region, or databases created with older * version of GTM */
		gtm_putmsg(VARLSTCNT(5) ERR_NOREPLCTDREG, 3, LEN_AND_LIT("instance file"), gld_fn);
		/* Error, has to shutdown all regions 'cos mupip needs exclusive access to turn replication on */
		gtmsource_exit(ABNORMAL_SHUTDOWN);
	}
	/* Assert that the jnl seqno of the instance is greater than or equal to the start_seqno of the last histinfo record in the
	 * instance file. If this was not the case, a REPLINSTSEQORD error would have been issued in "jnlpool_init"
	 */
	assert(!jnlpool_ctl->last_histinfo_seqno || (replinst_seqno >= jnlpool_ctl->last_histinfo_seqno));
	/* Check if jnl seqno in db and instance file match */
	if (0 != replinst_seqno)
	{
		if (db_seqno != replinst_seqno)
		{	/* Journal seqno from the databases does NOT match that stored in the replication instance file header. */
			udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
			gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTDBMATCH, 4, LEN_AND_STR(udi->fn), &replinst_seqno, &db_seqno);
			gtmsource_exit(ABNORMAL_SHUTDOWN);
		}
		if (is_supplementary)
		{	/* Check that each of the potentially 16 stream seqnos are also identical between db and instance file */
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			{
				strm_inst_seqno = jnlpool.repl_inst_filehdr->strm_seqno[idx];
				if (strm_inst_seqno && (strm_db_seqno[idx] != strm_inst_seqno))
				{
					udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
					gtm_putmsg(VARLSTCNT(7) ERR_REPLINSTDBSTRM, 5, LEN_AND_STR(udi->fn),
						&strm_inst_seqno, idx, &strm_db_seqno[idx]);
					assert(FALSE);
					gtmsource_exit(ABNORMAL_SHUTDOWN);
				}
			}
		}
	} else
	{	/* Instance file header has no seqno values. Initialize it from the db file header. */
		jnlpool.repl_inst_filehdr->jnl_seqno = db_seqno;
		if (is_supplementary)
		{	/* Initialize each of the potentially 16 stream seqnos from the db */
			idx = 0;
			jnlpool.repl_inst_filehdr->strm_seqno[idx] = strm_db_seqno[idx];
			idx++;
			/* For streams 1 thru 15, if the db seqno is at 1, it means that stream
			 * has no updates yet in this instance. In that case, keep the instance file
			 * header at seqno of 0 to avoid showing unused streams as being used.
			 * For stream 0 though, we use it always in case of a supplementary
			 * instance so initialize it even if it is to the seqno 1.
			 */
			for ( ; idx < MAX_SUPPL_STRMS; idx++)
			{
				assert(0 < strm_db_seqno[idx]);
				jnlpool.repl_inst_filehdr->strm_seqno[idx] = (1 < strm_db_seqno[idx]) ? strm_db_seqno[idx] : 0;
			}
		}
	}
	/* At this point, we are guaranteed there is no other process attached to the journal pool (since our parent source
	 * server command is still waiting with the ftok lock for the pool to be initialized by this child). Even then it
	 * does not hurt to get the lock on the journal pool before updating fields in there.
	 */
	DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
	assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	jnlpool_ctl->start_jnl_seqno = db_seqno;
	jnlpool_ctl->jnl_seqno = db_seqno;
	jnlpool_ctl->max_zqgblmod_seqno = zqgblmod_seqno;
	jnlpool_ctl->prev_jnlseqno_time = 0;
	if (is_supplementary)
	{	/* Copy stream jnl seqno info from instance file header to jnlpool.
		 * From this point onwards, only the jnlpool will have uptodate values for strm_seqno.
		 * Therefore only that should be used by whoever wants to find out the current strm_seqno.
		 */
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			jnlpool.jnlpool_ctl->strm_seqno[idx] = jnlpool.repl_inst_filehdr->strm_seqno[idx];
	}
	/* Initialize details for this side of the replication connection. Do it while we still have the jnlpool lock. */
	assert(this_side == &jnlpool_ctl->this_side);
	this_side->proto_ver = REPL_PROTO_VER_THIS;
	this_side->jnl_ver = JNL_VER_THIS;
	this_side->is_std_null_coll = this_side_std_null_coll;
	this_side->trigger_supported = GTMTRIG_ONLY(TRUE) NON_GTMTRIG_ONLY(FALSE);
	/* The following 3 members make sense only if the other side of a replication connection is also known. Since
	 * this_side talks about the properties of this instance, these 3 dont make sense in this context. When a connection
	 * to the other side is made, each source server's gtmsource_local->remote_side will have these fields appropriately set.
	 */
	this_side->cross_endian = FALSE;
	this_side->endianness_known = FALSE;
	this_side->null_subs_xform = FALSE;
	this_side->is_supplementary = is_supplementary;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	DEBUG_ONLY(
		/* Assert that seqno fields in "gtmsrc_lcl" array are within the instance journal seqno.
		 * This is taken care of in "mur_close_files".
		 */
		for (idx = 0; NUM_GTMSRC_LCL > idx; idx++)
		{
			if ('\0' != jnlpool.gtmsrc_lcl_array[idx].secondary_instname[0])
			{
				assert(jnlpool.gtmsrc_lcl_array[idx].resync_seqno <= db_seqno);
				assert(jnlpool.gtmsrc_lcl_array[idx].connect_jnl_seqno <= db_seqno);
			}
		}
	)
	jnlpool.jnlpool_ctl->pool_initialized = TRUE;	/* It is only now that the journal pool is completely initialized */
}
