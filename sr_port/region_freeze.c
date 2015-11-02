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

#include "gtm_stdio.h"

#ifdef VVMS_GTCX
#include <iodef.h>
#include <fab.h>
#include <efndef.h>

#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"

#ifdef VVMS_GTCX
#include "ccp.h"
#include "ccpact.h"
#include "iosp.h"
#endif
#include "send_msg.h"
#include "caller_id.h"
#include "sleep_cnt.h"
#include "interlock.h"
#include "add_inter.h"
#include "wcs_sleep.h"

GBLREF	bool		caller_id_flag;
GBLREF	bool		in_mupip_freeze;
GBLREF	uint4		process_id;
#ifdef UNIX
GBLREF	uint4		user_id;
#define FREEZE_ID	(0 == user_id ? FROZEN_BY_ROOT : user_id)
#define FREEZE_MATCH	process_id
#define OWNERSHIP	(in_mupip_freeze ? (csd->freeze == freeze_id) : (csd->image_count == FREEZE_MATCH))
#elif defined VMS
GBLREF	uint4		image_count;
#define FREEZE_ID	process_id
#define FREEZE_MATCH	image_count
#define OWNERSHIP	((csd->freeze == process_id) && (in_mupip_freeze || (csd->image_count == FREEZE_MATCH)))
#else
#error Unsupported Platform
#endif

#define SEND_FREEZEID(state)							\
{										\
	caller_id_flag = FALSE;							\
	send_msg(VARLSTCNT(9) ERR_FREEZEID, 7, LEN_AND_STR(state),		\
			DB_LEN_STR(region),					\
			freeze_id, FREEZE_MATCH, caller_id());			\
	caller_id_flag = TRUE;							\
}

freeze_status	region_freeze(gd_region *region, boolean_t freeze, boolean_t override, boolean_t wait_for_kip)
{
	uint4			freeze_id, sleep_counter;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	error_def(ERR_FREEZEID);

	freeze_id = FREEZE_ID;
	csa = &FILE_INFO(region)->s_addrs;
	csd = csa->hdr;
	if (freeze)
	{
		grab_crit(region);	/* really need this to be sure in UNIX, shouldn't be frequent anyway */
		INCR_INHIBIT_KILLS(csa->nl);
		if (OWNERSHIP)
		{
			DECR_INHIBIT_KILLS(csa->nl);
			rel_crit(region);
			return REG_FREEZE_SUCCESS;
		}
		if (!override && csd->freeze)
		{
			DECR_INHIBIT_KILLS(csa->nl);
			rel_crit(region);
			return REG_ALREADY_FROZEN;
		}
		/* If override is TRUE we need not wait for KIP to become zero */
		sleep_counter = 1;
		if (!override && wait_for_kip && (0 < csd->kill_in_prog))
		{
			rel_crit(region);
			do
			{
				grab_crit(region);
				if (!csd->kill_in_prog)
					break;
				rel_crit(region);
				wcs_sleep(sleep_counter);
			} while (MAX_CRIT_TRY > sleep_counter++);
		}
		/* if can't ever be true when override is true. */
		if (MAX_CRIT_TRY <= sleep_counter)
		{
			DECR_INHIBIT_KILLS(csa->nl);
			rel_crit(region);
			return REG_HAS_KIP;
		}
		csd->freeze = freeze_id;		/* the order of this line and the next is important */
		csd->image_count = FREEZE_MATCH;
		csa->freeze = TRUE;
		DECR_INHIBIT_KILLS(csa->nl);
		rel_crit(region);
#ifdef VVMS_GTCX
		if (csd->clustered)
		{
			unsigned short		iosb[4];

			(void)sys$qiow(EFN$C_ENF, FILE_INFO(region)->fab->fab$l_stv, IO$_WRITEVBLK, iosb, NULL, 0,
					csd, (MM_BLOCK - 1) * DISK_BLOCK_SIZE, 1, 0, 0, 0);
		}
#endif
#ifdef DEBUG_FREEZE
		SEND_FREEZEID("FREEZE");
#endif
		return REG_FREEZE_SUCCESS;
	}
	if (0 == csd->freeze)
		return REG_FREEZE_SUCCESS;
	if (override || OWNERSHIP)
	{
		csd->image_count = 0;		/* the order of this line and the next is important */
		csd->freeze = 0;
		csa->freeze = FALSE;
#ifdef VVMS_GTCX
	  	if (csd->clustered)
		{
			ccp_action_aux_value	msg;
			unsigned short		iosb[4];
			void			ccp_sendmsg();

			(void)sys$qiow(EFN$C_ENF, FILE_INFO(region)->fab->fab$l_stv, IO$_WRITEVBLK, iosb, NULL, 0,
					csd, (MM_BLOCK - 1) * DISK_BLOCK_SIZE, 1, 0, 0, 0);
			if (csa->nl->ccp_crit_blocked)
			{
				msg.exreq.fid = FILE_INFO(region)->file_id;
				msg.exreq.cycle = csa->nl->ccp_cycle;
				ccp_sendmsg(CCTR_EXITWM, &msg);
			}
		}
#endif
#ifdef DEBUG_FREEZE
		SEND_FREEZEID("UNFREEZE");
#endif
		return REG_FREEZE_SUCCESS;
	}
	return REG_ALREADY_FROZEN;
}
