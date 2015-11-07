/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rmsdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>

#include "gtm_inet.h"

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "error.h"		/* for EXIT_HANDLER macro used in SET_EXIT_HANDLER macro */
#include "cli.h"
#include "jnl.h"
#include "stp_parms.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "stringpool.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "desblk.h"		/* for desblk structure */
#include "util.h"
#include "lke.h"
#include "getjobname.h"
#include "getjobnum.h"
#include "generic_exit_handler.h"
#include "ladef.h"
#include "ast_init.h"
#include "get_page_size.h"
#include "init_secshr_addrs.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "patcode.h"
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"

GBLREF desblk		exi_blk;
GBLREF int4 		lkid;
GBLREF int4		exi_condition;
GBLREF spdesc		rts_stringpool, stringpool;

OS_PAGE_SIZE_DECLARE

extern int		lke_cmd();
extern int		CLI$DCL_PARSE();
extern int		CLI$DISPATCH();

$DESCRIPTOR		(output_qualifier, "OUTPUT");

static void lke_process(void);

void lke(void)
{
	char		buff[MAX_LINE];
	$DESCRIPTOR	(command, buff);
	uint4		status;
	short		len;
	bool		dcl;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	gtm_imagetype_init(LKE_IMAGE);
	gtm_env_init();	/* read in all environment variables */
	util_out_open(0);
	SET_EXIT_HANDLER(exi_blk, generic_exit_handler, exi_condition);	/* Establish exit handler */
	ESTABLISH(util_base_ch);
	status =lp_id(&lkid);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	get_page_size();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	getjobname();
	INVOKE_INIT_SECSHR_ADDRS;
	ast_init();
	initialize_pattern_table();
	gvinit();
	region_init(TRUE);
	getjobnum();
	status = lib$get_foreign(&command, 0, &len, 0);
	if ((status & 1) && len > 0)
	{
		command.dsc$w_length = len;
		status = CLI$DCL_PARSE(&command, &lke_cmd, &lib$get_input, 0, 0);
		if (CLI$_NORMAL == status)
		{
			util_out_open(&output_qualifier);
			CLI$DISPATCH();
			util_out_close();
		}
		lke_exit();
	}
	for (;;)
		lke_process();
}

static void lke_process(void)
{
	uint4		status;
	$DESCRIPTOR	(prompt, "LKE> ");

	ESTABLISH(util_ch);
	status = CLI$DCL_PARSE(0, &lke_cmd, &lib$get_input, &lib$get_input, &prompt);
	if (RMS$_EOF == status)
		lke_exit();
	else if (CLI$_NORMAL == status)
	{
		util_out_open(&output_qualifier);
		CLI$DISPATCH();
		util_out_close();
	}
}
