/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include <sys/sem.h>
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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

GBLREF	recvpool_addrs	recvpool;
GBLREF  gd_addr         *gd_header;
GBLREF  boolean_t        repl_allowed;
GBLREF  boolean_t        pool_init;

error_def(ERR_UPDATEFILEOPEN);

int updproc_init(gld_dbname_list **gld_db_files , seq_num *start_jnl_seqno)
{
	mval            	v;
	uint4			log_file_len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gld_dbname_list		*curr;

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_TEXT);

	recvpool_init(UPDPROC, FALSE, FALSE);
	upd_log_init(UPDPROC);

	/*
	 * Lock the update process count semaphore. Its value should
	 * be atmost 1
	 */
	if (0 != grab_sem_immediate(RECV, UPD_PROC_COUNT_SEM))
	{
		if (REPL_SEM_NOT_GRABBED)
			return(UPDPROC_EXISTS);
		else
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Receive pool semop error"), REPL_SEM_ERRNO);
	}

	v.mvtype = MV_STR; /* get the desired global directory */
        v.str.len = 0;
        gd_header = zgbldir(&v);
	*gld_db_files = read_db_files_from_gld(gd_header);/* read the global directory read
					all the database files to be opened */
        if (!updproc_open_files(gld_db_files, start_jnl_seqno)) /* open and initialize all regions */
                mupip_exit(ERR_UPDATEFILEOPEN);
	if (repl_allowed)
	{
		jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);
	        for (curr = *gld_db_files;  NULL != curr; curr = curr->next)
		{
			csa = &FILE_INFO(curr->gd)->s_addrs;
			csd = csa->hdr;
			csa->n_pre_read_trigger = (int)((csd->n_bts * (float)csd->reserved_for_upd / csd->avg_blks_per_100gbl) *
										csd->pre_read_trigger_factor / 100.0);
			REPL_DPRINT2("csa->nl->n_pre_read_trigger = %x", csa->n_pre_read_trigger);
		}
	}
	return UPDPROC_STARTED;
}
