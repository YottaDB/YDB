/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include <descrip.h>

#include "gtm_inet.h"

#include "stp_parms.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "cmd_qlf.h"
#include "cryptdef.h"
#include "ladef.h"
#include "iosp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "cli.h"
#include "op.h"
#include "io.h"
#include "source_file.h"
#include "lmdef.h"
#include "getjobnum.h"
#include "ast_init.h"
#include "comp_esc.h"
#include "get_page_size.h"
#include "init_secshr_addrs.h"
#include "print_exit_stats.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_threadgbl_init.h"
#include "gtmimagename.h"
#include "gtm_imagetype_init.h"

GBLREF int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF boolean_t		run_time;
GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF bool			licensed;
GBLREF int4			lkid, lid;
GBLREF spdesc			rts_stringpool, stringpool;

error_def	(LP_NOCNFDB);
error_def	(ERR_WILLEXPIRE);

OS_PAGE_SIZE_DECLARE

LITREF char	gtm_product[PROD];
LITREF int4	gtm_product_len;
LITREF char	gtm_version[VERS];
LITREF int4	gtm_version_len;

#define FILE_NAME_SIZE 255

int gtm$compile(void)
{
	unsigned short	len;
	char		source_file_string[FILE_NAME_SIZE + 1],
			obj_file[FILE_NAME_SIZE + 1],
			list_file[FILE_NAME_SIZE + 1],
			ceprep_file[FILE_NAME_SIZE + 1];
	int4		status;
	int4		inid = 0;
	int4		nid = 0;			/* node number		*/
	int4		days = 128;			/* days to expiration	*/
	int4		lic_x =	0;			/* license value	*/
	char		*h = NULL;			/* license data base	*/
	char		*pak = NULL;			/* pak record		*/
	int4		mdl = 0;			/* hardw. model type	*/
	$DESCRIPTOR	(dprd, gtm_product);
	$DESCRIPTOR	(dver, gtm_version);
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;			/* This is the first C routine in the VMS compiler so do init here */
	gtm_env_init();	/* read in all environment variables before any function call (particularly malloc) */
	gtm_imagetype_init(GTM_IMAGE);		/* While compile-only, pretending GTM_IMAGE is sufficient */
	op_open_ptr = op_open;
	licensed = TRUE;
#	ifdef	NOLICENSE
	status = SS$_NORMAL;
	lid = 1;
	lic_x = 32767;
#	else
	if (NULL == (h = la_getdb(LMDB)))		/* license db in mem	*/
		status = LP_NOCNFDB;
	else
		status = SS$_NORMAL;
	if (1 == (status & 1))				/* licensing: node + system  */
		status = lm_mdl_nid(&mdl, &nid, &inid);
	if (1 == (status & 1))				/* licensing: license */
	{
		dprd.dsc$w_length = gtm_product_len;
		dver.dsc$w_length = gtm_version_len;
		status = lp_licensed(h, &dprd, &dver, mdl, nid, &lid, &lic_x, &days, pak);
	}
#	endif
	get_page_size();
	getjobnum();
	INVOKE_INIT_SECSHR_ADDRS;
	if (1 == (status & 1))				/* licensing: license units  */
		status = LP_ACQUIRE(pak, lic_x, lid, &lkid);
	ast_init();
	io_init(TRUE);
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	run_time = FALSE;
	TREF(compile_time) = TRUE;
#	ifdef	NOLICENSE
	status = SS$_NORMAL;
#	else
	if (LP_NOCNFDB != status)
		la_freedb(h);
	if (1 == (status & 1))					/* licensing */
	{
		if (days < 14)
			lm_putmsgu(ERR_WILLEXPIRE, 0, 0);
	} else
	{
		licensed = FALSE;
		rts_error(VARLSTCNT(1) status);
	}
#	endif
	glb_cmd_qlf.object_file.str.addr = obj_file;
	glb_cmd_qlf.object_file.str.len = FILE_NAME_SIZE;
	glb_cmd_qlf.list_file.str.addr = list_file;
	glb_cmd_qlf.list_file.str.len = FILE_NAME_SIZE;
	glb_cmd_qlf.ceprep_file.str.addr = ceprep_file;
	glb_cmd_qlf.ceprep_file.str.len = FILE_NAME_SIZE;
	get_cmd_qlf(&glb_cmd_qlf);
	ce_init();	/* initialize compiler escape processing */
	TREF(dollar_zcstatus) = SS$_NORMAL;
	len = FILE_NAME_SIZE;
	for (status = cli_get_str("INFILE", source_file_string, &len);
		status;
		status = cli_get_str("INFILE", source_file_string, &len))
	{
		compile_source_file(len, source_file_string, TRUE);
		len = FILE_NAME_SIZE;
	}
	print_exit_stats();
	io_rundown(NORMAL_RUNDOWN);
	return TREF(dollar_zcstatus);
}
