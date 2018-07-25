/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
#include <sys/sem.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "repl_log.h"
#include "repl_sem.h"
#include "mupip_exit.h"
#include "dpgbldir.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "repl_dbg.h"

GBLREF	int			pool_init;
GBLREF	gd_addr			*gd_header;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	FILE			*updproc_log_fp;

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_TEXT);
error_def(ERR_UPDATEFILEOPEN);

int updproc_init(gld_dbname_list **gld_db_files , seq_num *start_jnl_seqno)
{
	mval            	v;
	int			save_errno;
	uint4			log_file_len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gld_dbname_list		*curr;

	gvinit();	/* get the desired global directory, sets "gd_header" */
	/* Do jnlpool_init ahead of recvpool_init so in case we have a ftok_counter_halted situation we have
	 * jnlpool->repl_inst_filehdr initialized to set it to TRUE. This is later relied upon by "gtmsource_shutdown"
	 * or "gtmrecv_shutdown" when they do the "ftok_sem_release".
	 */
	jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL, gd_header);
	recvpool_init(UPDPROC, FALSE);
	/* The log file can be initialized only after having attached to the receive pool as the update process log file name
	 * is derived from the receiver server log file name which is in turn available only in the receive pool.
	 */
	upd_log_init(UPDPROC);
	/* Lock the update process count semaphore. Its value should be atmost 1. The call to grab the update process
	 * COUNT semaphore can be done only after attaching to the receive pool as that is what initializes the semaphore
	 * set id which is in turn used by the "grab_sem_immediate" function.
	 */
	if (0 != grab_sem_immediate(RECV, UPD_PROC_COUNT_SEM))
	{
		save_errno = errno;
		if (REPL_SEM_NOT_GRABBED)
			return UPDPROC_EXISTS;
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Receive pool semop error"), save_errno);
	}
	repl_log(updproc_log_fp, TRUE, TRUE, "Attached to existing jnlpool with shmid = [%d] and semid = [%d]\n",
			jnlpool->repl_inst_filehdr->jnlpool_shmid, jnlpool->repl_inst_filehdr->jnlpool_semid);
	repl_log(updproc_log_fp, TRUE, TRUE, "Attached to existing recvpool with shmid = [%d] and semid = [%d]\n",
			jnlpool->repl_inst_filehdr->recvpool_shmid, jnlpool->repl_inst_filehdr->recvpool_semid);
	/* Note: dpzgbini() is already called as part of gtm_startup which in turn is invoked by init_gtm.
	 * So no need to do this initialization.
	 */
	*gld_db_files = read_db_files_from_gld(gd_header);/* read all the database files to be opened in this global directory */
	if (!updproc_open_files(gld_db_files, start_jnl_seqno)) /* open and initialize all regions */
		mupip_exit(ERR_UPDATEFILEOPEN);
	for (curr = *gld_db_files;  NULL != curr; curr = curr->next)
	{
		csa = &FILE_INFO(curr->gd)->s_addrs;
		csd = csa->hdr;
		csa->n_pre_read_trigger = (int)((csd->n_bts * (float)csd->reserved_for_upd / csd->avg_blks_per_100gbl) *
									csd->pre_read_trigger_factor / 100.0);
		REPL_DPRINT2("csa->nl->n_pre_read_trigger = %x", csa->n_pre_read_trigger);
	}
	return UPDPROC_STARTED;
}
