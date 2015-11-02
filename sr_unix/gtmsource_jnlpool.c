/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
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
#include "gtm_inet.h"
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
#include "repl_sem.h"
#include "util.h"
#include "repl_inst_dump.h"
#include "cli.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		detail_specified;		/* set to TRUE if -DETAIL is specified */
GBLREF	uint4			section_offset;		/* Used by PRINT_OFFSET_PREFIX macro in repl_inst_dump.c */

error_def(ERR_MUPCLIERR);

int gtmsource_jnlpool(void)
{
	uint4			offset, size;
	gtm_uint64_t		value;
	boolean_t		value_present;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	assert(NULL == jnlpool.gtmsource_local);
	if (CLI_PRESENT == cli_present("NAME"))
	{
		util_out_print("Error: NAME cannot be used with JNLPOOL", TRUE);
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
	}
	if (CLI_PRESENT == cli_present("SHOW"))
	{
		detail_specified = (CLI_PRESENT == cli_present("DETAIL"));
		section_offset = 0;
		repl_inst_dump_jnlpoolctl(jnlpool.jnlpool_ctl);
		section_offset = (uint4)((sm_uc_ptr_t)jnlpool.repl_inst_filehdr - (sm_uc_ptr_t)jnlpool.jnlpool_ctl);
		repl_inst_dump_filehdr(jnlpool.repl_inst_filehdr);
		section_offset = (uint4)((sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array - (sm_uc_ptr_t)jnlpool.jnlpool_ctl);
		repl_inst_dump_gtmsrclcl(jnlpool.gtmsrc_lcl_array);
		section_offset = (uint4)((sm_uc_ptr_t)jnlpool.gtmsource_local_array - (sm_uc_ptr_t)jnlpool.jnlpool_ctl);
		repl_inst_dump_gtmsourcelocal(jnlpool.gtmsource_local_array);
	}
	if (CLI_PRESENT == cli_present("CHANGE"))
	{
		mupcli_get_offset_size_value(&offset, &size, &value, &value_present);
		if (size > jnlpool.jnlpool_ctl->jnlpool_size)
		{
			util_out_print("Error: SIZE specified [0x!XL] is greater than size of journal pool [0x!XL]", TRUE,
				size, jnlpool.jnlpool_ctl->jnlpool_size);
			rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
		}
		mupcli_edit_offset_size_value(&((sm_uc_ptr_t)jnlpool.jnlpool_ctl)[offset], offset, size, value, value_present);
	}
	return (NORMAL_SHUTDOWN);
}
