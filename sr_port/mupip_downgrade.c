/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_downgrade.c: Driver program to downgrade database files - Unsupported since V7 */

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gtmio.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v6_gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"	/* For fd_type */
#include "error.h"
#include "util.h"
#include "gtmmsg.h"
#include "cli.h"
#include "repl_sp.h"
#include "mupip_exit.h"
#include "mupip_downgrade.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "mu_outofband_setup.h"
#include "anticipatory_freeze.h"
#include "mu_all_version_standalone.h"

#define	GTM_VER_LIT		"GT.M "
#define	MAX_VERSION_LEN		16	/* 16 bytes enough to hold V63000A, longest -VERSION= value possible */

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

static sem_info	*sem_inf;

static void mupip_downgrade_cleanup(void);

error_def(ERR_BADDBVER);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);
error_def(ERR_GTMCURUNSUPP);
error_def(ERR_DBRDONLY);
error_def(ERR_MUDWNGRDNOTPOS);
error_def(ERR_MUPGRDSUCC);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNODWNGRD);
error_def(ERR_MUSTANDALONE);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void mupip_downgrade(void)
{
	char		db_fn[MAX_FN_LEN + 1], ver_spec[MAX_VERSION_LEN + 1],
			dwngrd_ver[MAX_VERSION_LEN + STR_LIT_LEN(GTM_VER_LIT)];
	unsigned short	db_fn_len;	/* cli_get_str expects short */
	unsigned short	ver_spec_len;
	char		*hdr_ptr;
	fd_type		channel;
	int		save_errno, csd_size, rec_size;
	int		fstat_res, idx, dwngrd_ver_len;
	int4		status, rc;
	uint4		status2;
	off_t 		file_size;
	v6_sgmnt_data	csd;
	boolean_t	recovery_interrupted;
	struct stat	stat_buf;
	ZOS_ONLY(int	realfiletag;)
	int		ftrunc_status;

	sem_inf = (sem_info *)malloc(SIZEOF(sem_info) * FTOK_ID_CNT);
	memset(sem_inf, 0, SIZEOF(sem_info) * FTOK_ID_CNT);
	atexit(mupip_downgrade_cleanup);
	db_fn_len = SIZEOF(db_fn) - 1;
	if (!cli_get_str("FILE", db_fn, &db_fn_len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUNODBNAME);
	db_fn[db_fn_len] = '\0';	/* Null terminate */
	/* V7 can only downgrade a V6 database which needs it's block sized EOF block truncated to a 512-byte block */
	if (!cli_present("VERSION"))
		mupip_exit(ERR_GTMCURUNSUPP);
	ver_spec_len = MAX_VERSION_LEN;
	cli_get_str("VERSION", ver_spec, &ver_spec_len);
	ver_spec[ver_spec_len] = '\0';
	cli_strupper(ver_spec);
	if (memcmp(ver_spec, "V63000A", ver_spec_len))
		mupip_exit(ERR_GTMCURUNSUPP);
	/* Need to find out if this is a statsDB file. This necessitates opening the file to read the v6_sgmnt_data
	 * file header before we have the proper locks obtained for it so after checking, the file is closed again
	 * so it can be opened under lock to prevent race conditions. Downgrade is not supported on a V7 DB
	 */
	if (FD_INVALID != (channel = OPEN(db_fn, O_RDONLY)))
	{
		DO_FILE_READ(channel, 0, &csd, SIZEOF(v6_sgmnt_data), status, status2);
		if (0 == memcmp(csd.label, GDS_LABEL, STR_LIT_LEN(GDS_LABEL)))
			mupip_exit(ERR_GTMCURUNSUPP);
		if ((0 == memcmp(csd.label, V6_GDS_LABEL, STR_LIT_LEN(V6_GDS_LABEL))) && (RDBF_STATSDB_MASK == csd.reservedDBFlags))
		{
			F_CLOSE(channel, rc);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, db_fn_len, db_fn);
			mupip_exit(ERR_MUNODWNGRD);
		}
		F_CLOSE(channel, rc);
	}
	if (!mu_upgrd_dngrd_confirmed())
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Downgrade canceled by user"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Mupip downgrade started"));
	mu_all_version_get_standalone(db_fn, sem_inf);
	mu_outofband_setup();	/* Will ignore user interrupts. Note that the elapsed time for this is order of milliseconds */
	if (FD_INVALID == (channel = OPEN(db_fn, O_RDWR)))
	{
		save_errno = errno;
		if (FD_INVALID != (channel = OPEN(db_fn, O_RDONLY)))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_DBRDONLY, 2, db_fn_len, db_fn, errno, 0,
				MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Cannot downgrade read-only database"));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBOPNERR, 2, db_fn_len, db_fn, save_errno);
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* get file status */
	FSTAT_FILE(channel, &stat_buf, fstat_res);
	if (-1 == fstat_res)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"), CALLFROM, errno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBOPNERR, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNODWNGRD);
	}
	file_size = stat_buf.st_size;
	csd_size = SIZEOF(v6_sgmnt_data);
	DO_FILE_READ(channel, 0, &csd, csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNODWNGRD);
	}
	if (memcmp(csd.label, V6_GDS_LABEL, STR_LIT_LEN(V6_GDS_LABEL)))
	{ 	/* It is not V6.0-000 */
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		if (0 == memcmp(csd.label, GDS_LABEL, GDS_LABEL_SZ))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUDWNGRDNOTPOS);
		else if (memcmp(csd.label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBNOTGDS, 2, db_fn_len, db_fn);
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADDBVER, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNODWNGRD);
	}
	CHECK_DB_ENDIAN(&csd, db_fn_len, db_fn);
	/* It is V6.x version: So proceed with downgrade */
	if (csd.createinprogress)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
				2, LEN_AND_LIT("Database creation in progress"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	if (FROZEN(&csd))
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database is frozen"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* The following used to be a check for wc_blocked which is now unreachable because it resides
	 * in the shared memory.
	 */
	if (csd.machine_name[0])
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
			2, LEN_AND_LIT("Machine name in file header is non-null implying possible crash"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	if (csd.file_corrupt)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2, LEN_AND_LIT("Database corrupt"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	recovery_interrupted = FALSE;
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
	{
		if (csd.intrpt_recov_resync_strm_seqno[idx])
			recovery_interrupted = TRUE;
	}
	if (csd.intrpt_recov_tp_resolve_time || csd.intrpt_recov_resync_seqno || csd.recov_interrupted
						|| csd.intrpt_recov_jnl_state || csd.intrpt_recov_repl_state
						|| recovery_interrupted)
	{
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
				2, LEN_AND_LIT("Recovery was interrupted"));
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* V7 can only downgrade a V6 database which needs it's block sized EOF block truncated to a 512-byte block */
	csd.minor_dbver = GDSMV63000A;
	hdr_ptr = (char *)&csd;	/* No file header downgrade for V63000A case */
	/* Write updated file header first */
	DB_DO_FILE_WRITE(channel, 0, hdr_ptr, csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* After V6.3-000A, EOF block is a a GDS block. Before V6.3-000A, V4/V5/V6, had a 512-byte EOF block. Truncate as needed */
	FTRUNCATE(channel, file_size - csd.blk_size + DISK_BLOCK_SIZE, ftrunc_status);
	if (0 != ftrunc_status)
	{
		save_errno = errno;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("ftruncate()"), CALLFROM, save_errno);
		mupip_exit(ERR_MUNODWNGRD);
	}
	F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
	if (0 != rc)
	{
		save_errno = errno;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM, save_errno);
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* Print success message */
	MEMCPY_LIT(dwngrd_ver, GTM_VER_LIT);
	dwngrd_ver_len = STR_LIT_LEN(GTM_VER_LIT);
	memcpy(dwngrd_ver + dwngrd_ver_len, ver_spec, ver_spec_len);
	dwngrd_ver_len += ver_spec_len;
	assert(dwngrd_ver_len <= ARRAYSIZE(dwngrd_ver));
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUPGRDSUCC, 6, db_fn_len, db_fn,
					RTS_ERROR_LITERAL("downgraded"), dwngrd_ver_len, dwngrd_ver);
	mu_all_version_release_standalone(sem_inf);
	mupip_exit(SS_NORMAL);
}

static void mupip_downgrade_cleanup(void)
{
	if (sem_inf)
		mu_all_version_release_standalone(sem_inf);
}
