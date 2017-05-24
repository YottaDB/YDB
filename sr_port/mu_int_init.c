/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

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
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "wbox_test_init.h"
#include "gtmcrypt.h"

#define MSGBUF_SIZE 256

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data		mu_int_data;
GBLREF	unsigned char		*mu_int_master;
GBLREF	int			mu_int_skipreg_cnt;
GBLREF	enc_handles		mu_int_encr_handles;

error_def(ERR_DBFSTHEAD);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUSTANDALONE);

boolean_t mu_int_init(void)
{
	unsigned int		status;
	gtm_uint64_t		native_size;
	file_control		*fc;
	boolean_t		standalone;
	char			msgbuff[MSGBUF_SIZE], *msgptr;
	int			gtmcrypt_errno, read_len;
	gd_segment		*seg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	tsd;
	unix_db_info		*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_gv_cur_reg_init();	/* Creates a dummy segment with seg->asyncio FALSE so no DIO alignment issues to worry about
				 * in the FC_OPEN/FC_READ below
				 */
	/* get filename */
	gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
	if (!cli_get_str("WHAT", (char *)gv_cur_region->dyn.addr->fname, &gv_cur_region->dyn.addr->fname_len))
		mupip_exit(ERR_MUNODBNAME);
	if (!STANDALONE(gv_cur_region))
	{
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
		mu_int_skipreg_cnt++;
		return (FALSE);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS_NORMAL != status)
	{
		csa = &FILE_INFO(gv_cur_region)->s_addrs;
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(1) status);
		mu_int_skipreg_cnt++;
		return FALSE;
	}
	native_size = gds_file_size(fc);
	if (native_size < DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_MIN, DISK_BLOCK_SIZE) + MIN_DB_BLOCKS)
	{
		mu_int_err(ERR_DBFSTHEAD, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	assert(SGMNT_HDR_LEN == SIZEOF(sgmnt_data));
	fc->op = FC_READ;
	/* Do aligned reads if opened with O_DIRECT */
	udi = FC2UDI(fc);
	tsd = udi->fd_opened_with_o_direct ? (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned : &mu_int_data;
	fc->op_buff = (uchar_ptr_t)tsd;
	fc->op_len = SGMNT_HDR_LEN;
	fc->op_pos = 1;
	dbfilop(fc);
	/* Ensure "mu_int_data" is populated even if we did not directly read into it for the O_DIRECT case */
	if (udi->fd_opened_with_o_direct)
		memcpy(&mu_int_data, (TREF(dio_buff)).aligned, SGMNT_HDR_LEN);
	if (MASTER_MAP_SIZE_MAX < MASTER_MAP_SIZE(&mu_int_data) ||
	    native_size < DIVIDE_ROUND_UP(SGMNT_HDR_LEN + MASTER_MAP_SIZE(&mu_int_data), DISK_BLOCK_SIZE) + MIN_DB_BLOCKS)
	{
		mu_int_err(ERR_DBFSTHEAD, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (USES_ANY_KEY(&mu_int_data))
	{ 	/* Initialize encryption and the key information for the current segment to be used in mu_int_read. */
		ASSERT_ENCRYPTION_INITIALIZED;	/* should have been done in mu_rndwn_file called from STANDALONE macro */
		seg = gv_cur_region->dyn.addr;
		INIT_DB_OR_JNL_ENCRYPTION(&mu_int_encr_handles, &mu_int_data, seg->fname_len, (char *)seg->fname, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			mu_int_skipreg_cnt++;
			return FALSE;
		}
	}
	mu_int_master = malloc(mu_int_data.master_map_len);
	fc->op = FC_READ;
	read_len = MASTER_MAP_SIZE(&mu_int_data);
	/* Do aligned reads if opened with O_DIRECT */
	if (udi->fd_opened_with_o_direct)
	{
		read_len = ROUND_UP2(read_len, DIO_ALIGNSIZE(udi));
		DIO_BUFF_EXPAND_IF_NEEDED(udi, read_len, &(TREF(dio_buff)));
		fc->op_buff = (sm_uc_ptr_t)(TREF(dio_buff)).aligned;
	} else
		fc->op_buff = mu_int_master;
	fc->op_len = read_len;
	fc->op_pos = DIVIDE_ROUND_UP(SGMNT_HDR_LEN + 1, DISK_BLOCK_SIZE);
	dbfilop(fc);
	/* Ensure "mu_int_master" is populated even if we did not directly read into it for the O_DIRECT case */
	if (udi->fd_opened_with_o_direct)
		memcpy(mu_int_master, (TREF(dio_buff)).aligned, mu_int_data.master_map_len);
	return (mu_int_fhead());
}
