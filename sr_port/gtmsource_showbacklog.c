/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#include <errno.h>
#include "gtm_string.h"
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
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	seq_num			seq_num_zero;
GBLREF	seq_num			seq_num_one;

int gtmsource_showbacklog(void)
{
	seq_num		seq_num, jnl_seqno, read_jnl_seqno;
	unsigned char	seq_num_str[32], *seq_num_ptr;

	QWASSIGN(read_jnl_seqno, jnlpool.gtmsource_local->read_jnl_seqno);
	QWASSIGN(jnl_seqno, jnlpool.jnlpool_ctl->jnl_seqno);
	/* jnl_seqno >= read_jnl_seqno is the most common case; see gtmsource_readpool() for when the rare case can occur */
	seq_num = (jnl_seqno >= read_jnl_seqno) ? jnl_seqno - read_jnl_seqno : 0;
	seq_num_ptr = i2ascl(seq_num_str, seq_num);
	util_out_print("!AD : backlog number of transactions written to journal pool and "
			"yet to be sent by the source server", TRUE, seq_num_ptr - &seq_num_str[0], seq_num_str);
	QWASSIGN(seq_num, jnl_seqno);
	if (QWNE(seq_num, seq_num_zero))
		QWDECRBY(seq_num, seq_num_one);
	seq_num_ptr = i2ascl(seq_num_str, seq_num);
	util_out_print("!AD : sequence number of last transaction written to journal pool",
			TRUE, seq_num_ptr - &seq_num_str[0], seq_num_str);
	QWASSIGN(seq_num, read_jnl_seqno);
	if (QWNE(seq_num, seq_num_zero))
		QWDECRBY(seq_num, seq_num_one);
	seq_num_ptr = i2ascl(seq_num_str, seq_num);
	util_out_print("!AD : sequence number of last transaction sent by source server",
			TRUE, seq_num_ptr - &seq_num_str[0], seq_num_str);
	if (jnlpool.gtmsource_local->mode == GTMSOURCE_MODE_PASSIVE)
	{
		seq_num_ptr = i2ascl(seq_num_str, seq_num_zero);
		util_out_print("WARNING - Source Server is in passive mode, transactions are not being replicated", TRUE);
	}
	return (NORMAL_SHUTDOWN);
}
