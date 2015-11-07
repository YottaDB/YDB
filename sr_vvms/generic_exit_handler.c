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

#include <ssdef.h>
#include "gtm_inet.h"
#include <psldef.h>
#include <descrip.h>
#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gtm_stdio.h"	/* for FILE structure etc. */
#include "gtm_time.h"

#include "ast.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "iotimer.h"
#include "jnl.h"
#include "locklits.h"
#include "gtmrecv.h"	/* for recvpool etc. */
#include "io.h"
#include "iosp.h"
#include "sleep_cnt.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sem.h"
#include "repl_shm.h"
#include "desblk.h"
#include "gtmimagename.h"
#include "util.h"
#include "op.h"
#include "repl_log.h"
#include "generic_exit_handler.h"
#include "gv_rundown.h"
#include "have_crit.h"
#include "print_exit_stats.h"
#include "setzdir.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtmmsg.h"
#include "secshr_db_clnup.h"
#include "repl_shutdcode.h"

GBLREF	void			(*call_on_signal)();
GBLREF	uint4			dollar_tlevel;
GBLREF	int4			exi_condition;
GBLREF	desblk			exi_blk;
GBLREF	int			gtmsource_srv_count, gtmrecv_srv_count;
GBLREF	FILE			*gtmsource_log_fp, *gtmrecv_log_fp;
GBLREF	boolean_t        	is_src_server, is_rcvr_server, is_tracing_on, is_updproc, is_updhelper;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;
GBLREF	uint4			process_id;
GBLREF	recvpool_addrs		recvpool;
GBLREF	FILE			*updproc_log_fp;
GBLREF	mval			original_cwd;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	volatile boolean_t	in_wcs_recover; /* TRUE if in "wcs_recover" */
GBLREF	boolean_t		exit_handler_active;
GBLREF	int			process_exiting;

error_def(ERR_ACK);
error_def(ERR_CRITRESET);
error_def(ERR_FORCEDHALT);
error_def(ERR_MUJNLSTAT);
error_def(ERR_UNKNOWNFOREX);

