/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>	/* for ENOSPC */

#include "anticipatory_freeze.h"
#include "wait_for_disk_space.h"
#include "gtmio.h"
#include "tp_grab_crit.h"
#include "have_crit.h"
#include "filestruct.h"
#include "jnl.h"
#include "error.h"
#include "gtmmsg.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	volatile int4		exit_state;
GBLREF	int4			exi_condition;
GBLREF	int4			forced_exit_err;

error_def(ERR_DSKNOSPCAVAIL);
error_def(ERR_DSKNOSPCBLOCKED);
error_def(ERR_DSKSPCAVAILABLE);
error_def(ERR_ENOSPCQIODEFER);

/* In case of ENOSPC, if anticipatory freeze scheme is in effect and this process has attached to the
 * journal pool, trigger an instance freeze in this case and wait for the disk space to be available
 * at which point unfreeze the instance.
 */
void wait_for_disk_space(sgmnt_addrs *csa, char *fn, int fd, off_t offset, char *buf, size_t count, int *save_errno)
{
	boolean_t	was_crit;
	gd_region	*reg;
	int		fn_len, tmp_errno;
	boolean_t	freeze_cleared;
	char		wait_comment[MAX_FREEZE_COMMENT_LEN];
	DCL_THREADGBL_ACCESS;	/* needed by ANTICIPATORY_FREEZE_AVAILABLE macro */

	SETUP_THREADGBL_ACCESS;	/* needed by ANTICIPATORY_FREEZE_AVAILABLE macro */
	/* If anticipatory freeze scheme is not in effect OR if this database does not care about it, return right away */
	if (!ANTICIPATORY_FREEZE_ENABLED(csa))
		return;
	fn_len = STRLEN(fn);
	was_crit = csa->now_crit;
	reg = csa->region;
	/* Let us take the case this process has opened the database but does not hold crit on it. If we come in to this
	 * function while trying to flush either to the db or jnl, setting the instance freeze would require a "grab_lock"
	 * which could hang due to another process holding that and in turn waiting for the exact same db or jnl write
	 * to succeed. This has the potential of creating a deadlock so we avoid that by returning right away. Since we
	 * dont hold crit, this call to do the jnl or db write is not critical in the sense no process will be affected
	 * because this write did not happen. Therefore it is okay to return right away. On the other hand if this
	 * process holds crit, then it is not possible that the other process holds the jnlpool lock
	 * and is waiting for the db or jnl qio (since that flow usually happens in t_end and tp_tend
	 * where db crit is first obtained before jnlpool lock is). Therefore it is safe to do a grab_lock
	 * in that case without worrying about potential deadlocks.
	 * Update *save_errno to indicate this is not a ENOSPC condition (since we have chosen to defer
	 * the ENOSPC condition to some other process that encounters it while holding crit).
	 *
	 * There is a possibility that if the caller is jnl_wait we will retry this logic indefinitely without ever
	 * setting instance freeze because we dont hold crit. To avoid that, do tp_grab_crit to see if it is available.
	 * If so, go ahead with freezing the instance. If not issue QIODEFER message and return. It is still possible
	 * the same process issues multiple QIODEFER messages before the instance gets frozen. But it should be rare.
	 */
	if (!was_crit)
		tp_grab_crit(reg);
	if (!csa->now_crit)
	{
		send_msg(VARLSTCNT(4) ERR_ENOSPCQIODEFER, 2, fn_len, fn);
		*save_errno = ERR_ENOSPCQIODEFER;
		return;
	}
	/* We either came into this function holding crit or "tp_grab_crit" succeeded */
	assert(NULL != jnlpool.jnlpool_ctl);
	assert(NULL != fn);	/* if "csa" is non-NULL, fn better be non-NULL as well */
	/* The "send_msg" of DSKNOSPCAVAIL done below will set instance freeze (if configuration files includes it). After that, we
	 * will keep retrying the IO waiting for disk space to become available. If yes, we will clear the freeze. Until that is
	 * done, we should not allow ourselves to be interrupted as otherwise interrupt code can try to write to the db/jnl (as
	 * part of DB_LSEEKWRITE) and the first step there would be to wait for the freeze to be lifted off. Since we were the ones
	 * who set the freeze in the first place, the auto-clearing of freeze (on disk space freeup) will no longer work in that
	 * case. Hence the reason not to allow interrupts.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_WAIT_FOR_DISK_SPACE);
	send_msg(VARLSTCNT(4) ERR_DSKNOSPCAVAIL, 2, fn_len, fn); /* this should set the instance freeze */
	/* Hang waiting for the disk space situation to be cleared */
	if (IS_REPL_INST_FROZEN)
	{
		GENERATE_INST_FROZEN_COMMENT(wait_comment, MAX_FREEZE_COMMENT_LEN, ERR_DSKNOSPCAVAIL);
		tmp_errno = *save_errno;
		assert(ENOSPC == tmp_errno);
		for ( ; ENOSPC == tmp_errno; )
		{
			if (!IS_REPL_INST_FROZEN)
			{	/* Some other process cleared the instance freeze. But we still dont have our disk
				 * space issue resolved so set the freeze flag again until space is available for us.
				 */
				send_msg(VARLSTCNT(4) ERR_DSKNOSPCAVAIL, 2, fn_len, fn);
			} else if (exit_state != 0)
			{
				send_msg(VARLSTCNT(1) forced_exit_err);
				gtm_putmsg(VARLSTCNT(1) forced_exit_err);
				exit(-exi_condition);
			}
			/* Sleep for a while before retrying the write. Do not use "hiber_start" as that
			 * uses timers and if we are already in a timer handler now, nested timers wont work.
			 */
			SHORT_SLEEP(SLEEP_IORETRYWAIT);
			/* If some other process froze the instance and changed the comment, a retry of the
			 * LSEEKWRITE may not be appropriate, so just loop waiting for the freeze to be lifted.
			 */
			if (IS_REPL_INST_FROZEN && (STRCMP(wait_comment, jnlpool.jnlpool_ctl->freeze_comment) != 0))
			{
				send_msg(VARLSTCNT(4) ERR_DSKNOSPCBLOCKED, 2, fn_len, fn);
				WAIT_FOR_REPL_INST_UNFREEZE(csa)
			}
			LSEEKWRITE(fd, offset, buf, count, tmp_errno);
		}
		if (STRCMP(wait_comment, jnlpool.jnlpool_ctl->freeze_comment) == 0)
		{
			send_msg(VARLSTCNT(4) ERR_DSKSPCAVAILABLE, 2, fn_len, fn);
			CLEAR_ANTICIPATORY_FREEZE(freeze_cleared);
			REPORT_INSTANCE_UNFROZEN(freeze_cleared);
		}
		*save_errno = tmp_errno;
	} /* else ERR_DSKNOSPCAVAIL is not present in the configuration file. So, no freeze is triggered and the caller will handle
	   * it accordingly
	   */
	ENABLE_INTERRUPTS(INTRPT_IN_WAIT_FOR_DISK_SPACE);
	if (!was_crit)
		rel_crit(reg);
	return;
}
