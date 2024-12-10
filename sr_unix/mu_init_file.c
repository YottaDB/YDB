/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_statvfs.h"

#include "parse_file.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gtm_atomic.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "gtmio.h"
#include "send_msg.h"
#include "is_raw_dev.h"
#include "disk_block_available.h"
#include "mucregini.h"
#include "mu_cre_file.h"
#include "mucblkini.h"
#include "gtmmsg.h"
#include "util.h"
#include "gtmdbglvl.h"
#include "anticipatory_freeze.h"
#include "gtmcrypt.h"
#include "shmpool.h"	/* Needed for the shmpool structures */
#include "jnl.h"
#include "db_write_eof_block.h"
#include "get_fs_block_size.h"
#include "gtm_permissions.h"
#include "getzposition.h"
#include "error.h"
#include "db_header_conversion.h"
#include "ftok_sems.h"
#include "io.h"
#include "gtm_reservedDB.h"

STATICDEF	int			mu_init_file_fd = FD_INVALID;		/* needed for "mu_init_file_ch" */
STATICDEF	char			*mu_init_file_path = NULL;	/* needed for "mu_init_file_ch" */
STATICDEF	sgmnt_data_ptr_t	mu_init_cs_data = NULL;
STATICDEF	boolean_t		mu_init_had_ftok_sem = FALSE;
STATICDEF	gd_region		*mu_init_region	= NULL;
STATICDEF	boolean_t		mu_init_ftok_counter_halted = FALSE;
/* Note: CLEANUP macro is invoked by "mu_init_file_ch" and "mu_init_file" and hence needs to use "static" variables
 * (mu_init_file_fd & mu_init_file_path) instead of "local" variables.
 */

#define MARK_CREATE_COMPLETE(CSA, CSD, UDI, STATUS)									\
MBSTART	{														\
	(CSD)->createcomplete = TRUE;											\
	DB_LSEEKWRITE(CSA, UDI, (UDI)->fn, (UDI)->fd, OFFSETOF(sgmnt_data, createcomplete), &((CSD)->createcomplete), 	\
			SIZEOF((CSD)->createcomplete), STATUS);								\
} MBEND

#define CLEANUP(XX)												\
MBSTART {													\
	int	rc;												\
	unix_db_info	*lcl_udi = NULL;									\
	sgmnt_addrs	*lcl_csa = NULL;									\
														\
														\
	if (NULL != mu_init_region)										\
		lcl_udi = FILE_INFO(mu_init_region);								\
	if (mu_init_cs_data)											\
	{													\
		free(mu_init_cs_data);										\
		mu_init_cs_data = NULL;										\
		if (lcl_udi)											\
		{												\
			lcl_csa = &lcl_udi->s_addrs;								\
			lcl_csa->hdr = NULL;									\
			lcl_csa->ti = NULL;									\
			lcl_csa->bmm = NULL;									\
		}												\
	}													\
	if (FD_INVALID != mu_init_file_fd)									\
	{													\
		assert(!lcl_udi || (mu_init_file_fd == lcl_udi->fd));						\
		CLOSEFILE_RESET(mu_init_file_fd, rc); /* resets "mu_init_file_fd" to FD_INVALID */		\
		if (lcl_udi)											\
			lcl_udi->fd = FD_INVALID;								\
	}													\
	if ((EXIT_ERR == (XX)) && (NULL != mu_init_file_path))							\
	{													\
		rc = UNLINK(mu_init_file_path);									\
		assert(0 == rc);										\
	}													\
	if (lcl_udi && !mu_init_had_ftok_sem && lcl_udi->grabbed_ftok_sem)					\
		ftok_sem_release(mu_init_region, !mu_init_ftok_counter_halted, FALSE);				\
	mu_init_ftok_counter_halted = FALSE;									\
	mu_init_file_path = NULL;										\
	mu_init_file_fd = FD_INVALID;										\
	mu_init_region = NULL;											\
	mu_init_had_ftok_sem = FALSE;										\
} MBEND