void generic_exit_handler(void)
{
	void		(*signal_routine)();
	boolean_t	is_mupip, is_gtm;
	int4		lcnt;
	gd_addr		*addr_ptr;
	gd_region	*reg, *r_top;
	sgmnt_addrs	*csa;
	static int	invoke_cnt = 0;	/* how many times generic_exit_handler was invoked in the lifetime of this process */

	is_gtm = IS_GTM_IMAGE;
	is_mupip = IS_MUPIP_IMAGE;
	exit_handler_active = TRUE;
	sys$setast(ENABLE);	/* safer and doesn't hurt much */
	if (is_tracing_on)
		turn_tracing_off(NULL);
	/* We expect generic_exit_handler to be invoked at most twice. Once by MUPIP STOP at which time we might have decided
	 * to defer exiting (because of holding crit etc.) but after all those resources which prevented us from exiting have
	 * been released, we should reinvoke generic_exit_handler once more and that invocation SHOULD EXIT. Assert this.
	 */
	assert(invoke_cnt < 2);
	DEBUG_ONLY(invoke_cnt++;)
	/* We can defer exit-handling if it was a forced-halt and we are in an AST or have crit in any region.
	 * If we are in an AST when a fatal exception occurs we can neither defer exiting nor do normal exit-handling,
	 * 	so we return immediately with the hope that the privileged exit-handler in GTMSECSHR,
	 *	secshr_db_clnup(ABNORMAL_TERMINATION) will do the necessary cleanup.
	 * Note that even if we hold crit in any region when a non-deferrable exception occurs, we can still go ahead with
	 *	normal exit-handling chores. secshr_db_clnup(NORMAL_TERMINATION) (invoked below) will cleanup the crits for us.
	 */
	if (ERR_FORCEDHALT == exi_condition || 0 == exi_condition)
	{
		if (lib$ast_in_prog())	/* The rest of this assumes that it may use AST's */
		{
			assert(!process_exiting);
			EXIT_HANDLER(&exi_blk);				/* reestablish the exit handler */
			sys$forcex(&process_id);			/* make the forcex come back as an AST */
			ESTABLISH(exi_ch);				/* set a condition handler to unwind exit handler levels */
			rts_error_csa(CSA_ARG(NULL) ERR_ACK);		/* and signal success */
		}
		assert(!lib$ast_in_prog());
		/* There are at least three cases where we definitely want to defer exiting as abnormally terminating could
		 * potentially cause database damage. They are
		 *	a) Commit phase (beginning from when early_tn is curr_tn + 1 to when they become equal)
		 *	b) Cache recovery phase (if we are in "wcs_recover" recovering the cache). In that case too
		 *		early_tn is set to curr_tn + 1 (just like (a)) so no special processing is needed.
		 *	c) When the intrpt_ok_state is flagged
		 */
		if ((0 != have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)) || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state))
		{
			assert(!process_exiting);
			EXIT_HANDLER(&exi_blk);
			ESTABLISH(exi_ch);
			rts_error_csa(CSA_ARG(NULL) exi_condition ? exi_condition : ERR_FORCEDHALT);
		}
		assert(!in_wcs_recover); /* case (b) above should have been handled by have_crit(CRIT_IN_COMMIT) itself */
	} else if (lib$ast_in_prog())
		rts_error_csa(CSA_ARG(NULL) exi_condition);	/* this shouldn't return */
	process_exiting = TRUE;
	sys$cantim(0,0); /* cancel all outstanding timers.  prevents unwelcome surprises...DO this only after AFTER we decide NOT
			  * to defer exit-handling as otherwise we might cause a deadlock due to cancelling a timer that the
			  * mainline code holding CRIT is waiting for */
	SET_FORCED_EXIT_STATE_ALREADY_EXITING;
	ESTABLISH(lastchance1);
	/* call_on_signal is currently used only by certain mupip commands, but it will be NULL if unused */
	if ((SS$_NORMAL != exi_condition) && call_on_signal && (!(IS_GTM_ERROR(exi_condition)) || ERR_FORCEDHALT == exi_condition))
	{
		assert(!lib$ast_in_prog());
		signal_routine = call_on_signal;
		call_on_signal = NULL;
		(*signal_routine)();
	}
	finish_active_jnl_qio();
	if (jgbl.mupip_journal)
		mur_close_files(); /* Will it increase gtm image size pulling mur*.c ??? */
	DETACH_FROM_JNLPOOL(pool_init, jnlpool, jnlpool_ctl);
	secshr_db_clnup(NORMAL_TERMINATION);
	if (dollar_tlevel)
		OP_TROLLBACK(0);
	if (is_gtm)	/* do lock rundown only for GT.M (though it probably is harmless if invoked by the others) */
	{
		op_lkinit();	/* seems to be GT.CM client specific */
		op_unlock();
		op_zdeallocate(NO_M_TIMEOUT);
	}
	REVERT;
	ESTABLISH(lastchance2);
	gv_rundown();
	REVERT;
	ESTABLISH(lastchance3);
	print_exit_stats();
	io_rundown(NORMAL_RUNDOWN);
	if (0 == exi_condition)
		exi_condition = ERR_UNKNOWNFOREX;
	if (is_mupip)		/* do mupip-specific replication processing  */
	{
		assert(!lib$ast_in_prog());
		if (is_updhelper && NULL != helper_entry) /* haven't had a chance to cleanup, must be an abnormal exit */
		{
			helper_entry->helper_shutdown = ABNORMAL_SHUTDOWN;
			helper_entry->helper_pid = 0; /* vacate my slot */
			helper_entry = NULL;
		}
		if (recvpool.recvpool_ctl)
		{
			detach_shm(recvpool.shm_range);
			signoff_from_gsec(recvpool.shm_lockid);
			memset(&recvpool, 0, SIZEOF(recvpool));
		}
		if (is_src_server && sem_set_exists(SOURCE) && gtmsource_srv_count && (0 != rel_sem(SOURCE, SRC_SERV_COUNT_SEM)))
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Error releasing the source server count semaphore : %s\n",
					REPL_SEM_ERROR);
		if (is_rcvr_server && sem_set_exists(RECV) && gtmrecv_srv_count && (0 != rel_sem(RECV, RECV_SERV_COUNT_SEM)))
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Error releasing the receiver server count semaphore : %s\n",
					REPL_SEM_ERROR);
		if (is_updproc && sem_set_exists(RECV) && (0 != rel_sem(RECV, UPD_PROC_COUNT_SEM)))
			repl_log(updproc_log_fp, TRUE, TRUE, "Error releasing the update process count semaphore : %s\n",
					REPL_SEM_ERROR);
		/* log the exit of replication servers */
		if (is_src_server)
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server exiting...\n");
		else if (is_rcvr_server)
			repl_log(gtmrecv_log_fp, TRUE, TRUE, "Receiver server exiting...\n");
		else if (is_updproc)
			repl_log(updproc_log_fp, TRUE, TRUE, "Update process exiting...\n");
		else if (is_updhelper)
			repl_log(updproc_log_fp, TRUE, TRUE, "Helper exiting...\n");
		util_out_close();
	}
	if (is_gtm)
		setzdir(&original_cwd, NULL); /* Restore the image startup cwd. This is done here 'cos VMS chdir() doesn't behave
					       * as documented. See comments in setzdir(). */
}
