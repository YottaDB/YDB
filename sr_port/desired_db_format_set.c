/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "util.h"

/* Prototypes */
#include "gtmmsg.h"		/* for gtm_putmsg prototype */
#include "desired_db_format_set.h"
#include "send_msg.h"		/* for send_msg */
#include "wcs_phase2_commit_wait.h"

#define	WCS_PHASE2_COMMIT_WAIT_LIT	"wcb_phase2_commit_wait"

LITREF	char			*gtm_dbversion_table[];

GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			process_id;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */

error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_CRYPTNOV4);
error_def(ERR_DBDSRDFMTCHNG);
error_def(ERR_MMNODYNDWNGRD);
error_def(ERR_MUDWNGRDTN);
error_def(ERR_MUNOACTION);
error_def(ERR_SNAPSHOTNOV4);
error_def(ERR_WCBLOCKED);

/* input parameter "command_name" is a string that is either "MUPIP REORG UPGRADE/DOWNGRADE" or "MUPIP SET VERSION" */
int4	desired_db_format_set(gd_region *reg, enum db_ver new_db_format, char *command_name)
{
	boolean_t		was_crit;
	char			*db_fmt_str;
	char			*wcblocked_ptr;
	int4			status;
	uint4			jnl_status;
	inctn_opcode_t		save_inctn_opcode;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	trans_num		curr_tn;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;

	assert(reg->open);
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	GTMCRYPT_ONLY(
		/* We don't allow databases to be encrypted if the version is V4 */
		if (csd->is_encrypted && (GDSV4 == new_db_format))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRYPTNOV4, 2, DB_LEN_STR(reg));
			return ERR_CRYPTNOV4;
		}
	)
	GTM_SNAPSHOT_ONLY(
		/* We don't allow databases to be downgraded when snapshots are in progress */
		if (SNAPSHOTS_IN_PROG(csa->nl) && (GDSV4 == new_db_format))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_SNAPSHOTNOV4, 3, csa->nl->num_snapshots_in_effect, DB_LEN_STR(reg));
			return ERR_SNAPSHOTNOV4;
		}
	)
	was_crit = csa->now_crit;
	if (FALSE == was_crit)
		grab_crit(reg);
	/* if MM and desired_db_format is not V5, gvcst_init would have issued MMNODYNDWNGRD error. assert that. */
	assert(dba_bg == csd->acc_meth || (dba_mm == csd->acc_meth) && (GDSV6 == csd->desired_db_format));
	if (csd->desired_db_format == new_db_format)
	{	/* no change in db_format. fix max_tn_warn if necessary and return right away. */
		status = ERR_MUNOACTION;
		assert(csd->trans_hist.curr_tn <= csd->max_tn);
		if ((GDSV4 == new_db_format) && (MAX_TN_V4 < csd->max_tn))
		{	/* reset max_tn to MAX_TN_V4 only if V4 format and the new value will still be greater than curr_tn */
			if (MAX_TN_V4 >= csd->trans_hist.curr_tn)
			{
				csd->max_tn = MAX_TN_V4;
				/* since max_tn changed above, max_tn_warn might also need to correspondingly change */
				SET_TN_WARN(csd, csd->max_tn_warn);
			} else
				GTMASSERT;	/* out-of-design state where curr_tn > MAX_TN_V4 in GDSV4 */
		}
		if (FALSE == was_crit)
			rel_crit(reg);
		return status;
	}
	if (dba_mm == csd->acc_meth)
	{
		status = ERR_MMNODYNDWNGRD;
		gtm_putmsg(VARLSTCNT(4) status, 2, REG_LEN_STR(reg));
		if (FALSE == was_crit)
			rel_crit(reg);
		return status;
	}
	/* check if curr_tn is too high to downgrade */
	curr_tn = csd->trans_hist.curr_tn;
	if ((GDSV4 == new_db_format) && (MAX_TN_V4 <= curr_tn))
	{
		status = ERR_MUDWNGRDTN;
		gtm_putmsg(VARLSTCNT(5) status, 3, &curr_tn, DB_LEN_STR(reg));
		if (FALSE == was_crit)
			rel_crit(reg);
		return status;
	}
	/* Wait for concurrent phase2 commits to complete before switching the desired db format */
	if (csa->nl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(csa, NULL))
	{	/* Set wc_blocked so next process to get crit will trigger cache-recovery */
		SET_TRACEABLE_VAR(csa->nl->wc_blocked, TRUE);
		wcblocked_ptr = WCS_PHASE2_COMMIT_WAIT_LIT;
		send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
			process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
		status = ERR_COMMITWAITSTUCK;
		gtm_putmsg(VARLSTCNT(7) status, 5, process_id, 1, csa->nl->wcs_phase2_commit_pidcnt, DB_LEN_STR(reg));
		if (FALSE == was_crit)
			rel_crit(reg);
		return status;
	}
	if (JNL_ENABLED(csd))
	{
		SET_GBL_JREC_TIME;	/* needed for jnl_ensure_open, jnl_put_jrt_pini and jnl_write_aimg_rec */
		jpc = csa->jnl;
		jbp = jpc->jnl_buff;
		/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl records.
		 * This needs to be done BEFORE the jnl_ensure_open as that could write journal records
		 * (if it decides to switch to a new journal file)
		 */
		ADJUST_GBL_JREC_TIME(jgbl, jbp);
		jnl_status = jnl_ensure_open();
		if (0 == jnl_status)
		{
			save_inctn_opcode = inctn_opcode;
			inctn_opcode = inctn_db_format_change;
			inctn_detail.blks2upgrd_struct.blks_to_upgrd_delta = csd->blks_to_upgrd;
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			jnl_write_inctn_rec(csa);
			inctn_opcode = save_inctn_opcode;
		} else
			gtm_putmsg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
	}
	csd->desired_db_format = new_db_format;
	csd->fully_upgraded = FALSE;
	csd->desired_db_format_tn = curr_tn;
	switch(new_db_format)
	{
		case GDSV4:
			csd->max_tn = MAX_TN_V4;
			break;
		case GDSV6:
			csd->max_tn = MAX_TN_V6;
			break;
		default:
			GTMASSERT;
	}
	SET_TN_WARN(csd, csd->max_tn_warn);	/* if max_tn changed above, max_tn_warn also needs a corresponding change */
	assert(curr_tn < csd->max_tn);	/* ensure CHECK_TN macro below will not issue TNTOOLARGE rts_error */
	CHECK_TN(csa, csd, curr_tn);	/* can issue rts_error TNTOOLARGE */
	/* increment csd->trans_hist.curr_tn */
	assert(csd->trans_hist.early_tn == csd->trans_hist.curr_tn);
	csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
	INCREMENT_CURR_TN(csd);
	if (FALSE == was_crit)
		rel_crit(reg);
	status = SS_NORMAL;
	send_msg(VARLSTCNT(11) ERR_DBDSRDFMTCHNG, 9, DB_LEN_STR(reg), LEN_AND_STR(gtm_dbversion_table[new_db_format]),
		LEN_AND_STR(command_name), process_id, process_id, &curr_tn);
	return status;
}
