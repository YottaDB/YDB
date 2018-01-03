/****************************************************************
 *								*
 * Copyright (c) 2005-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_downgrade.c: Driver program to downgrade V6.x database files to V4.x */

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
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
#include "jnl.h"	/* For fd_type */
#include "error.h"
#include "util.h"
#include "gtmmsg.h"
#include "cli.h"
#include "repl_sp.h"
#include "mupip_exit.h"
#include "mupip_downgrade.h"
#include "mu_upgrd_dngrd_hdr.h"
#include "mu_upgrd_dngrd_confirmed.h"
#include "mu_outofband_setup.h"
#include "anticipatory_freeze.h"
#include "mu_all_version_standalone.h"

#define	GTM_VER_LIT		"GT.M "
#define	MAX_VERSION_LEN		16	/* 16 bytes enough to hold V63000A, longest -VERSION= value possible */

LITREF  char     	 	gtm_release_name[];
LITREF  int4            	gtm_release_name_len;

static sem_info	*sem_inf;

static void mupip_downgrade_cleanup(void);

error_def(ERR_BADDBVER);
error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPREMATEOF);
error_def(ERR_PREMATEOF);
error_def(ERR_DBRDONLY);
error_def(ERR_MUINFOUINT4);
error_def(ERR_MUINFOUINT8);
error_def(ERR_MUPGRDSUCC);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNODWNGRD);
error_def(ERR_MUDWNGRDTN);
error_def(ERR_MUDWNGRDNOTPOS);
error_def(ERR_MUDWNGRDNRDY);
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
	v15_sgmnt_data	v15_csd;
	sgmnt_data	csd;
	boolean_t	recovery_interrupted;
 	struct stat    	stat_buf;
	ZOS_ONLY(int	realfiletag;)
	unsigned char	new_master_map[MASTER_MAP_SIZE_V4];
	enum db_ver	desired_dbver;
	int		ftrunc_status;

	/* Structure checks .. */
	assert((24 * 1024) == SIZEOF(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */

	sem_inf = (sem_info *)malloc(SIZEOF(sem_info) * FTOK_ID_CNT);
	memset(sem_inf, 0, SIZEOF(sem_info) * FTOK_ID_CNT);
	atexit(mupip_downgrade_cleanup);
	db_fn_len = SIZEOF(db_fn) - 1;
	if (!cli_get_str("FILE", db_fn, &db_fn_len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUNODBNAME);
	db_fn[db_fn_len] = '\0';	/* Null terminate */
	if (cli_present("VERSION"))
	{
		ver_spec_len = ARRAYSIZE(ver_spec) - 1;		/* Ensure spare for nul */
		cli_get_str("VERSION", ver_spec, &ver_spec_len);
		ver_spec[ver_spec_len] = '\0';
		cli_strupper(ver_spec);
		if (0 == memcmp(ver_spec, "V4", ver_spec_len))
		{	/* Downgrade file header to V4 format and downgrade EOF block from GDS-block to 512-byte block */
			desired_dbver = GDSV4;
		} else if (0 == memcmp(ver_spec, "V5", ver_spec_len))
		{	/* Downgrade file header to V5 format and downgrade EOF block from GDS-block to 512-byte block */
			desired_dbver = GDSV5;
		} else if (0 == memcmp(ver_spec, "V63000A", ver_spec_len))
		{	/* No file header downgrade needed. Only EOF block downgrade needed. */
			desired_dbver = GDSNOVER;
		} else
			assert(FALSE);              /* CLI should prevent us ever getting here */
	} else
	{
		MEMCPY_LIT(ver_spec, "V4");
		ver_spec_len = STR_LIT_LEN("V4");
		desired_dbver = GDSV4;       /* really want to keep version, which has not yet been read */
	}
	/* Need to find out if this is a statsDB file. This necessitates opening the file to read the sgmnt_data
	 * file header before we have the proper locks obtained for it so after checking, the file is closed again
	 * so it can be opened under lock to prevent race conditions.
	 */
	if (FD_INVALID != (channel = OPEN(db_fn, O_RDONLY)))
	{
		DO_FILE_READ(channel, 0, &csd, SIZEOF(sgmnt_data), status, status2);
		if ((0 == memcmp(csd.label, GDS_LABEL, STR_LIT_LEN(GDS_LABEL))) && (RDBF_STATSDB_MASK == csd.reservedDBFlags))
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
#	if defined(__MVS__)
	if (-1 == gtm_zos_tag_to_policy(channel, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(db_fn, errno, realfiletag, TAG_BINARY);
#	endif
	csd_size = SIZEOF(sgmnt_data);
	DO_FILE_READ(channel, 0, &csd, csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNODWNGRD);
	}
	if (memcmp(csd.label, GDS_LABEL, STR_LIT_LEN(GDS_LABEL)))
	{ 	/* It is not V5.0-000 */
		F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
		if (memcmp(csd.label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBNOTGDS, 2, db_fn_len, db_fn);
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADDBVER, 2, db_fn_len, db_fn);
		mupip_exit(ERR_MUNODWNGRD);
	}
	CHECK_DB_ENDIAN(&csd, db_fn_len, db_fn);
	/* It is V5.x version: So proceed with downgrade */
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
	if (GDSV5 == desired_dbver)	/* Downgrading to V5 version*/
	{
		if ((START_VBN_V6 == csd.start_vbn) || (MASTER_MAP_BLOCKS_DFLT == csd.master_map_len))
		{	/* DB is created with V6 version*/
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("Database is created with V6 version."));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (!csd.span_node_absent)
		{	/* DB might contain spanning node */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("Spanning node might be present."));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (!csd.maxkeysz_assured)
		{	/* DB might contain keys larger than max_key_sz in db header */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
			LEN_AND_LIT("Database might contain keys larger than MAX KEY SIZE in DB header"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (csd.max_key_size > OLD_MAX_KEY_SZ)
		{	/* DB might contain keys larger than 255 */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("Database might contain keys larger than 255 bytes"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (IS_ENCRYPTED(csd.is_encrypted) && csd.non_null_iv)
		{	/* Blocks are encrypted using non-null IVs */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("Database is encrypted using non-null IVs"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (USES_NEW_KEY(&csd))
		{	/* Database is being (re)encrypted */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("Database is being (re)encrypted"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		/* Determine the max record size which is safe from spanning node perspective */
		rec_size = csd.blk_size - csd.reserved_bytes - SIZEOF(blk_hdr);
		if (csd.max_rec_size > rec_size)
		{	/* max_rec_size is not supported for given blk_size in V5 */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR), 2,
				LEN_AND_LIT("MAX REC SIZE is not supported for given BLK SIZE in V5"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		csd.freeze_on_fail = FALSE;
		csd.span_node_absent = TRUE;
		csd.maxkeysz_assured = FALSE;
		csd.minor_dbver = GDSMV63000A;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
				LEN_AND_LIT("V5 supportable record size for current DB configuration "), rec_size, rec_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
				LEN_AND_LIT("V5 supportable max key size for current DB configuration"),
				OLD_MAX_KEY_SZ, OLD_MAX_KEY_SZ);
		hdr_ptr = (char *)&csd;
	} else if (GDSV4 == desired_dbver)	/* Downgrading to V4 version*/
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file header size"),
			csd_size, csd_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Old file length"),
			&file_size, &file_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4,
				4, LEN_AND_LIT("Old file start_vbn"), csd.start_vbn, csd.start_vbn);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4,
				4, LEN_AND_LIT("Old file gds blk_size"), csd.blk_size, csd.blk_size);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Old file total_blks"),
					csd.trans_hist.total_blks, csd.trans_hist.total_blks);
		assert(ROUND_DOWN2(csd.blk_size, DISK_BLOCK_SIZE) == csd.blk_size);
		assert((((off_t)csd.start_vbn - 1) * DISK_BLOCK_SIZE +
			(off_t)csd.trans_hist.total_blks * csd.blk_size + (off_t)DISK_BLOCK_SIZE == file_size) ||
		   (((off_t)csd.start_vbn - 1) * DISK_BLOCK_SIZE +
			(off_t)csd.trans_hist.total_blks * csd.blk_size + (off_t)csd.blk_size == file_size));
		if (START_VBN_V4 != csd.start_vbn)
		{	/* start_vbn is not something that GT.M V4 can handle. signal downgrade not possible */
			F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUDWNGRDNOTPOS, 2, csd.start_vbn, START_VBN_V4);
			mupip_exit(ERR_MUNODWNGRD);
		}
		if ((trans_num)MAX_TN_V4 < csd.trans_hist.curr_tn)
		{
			F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_MUDWNGRDTN, 3, &csd.trans_hist.curr_tn, db_fn_len, db_fn);
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (csd.blks_to_upgrd != (csd.trans_hist.total_blks - csd.trans_hist.free_blocks))
		{
			F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_MUDWNGRDNRDY, 3, db_fn_len, db_fn,
				   (csd.trans_hist.total_blks - csd.trans_hist.free_blocks - csd.blks_to_upgrd));
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (MASTER_MAP_SIZE_V4 < csd.master_map_len || MAXTOTALBLKS_V4 < csd.trans_hist.total_blks)
		{
			F_CLOSE(channel, rc);	/* resets "channel" to FD_INVALID */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("master_map_len"),
				   csd.master_map_len,  csd.master_map_len);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_TYPE(ERR_TEXT, ERROR),
					2, LEN_AND_LIT("Master map is too large"));
			mupip_exit(ERR_MUNODWNGRD);
		}
		DO_FILE_READ(channel, 0, new_master_map, csd.master_map_len, status, status2);
		if (SS_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
			mupip_exit(ERR_MUNODWNGRD);
		}
		if (csd.master_map_len < MASTER_MAP_SIZE_V4)
			memset(new_master_map + csd.master_map_len, 0xFF, MASTER_MAP_SIZE_V4 - csd.master_map_len);
		/* Now call mu_dwngrd_header to do file header downgrade */
		mu_dwngrd_header(&csd, &v15_csd);
		memcpy(v15_csd.master_map, new_master_map, MASTER_MAP_SIZE_V4);
		hdr_ptr = (char *)&v15_csd;
	} else
	{
		assert(GDSNOVER == desired_dbver);	/* Downgrading to V4 version*/
		csd.minor_dbver = GDSMV63000A;
		hdr_ptr = (char *)&csd;	/* No file header downgrade for V63000A case */
	}
	/* Write updated file header first */
	DB_DO_FILE_WRITE(channel, 0, hdr_ptr, csd_size, status, status2);
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, db_fn_len, db_fn, status);
		mupip_exit(ERR_MUNODWNGRD);
	}
	/* V6 format had a GDS block EOF block whereas V4 format has only a 512-byte EOF block. Do truncate as necessary */
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
