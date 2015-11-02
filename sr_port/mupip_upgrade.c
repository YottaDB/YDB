/****************************************************************
 *								*
 *	Copyright 2005, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_upgrade.c: Driver program to upgrade v4.x database files to v5.0-000 */

#include "mdef.h"

#ifdef UNIX
#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_stdlib.h"
#else
#include <descrip.h>
#include <fab.h>
#include <ssdef.h>
#include <rms.h>
#include <iodef.h>
#include <efndef.h>
#endif
#include "gtm_string.h"

#include "gtmio.h"
#include "iosp.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "gdsblk.h"
#include "jnl.h"	/* For fd_type */
#include "error.h"
#include "util.h"
#include "gtmmsg.h"
#include "cli.h"
#include "repl_sp.h"
#include "mupip_exit.h"
#include "mupip_upgrade.h"
#include "mu_upgrd_dngrd_hdr.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "mu_outofband_setup.h"
#ifdef UNIX
#include "mu_all_version_standalone.h"
#endif

LITREF  char            	gtm_release_name[];
LITREF  int4           		gtm_release_name_len;

UNIX_ONLY(static sem_info	*sem_inf;)

UNIX_ONLY(static void mupip_upgrade_cleanup(void);)

void mupip_upgrade(void)
{
	char		db_fn[MAX_FN_LEN + 1];
	unsigned short	db_fn_len; 	/* cli_get_str expects short */
	fd_type		channel;
	int		save_errno, v15_csd_size, max_max_rec_size;
	int		fstat_res;
	int4		status, rc;
	uint4		status2;
	off_t 		v15_file_size;
	v15_sgmnt_data	v15_csd;
	sgmnt_data	csd;
#ifdef UNIX
 	struct stat    	stat_buf;
#elif defined(VMS)
	struct FAB	mupfab;
	struct XABFHC	xabfhc;
#endif
	unsigned char	new_master_map[MASTER_MAP_SIZE_V4];

	error_def(ERR_BADDBVER);
	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBMAXREC2BIG);
	error_def(ERR_DBMINRESBYTES);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_DBOPNERR);
	error_def(ERR_DBPREMATEOF);
	error_def(ERR_DBRDONLY);
	error_def(ERR_PREMATEOF);
	error_def(ERR_MUINFOUINT4);
	error_def(ERR_MUINFOUINT8);
	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUNOUPGRD);
	error_def(ERR_MUPGRDSUCC);
	error_def(ERR_MUSTANDALONE);
	error_def(ERR_MUUPGRDNRDY);
	error_def(ERR_SYSCALL);
	error_def(ERR_TEXT);

	/* Structure checks .. */
#ifndef __ia64
	assert((24 * 1024) == sizeof(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */
#endif

	UNIX_ONLY(sem_inf = (sem_info *)malloc(sizeof(sem_info) * 2);
		  memset(sem_inf, 0, sizeof(sem_info) * 2);
		  atexit(mupip_upgrade_cleanup));
	db_fn_len = sizeof(db_fn);
	if (!cli_get_str("FILE", db_fn, &db_fn_len))
		rts_error(VARLSTCNT(1) ERR_MUNODBNAME);
	db_fn[db_fn_len] = '\0';	/* Null terminate */
	if (!mu_upgrd_dngrd_confirmed())
	{
		gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Upgrade canceled by user"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Mupip upgrade started"));
	UNIX_ONLY(mu_all_version_get_standalone(db_fn, sem_inf));
	mu_outofband_setup();	/* Will ignore user interrupts. Note that now the
				 * elapsed time for this is order of milliseconds */
	/* ??? Should we set this just before DO_FILE_WRITE to have smallest time window of ignoring signal? */
#ifdef VMS
	mupfab = cc$rms_fab;
	mupfab.fab$l_fna = db_fn;
	mupfab.fab$b_fns = db_fn_len;
	mupfab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_UPD ;
	mupfab.fab$l_fop = FAB$M_UFO;
	xabfhc = cc$rms_xabfhc;
	mupfab.fab$l_xab = &xabfhc;
	status = sys$open(&mupfab);
	if (0 == (status & 1))
	{
		if (RMS$_FLK == status)
			gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR), 2, db_fn_len, db_fn);
		else
			gtm_putmsg(VARLSTCNT(6) ERR_DBOPNERR, 2, db_fn_len, db_fn, status, mupfab.fab$l_stv);
		mupip_exit(ERR_MUNOUPGRD);
	}
	channel = mupfab.fab$l_stv;
	v15_file_size =  xabfhc.xab$l_ebk;
