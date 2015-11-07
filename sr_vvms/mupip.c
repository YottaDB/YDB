/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#include "cryptdef.h"
#include "ladef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "error.h"		/* for EXIT_HANDLER macro used in SET_EXIT_HANDLER macro */
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "min_max.h"		/* needed for init_root_gv.h */
#include "init_root_gv.h"
#include "desblk.h"		/* for desblk structure */
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "util.h"
#include "mupip_exit.h"
#include "lmdef.h"
#include "getjobnum.h"
#include "patcode.h"
#include "generic_exit_handler.h"
#include "ast_init.h"
#include "get_page_size.h"
#include "init_secshr_addrs.h"
#include "mupip_getcmd.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_imagetype_init.h"
#include "gtm_threadgbl_init.h"

GBLREF desblk		exi_blk;
GBLREF bool		licensed;
GBLREF int4		lkid, lid;
GBLREF bool		in_backup;
GBLREF mval		curr_gbl_root;
GBLREF int4		exi_condition;
GBLREF spdesc		rts_stringpool, stringpool;

error_def	(ERR_WILLEXPIRE);
error_def	(LP_NOCNFDB);
error_def	(LP_INVCSM);

OS_PAGE_SIZE_DECLARE

LITREF char		gtm_product[PROD];
LITREF int4		gtm_product_len;
LITREF char		gtm_version[VERS];
LITREF int4		gtm_version_len;

mupip()
{
	unsigned int	status;
	int4		inid = 0;
	int4		nid = 0;		/* system ID, node number */
	int4		days = 128;		/* days to expiration	*/
	int4		lic_x = 0;		/* license value	*/
	char		*h = NULL;		/* license data base	*/
	char		*pak = NULL;		/* pak record		*/
	int4		mdl = 0;		/* hardw. model type	*/
	$DESCRIPTOR(dprd, gtm_product);
	$DESCRIPTOR(dver, gtm_version);
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	gtm_imagetype_init(MUPIP_IMAGE);
	gtm_env_init();	/* read in all environment variables */
	licensed = TRUE;
	TREF(transform) = TRUE;
	in_backup = FALSE;
	util_out_open(0);
	SET_EXIT_HANDLER(exi_blk, generic_exit_handler, exi_condition);	/* Establish exit handler */
	ESTABLISH(util_base_ch);
	get_page_size();
	getjobnum();
	INVOKE_INIT_SECSHR_ADDRS;
#	ifdef	NOLICENSE
	status = SS$_NORMAL;
	lid = 1;
	lic_x = 32767;
#	else
	if (NULL == (h = la_getdb(LMDB)))		/* license db in mem */
		status = LP_NOCNFDB;
	else
		status = SS$_NORMAL;
	if (1 == (status & 1))				/* licensing: node+ system  */
		status = lm_mdl_nid(&mdl, &nid, &inid);
	if (1 == (status & 1))				/* licensing: license */
	{
		dprd.dsc$w_length = gtm_product_len;
		dver.dsc$w_length = gtm_version_len;
		status = lp_licensed(h, &dprd, &dver, mdl, nid, &lid, &lic_x, &days, pak);
	}
#	endif
	if (1 == (status & 1))					/* licensing: license units  */
		status = LP_ACQUIRE(pak, lic_x, lid, &lkid);	/* def in cryptdef */
#	ifdef	NOLICENSE
	status = SS$_NORMAL;
#	else
	if (LP_NOCNFDB != status)
		la_freedb(h);
	if (1 == (status & 1))					/* licensing */
	{
		if (days < 14)
			lm_putmsgu(ERR_WILLEXPIRE, 0, 0);
	}
	else
	{
		licensed = FALSE;
		if (LP_INVCSM != status)
			rts_error(VARLSTCNT(1) status);
	}
#	endif
	ast_init();
	initialize_pattern_table();
	INIT_GBL_ROOT();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	mupip_getcmd();
	mupip_exit(SS$_NORMAL);
}
