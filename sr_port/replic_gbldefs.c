/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /* General repository for mupip journal command related global variable definitions.
  * This keeps us from pulling in modules and all their references
  * when all we wanted was the global data def.. */

#include "mdef.h"

#include "gtm_inet.h"
#if defined(VMS)
#include <descrip.h> /* Required for gtmsource.h */
#endif
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "buddy_list.h"
#include "muprec.h"
#include "repl_filter.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "read_db_files_from_gld.h"

GBLDEF	unsigned char	*gtmsource_tcombuff_start = NULL;
GBLDEF	unsigned char	*repl_filter_buff = NULL;
GBLDEF	int		repl_filter_bufsiz = 0;
GBLDEF	unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLDEF	unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLDEF	char		*ext_stop;
GBLDEF	char		*jb_stop;
GBLDEF	seq_num		lastlog_seqno;
GBLDEF	qw_num		trans_sent_cnt, last_log_tr_sent_cnt, trans_recvd_cnt, last_log_tr_recvd_cnt;
GBLDEF	upd_helper_entry_ptr_t	helper_entry;

#ifdef VMS
GBLDEF	unsigned char	jnl_ver, remote_jnl_ver;
GBLDEF	boolean_t	primary_side_std_null_coll;
GBLDEF	boolean_t	primary_side_trigger_support;
GBLDEF	boolean_t	secondary_side_std_null_coll;
GBLDEF	boolean_t	secondary_side_trigger_support;
#endif
