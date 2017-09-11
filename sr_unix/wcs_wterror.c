/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "interlock.h"
#include "relqueopi.h"
#include "gdsbgtr.h"
#include "aio_shim.h"
#include "gtmio.h"
#include "is_proc_alive.h"
#include "anticipatory_freeze.h"
#include "add_inter.h"
#include "gtm_multi_proc.h"	/* for "multi_proc_in_use" GBLREF */
#include "wcs_wt.h"
#include "compswap.h"

#define DBIOERR_LOGGING_PERIOD			100
#define DSKSPACE_MSG_INTERVAL 			60 	/* 60 seconds, epoch time */

#ifdef USE_LIBAIO
GBLREF char	*aio_shim_errstr;
#endif

error_def(ERR_DBFILERR);
error_def(ERR_DBIOERR);
error_def(ERR_SYSCALL);
error_def(ERR_ENOSPCQIODEFER);

STATICDEF volatile uint4 	eagain_error_count;

/* This function is called from wcs_wtstart (for noasyncio and asyncio cases) and/or wcs_wtfini (for asyncio) when they each
 * encounter an error in a write to the database file on disk. It could be ENOSPC or some other IO error. Handle all of them
 * by sending periodic syslog messages etc.
 */
void	wcs_wterror(gd_region *reg, int4 save_errno)
{
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;
	gtm_uint64_t		dskspace_next_fire;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	BG_TRACE_PRO_ANY(csa, wcs_wterror_invoked);
	cnl = csa->nl;
	if (ENOSPC == save_errno)
	{	/* Determine whether or not to ignore this error, based on when the last ENOSPC error was reported. */
		dskspace_next_fire = cnl->dskspace_next_fire;
		if ((dskspace_next_fire + DSKSPACE_MSG_INTERVAL <= time(NULL))
				/* We use a CAS instruction to ensure that concurrent accesses to this location don't fire
				 * multiple times by different processes; the first one to swap dskspace_next_fire is the
				 * only one to report the ENOSPC error. A blind interlock_add() would not prevent this.
				 */
				&& COMPSWAP_LOCK((sm_global_latch_ptr_t)&cnl->dskspace_next_fire,
						 dskspace_next_fire, 0, time(NULL), 0))
		{	/* Report ENOSPC errors for first time and every minute after that. */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBIOERR, 2, DB_LEN_STR(reg),
				 ERR_TEXT, 2, RTS_ERROR_TEXT("Error during flush write"), save_errno);
			if (!IS_GTM_IMAGE)
			{
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBIOERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error during flush write"), save_errno);
			}
		}
	} else if (EAGAIN == save_errno)
	{ 	/* When using POSIX AIO we don't ever expect to see an EAGAIN error. */
		assert(IF_LIBAIO_ELSE(NULL != aio_shim_errstr, FALSE));
		/* If EAGAIN occurs from "io_submit", do not treat it as an ERROR. We know it can happen if more than the
		 * allocated aio slots are issued as writes concurrently by this process. In that case, the count of
		 * wcs_wterror_invoked in file header is enough to indicate how many times such events occured.
		 */
#		ifdef USE_LIBAIO
		if ((NULL != aio_shim_errstr) && STRCMP(aio_shim_errstr, "io_submit()"))
		{
#		endif
			eagain_error_count++;
			if (1 == (eagain_error_count % DBIOERR_LOGGING_PERIOD))
			{	/* See below; every 100th failed attempt, issue a warning. We cannot issue a DBIOERR in
				 * the case of an EAGAIN because it is innocuous and can easily be retried -- a DBIOERR
				 * will freeze the database forcing us to not perform a retry at all.
				 */
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
					LEN_AND_STR(IF_LIBAIO_ELSE(aio_shim_errstr, "aio_write()")), CALLFROM, save_errno);
			}
#		ifdef USE_LIBAIO
			aio_shim_errstr = NULL;
		}
#		endif
	} else if (ERR_ENOSPCQIODEFER != save_errno)
	{
		cnl->wtstart_errcnt++;
		if (1 == (cnl->wtstart_errcnt % DBIOERR_LOGGING_PERIOD))
		{	/* Every 100th failed attempt, issue an operator log indicating an I/O error.
			 * wcs_wtstart is typically invoked during periodic flush timeout and since there
			 * cannot be more than 2 pending flush timers per region, number of concurrent
			 * processes issuing the below send_msg should be relatively small even if there
			 * are 1000s of processes.
			 */
			/* Below assert is to account for some white-box tests which exercise this code as
			 * well as tests which could trigger a CRYPTOPFAILED inside the encryption plugin.
			 * Neither of those are real IO errors.
			 */
			assert(gtm_white_box_test_case_enabled
				|| (SET_REPEAT_MSG_MASK(SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)) == save_errno));
#ifdef 			USE_LIBAIO
			if (NULL == aio_shim_errstr)
			{
#			endif
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_DBIOERR, 4, REG_LEN_STR(reg), DB_LEN_STR(reg),
						save_errno);
#ifdef			USE_LIBAIO
			} else
			{	/* If the error string was set, then we can output the syscall that failed as well. */
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(15) ERR_DBIOERR, 4, REG_LEN_STR(reg), DB_LEN_STR(reg),
						ERR_SYSCALL, 5, LEN_AND_STR(aio_shim_errstr), CALLFROM,
						save_errno);
				aio_shim_errstr = NULL;
			}
#			endif
		}
	}
	/* If (ERR_ENOSPCQIODEFER == save_errno): DB_LSEEKWRITE above encountered ENOSPC but could not
	 * trigger a freeze as it did not hold crit. It is okay to return as this is not a critical write.
	 * Eventually, some crit holding process will trigger a freeze and wait for space to be freed up.
	 * Analogously, if we detected that encryption settings have changed during a transaction, it is OK
	 * to skip this write because this transaction will be retried after encryption settings update in
	 * t_retry or tp_restart.
	 */
	return;
}
