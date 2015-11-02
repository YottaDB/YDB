/****************************************************************
 *								*
 *	Copyright 2001,2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_inet.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "gdscc.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "mupipbckup.h"
#include "mu_rndwn_file.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_all.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "dpgbldir.h"
#include "gtmio.h"
#include "dpgbldir_sysops.h"
#include "mu_gv_cur_reg_init.h"
#include "mupip_rundown.h"
#include "gtmmsg.h"
#include "repl_instance.h"
#include "mu_rndwn_repl_instance.h"

GBLDEF	bool		in_backup;
GBLREF	bool		error_mupip;
GBLREF	tp_region	*grlist;
GBLREF	gd_region	*gv_cur_region;
GBLREF	boolean_t	mu_star_specified;

#define	TMP_BUF_LEN	50

void mupip_rundown(void)
{
	int			exit_status;
	boolean_t		region, file;
	tp_region		*rptr, single;
	replpool_identifier	replpool_id;
	unsigned int		full_len;

	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUNOTALLSEC);
	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_TEXT);
	error_def(ERR_MUQUALINCOMP);
	error_def(ERR_MUFILRNDWNSUC);
	error_def(ERR_MUJPOOLRNDWNSUC);
	error_def(ERR_MURPOOLRNDWNSUC);
	error_def(ERR_MUJPOOLRNDWNFL);
	error_def(ERR_MURPOOLRNDWNFL);

	exit_status = SS_NORMAL;
	file = (cli_present("FILE") == CLI_PRESENT);
	region = (cli_present("REGION") == CLI_PRESENT);

	if (file == region && TRUE == file)
		mupip_exit(ERR_MUQUALINCOMP);

	if (region)
	{
		gvinit();
		mu_getlst("WHAT", SIZEOF(tp_region));
		rptr = grlist;
		if (error_mupip)
			exit_status = ERR_MUNOTALLSEC;
	} else if (file)
	{
		mu_gv_cur_reg_init();
		gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
		if (!cli_get_str("WHAT",  (char *)&gv_cur_region->dyn.addr->fname[0], &gv_cur_region->dyn.addr->fname_len))
			mupip_exit(ERR_MUNODBNAME);
		*(gv_cur_region->dyn.addr->fname + gv_cur_region->dyn.addr->fname_len) = 0;
		rptr = &single;		/* a dummy value that permits one trip through the loop */
		rptr->fPtr = NULL;
	}
	in_backup = FALSE;		/* Only want yes/no from mupfndfil, not an address */
	if (region || file)
	{
		for ( ; NULL != rptr; rptr = rptr->fPtr)
		{
			if (region)
			{
				if (!mupfndfil(rptr->reg, NULL))
				{
					exit_status = ERR_MUNOTALLSEC;
					continue;
				}
				gv_cur_region = rptr->reg;
				if (NULL == gv_cur_region->dyn.addr->file_cntl)
				{
					gv_cur_region->dyn.addr->acc_meth = dba_bg;
					gv_cur_region->dyn.addr->file_cntl =
						(file_control *)malloc(SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					memset(gv_cur_region->dyn.addr->file_cntl, 0, SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
					gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(SIZEOF(GDS_INFO));
					memset(gv_cur_region->dyn.addr->file_cntl->file_info, 0, SIZEOF(GDS_INFO));
				}
			}

			if (TRUE == mu_rndwn_file(gv_cur_region, FALSE))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_MUFILRNDWNSUC, 2, gv_cur_region->dyn.addr->fname_len,
						gv_cur_region->dyn.addr->fname);
			}
			else
				exit_status = ERR_MUNOTALLSEC;
		}
		if (region && mu_star_specified)		/* rundown repl pools belonging to this global directory */
		{
			if (repl_inst_get_name((char *)replpool_id.instfilename, &full_len, SIZEOF(replpool_id.instfilename),
					return_on_error))
			{
				if (!mu_rndwn_repl_instance(&replpool_id, TRUE, TRUE))
					exit_status = ERR_MUNOTALLSEC;
			}
		}
	} else
	{
		exit_status = mu_rndwn_all();
		if (SS_NORMAL == exit_status)
			exit_status = mu_rndwn_sem_all();
		else
			mu_rndwn_sem_all();
	}
	mupip_exit(exit_status);
}
