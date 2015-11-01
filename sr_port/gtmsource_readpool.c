/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <sys/stat.h>
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
#include "jnl.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "longcpy.h"

GBLREF	jnlpool_addrs	jnlpool;
GBLREF	qw_off_t	jnlpool_size;

boolean_t jnlpool_hasnt_overflowed(seq_num read_addr)
{
	qw_off_t	unread_data;

	QWSUB(unread_data, jnlpool.jnlpool_ctl->early_write_addr, read_addr);
	return (QWLE(unread_data, jnlpool_size));
}

int gtmsource_readpool(uchar_ptr_t buff, int *data_len, int maxbufflen)
{

	uint4 		bufflen, read_size;
	int4		wrap_size;
	unsigned char   seq_num_str[32], *seq_num_ptr;

#ifdef REPL_DEBUG
	int	repl_dbg_source_save_read;
	seq_num	repl_dbg_save_read_jnl_seqno;
#endif

	if (jnlpool_hasnt_overflowed(jnlpool.gtmsource_local->read_addr))
	{
		/* No overflow yet */
		assert(jnlpool.gtmsource_local->read + sizeof(jnldata_hdr_struct) <= jnlpool.jnlpool_ctl->jnlpool_size);
		bufflen = ((jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jnlpool.gtmsource_local->read))->jnldata_len;
		if (0 < bufflen && jnlpool.jnlpool_ctl->jnlpool_size >= bufflen)
		{
			read_size = bufflen - sizeof(jnldata_hdr_struct);
			if (0 < read_size && maxbufflen >= read_size)
			{
				if (0 < (wrap_size = (int4)(jnlpool.gtmsource_local->read +
							    bufflen - jnlpool.jnlpool_ctl->jnlpool_size)))
					read_size -= wrap_size;
				longcpy(buff,
    		               	       jnlpool.jnldata_base +
		                       jnlpool.gtmsource_local->read +
	       	                       sizeof(jnldata_hdr_struct),
		       	               read_size);
				if (0 < wrap_size)
					longcpy(buff + read_size, jnlpool.jnldata_base, wrap_size);

				if (jnlpool_hasnt_overflowed(jnlpool.gtmsource_local->read_addr))
				{
					/* No overflow */
					assert(QWEQ(jnlpool.gtmsource_local->read_jnl_seqno,
							((jnl_record *)buff)->val.jrec_tset.jnl_seqno));

#ifdef REPL_DEBUG
					repl_dbg_source_save_read = jnlpool.gtmsource_local->read;
					QWASSIGN(repl_dbg_save_read_jnl_seqno, jnlpool.gtmsource_local->read_jnl_seqno);
#endif
					jnlpool.gtmsource_local->read = ((0 > wrap_size) ?
							jnlpool.gtmsource_local->read + bufflen : wrap_size);
					QWINCRBYDW(jnlpool.gtmsource_local->read_addr, bufflen);
					QWINCRBYDW(jnlpool.gtmsource_local->read_jnl_seqno, 1);
					*data_len = bufflen - sizeof(jnldata_hdr_struct);
					REPL_DPRINT6("Pool read seqno : "INT8_FMT" size : %d  Read %d  : Next Read : %d%s\n",
						INT8_PRINT(repl_dbg_save_read_jnl_seqno), *data_len, repl_dbg_source_save_read,
						jnlpool.gtmsource_local->read, (0 < wrap_size) ? "  READ WRAPPED" : "");
					return (0);
				} /* else overflow happened, or about
				   * to happen */
			} else if (0 < read_size && jnlpool_hasnt_overflowed(jnlpool.gtmsource_local->read_addr))
			{
				/* Buffer cannot accommodate data */
				*data_len = read_size;
				return (-1);
			} /* else
			   * I read a corrupt (overwritten) large value, or
			   * read_size == 0, both of which imply overflow.
			   * read_size == 0 => overflow because every
			   * transaction generates non-zero bytes of jnl data
			   */
		} /* else
		   * I read a corrupt (overwritten) large value, or read 0,
		   * both of which imply overflow. bufflen == 0 => overflow
		   * because every transaction generates non-zero bytes of jnl
		   * data */
	} /* else overflow happened, or about to happen */
	*data_len = -1;
	return (-1); /* Error indication */
}
