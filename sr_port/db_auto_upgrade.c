/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "io.h"
#include "jnl.h"
#include "mutex.h"
#include "wcs_phase2_commit_wait.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */

GBLREF  boolean_t       dse_running;

error_def(ERR_DBBADUPGRDSTATE);

void db_auto_upgrade(gd_region *reg)
{
	/* detect unitialized file header fields for this version of GT.M and do a mini auto-upgrade, initializing such fields
	 * to default values in the new GT.M version
	 */

	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	assert(NULL != reg);
	if (NULL == reg)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	if (NULL == csd)
		return;

	if (0 == csd->mutex_spin_parms.mutex_hard_spin_count)
		csd->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	if (0 == csd->mutex_spin_parms.mutex_sleep_spin_count)
		csd->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	/* zero is a legitimate value for csd->mutex_spin_parms.mutex_spin_sleep_mask; so can't detect if need re-initialization */
	INIT_NUM_CRIT_ENTRY_IF_NEEDED(csd);

	/* Auto upgrade based on minor database version number. This code currently only does auto upgrade and does not
	 * do auto downgrade although that certainly is possible to implement if necessary. For now, if the current version
	 * is at a lower level than the minor db version, we do nothing.
	 *
	 * Note the purpose of the minor_dbver field is so that some part of gtm (either runtime, or conversion utility) some
	 * time and several versions down the road from now knows by looking at this field what fields in the fileheader are
	 * valid so it is important that the minor db version be updated each time the fileheader is updated and this routine
	 * correspondingly updated. SE 5/2006.
	 */
	if (csd->minor_dbver < GDSMVCURR)
	{	/* In general, the method for adding new versions is:
		 * 1) If there are no automatic updates for this version, it is optional to add the version to the switch
		 *    statement below. Those there are more for example at this time (through V53000).
		 * 2) Update (or add) a case for the previous version to update any necessary fields.
		 */
		if (!csd->opened_by_gtmv53 && !csd->db_got_to_v5_once)
		{
			csd->opened_by_gtmv53 = TRUE;
			/* This is a case of a database that has been used by a pre-V53 version of GT.M that did not contain
			 * the fix (C9H07-002873). At this point, the database might contain RECYCLED blocks that are a mix of
			 *	a) Those blocks that were RECYCLED at the time of the MUPIP UPGRADE from V4 to V5.
			 *	b) Those blocks that became RECYCLED due to M-kills in V5.
			 * It is only (a) that we have to mark as FREE as it might contain too-full v4 format blocks. But there
			 * is no way to distinguish the two. So we mark both (a) and (b) as FREE. This will mean no PBLKs written
			 * for (b) and hence no backward journal recovery possible to a point before the start of the REORG UPGRADE.
			 * We force a MUPIP REORG UPGRADE rerun (to mark RECYCLED blocks FREE) by setting fully_upgraded to FALSE.
			 * Note that this does not need to be done for databases created by a V5 version (C9I05-002987).
			 */
			if (MASTER_MAP_SIZE_V4 == csd->master_map_len)
			{
				csd->fully_upgraded = FALSE;
				csd->reorg_upgrd_dwngrd_restart_block = 0;	/* reorg upgrade should restart from block 0 */
				/* Ensure reorg_db_fmt_start_tn and desired_db_format_tn are set to different
				 * values so fresh reorg upgrade can set fully_upgraded to TRUE once it is done.
				 */
				csd->reorg_db_fmt_start_tn = 0;
				csd->desired_db_format_tn = 1;
			} else
				csd->db_got_to_v5_once = TRUE;	/* db was created by V5 so safe to set this */
		}
		/* When adding a new minor version, the following template should be maintained
		 * a) Remove the penultimate 'break'
		 * b) Remove the assert(FALSE) in the last case (most recent minor version)
		 * c) If there are any file header fields added in the new minor version, initialize the fields to default values
		 *    in the last case
		 * d) Add a new case with the new minor version
		 * e) Add assert(FALSE) and break (like it was before)
		 */
		switch(csd->minor_dbver)
		{	/* Note that handling for any fields introduced in a version will not go in the "switch-case" block
			 * of code introduced for the new version but will go in the PREVIOUS "switch-case" block.
			 */
			case GDSMV51000:		/* Multi-site replication available */
			case GDSMV52000:		/* Unicode */
			case GDSMV53000:		/* M-Itanium release */
				gvstats_rec_upgrade(csa); /* Move GVSTATS information to new place in file header */
			case GDSMV54002:
				/* GT.M V54002B introduced jnl_eov_tn for backward recovery */
				csd->jnl_eovtn = csd->trans_hist.curr_tn;
			case GDSMV54002B:
				/* GT.M V55000 introduced strm_reg_seqno, save_strm_reg_seqno, intrpt_recov_resync_strm_seqno
				 * AND obsoleted dualsite_resync_seqno. For new fields, we are guaranteed they are
				 * zero (in formerly unused sections of the file header) so no need for any initialization.
				 * For obsoleted fields, it would be good to clear them here so we dont run into issues later.
				 */
				UNIX_ONLY(csd->filler_seqno = 0;)	/* was "dualsite_resync_seqno" in pre-V55000 versions */
				/* In addition, V55000 introduced before_trunc_total_blks for MUPIP REORG -TRUNCATE.
				 * Since it is a new field no initialization necessary.
				 */
			case GDSMV55000:
				UNIX_ONLY(csd->freeze_on_fail = FALSE;)
				UNIX_ONLY(csd->span_node_absent = TRUE;)
				UNIX_ONLY(csd->maxkeysz_assured = FALSE;)
			case GDSMV60000:
			case GDSMV60001:
				/* GT.M V60002 introduced mutex_spin_parms.mutex_que_entry_space_size */
				NUM_CRIT_ENTRY(csd) = DEFAULT_NUM_CRIT_ENTRY;
				break;
			case GDSMV60002:
				/* Nothing to do for this version since it is GDSMVCURR for now. */
				assert(FALSE);		/* When this assert fails, it means a new GDSMV* was created, */
				break;			/* 	so a new "case" needs to be added BEFORE the assert. */
		}
		csd->minor_dbver = GDSMVCURR;
		if (0 == csd->wcs_phase2_commit_wait_spincnt)
			csd->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
	}
	csd->last_mdb_ver = GDSMVCURR;
	if (csd->fully_upgraded && !csd->db_got_to_v5_once)
	{	/* Database is fully upgraded but the db_got_to_v5_once field says different.
                 * Don't know how that could happen, except with DSE which can change both the database file header fields
                 */
		assert(!dse_running);
		csd->db_got_to_v5_once = TRUE; /* fix it in PRO */
		send_msg(VARLSTCNT(6) ERR_DBBADUPGRDSTATE, 4, REG_LEN_STR(reg), DB_LEN_STR(reg));
	}
	return;
}