/* Macros to send warning or error messages to the correct destination:
 *  - If MUPIP image, message goes to stderr of the process.
 *  - Else MUMPS image captures the error message and wraps it with MUCREFILERR and sends it to the system log.
 * In addition, some messages require cleanup if they are emitted past a certain point in the processing (said point
 * setting the 'cleanup_needed' flag to TRUE.
 */
#define PUTMSG_ERROR_CSA(CSAARG, REG, VARCNT, ERRORID, ...)								\
MBSTART {														\
	if (cleanup_needed)												\
		CLEANUP(EXIT_ERR);											\
	PUTMSG_MSG_ROUTER_CSA(CSAARG, REG, VARCNT, ERRORID, __VA_ARGS__);						\
} MBEND
#define PUTMSG_WARN_CSA(CSAARG, REG, VARCNT, ERRORID, ...) PUTMSG_MSG_ROUTER_CSA(CSAARG, REG, VARCNT, ERRORID, __VA_ARGS__)

#define	OUT_LINE	(512 + 1)
#define	USUAL_UMASK	022

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			gtmDebugLevel;
GBLREF	uint4			process_id;
GBLREF 	uint4			mu_upgrade_in_prog;
GBLREF	enum db_ver		gtm_db_create_ver;              /* database creation version */
#ifdef DEBUG
GBLREF	boolean_t		in_mu_init_file;
#endif


error_def(ERR_DBBLKSIZEALIGN);
error_def(ERR_DBNOTGDS);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_INVSTATSDB);
error_def(ERR_BADDBVER);
error_def(ERR_DBFILECREATED);
error_def(ERR_DBOPNERR);
error_def(ERR_DSKSPCCHK);
error_def(ERR_FILECREERR);
error_def(ERR_FNTRANSERROR);
error_def(ERR_LOWSPACECRE);
error_def(ERR_MUCREFILERR);
error_def(ERR_MUNOSTRMBKUP);
error_def(ERR_NOCREMMBIJ);
error_def(ERR_NOCRENETFILE);
error_def(ERR_NOSPACECRE);
error_def(ERR_PARNORMAL);
error_def(ERR_PREALLOCATEFAIL);
error_def(ERR_RAWDEVUNSUP);
error_def(ERR_DBFILNOFULLWRT);
error_def(ERR_DBFILERR);

/* Condition handler primarily to handle ERR_MEMORY errors by cleaning up the file that we created
 * before passing on control to higher level condition handlers.
 */
CONDITION_HANDLER(mu_init_file_ch)
{
	START_CH(TRUE);
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
	{
		assert(FALSE);	/* don't know of any possible INFO/SUCCESS errors */
		CONTINUE;			/* Keep going for non-error issues */
	}
	CLEANUP(EXIT_ERR);
	NEXTCH;
}

