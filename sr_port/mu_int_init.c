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

#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#define MSGBUF_SIZE 256

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data		mu_int_data;
GBLREF	unsigned char		*mu_int_master;
GBLREF	int			mu_int_skipreg_cnt;
#ifdef GTM_CRYPT
GBLREF	gtmcrypt_key_t		mu_int_encrypt_key_handle;
#endif

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
#	ifdef GTM_CRYPT
	int			gtmcrypt_errno;
	gd_segment		*seg;
#	endif

	mu_gv_cur_reg_init();
	/* get filename */
	gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
	if (!cli_get_str("WHAT", (char *)gv_cur_region->dyn.addr->fname, &gv_cur_region->dyn.addr->fname_len))
		mupip_exit(ERR_MUNODBNAME);
	if (!STANDALONE(gv_cur_region))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
		mu_int_skipreg_cnt++;
		return (FALSE);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = dba_bg;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
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
	fc->op_buff = (uchar_ptr_t)&mu_int_data;
	fc->op_len = SGMNT_HDR_LEN;
	fc->op_pos = 1;
	dbfilop(fc);
	if (MASTER_MAP_SIZE_MAX < MASTER_MAP_SIZE(&mu_int_data) ||
	    native_size < DIVIDE_ROUND_UP(SGMNT_HDR_LEN + MASTER_MAP_SIZE(&mu_int_data), DISK_BLOCK_SIZE) + MIN_DB_BLOCKS)
	{
		mu_int_err(ERR_DBFSTHEAD, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
#	ifdef GTM_CRYPT
	if (mu_int_data.is_encrypted)
	{ 	/* Initialize encryption and the key information for the current segment to be used in mu_int_read. */
		ASSERT_ENCRYPTION_INITIALIZED;	/* should have been done in mu_rndwn_file called from STANDALONE macro */
		GTMCRYPT_GETKEY(NULL, mu_int_data.encryption_hash, mu_int_encrypt_key_handle, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			mu_int_skipreg_cnt++;
			return FALSE;
		}
	}
#	endif
	mu_int_master = malloc(mu_int_data.master_map_len);
	fc->op = FC_READ;
	fc->op_buff = mu_int_master;
	fc->op_len = MASTER_MAP_SIZE(&mu_int_data);
	fc->op_pos = DIVIDE_ROUND_UP(SGMNT_HDR_LEN + 1, DISK_BLOCK_SIZE);
	dbfilop(fc);
	return (mu_int_fhead());
}
