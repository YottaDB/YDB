/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "mu_file_size.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "wbox_test_init.h"

#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#define MSGBUF_SIZE 256

GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_data		mu_int_data;
GBLREF unsigned char		*mu_int_master;
GTMCRYPT_ONLY(
GBLREF gtmcrypt_key_t		mu_int_encrypt_key_handle;
)
boolean_t mu_int_init(void)
{
	unsigned int	native_size, size, status;
	file_control	*fc;
	boolean_t	standalone;
	char		msgbuff[MSGBUF_SIZE], *msgptr;
	GTMCRYPT_ONLY(
		int	crypt_status;
	)
	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUSTANDALONE);
	error_def(ERR_DBFSTHEAD);

	mu_gv_cur_reg_init();
	/* get filename */
	gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
	if (!cli_get_str("WHAT", (char *)gv_cur_region->dyn.addr->fname, &gv_cur_region->dyn.addr->fname_len))
		mupip_exit(ERR_MUNODBNAME);
	if (!STANDALONE(gv_cur_region))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
		return (FALSE);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = dba_bg;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		return FALSE;
	}
	native_size = mu_file_size(fc);
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
	/* Initialize encryption and the key information for the current segment to be used in mu_int_read.
	 * Note that this is done here and will be called only if mupip integ is called with -file option.
	 * In other case where mupip integ is called with -reg option, the following initialization will be done
	 * in db_init. */
	if (mu_int_data.is_encrypted)
	{
		INIT_PROC_ENCRYPTION(crypt_status);
		if (0 == crypt_status)
			GTMCRYPT_GETKEY(mu_int_data.encryption_hash, mu_int_encrypt_key_handle, crypt_status);
		if (0 != crypt_status)
		{
			GC_GTM_PUTMSG(crypt_status, (gv_cur_region->dyn.addr->fname));
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