unsigned char mu_init_file(gd_region *reg, boolean_t has_ftok)
{
	char		path[MAX_FN_LEN + 1];
	int		norm_vbn, i;
	ssize_t		status;
	size_t		read_len;
	int4		save_errno;
	gtm_uint64_t	avail_blocks, blocks_for_create, blocks_for_extension, delta_blocks;
	mstr		file;
	parse_blk	pblk;
	unix_db_info	*udi;
	gd_segment	*seg;
	gd_region	*baseDBreg;
	node_local_ptr_t	baseDBnl;
	char		hash[GTMCRYPT_HASH_LEN];
	int		retcode, perms, user_id, group_id, umask_orig;
	uint4		gtmcrypt_errno;
	off_t		new_eof;
	uint4		fsb_size;
	boolean_t	cleanup_needed = FALSE;
	boolean_t	statsdb_was_created;
	uint4		statsdb_cycle;
	sgmnt_addrs	*baseDBcsa, *mu_init_cs_addrs;
	struct stat	stat_buf;
	struct perm_diag_data	pdd;
	uint4		fbwsize;
	int4		dblksize;
	uint4 		sgmnt_hdr_plus_bmm_sz;
	block_id 	start_vbn_target;
	enum db_validity db_invalid;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!reg->file_initialized);
	mu_init_had_ftok_sem = has_ftok;
	assert(FD_INVALID == mu_init_file_fd);
	assert(NULL == mu_init_file_path);
	if (GDSVCURR == gtm_db_create_ver)
	{	/* Current version defaults */
		sgmnt_hdr_plus_bmm_sz	= SIZEOF_FILE_HDR_DFLT;
		start_vbn_target	= START_VBN_CURRENT;
	} else
	{	/* Prior version settings */
		sgmnt_hdr_plus_bmm_sz	= SIZEOF_FILE_HDR_V6;
		start_vbn_target	= START_VBN_V6;
	}
	DEBUG_ONLY(in_mu_init_file = TRUE;)
	assert((-(SIZEOF(uint4) * 2) & SIZEOF_FILE_HDR_DFLT) == SIZEOF_FILE_HDR_DFLT);
	assert(NULL == mu_init_cs_data);
	mu_init_region = reg;
	seg = reg->dyn.addr;
	FILE_CNTL_INIT_IF_NULL(seg);
	udi = FILE_INFO(reg);
	mu_init_cs_addrs = &udi->s_addrs;
	assert(NULL == mu_init_cs_addrs->hdr);
	assert(NULL == mu_init_cs_addrs->ti);
	seg->read_only = FALSE;
	if (!mu_init_had_ftok_sem)
	{
		if (!ftok_sem_get(reg, TRUE, GTM_ID, FALSE, &mu_init_ftok_counter_halted))
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		}
	}
	cleanup_needed = TRUE;
	assert(udi->grabbed_ftok_sem);
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	pblk.buffer = path;
	pblk.buff_size = MAX_FN_LEN;
	file.addr = (char*)reg->dyn.addr->fname;
	file.len = reg->dyn.addr->fname_len;
	strncpy(path, file.addr, file.len);
	*(path + file.len) = '\0';
	if (is_raw_dev(path))
	{
		assert(FALSE);
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_RAWDEVUNSUP, 2, REG_LEN_STR(reg));
		return EXIT_ERR;
	}
	pblk.def1_buf = DEF_DBEXT;
	pblk.def1_size = SIZEOF(DEF_DBEXT) - 1;
	if (ERR_PARNORMAL != (retcode = parse_file(&file, &pblk)))	/* Note assignment */
	{
		assert(FALSE);
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_FNTRANSERROR, 2, REG_LEN_STR(reg));
		return EXIT_ERR;
	}
	path[pblk.b_esl] = 0;
	if (pblk.fnb & F_HAS_NODE)
	{	/* Remote node specification given */
		assert(FALSE);
		assert(pblk.b_node);
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_NOCRENETFILE, 2, LEN_AND_STR(path));
		return EXIT_WRN;
	}
	assert(!pblk.b_node);
	memcpy(seg->fname, pblk.buffer, pblk.b_esl);
	seg->fname[pblk.b_esl] = '\0';
	seg->fname_len = pblk.b_esl;
	udi->fn = (char *)seg->fname;
	assert(!udi->raw);
	mu_init_file_fd = udi->fd;
	errno = EINTR;
	assert(FD_INVALID == mu_init_file_fd);
	while ((FD_INVALID == mu_init_file_fd) && (EINTR == errno))
	{
		OPENFILE_CLOEXEC(pblk.l_dir, O_RDWR, mu_init_file_fd);
	}
	if (FD_INVALID == mu_init_file_fd)
	{
		save_errno = errno;
		TREF(mu_cre_file_openrc) = save_errno;		/* Save for gvcst_init() */
		assert(ENOENT == save_errno);
		/* If this is an AUTODB (including STATSDB) and the error is ENOENT or something similar, it indicates that
		 * another process got to mu_init_file first, attempted the mu_init_file, and encountered an error there wheich
		 * caused it to delete the newly created file.
		 */
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 5, ERR_DBOPNERR, 2, LEN_AND_STR(path), save_errno);
		return EXIT_ERR;
	}
	errno = 0;
	/* mu_init_file_fd is also needed by "mu_init_file_ch" but that is already set */
	assert((FD_INVALID == udi->fd));
	udi->fd = mu_init_file_fd;
	mu_init_cs_data = (sgmnt_data_ptr_t)malloc(sgmnt_hdr_plus_bmm_sz);
	/* The following function memsets mu_init_cs_data to zero and reads as much as possible into it from the file header */
	db_invalid = DB_INVALID_SHORT;
	if (!mu_init_had_ftok_sem)
	{
		db_invalid = read_db_file_header(udi, reg, mu_init_cs_data);
		switch (db_invalid)
		{
			case DB_VALID:
			case DB_VALID_DBGLDMISMATCH:
				assert(reg->file_initialized);
				if (IS_STATSDB_REG(reg))
				{
					STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
					DBGRDB((stderr, "%s:%d:%s: process id %d found statsdb file %s for region %s, base region "
								"%s already initialized, returning\n", __FILE__, __LINE__, __func__,
								process_id, reg->dyn.addr->fname, reg->rname, baseDBreg->rname));
				} else
				{
					DBGRDB((stderr, "%s:%d:%s: process id %d found file %s for region %s already initialized, "
								"returning\n", __FILE__, __LINE__, __func__, process_id,
								reg->dyn.addr->fname, reg->rname));
				}
				CLEANUP(EXIT_NRM);
				return EXIT_NRM;
			case DB_INVALID_SHORT:
				DBGRDB((stderr, "%s:%d:%s: process id %d found truncated file %s for region %s, doing "
							"initialization\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				break;
			case DB_INVALID_CREATEINPROGRESS:
				DBGRDB((stderr, "%s:%d:%s: process id %d found create-in-progress file %s for region %s, doing "
							"initialization\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				break;
			case DB_READERR:
				save_errno = errno;
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 5, ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
				return EXIT_ERR;
			case DB_INVALID_NOTGDS:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_DBNOTGDS, 2, DB_LEN_STR(reg));
				return EXIT_ERR;
			case DB_INVALID_BADDBVER:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_BADDBVER, 2, DB_LEN_STR(reg));
				return EXIT_ERR;
			case DB_INVALID_STATSDBNOTSUPP:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(reg));
				return EXIT_ERR;
			case DB_INVALID_INVSTATSDB:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 6, ERR_INVSTATSDB, 4, DB_LEN_STR(reg), REG_LEN_STR(reg));
				return EXIT_ERR;
			case DB_INVALID_NOTOURSTATSDB:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 8, ERR_STATSDBINUSE, 6, DB_LEN_STR(reg),
						mu_init_cs_data->basedb_fname_len, mu_init_cs_data->basedb_fname,
						DB_LEN_STR(baseDBreg));
				return EXIT_ERR;
			case DB_INVALID_BLKSIZEALIGN:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				fbwsize = get_fs_block_size(udi->fd);
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 6, ERR_DBBLKSIZEALIGN, 4, DB_LEN_STR(reg),
						mu_init_cs_data->blk_size, fbwsize);
				return EXIT_ERR;
			case DB_INVALID_ENDIAN:
				DBGRDB((stderr, "%s:%d:%s: process id %d saw read_db_file_header error for file %s for region %s, "
							"returning\n", __FILE__, __LINE__, __func__, process_id,
							reg->dyn.addr->fname, reg->rname));
				PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 6, ERR_DBENDIAN, 4, DB_LEN_STR(reg), ENDIANOTHER,
						ENDIANTHIS);
				return EXIT_ERR;
			default:
				assert(FALSE);
		}
	}
