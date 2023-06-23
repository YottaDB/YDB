/****************************************************************
 *								*
 * Copyright (c) 2005-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include <sys/sem.h>
#include <sys/shm.h>
#include "gtm_un.h"
#ifndef __MVS__
#include <sys/param.h>
#endif

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
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "mupip_exit.h"
#include "repl_log.h"

GBLREF	void			(*call_on_signal)();
GBLREF	recvpool_addrs		recvpool;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	int4			forced_exit_err;
GBLREF	int4			exi_condition;

error_def(ERR_FORCEDHALT);

static void updhelper_common_cleanup(boolean_t exit)
{ /* common cleanup actions for reader and writer */
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	uint4			status;


	upd_helper_ctl = recvpool.upd_helper_ctl;
	if (NULL != helper_entry)
	{
		if (exit || ERR_FORCEDHALT == forced_exit_err)
		{ /* Let the receiver know this was a clean shutdown and the slot is available for re-use. Checkhealth will not
		   * report  NOT alive for this type of shutdown */
			helper_entry->helper_shutdown = NORMAL_SHUTDOWN;
		} else
		{ /* Abnormal termination. Slot is available for re-use. However, checkhealth will report NOT alive status
		   * until a new helper takes up this slot or forcefully cleared by a subsequent start -helpers command */
			helper_entry->helper_shutdown = ABNORMAL_SHUTDOWN;
		}
		helper_entry->helper_pid = 0; /* vacate my slot */
		if (!exit) /* terminating due to a signal */
		{
			assert(NULL != upd_helper_ctl);
			upd_helper_ctl->reap_helpers = HELPER_REAP_NOWAIT; /* let the receiver know it should reap my slot */
		}
	}
	SHMDT(recvpool.recvpool_ctl);
	recvpool.recvpool_ctl = NULL;
	helper_entry = NULL;
	return;
}

static void updhelper_reader_stop(boolean_t exit)
{
	call_on_signal = NULL; /* do not re-enter on error */
	updhelper_common_cleanup(exit);
	if (exit)
		mupip_exit(SS_NORMAL);
	return;
}

static void updhelper_writer_stop(boolean_t exit)
{
	call_on_signal = NULL; /* do not re-enter on error */
	updhelper_common_cleanup(exit);
	if (exit)
		mupip_exit(SS_NORMAL);
	return;
}

void updhelper_reader_sigstop(void)
{ /* reader termination due to a signal */
	updhelper_reader_stop(FALSE);
	return;
}

void updhelper_reader_end(void)
{ /* reader shutdown */
	updhelper_reader_stop(TRUE);
	return;
}

void updhelper_writer_sigstop(void)
{ /* writer termination due to a signal  */
	updhelper_writer_stop(FALSE);
	return;
}

void updhelper_writer_end(void)
{ /* writer shutdown */
	updhelper_writer_stop(TRUE);
	return;
}
