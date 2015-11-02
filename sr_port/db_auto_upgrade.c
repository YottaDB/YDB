/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

	/* Auto upgrade based on minor database version number. This code currently only does auto upgrade and does not
	   do auto downgrade although that certainly is possible to implement if necessary. For now, if the current version
	   is at a lower level than the minor db version, we do nothing.

	   Note the purpose of the minor_dbver field is so that some part of gtm (either runtime, or conversion utility) some
	   time and several versions down the road from now knows by looking at this field what fields in the fileheader are
	   valid so it is important that the minor db version be updated each time the fileheader is updated and this routine
	   correspondingly updated. SE 5/2006.
	*/
	if (csd->minor_dbver < GDSMVCURR)
	{	/* In general, the method for adding new versions is:
		   1) If there are no automatic updates for this version, it is optional to add the version to the switch
		      statement below. Those there are more for example at this time (through V53000).
		   2) Update (or add) a case for the previous version to update any necessary fields.
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
			 */
			csd->fully_upgraded = FALSE;
			csd->reorg_upgrd_dwngrd_restart_block = 0;	/* reorg upgrade should restart from block 0 */
			/* Ensure reorg_db_fmt_start_tn and desired_db_format_tn are set to different values so fresh reorg
			 * upgrade can set fully_upgraded to TRUE once it is done.
			 */
			csd->reorg_db_fmt_start_tn = 0;
			csd->desired_db_format_tn = 1;
		}
		switch(csd->minor_dbver)
		{
			case GDSMV51000:		/* Multi-site replication available */
			case GDSMV52000:		/* Unicode */
			case GDSMV53000:		/* M-Itanium release.*/
				break;			/* Nothing to do for this version */
		}
		csd->minor_dbver = (enum mdb_ver)GDSMVCURR;
		if (0 == csd->wcs_phase2_commit_wait_spincnt)
			csd->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
	}
	return;
}