#ifdef DEBUG
	if (mu_init_had_ftok_sem)
	{
		db_invalid = read_db_file_header(udi, reg, mu_init_cs_data);
		assert((DB_INVALID_SHORT == db_invalid) || (DB_INVALID_CREATEINPROGRESS == db_invalid));
	}
#endif
	assert(db_invalid && (DB_POTENTIALLY_VALID > db_invalid));
	/* We've established that this is a file that requires initialization and is a valid db file at some stage of the
	 * initialization process. Establish a condition handler which will delete the file if we fail somewhere here
	 * (and allow someone else to recreate it from scratch)
	 */
	mu_init_file_path = &path[0];	/* needed by "mu_init_file_ch", now that we have the right to delete */
	ESTABLISH_RET(mu_init_file_ch, EXIT_ERR);
	if (0 != (save_errno = disk_block_available(mu_init_file_fd, &avail_blocks, FALSE)))
	{
		/* Note - internally invokes CLEANUP */
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 5, ERR_DSKSPCCHK, 2, LEN_AND_STR(path), save_errno);
		REVERT;
		return EXIT_ERR;
	}
	if (seg->asyncio)
	{	/* AIO = ON, implies we need to use O_DIRECT. Check for db vs fs blksize alignment issues. */
		fsb_size = get_fs_block_size(mu_init_file_fd);
		if (0 != (seg->blk_size % fsb_size))
		{
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 6, ERR_DBBLKSIZEALIGN, 4, LEN_AND_STR(path), seg->blk_size,
					fsb_size);
				/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
	}
	/* Blocks_for_create is in the unit of DISK_BLOCK_SIZE */
	blocks_for_create = (gtm_uint64_t)(DIVIDE_ROUND_UP(sgmnt_hdr_plus_bmm_sz, DISK_BLOCK_SIZE) + 1
					   + (seg->blk_size / DISK_BLOCK_SIZE
					      * (gtm_uint64_t)((DIVIDE_ROUND_UP(seg->allocation, BLKS_PER_LMAP - 1))
							       + seg->allocation)));
	blocks_for_extension = (seg->blk_size / DISK_BLOCK_SIZE
				* ((DIVIDE_ROUND_UP(EXTEND_WARNING_FACTOR * (gtm_uint64_t)seg->ext_blk_count,
						    BLKS_PER_LMAP - 1))
				   + EXTEND_WARNING_FACTOR * (gtm_uint64_t)seg->ext_blk_count));
	if (!(gtmDebugLevel & GDL_IgnoreAvailSpace))
	{	/* Bypass this space check if debug flag above is on. Allows us to create a large sparse DB
		 * in space it could never fit it if wasn't sparse. Needed for some tests.
		 * Also, if the anticipatory freeze scheme is in effect at this point, we would have issued
		 * a NOSPACECRE warning (see NOSPACEEXT message which goes through a similar transformation).
		 * But at this point, we are guaranteed to not have access to the journal pool or csa both
		 * of which are necessary for the INST_FREEZE_ON_ERROR_ENABLED(csa) macro so we don't bother
		 * to do the warning transformation in this case. The only exception to this is a statsdb
		 * which is anyways not journaled so need not worry about INST_FREEZE_ON_ERROR_ENABLED.
		 */
		assert(((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl)) || IS_AUTODB_REG(reg));
		if (avail_blocks < blocks_for_create)
		{
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 6, ERR_NOSPACECRE, 4, LEN_AND_STR(path), &blocks_for_create,
					&avail_blocks);
				/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			if (IS_MUPIP_IMAGE)
				send_msg_csa(CSA_ARG(mu_init_cs_addrs) VARLSTCNT(6) ERR_NOSPACECRE, 4, LEN_AND_STR(path),
						&blocks_for_create, &avail_blocks);
			REVERT;
			return EXIT_ERR;
		}
		delta_blocks = avail_blocks - blocks_for_create;
		if (delta_blocks < blocks_for_extension)
		{
			PUTMSG_WARN_CSA(mu_init_cs_addrs, reg, 8, ERR_LOWSPACECRE, 6, LEN_AND_STR(path), EXTEND_WARNING_FACTOR,
					&blocks_for_extension, DISK_BLOCK_SIZE, &delta_blocks);
			if (IS_MUPIP_IMAGE)	/* Is not mupip, msg already went to operator log */
				send_msg_csa(CSA_ARG(mu_init_cs_addrs) VARLSTCNT(8) ERR_LOWSPACECRE, 6, LEN_AND_STR(path),
						EXTEND_WARNING_FACTOR, &blocks_for_extension, DISK_BLOCK_SIZE, &delta_blocks);
		}
	}
	memset(mu_init_cs_data, 0, sizeof(*mu_init_cs_data));
	mu_init_cs_data->semid = INVALID_SEMID;
	mu_init_cs_data->shmid = INVALID_SHMID;
	/* We want our datablocks to start on what would be a block boundary within the file which will aid I/O
	 * so pad the fileheader if necessary to make this happen.
	 */
	norm_vbn = DIVIDE_ROUND_UP(sgmnt_hdr_plus_bmm_sz, DISK_BLOCK_SIZE) + 1; /* Convert bytes to blocks */
	assert(start_vbn_target >= norm_vbn);
	mu_init_cs_data->start_vbn = start_vbn_target;
	mu_init_cs_data->free_space += (start_vbn_target - norm_vbn) * DISK_BLOCK_SIZE;
	assert(0 == mu_init_cs_data->free_space);	/* As long as everything is in DISK_BLOCK_SIZE, this is TRUE */
	mu_init_cs_data->acc_meth = reg->dyn.addr->acc_meth;
	if ((dba_mm == mu_init_cs_data->acc_meth) && (reg->jnl_before_image))
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 4, ERR_NOCREMMBIJ, 2, LEN_AND_STR(path));
			/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	mu_init_cs_data->write_fullblk = seg->full_blkwrt;
	if (mu_init_cs_data->write_fullblk)
        {       /* We have been asked to do FULL BLOCK WRITES for this database. On *NIX, attempt to get the filesystem
                 * blocksize from statvfs. This allows a full write of a blockwithout the OS having to fetch the old
                 * block for a read/update operation. We will round the IOs to the next filesystem blocksize if the
                 * following criteria are met:
                 *
                 * 1) Database blocksize must be a whole multiple of the filesystem blocksize for the above
                 *    mentioned reason.
                 *
                 * 2) Filesystem blocksize must be a factor of the location of the first data block
                 *    given by the start_vbn.
                 *
                 * The saved length (if the feature is enabled) will be the filesystem blocksize and will be the
                 * length that a database IO is rounded up to prior to initiation of the IO.
                 */
                fbwsize = get_fs_block_size(udi->fd);
                dblksize = seg->blk_size;
                if (0 == fbwsize || (0 != dblksize % fbwsize) || (0 != (BLK_ZERO_OFF(mu_init_cs_data->start_vbn)) % fbwsize))
		{
                        mu_init_cs_data->write_fullblk = 0;         /* This region is not fullblockwrite enabled */
			if (!IS_STATSDB_REGNAME(reg))
			{
				if (!fbwsize)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_DBFILNOFULLWRT, 5,
					LEN_AND_LIT("Could not get native filesize"),
					LEN_AND_LIT("File size extracted: "), fbwsize);
				else if (0 != dblksize % fbwsize)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_DBFILNOFULLWRT, 5,
					LEN_AND_LIT("Database block size not a multiple of file system block size\n"),
					LEN_AND_LIT("DB blocks size: "), dblksize);
			}
		}
                /* Report this length in DSE even if not enabled */
                mu_init_cs_addrs->fullblockwrite_len = fbwsize;              /* Length for rounding fullblockwrite */
        }
	mu_init_cs_data->trans_hist.total_blks = reg->dyn.addr->allocation;
	/* There are (bplmap - 1) non-bitmap blocks per bitmap, so add (bplmap - 2) to number of non-bitmap blocks
	 * and divide by (bplmap - 1) to get total number of bitmaps for expanded database. (must round up in this
	 * manner as every non-bitmap block must have an associated bitmap)
	 */
	mu_init_cs_data->trans_hist.total_blks += DIVIDE_ROUND_UP(mu_init_cs_data->trans_hist.total_blks, BLKS_PER_LMAP - 1);
	mu_init_cs_data->extension_size = reg->dyn.addr->ext_blk_count;
	/* Check if this file is an encrypted database. If yes, do init */
	if (IS_ENCRYPTED(seg->is_encrypted))
	{
		GTMCRYPT_HASH_GEN(mu_init_cs_addrs, strnlen(path, sizeof(path)), path, 0, NULL, hash, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, file.len, file.addr);
			REVERT;
			CLEANUP(EXIT_ERR);
			return EXIT_ERR;
		}
		memcpy(mu_init_cs_data->encryption_hash, hash, GTMCRYPT_HASH_LEN);
		SET_AS_ENCRYPTED(mu_init_cs_data->is_encrypted); /* Mark this file as encrypted */
		INIT_DB_OR_JNL_ENCRYPTION(mu_init_cs_addrs, mu_init_cs_data, strnlen(path, sizeof(path)), path, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, file.len, file.addr);
			REVERT;
			CLEANUP(EXIT_ERR);
			return EXIT_ERR;
		}
	} else
		SET_AS_UNENCRYPTED(mu_init_cs_data->is_encrypted);
	mu_init_cs_data->non_null_iv = TRUE;
	mu_init_cs_data->encryption_hash_cutoff = UNSTARTED;
	mu_init_cs_data->encryption_hash2_start_tn = 0;
	mu_init_cs_data->span_node_absent = TRUE;
	mu_init_cs_data->maxkeysz_assured = TRUE;
	mu_init_cs_data->problksplit = DEFAULT_PROBLKSPLIT;
	mucregini(reg, mu_init_cs_data, gtm_db_create_ver);
	assertpro(mu_init_cs_data == FILE_INFO(reg)->s_addrs.hdr);
	ASSERT_NO_DIO_ALIGN_NEEDED(udi);	/* because we are creating the database and so effectively have standalone access */
	DB_LSEEKWRITE(mu_init_cs_addrs, udi, udi->fn, udi->fd, 0, mu_init_cs_data, SIZEOF(*mu_init_cs_data), status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("writing out file header"),
				LEN_AND_LIT(path), status);
			/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	for (i = 0; i < mu_init_cs_data->trans_hist.total_blks ; i += mu_init_cs_data->bplmap)
	{
		status = bml_init(reg, i, 0);
		if (status != SS_NORMAL)
		{
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 5, ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
			REVERT;
			return EXIT_ERR;
		}
	}
	if (EXIT_NRM != mucblkini(reg, gtm_db_create_ver))
	{
		/* Should already have reported error from mucblkini */
		CLEANUP(EXIT_ERR); /* Only EXIT_ERR or EXIT_NRM from mucblkini */
		REVERT;
		return EXIT_ERR;
	}
	if (GDSVCURR != gtm_db_create_ver)
		db_header_dwnconv(mu_init_cs_data);
	ASSERT_NO_DIO_ALIGN_NEEDED(udi);	/* because we are creating the database and so effectively have standalone access */
	DB_LSEEKWRITE(mu_init_cs_addrs, udi, udi->fn, udi->fd, 0, mu_init_cs_data, sgmnt_hdr_plus_bmm_sz, status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("writing out file header"),
				LEN_AND_LIT(path), status);
			/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	new_eof = (off_t)BLK_ZERO_OFF(mu_init_cs_data->start_vbn)
		+ (off_t)mu_init_cs_data->trans_hist.total_blks * mu_init_cs_data->blk_size;
	status = db_write_eof_block(udi, udi->fd, mu_init_cs_data->blk_size, new_eof, &(TREF(dio_buff)));
	if (0 != status)
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("writing out end-of-file marker"),
				LEN_AND_LIT(path),
				 status); /* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	if (!mu_init_cs_data->defer_allocate)
	{
		POSIX_FALLOCATE(udi->fd, 0, BLK_ZERO_OFF(mu_init_cs_data->start_vbn) +
					 ((off_t)(mu_init_cs_data->trans_hist.total_blks + 1) * mu_init_cs_data->blk_size), status);
		if (0 != status)
		{
			assert(ENOSPC == status);
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 5, ERR_PREALLOCATEFAIL, 2, DB_LEN_STR(reg), status);
				/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
	}
	/* If we are opening a statsDB, use IPC type permissions derived from the baseDB */
	if (IS_STATSDB_REG(reg))
	{
		STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
		assert(baseDBreg->open);
		baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
		baseDBnl = baseDBcsa->nl;
		assert(baseDBnl);
		STAT_FILE((char *)baseDBcsa->nl->fname, &stat_buf, retcode);
		if (0 > retcode)
		{	/* Should be rare-if-ever message as we just opened the baseDB so it should be there */
			save_errno = errno;
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4,
					 LEN_AND_LIT("getting base file information"), LEN_AND_STR(path), save_errno);
				/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
		if (!gtm_permissions(&stat_buf, &user_id, &group_id, &perms, PERM_IPC, &pdd))
		{	/* Not sure what could cause this as we would have done the same call when opening the baseDB but
			 * make sure it is present just in case.
			 */
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4,
					 LEN_AND_LIT("obtaining permissions from base DB"),  LEN_AND_STR(path), EPERM);
				/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
	} else
	{
		/* gtm_permissions() is derived from the DB file permissions. Only enforce umask when creating DBs */
		umask_orig = umask(USUAL_UMASK);	/* determine umask (destructive) */
		if (USUAL_UMASK != umask_orig)
			(void)umask(umask_orig);	/* reset umask */
		perms = 0666 & ~umask_orig;
	}
	if (-1 == CHMOD(pblk.l_dir, perms))
	{
		save_errno = errno;
		PUTMSG_WARN_CSA(mu_init_cs_addrs, reg, 7, MAKE_MSG_WARNING(ERR_FILECREERR), 4, LEN_AND_LIT("changing file mode"),
				LEN_AND_LIT(path), save_errno);
		MARK_CREATE_COMPLETE(mu_init_cs_addrs, mu_init_cs_data, udi, status);
		if (0 != status)
		{
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("writing out file header"),
					LEN_AND_LIT(path), status);
			/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
		GTM_FSYNC(udi->fd, status);
		if (0 != status)
		{
			PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("Flushing new file to disk"),
					LEN_AND_LIT(path), status);
			/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
			REVERT;
			return EXIT_ERR;
		}
		REVERT;
		reg->file_initialized = true;
		reg->did_file_initialization = true;
		CLEANUP(EXIT_WRN);
		return EXIT_WRN;
	}
	if ((32 * 1024 - SIZEOF(shmpool_blk_hdr)) < mu_init_cs_data->blk_size)
		PUTMSG_WARN_CSA(mu_init_cs_addrs, reg, 5, ERR_MUNOSTRMBKUP, 3, RTS_ERROR_STRING(path), 32 * 1024 - DISK_BLOCK_SIZE);
	if (!(RDBF_AUTODB & reg->reservedDBFlags))
		PUTMSG_WARN_CSA(mu_init_cs_addrs, reg, 4, ERR_DBFILECREATED, 2, LEN_AND_STR(path));
	MARK_CREATE_COMPLETE(mu_init_cs_addrs, mu_init_cs_data, udi, status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("writing out file header"),
				LEN_AND_LIT(path), status);
		/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	GTM_FSYNC(udi->fd, status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA(mu_init_cs_addrs, reg, 7, ERR_FILECREERR, 4, LEN_AND_LIT("Flushing new file to disk"),
				LEN_AND_LIT(path), status);
		/* Note: Above macro internally invokes CLEANUP(EXIT_ERR) */
		REVERT;
		return EXIT_ERR;
	}
	REVERT;
	reg->file_initialized = true;
	reg->did_file_initialization = true;
	CLEANUP(EXIT_NRM);
	if (IS_STATSDB_REG(reg))
	{
		STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
		DBGRDB((stderr, "%s:%d:%s: process id %d finished initializing statsdb file %s for region %s, base region %s\n",
					__FILE__, __LINE__, __func__, process_id, reg->dyn.addr->fname, reg->rname,
					baseDBreg->rname));
	} else
		DBGRDB((stderr, "%s:%d:%s: process id %d finished initializing file %s for region %s\n", __FILE__, __LINE__,
					__func__, process_id, reg->dyn.addr->fname, reg->rname));
	return EXIT_NRM;
}
