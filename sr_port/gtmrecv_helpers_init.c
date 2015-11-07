/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include <sys/types.h>
#include "gtm_unistd.h"
#include "fork_init.h"
#include <sys/wait.h>
#elif defined(VMS)
#include <descrip.h> /* Required for gtmrecv.h */
#endif
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "repl_errno.h"
#include "gtm_stdio.h"
#include "repl_sem.h"
#include "io.h"
#include "is_proc_alive.h"
#include "gtmmsg.h"
#include "trans_log_name.h"
#include "repl_log.h"
#include "eintr_wrappers.h"
#include "memcoherency.h"
#include "getjobnum.h"

#define UPDHELPER_CMD_MAXLEN		1024
#define UPDHELPER_CMD			"$gtm_dist/mupip"
#define UPDHELPER_CMD_FILE		"mupip"
#define UPDHELPER_CMD_ARG1		"replicate"
#define UPDHELPER_CMD_ARG2		"-updhelper"
#define UPDHELPER_READER_CMD_ARG3	"-reader"
#define UPDHELPER_WRITER_CMD_ARG3	"-writer"
#define UPDHELPER_READER_CMD_STR	"REPLICATE/UPDHELPER/READER"
#define UPDHELPER_WRITER_CMD_STR	"REPLICATE/UPDHELPER/WRITER"
#define UPDHELPER_MBX_PREFIX		"GTMH"	/* first three must be GTM, and only character left for uniqueness  */
						/* U for update process, R for receiver, S for source, H for helper */

GBLREF	recvpool_addrs		recvpool;
GBLREF	FILE			*gtmrecv_log_fp;
GBLREF	uint4			process_id;

error_def(ERR_LOGTOOLONG);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_TEXT);

static int helper_init(upd_helper_entry_ptr_t helper, recvpool_user helper_type)
{
	int			save_errno, save_shutdown;
	char			helper_cmd[UPDHELPER_CMD_MAXLEN];
	int			status;
	int4			i4status;
	mstr			helper_log_cmd, helper_trans_cmd;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
#ifdef UNIX
	pid_t			helper_pid, waitpid_res;
#elif defined(VMS)
	uint4			helper_pid, cmd_channel;
	char			mbx_suffix[2 + 1]; /* hex representation of numbers 0 through MAX_UPD_HELPERS-1, +1 for '\0' */
	$DESCRIPTOR(cmd_desc_reader, UPDHELPER_READER_CMD_STR);
	$DESCRIPTOR(cmd_desc_writer, UPDHELPER_WRITER_CMD_STR);
#endif

	upd_helper_ctl = recvpool.upd_helper_ctl;
	save_shutdown = helper->helper_shutdown;
	helper->helper_shutdown = NO_SHUTDOWN;
#ifdef UNIX
	FORK(helper_pid);	/* BYPASSOK: we exec immediately, no FORK_CLEAN needed */
	if (0 > helper_pid)
	{
		save_errno = errno;
		helper->helper_shutdown = save_shutdown;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Could not fork update process"), save_errno);
		repl_errno = EREPL_UPDSTART_FORK;
		return UPDPROC_START_ERR;
	}
	if (0 == helper_pid)
	{	/* helper */
		getjobnum();
		helper->helper_pid_prev = process_id; /* identify owner of slot */
		helper_log_cmd.len  = STR_LIT_LEN(UPDHELPER_CMD);
		helper_log_cmd.addr = UPDHELPER_CMD;
		if (SS_NORMAL != (i4status = TRANS_LOG_NAME(&helper_log_cmd, &helper_trans_cmd, helper_cmd, SIZEOF(helper_cmd),
								dont_sendmsg_on_log2long)))
		{
			helper->helper_shutdown = save_shutdown;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				   LEN_AND_LIT("Could not find path of Helper Process. Check value of $gtm_dist"));
			if (SS_LOG2LONG == i4status)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3,
						LEN_AND_LIT(UPDHELPER_CMD), SIZEOF(helper_cmd) - 1);
			repl_errno = EREPL_UPDSTART_BADPATH;
			return UPDPROC_START_ERR;
		}
		helper_cmd[helper_trans_cmd.len] = '\0';
		if (-1 == EXECL(helper_cmd, helper_cmd, UPDHELPER_CMD_ARG1, UPDHELPER_CMD_ARG2,
				(UPD_HELPER_READER == helper_type) ? UPDHELPER_READER_CMD_ARG3 : UPDHELPER_WRITER_CMD_ARG3, NULL))
		{
			save_errno = errno;
			helper->helper_shutdown = save_shutdown;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				   LEN_AND_LIT("Could not exec Helper Process"), save_errno);
			repl_errno = EREPL_UPDSTART_EXEC;
			return UPDPROC_START_ERR;
		}
	}