#else
	if (-1 == (channel = OPEN(db_fn, O_RDWR)))
	{
		save_errno = errno;
		if (-1 != (channel = OPEN(db_fn, O_RDONLY)))
			gtm_putmsg(VARLSTCNT(10) ERR_DBRDONLY, 2, db_fn_len, db_fn, errno, 0,
				   MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Cannot upgrade read-only database"));
		else
			gtm_putmsg(VARLSTCNT(5) ERR_DBOPNERR, 2, db_fn_len, db_fn, save_errno);
		mupip_exit(ERR_MUNOUPGRD);
	}
	/* get file status */
	FSTAT_FILE(channel, &stat_buf, fstat_res);
	if (-1 == fstat_res)
	{
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"), CALLFROM, errno);
		gtm_putmsg(VARLSTCNT(4) ERR_DBOPNERR, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNOUPGRD);
	}
	v15_file_size = stat_buf.st_size;
#endif
	v15_csd_size = sizeof(v15_sgmnt_data);
	DO_FILE_READ(channel, 0, &v15_csd, v15_csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (!memcmp(v15_csd.label, GDS_LABEL, STR_LIT_LEN(GDS_LABEL)))
	{ 	/* It is already V5.0-000 */
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database already upgraded"));
		mupip_exit(ERR_MUNOUPGRD);
	} else if (memcmp(v15_csd.label, V15_GDS_LABEL, STR_LIT_LEN(V15_GDS_LABEL)))
	{ 	/* It is not a version we can upgrade, that is, not V4.0-000 to V5.0-FT01 */
		F_CLOSE(channel, rc);
		if (memcmp(v15_csd.label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg(VARLSTCNT(4) ERR_DBNOTGDS, 2, db_fn_len, db_fn);
		else
			gtm_putmsg(VARLSTCNT(4) ERR_BADDBVER, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNOUPGRD);
	}
	/* It is V4.x or V5.0-FT01 version : Se proceed with upgrade */
	if (v15_csd.createinprogress)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database creation in progress"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.freeze)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database is frozen"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.wc_blocked)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
			   2, LEN_AND_LIT("Database modifications are disallowed because wc_blocked is set"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.file_corrupt)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database corrupt"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.intrpt_recov_tp_resolve_time || v15_csd.intrpt_recov_resync_seqno || v15_csd.recov_interrupted
	    || v15_csd.intrpt_recov_jnl_state || v15_csd.intrpt_recov_repl_state)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Recovery was interrupted"));
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (GDSVCURR != v15_csd.certified_for_upgrade_to)
	{
		F_CLOSE(channel, rc);
		gtm_putmsg(VARLSTCNT(6) ERR_MUUPGRDNRDY, 4, db_fn_len, db_fn, gtm_release_name_len, gtm_release_name);
		mupip_exit(ERR_MUNOUPGRD);
	}
	max_max_rec_size = v15_csd.blk_size - SIZEOF(blk_hdr);
	if (VMS_ONLY(9) UNIX_ONLY(8) > v15_csd.reserved_bytes)
	{
		gtm_putmsg(VARLSTCNT(6) ERR_DBMINRESBYTES, 4, VMS_ONLY(9) UNIX_ONLY(8), v15_csd.reserved_bytes);
		mupip_exit(ERR_MUNOUPGRD);
	}
	if (v15_csd.max_rec_size > max_max_rec_size)
	{
		rts_error(VARLSTCNT(5) ERR_DBMAXREC2BIG, 3, v15_csd.max_rec_size, v15_csd.blk_size, max_max_rec_size);
		mupip_exit(ERR_MUNOUPGRD);
	}
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file header size"), v15_csd_size, v15_csd_size);
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Old file length"), &v15_file_size, &v15_file_size);
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file start_vbn"), v15_csd.start_vbn, v15_csd.start_vbn);
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file gds blk_size"), v15_csd.blk_size, v15_csd.blk_size);
	gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file total_blks"),
		   v15_csd.trans_hist.total_blks, v15_csd.trans_hist.total_blks);
	assert(ROUND_DOWN2(v15_csd.blk_size, DISK_BLOCK_SIZE) == v15_csd.blk_size);
	assert(((off_t)v15_csd.start_vbn) * DISK_BLOCK_SIZE +
			(off_t)v15_csd.trans_hist.total_blks * v15_csd.blk_size == v15_file_size VMS_ONLY(* DISK_BLOCK_SIZE));
	/* Now call mu_upgrd_header() to do file header upgrade in memory */
        mu_upgrd_header(&v15_csd, &csd);
	csd.master_map_len = MASTER_MAP_SIZE_V4;	/* V5 master map is not part of file header */
	memcpy(new_master_map, v15_csd.master_map, MASTER_MAP_SIZE_V4);
	DO_FILE_WRITE(channel, 0, &csd, sizeof(csd), status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	DO_FILE_WRITE(channel, sizeof(csd), new_master_map, MASTER_MAP_SIZE_V4, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNOUPGRD);
	}
	F_CLOSE(channel, rc);
	UNIX_ONLY(mu_all_version_release_standalone(sem_inf));
	gtm_putmsg(VARLSTCNT(8) ERR_MUPGRDSUCC, 6, db_fn_len, db_fn, RTS_ERROR_LITERAL("upgraded"),
		   gtm_release_name_len, gtm_release_name);
	mupip_exit(SS_NORMAL);
}

#ifdef UNIX
static void mupip_upgrade_cleanup(void)
{
	if (sem_inf)
		mu_all_version_release_standalone(sem_inf);
}
#endif
