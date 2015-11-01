/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
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

#if defined(UNIX)
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#elif defined(VMS)
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
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "repl_filter.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "read_db_files_from_gld.h"

GBLDEF	unsigned char	*gtmsource_tcombuff_start = NULL;
GBLDEF	unsigned char	*repl_filter_buff = NULL;
GBLDEF	int		repl_filter_bufsiz = 0;
GBLDEF	unsigned char	jnl_ver, remote_jnl_ver;
GBLDEF	unsigned int	jnl_source_datalen, jnl_dest_maxdatalen;
GBLDEF	unsigned char	jnl_source_rectype, jnl_dest_maxrectype;
GBLDEF	char		*ext_stop;
GBLDEF	char		*jb_stop;
GBLDEF  gld_dbname_list	*upd_db_files;