#elif defined(VMS)
	/* Create detached server and write startup commands to it */
	i2hex(helper - upd_helper_ctl->helper_list, LIT_AND_LEN(mbx_suffix));
	mbx_suffix[SIZEOF(mbx_suffix) - 1] = '\0';
	/* A mailbox is created per helper, and the mailbox name is assigned to a logical. This logical will persist until the
	 * helper terminates. So, we need to assign a unique logical per helper. Hence the suffix. */
	if (SS_NORMAL != (status = repl_create_server((UPD_HELPER_READER == helper_type) ?  &cmd_desc_reader : &cmd_desc_writer,
					    		UPDHELPER_MBX_PREFIX, mbx_suffix, &cmd_channel, &helper->helper_pid_prev,
							ERR_RECVPOOLSETUP)))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
				ERR_TEXT, 2, LEN_AND_LIT("Unable to spawn Helper process"), status);
		helper->helper_shutdown = save_shutdown;
		repl_errno = EREPL_UPDSTART_FORK;
		return UPDPROC_START_ERR;
	}
	helper_pid = helper->helper_pid_prev;
#endif
	/* Wait for helper to startup */
	while (helper_pid != helper->helper_pid && is_proc_alive(helper_pid, 0))
	{
		SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
		UNIX_ONLY(WAITPID(helper_pid, &status, WNOHANG, waitpid_res);) /* Release defunct helper process if dead */
	}
	/* The helper has now gone far enough in the initialization, or died before initialization. Consider startup completed. */
#if defined(VMS)
	/* Deassign the send-cmd mailbox channel */
	if (SS_NORMAL != (status = sys$dassgn(cmd_channel)))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to close upd-send-cmd mbox channel"), status);
		helper->helper_shutdown = save_shutdown;
		repl_errno = EREPL_UPDSTART_BADPATH; /* Just to make an auto-shutdown */
		return UPDPROC_START_ERR;
	}
#endif
	repl_log(gtmrecv_log_fp, TRUE, TRUE, "Helper %s started. PID %d [0x%X]\n",
			(UPD_HELPER_READER == helper_type) ? "reader" : "writer", helper_pid, helper_pid);
	return UPDPROC_STARTED;
}

int gtmrecv_helpers_init(int n_readers, int n_writers)
{ /* Receiver server interface to start n_readers and n_writers helper processes */
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;
	int			reader_count, writer_count, error_count, avail_slots, status;

	assert(0 != n_readers || 0 != n_writers);
	upd_helper_ctl = recvpool.upd_helper_ctl;
	for (avail_slots = 0, helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS;
		helper < helper_top; helper++)
	{
		if (0 == helper->helper_pid)
			avail_slots++;
	}
	if (n_readers + n_writers > avail_slots)
	{ /* adjust reader/writer count for available slots according to the percentage specified by user */
		n_writers = (int)(((float)n_writers/(n_readers + n_writers)) * (float)avail_slots); /* may round down */
		n_readers = avail_slots - n_writers; /* preference to readers, writer count may round down */
	}
	/* Start helpers, readers first */
	for (helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS,
		reader_count = 0, writer_count = 0, error_count = 0;
		(reader_count + writer_count + error_count) < (n_readers + n_writers); )
	{
		for (; 0 != helper->helper_pid && helper < helper_top; helper++) /* find next vacant slot */
			;
		if (helper == helper_top)
			GTMASSERT;
		status = helper_init(helper, ((reader_count + error_count) < n_readers) ? UPD_HELPER_READER : UPD_HELPER_WRITER);
		if (UPDPROC_STARTED == status)
		{
			if ((reader_count + error_count) < n_readers)
				reader_count++;
			else
				writer_count++;
		} else /* UPDPROC_START_ERR == status */
		{
			if ((EREPL_UPDSTART_BADPATH == repl_errno) /* receiver server lost gtm_dist environment, bad situation */
					|| (EREPL_UPDSTART_EXEC == repl_errno)) /* in forked child, could not exec, should exit */
				gtmrecv_exit(ABNORMAL_SHUTDOWN);
			error_count++;
		}
	}
	upd_helper_ctl->start_n_readers = reader_count;
	upd_helper_ctl->start_n_writers = writer_count;
	SHM_WRITE_MEMORY_BARRIER;
	upd_helper_ctl->start_helpers = FALSE;
	return ((0 == error_count) ? NORMAL_SHUTDOWN : ABNORMAL_SHUTDOWN);
}
