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

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "util.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	gtmrecv_options_t	gtmrecv_options;
GBLREF	seq_num			seq_num_zero;
GBLREF	seq_num			seq_num_one;

int gtmrecv_showbacklog(void)
{
	seq_num		seq_num, read_jnl_seqno, jnl_seqno;

	QWASSIGN(jnl_seqno, recvpool.recvpool_ctl->jnl_seqno);
	if (QWEQ(jnl_seqno, seq_num_zero))
		 QWASSIGN(jnl_seqno, recvpool.recvpool_ctl->old_jnl_seqno);
	QWASSIGN(read_jnl_seqno, recvpool.upd_proc_local->read_jnl_seqno);

	QWSUB(seq_num, jnl_seqno, read_jnl_seqno);
	util_out_print("!@UQ : number of backlog transactions received by receiver server and yet to be processed "
		       "by update process", TRUE, &seq_num);

	QWASSIGN(seq_num, jnl_seqno);
	if (QWNE(seq_num, seq_num_zero))
		QWDECRBY(seq_num, seq_num_one);
	util_out_print("!@UQ : sequence number of last transaction received from Source Server and written to receive pool",
			TRUE, &seq_num);

	QWASSIGN(seq_num, read_jnl_seqno);
	if (QWNE(seq_num, seq_num_zero))
		QWDECRBY(seq_num, seq_num_one);
	util_out_print("!@UQ : sequence number of last transaction processed by update process", TRUE, &seq_num);

	return (NORMAL_SHUTDOWN);
}
