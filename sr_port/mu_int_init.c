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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "iosp.h"
#include "mlkdef.h"
#include "cli.h"
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#include "mupint.h"
#include "mu_file_size.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"

#define MSGBUF_SIZE 256

#ifdef VMS
#define STANDALONE(x) mu_rndwn_file(TRUE)
#elif defined(UNIX)
#define STANDALONE(x) mu_rndwn_status = mu_rndwn_file(x, TRUE)
#else
#error unsupported platform
#endif

GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_data		mu_int_data;
GBLREF boolean_t		mu_rndwn_status;

boolean_t mu_int_init(void)
{
	unsigned int	native_size, size, status;
	sgmnt_addrs	*mu_int_addrs;
	uchar_ptr_t	p1;
	file_control	*fc;
	boolean_t	standalone;
	char		msgbuff[MSGBUF_SIZE], *msgptr;

	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUSTANDALONE);
	error_def(ERR_DBFSTHEAD);

	mu_gv_cur_reg_init();
	/* get filename */
	gv_cur_region->dyn.addr->fname_len = sizeof(gv_cur_region->dyn.addr->fname);
	if (!cli_get_str("WHAT", (char *)gv_cur_region->dyn.addr->fname, &gv_cur_region->dyn.addr->fname_len))
		mupip_exit(ERR_MUNODBNAME);
#ifdef VMS /* On VMS mu_rndwn_file() expects the file tobe opened already */
	/* open file */
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = dba_bg;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		return FALSE;
	}
#endif
	STANDALONE(gv_cur_region);
	if (!(standalone = mu_rndwn_status))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2,
			DB_LEN_STR(gv_cur_region));
		return (FALSE);
	}
#ifdef UNIX
	/* open file */
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = dba_bg;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		return FALSE;
	}
#endif
	native_size = mu_file_size(fc);
	if (native_size < DIVIDE_ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE) + MIN_DB_BLOCKS)
	{
		mu_int_err(ERR_DBFSTHEAD, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	p1 = (unsigned char *)malloc(ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));
	fc->op = FC_READ;
	fc->op_buff = p1;
	fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
	fc->op_pos = 1;
	dbfilop(fc);
	memcpy(&mu_int_data, p1, sizeof(sgmnt_data));
	return (mu_int_fhead());
}
