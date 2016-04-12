/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "memcoherency.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#ifdef VMS
#include <descrip.h>
#endif
#include "gtmsource.h"

boolean_t jnlpool_hasnt_overflowed(jnlpool_ctl_ptr_t jctl, uint4 jnlpool_size, qw_num read_addr)
{ /* the advantage of passing the three arguments is they are likely (on most platforms) to be scratch registers that can also
   * be used for computation. Also, besides all callers would have already loaded jctl and jnlpool_size */

	/* For systems with UNORDERED memory access (example, ALPHA, POWER4, PA-RISC 2.0), on a multi processor system, it is
	 * possible that the source server notices the change in jnlpool_ctl->write_addr before seeing the change to the content
	 * to be read (including the jnl_data_header). To avoid such conditions, we should commit the order of shared memory
	 * updates before and after the content is updated (see t_end.c, tp_tend.c). To ensure the source server reads content
	 * that is correct, it should invalidate its cache before the read. After the read, to ensure that the content is correct
	 * (not some that may have been overwritten), it has to invalidate its cache to fetch the latest value of early_write_addr.
	 *
	 */
	SHM_READ_MEMORY_BARRIER; /* to fetch the latest early_write_addr */
	return (jnlpool_size >= (jctl->early_write_addr - read_addr));
}
