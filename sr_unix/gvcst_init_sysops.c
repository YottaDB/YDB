/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#include <errno.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include "gtm_stdlib.h"
#include "gtm_ipc.h"
#include "gtm_un.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_sem.h"
#include "gtm_statvfs.h"
#include "hugetlbfs_overrides.h"	/* for the ADJUST_SHM_SIZE_FOR_HUGEPAGES macro */

#include "gt_timer.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "parse_file.h"
#include "jnl.h"
#include "interlock.h"
#include "io.h"
#include "iosp.h"
#include "error.h"
#include "mutex.h"
#include "gtmio.h"
#include "mupipbckup.h"
#include "gtmimagename.h"
#include "mmseg.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "ftok_sems.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"

/* Include prototypes */
#include "mlk_shr_init.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "is_file_identical.h"
#include "repl_instance.h"

#include "util.h"
#include "dbfilop.h"
#include "gvcst_protos.h"
#include "is_raw_dev.h"
#include "gv_match.h"
#include "do_semop.h"
#include "gvcmy_open.h"
#include "wcs_sleep.h"
#include "do_shmat.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "shmpool.h"
#include "gtm_permissions.h"
#include "wbox_test_init.h"
#include "gtmcrypt.h"
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "db_snapshot.h"
#include "lockconst.h"	/* for LOCK_AVAILABLE */
#include "recover_truncate.h"
#include "get_fs_block_size.h"
#include "add_inter.h"
#include "dpgbldir.h"
#include "tp_change_reg.h"
#include "mu_gv_cur_reg_init.h"

#define REQRUNDOWN_TEXT		"semid is invalid but shmid is valid or at least one of sem_ctime or shm_ctime are non-zero"
#define MAX_ACCESS_SEM_RETRIES	2	/* see comment below where this macro is used for why it needs to be 2 */

#define RTS_ERROR(...)		rts_error_csa(CSA_ARG(csa) __VA_ARGS__)
#define SEND_MSG(...)		send_msg_csa(CSA_ARG(csa) __VA_ARGS__)

#define SS_INFO_INIT(CSA)												\
MBSTART {														\
	shm_snapshot_ptr_t	ss_shm_ptr;										\
	node_local_ptr_t	lcl_cnl;										\
															\
	lcl_cnl = CSA->nl;												\
	lcl_cnl->ss_shmid = INVALID_SHMID;										\
	lcl_cnl->ss_shmcycle = 0;											\
	CLEAR_SNAPSHOTS_IN_PROG(lcl_cnl);										\
	lcl_cnl->num_snapshots_in_effect = 0;										\
	SET_LATCH_GLOBAL(&lcl_cnl->snapshot_crit_latch, LOCK_AVAILABLE);						\
	assert(1 == MAX_SNAPSHOTS); /* To ensure that we revisit this whenever multiple snapshots is implemented */	\
	ss_shm_ptr = (shm_snapshot_ptr_t)(SS_GETSTARTPTR(CSA));								\
	SS_DEFAULT_INIT_POOL(ss_shm_ptr);										\
} MBEND

/* If a current value doesn't match the saved value used in the original calculation revert to the original */
#define DSE_VERIFY_AND_RESTORE(CSA, TSD, VAR)						\
MBSTART {										\
	assert(IS_DSE_IMAGE);								\
	if (CSA->nl->saved_##VAR != TSD->VAR)						\
	{										\
		gtm_putmsg_csa(CSA_ARG(CSA) VARLSTCNT(6) ERR_NLRESTORE, 4,		\
				LEN_AND_LIT(#VAR), TSD->VAR, CSA->nl->saved_##VAR);	\
		TSD->VAR = CSA->nl->saved_##VAR;					\
	}										\
} MBEND

#define GTM_ATTACH_CHECK_ERROR												\
MBSTART {														\
	if (-1 == status_l)												\
	{														\
		RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),						\
			  ERR_TEXT, 2, LEN_AND_LIT("Error attaching to database shared memory"), errno);		\
	}														\
} MBEND

#define GTM_ATTACH_SHM													\
MBSTART {														\
	status_l = (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)do_shmat(udi->shmid, 0, SHM_RND));			\
	GTM_ATTACH_CHECK_ERROR;												\
	csa->nl = (node_local_ptr_t)csa->db_addrs[0];									\
} MBEND

/* These values may potentially be used by DSE to calculate various offsets into the shared memory layout.
 * Note that blk_size does not affect the offset calculation for MM. */
#define GTM_CACHE_INTO_SHM(CSA, TSD)					\
MBSTART {								\
	CSA->nl->saved_acc_meth		= TSD->acc_meth;		\
	CSA->nl->saved_lock_space_size	= TSD->lock_space_size;		\
	CSA->nl->saved_blk_size		= TSD->blk_size;		\
	CSA->nl->saved_jnl_buffer_size	= TSD->jnl_buffer_size;		\
} MBEND

#define GTM_ATTACH_SHM_AND_CHECK_VERS(VERMISMATCH, SHM_SETUP_OK)								\
MBSTART {															\
	GTM_ATTACH_SHM;														\
	/* The following checks for GDS_LABEL_GENERIC and  gtm_release_name ensure that the shared memory under consideration	\
	 * is valid.  If shared memory is already initialized, do VERMISMATCH check BEFORE referencing any other fields in	\
	 * shared memory.													\
	 */															\
	VERMISMATCH = FALSE;													\
	SHM_SETUP_OK = FALSE;													\
	if (!MEMCMP_LIT(csa->nl->label, GDS_LABEL_GENERIC))									\
	{															\
		if (memcmp(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1))					\
		{	/* Copy csa->nl->now_running into a local variable before passing to rts_error due to the following	\
			 * issue:												\
			 * In VMS, a call to rts_error copies only the error message and its arguments (as pointers) and	\
			 *  transfers control to the topmost condition handler which is dbinit_ch() in this case. dbinit_ch()	\
			 *  does a PRN_ERROR only for SUCCESS/INFO (VERMISMATCH is neither of them) and in addition		\
			 *  nullifies csa->nl as part of its condition handling. It then transfers control to the next level	\
			 *  condition handler which does a PRN_ERROR but at that point in time, the parameter			\
			 *  csa->nl->now_running is no longer accessible and hence no \parameter substitution occurs (i.e. the	\
			 *  error message gets displayed with plain !ADs).							\
			 * In UNIX, this is not an issue since the first call to rts_error does the error message		\
			 *  construction before handing control to the topmost condition handler. But it does not hurt to do	\
			 *  the copy.												\
			 */													\
			assert(strlen(csa->nl->now_running) < SIZEOF(now_running));						\
			memcpy(now_running, csa->nl->now_running, SIZEOF(now_running));						\
			now_running[SIZEOF(now_running) - 1] = '\0'; /* protection against bad csa->nl->now_running values */	\
			VERMISMATCH = TRUE;											\
		} else														\
			SHM_SETUP_OK = TRUE;											\
	}															\
} MBEND

#define GTM_VERMISMATCH_ERROR												\
MBSTART {														\
	if (!vermismatch_already_printed)										\
	{														\
		vermismatch_already_printed = TRUE;									\
		RTS_ERROR(VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len, gtm_release_name,	\
			  LEN_AND_STR(now_running));									\
	}														\
} MBEND

#define INIT_DB_ENCRYPTION_IF_NEEDED(DO_CRYPT_INIT, INIT_STATUS, REG, CSA, TSD, CRYPT_WARNING)				\
MBSTART {														\
	int	fn_len;													\
	char	*fn;													\
															\
	if (DO_CRYPT_INIT)												\
	{														\
		fn = (char *)(REG->dyn.addr->fname);									\
		fn_len = REG->dyn.addr->fname_len;									\
		if (0 == INIT_STATUS)											\
			INIT_DB_OR_JNL_ENCRYPTION(CSA, TSD, fn_len, fn, INIT_STATUS);					\
	}														\
	DO_ERR_PROC_ENCRYPTION_IF_NEEDED(REG, DO_CRYPT_INIT, INIT_STATUS, CRYPT_WARNING);				\
} MBEND
#define INIT_PROC_ENCRYPTION_IF_NEEDED(CSA, DO_CRYPT_INIT, INIT_STATUS)							\
MBSTART {														\
	if (DO_CRYPT_INIT)												\
		INIT_PROC_ENCRYPTION(CSA, INIT_STATUS);									\
} MBEND

#define DO_ERR_PROC_ENCRYPTION_IF_NEEDED(REG, DO_CRYPT_INIT, INIT_STATUS, CRYPT_WARNING)				\
MBSTART {														\
	int	fn_len;													\
	char	*fn;													\
															\
	if (DO_CRYPT_INIT && (0 != INIT_STATUS) && !(CRYPT_WARNING))							\
	{														\
		fn = (char *)(REG->dyn.addr->fname);									\
		fn_len = REG->dyn.addr->fname_len;									\
		if (IS_GTM_IMAGE || mu_reorg_encrypt_in_prog)								\
		{													\
			GTMCRYPT_REPORT_ERROR(INIT_STATUS, rts_error, fn_len, fn);					\
		} else													\
		{													\
			GTMCRYPT_REPORT_ERROR(MAKE_MSG_WARNING(INIT_STATUS), gtm_putmsg, fn_len, fn);			\
			CRYPT_WARNING = TRUE;										\
		}													\
	}														\
} MBEND

#define READ_DB_FILE_HEADER(REG, TSD, ERR_RET)								\
MBSTART {												\
	file_control    	*fc;									\
	gd_segment		*seg;									\
	mstr			*gld_str;								\
	unix_db_info    	*udi;									\
	uint4			fsb_size;								\
	gd_region		*baseDBreg;								\
													\
	seg = REG->dyn.addr;										\
	fc = seg->file_cntl;										\
	fc->op = FC_READ;										\
	fc->op_buff = (sm_uc_ptr_t)TSD;									\
	fc->op_pos = 1;											\
	fc->op_len = SGMNT_HDR_LEN;									\
	dbfilop(fc);											\
	ERR_RET = 0;											\
	if (IS_AIO_DBGLDMISMATCH(seg, TSD))								\
	{	/* Copy tsd setting for asyncio into seg and retry "dbfilopn"/"db_init"			\
		 * sequence in caller "gvcst_init" by returning with an error.				\
		 */											\
		COPY_AIO_SETTINGS(seg, TSD);	/* copies from TSD to seg */				\
		ERR_RET = ERR_DBGLDMISMATCH;								\
	}												\
	if (IS_RDBF_STATSDB(TSD) && !IS_STATSDB_REGNAME(REG))						\
	{	/* This is a case where the file header says it is a statsDB but the region		\
		 * name is not lower case as it should be for a statsDB. Not supported.			\
		 */											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(REG));	\
	}												\
	if (!IS_RDBF_STATSDB(TSD) && IS_STATSDB_REGNAME(REG))						\
	{	/* This is a case where the file header does NOT say it is a statsDB but the region	\
		 * name is lower case as it should be for a statsDB. This is also an illegal		\
		 * combination so give an appropriate error.						\
		 */											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSTATSDB, 4, DB_LEN_STR(REG),		\
		 	      REG_LEN_STR(REG));							\
	}												\
	if (TSD->asyncio)										\
	{	/* AIO = ON, implies we need to use O_DIRECT.						\
		 * Check for db vs fs blksize alignment issues.						\
		 */											\
		udi = FC2UDI(fc);									\
		fsb_size = get_fs_block_size(udi->fd);							\
		if (0 != (TSD->blk_size % fsb_size))							\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBBLKSIZEALIGN, 4,			\
						DB_LEN_STR(REG), TSD->blk_size, fsb_size);		\
	}												\
} MBEND

#define READ_DB_FILE_MASTERMAP(REG, CSD)		\
MBSTART {						\
	file_control    	*fc;			\
							\
	fc = REG->dyn.addr->file_cntl;			\
	fc->op = FC_READ;				\
	fc->op_buff = MM_ADDR(CSD);			\
	fc->op_len = MASTER_MAP_SIZE(CSD);		\
	fc->op_pos = MM_BLOCK;				\
	dbfilop(fc);					\
} MBEND

/* Depending on whether journaling and/or replication was enabled at the time of the crash,
 * print REQRUNDOWN, REQRECOV, or REQROLLBACK error message.
 */
#define PRINT_CRASH_MESSAGE(CNT, ARG, ...)							\
MBSTART {											\
	if (JNL_ENABLED(tsd))									\
	{											\
		if (REPL_ENABLED(tsd) && tsd->jnl_before_image)					\
			RTS_ERROR(VARLSTCNT(10 + CNT) ERR_REQROLLBACK, 4, DB_LEN_STR(reg),	\
				LEN_AND_STR((ARG)->machine_name), __VA_ARGS__);			\
		else										\
			RTS_ERROR(VARLSTCNT(10 + CNT) ERR_REQRECOV, 4, DB_LEN_STR(reg),		\
				LEN_AND_STR((ARG)->machine_name), __VA_ARGS__);			\
	} else											\
		RTS_ERROR(VARLSTCNT(10 + CNT) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), 		\
			LEN_AND_STR((ARG)->machine_name), __VA_ARGS__);				\
} MBEND

#define	START_SEM_TIMERS(SEM_STACKTRACE_TIME, SEM_TIMEDOUT, MAX_HRTBT_DELTA)		\
MBSTART {										\
	TIMEOUT_INIT(SEM_TIMEDOUT, MAX_HRTBT_DELTA * MILLISECS_IN_SEC);			\
	TIMEOUT_INIT(SEM_STACKTRACE_TIME, MAX_HRTBT_DELTA * MILLISECS_IN_SEC / 2);	\
} MBEND

#define	CANCEL_SEM_TIMERS(SEM_STACKTRACE_TIME, SEM_TIMEDOUT)	\
MBSTART {							\
	TIMEOUT_DONE(SEM_STACKTRACE_TIME);			\
	TIMEOUT_DONE(SEM_TIMEDOUT);				\
} MBEND

#define	RETURN_IF_BYPASSED(BYPASSED_FTOK, INDEFINITE_WAIT, SEM_STACKTRACE_TIME, SEM_TIMEDOUT)	\
	RETURN_IF_ERROR(BYPASSED_FTOK, -1, INDEFINITE_WAIT, SEM_STACKTRACE_TIME, SEM_TIMEDOUT)

#define	RETURN_IF_ERROR(COND, RETVAL, INDEFINITE_WAIT, SEM_STACKTRACE_TIME, SEM_TIMEDOUT)	\
MBSTART {											\
	if (COND)										\
	{											\
		if (!INDEFINITE_WAIT)								\
			CANCEL_SEM_TIMERS(SEM_STACKTRACE_TIME, SEM_TIMEDOUT);			\
		REVERT;										\
		return RETVAL; /* "gvcst_init" does cleanup and retries the "db_init" call */	\
	}											\
} MBEND

#define MU_GV_CUR_REG_FREE(TMP_REG, SAVE_REG)		\
{							\
	GBLREF	gd_region	*gv_cur_region;		\
							\
	SAVE_REG = gv_cur_region;			\
	gv_cur_region = TMP_REG;			\
	mu_gv_cur_reg_free();				\
	gv_cur_region = SAVE_REG;			\
}

GBLREF	int4			gtm_fullblockwrites;	/* Do full (not partial) database block writes */
GBLREF	boolean_t		is_src_server;
GBLREF  boolean_t               mupip_jnl_recover;
GBLREF	gd_region		*gv_cur_region, *db_init_region;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	gd_addr			*gd_header;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	node_local_ptr_t	locknl;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF  uint4                   process_id;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			mu_reorg_encrypt_in_prog;
GBLREF	int			pool_init;
GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	mstr			extnam_str;
GBLREF	mval			dollar_zgbldir;
#ifndef MUTEX_MSEM_WAKE
GBLREF	int 	mutex_sock_fd;
#endif

LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

OS_PAGE_SIZE_DECLARE

error_def(ERR_BADDBVER);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBBLKSIZEALIGN);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBFILERR);
error_def(ERR_DBFLCORRP);
error_def(ERR_DBGLDMISMATCH);
error_def(ERR_DBIDMISMATCH);
error_def(ERR_DBNAMEMISMATCH);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBSHMNAMEDIFF);
error_def(ERR_FILENOTFND);
error_def(ERR_HOSTCONFLICT);
error_def(ERR_INVOPNSTATSDB);
error_def(ERR_INVSTATSDB);
error_def(ERR_JNLBUFFREGUPD);
error_def(ERR_MMNODYNUPGRD);
error_def(ERR_NLMISMATCHCALC);
error_def(ERR_NLRESTORE);
error_def(ERR_REQRECOV);
error_def(ERR_REQROLLBACK);
error_def(ERR_REQRUNDOWN);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

gd_region *dbfilopn(gd_region *reg)
{
	unix_db_info		*tmp_udi, *udi;
	parse_blk		pblk;
	mstr			file;
	char			*fnptr, fbuff[MAX_FBUFF + 1], tmpbuff[MAX_FBUFF + 1];
	struct stat		buf;
	gd_region		*prev_reg, *save_gv_cur_region, *tmp_reg;
	gd_segment		*seg;
	int			status;
	boolean_t		raw;
	boolean_t		open_read_only;
	int			stat_res, rc, save_errno;
	sgmnt_addrs		*csa;
	sgmnt_data		tsdbuff;
	sgmnt_data_ptr_t        tsd;
	file_control    	*fc;
	unsigned char		cstatus;
	boolean_t		ftok_counter_halted;
	ZOS_ONLY(int		realfiletag;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	seg = reg->dyn.addr;
	assert(IS_ACC_METH_BG_OR_MM(seg->acc_meth));
	FILE_CNTL_INIT_IF_NULL(seg);
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	file.addr = (char *)seg->fname;
	file.len = seg->fname_len;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = fbuff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	memcpy(fbuff, file.addr, file.len);
	*(fbuff + file.len) = '\0';
	if (is_raw_dev(fbuff))
	{
		raw = TRUE;
		pblk.def1_buf = DEF_NODBEXT;
		pblk.def1_size = SIZEOF(DEF_NODBEXT) - 1;
	} else
	{
		raw = FALSE;
		pblk.def1_buf = DEF_DBEXT;
		pblk.def1_size = SIZEOF(DEF_DBEXT) - 1;
	}
	status = parse_file(&file, &pblk);
	/* When opening a reservedDB type database file, it is possible that it does not (yet) exist. In that case,
	 * make a call out to create it.
	 */
	if (!(status & 1))
	{
		if (!IS_GTCM_GNP_SERVER_IMAGE)
		{
			free(seg->file_cntl->file_info);
			free(seg->file_cntl);
			seg->file_cntl = NULL;
		}
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
	}
	assert(((int)pblk.b_esl + 1) <= SIZEOF(seg->fname));
	memcpy(seg->fname, pblk.buffer, pblk.b_esl);
	pblk.buffer[pblk.b_esl] = 0;
	seg->fname[pblk.b_esl] = 0;
	seg->fname_len = pblk.b_esl;
	if (pblk.fnb & F_HAS_NODE)
	{	/* Remote node specification given */
		assert(pblk.b_node && pblk.l_node[pblk.b_node - 1] == ':');
		gvcmy_open(reg, &pblk);
		return (gd_region *)-1L;
	}
	fnptr = (char *)seg->fname + pblk.b_node;
	udi->raw = raw;
	udi->fn = (char *)fnptr;
	/* If MUPIP JOURNAL -EXTRACT, then open db in read-only mode or else we could incorrectly reset crash field
	 * in journal file header as part of gds_rundown because we have the db open in read_write mode (GTM-8483).
	 */
	open_read_only = (jgbl.mur_extract && !jgbl.mur_update);
	for ( ; ; )
	{
		if (!open_read_only)
		{
			OPENFILE_DB(fnptr, O_RDWR, udi, seg);
			/* When opening an auto-create type of reservedDB database file, it is possible
			 * that it does not (yet) exist. In that case, make a call out to create it. Note that
			 * a statsdb (which is also an autodb) gets created in "gvcst_init" so skip that here.
			 */
			if ((FD_INVALID == udi->fd) && (ENOENT == errno) && IS_AUTODB_REG(reg) && !IS_STATSDB_REG(reg))
			{	/* We want to get a lock to ensure only one process does the create. But the lock needs to be
				 * based on an existing file ("ftok_sem_get" relies on that). So use "/" directory. That is
				 * guaranteed to exist on any system and will serve as a sync point for ANY/ALL autodb creates.
				 */
				/* First check if we have enough space to do ".new" suffix addition (done a little later) */
				if (ARRAYSIZE(seg->fname) <= seg->fname_len + STR_LIT_LEN(EXT_NEW))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				save_gv_cur_region = gv_cur_region;
				mu_gv_cur_reg_init();
				tmp_reg = gv_cur_region;
				gv_cur_region = save_gv_cur_region;
				tmp_reg->dyn.addr->fname[0] = '/';
				tmp_reg->dyn.addr->fname[1] = '\0';
				tmp_reg->dyn.addr->fname_len = 1;
				tmp_udi = FILE_INFO(tmp_reg);
				tmp_udi->fn = (char *)tmp_reg->dyn.addr->fname;
				if (!ftok_sem_get(tmp_reg, FALSE, GTM_ID, FALSE, &ftok_counter_halted))
				{
					MU_GV_CUR_REG_FREE(tmp_reg, save_gv_cur_region);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				}
				assert(!ftok_counter_halted);	/* since we specified FALSE as the 2nd parameter ("incr_cnt") */
				/* Now that we got the ftok lock, check if the file exists. If so skip the create. */
				OPENFILE_DB(fnptr, O_RDWR, udi, seg);
				if ((FD_INVALID == udi->fd) && (ENOENT == errno))
				{	/* File still does not exist. Create it. */
					/* If autodb file name is say "tmp.dat", point region to a temporary filename
					 * "tmp.dat_%YGTM" (suffix EXT_NEW). This is similar to the logic used in "cre_jnl_file"
					 * with ".mjl" and ".mjl_%YGTM" files. Create database "tmp.dat_%YGTM" and initialize it.
					 * And finally rename it to "tmp.dat". If we instead create "tmp.dat" and then fill it
					 * with GT.M db file header content, it is possible while this is happening, other
					 * processes see the file exist (OPENFILE_DB returns TRUE above) and proceed assuming the
					 * file is a valid GT.M database file when actually the creation is not yet done.
					 */
					MEMCPY_LIT(seg->fname + seg->fname_len, EXT_NEW);
					seg->fname_len += STR_LIT_LEN(EXT_NEW);
					seg->fname[seg->fname_len] = '\0';
					cstatus = gvcst_cre_autoDB(reg);
					if (EXIT_NRM != cstatus)
					{
						ftok_sem_release(tmp_reg, FALSE, FALSE);
						MU_GV_CUR_REG_FREE(tmp_reg, save_gv_cur_region);
						save_errno = TREF(mu_cre_file_openrc);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
					}
					memcpy(tmpbuff, seg->fname, seg->fname_len + 1); /* + 1 to include terminating '\0' */
					seg->fname_len -= STR_LIT_LEN(EXT_NEW);
					seg->fname[seg->fname_len] = '\0';
					/* Rename "tmp.dat_%YGTM" to "tmp.dat" */
					if (-1 == RENAME(tmpbuff, (char *)seg->fname))
					{
						save_errno = errno;
						ftok_sem_release(tmp_reg, FALSE, FALSE);
						MU_GV_CUR_REG_FREE(tmp_reg, save_gv_cur_region);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg),
														save_errno);
					}
				}
				ftok_sem_release(tmp_reg, FALSE, FALSE);
				MU_GV_CUR_REG_FREE(tmp_reg, save_gv_cur_region);
				/* DB file exists now - retry the open */
				OPENFILE_DB(fnptr, O_RDWR, udi, seg);
			}
			if (!udi->grabbed_access_sem)
			{	/* If the process already has standalone access, these fields are initialized in mu_rndwn_file */
				udi->ftok_semid = INVALID_SEMID;
				udi->semid = INVALID_SEMID;
				udi->shmid = INVALID_SHMID;
				udi->gt_sem_ctime = 0;
				udi->gt_shm_ctime = 0;
			}
			reg->read_only = FALSE;		/* maintain csa->read_write simultaneously */
			csa->read_write = TRUE;		/* maintain reg->read_only simultaneously */
			csa->orig_read_write = TRUE;
		}
		if (open_read_only || (FD_INVALID == udi->fd))
		{
			OPENFILE_DB(fnptr, O_RDONLY, udi, seg);
			if (FD_INVALID == udi->fd)
			{
				save_errno = errno;
				if (!IS_GTCM_GNP_SERVER_IMAGE)
				{
					free(seg->file_cntl->file_info);
					free(seg->file_cntl);
					seg->file_cntl = NULL;
				}
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
			}
			reg->read_only = TRUE;		/* maintain csa->read_write simultaneously */
			csa->read_write = FALSE;	/* maintain reg->read_only simultaneously */
			csa->orig_read_write = FALSE;
		}
		if (!reg->owning_gd->is_dummy_gbldir && (pool_init || !jnlpool_init_needed || !CUSTOM_ERRORS_AVAILABLE))
			break;
		/* Caller created a dummy region (not a region from a gld). So determine asyncio & acc_meth setting
		 * from db file header and copy that to segment to avoid DBGLDMISMATCH error later in "db_init".
		 * Or need to check replication state to decide if an early call to jnlpool_init is needed in gvcst_init
		 */
		tsd = udi->fd_opened_with_o_direct ? (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned : &tsdbuff;
		/* If O_DIRECT, use aligned buffer */
		fc = seg->file_cntl;
		fc->op = FC_READ;
		fc->op_buff = (sm_uc_ptr_t)tsd;
		fc->op_pos = 1;
		fc->op_len = SGMNT_HDR_LEN;
		dbfilop(fc);
		if (!pool_init && jnlpool_init_needed && CUSTOM_ERRORS_AVAILABLE)
			csa->repl_state = tsd->repl_state;	/* needed in gvcst_init */
		if (!reg->owning_gd->is_dummy_gbldir)
			break;
		if (!IS_AIO_DBGLDMISMATCH(seg, tsd))
			break;
		CLOSEFILE_RESET(udi->fd, rc);	/* close file and reopen it with correct asyncio setting */
		COPY_AIO_SETTINGS(seg, tsd);	/* copies from tsd to seg */
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(udi->fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_SEND_MSG(fnptr, errno, realfiletag, TAG_BINARY);
#	endif
	/* Now that the file has been opened, use FSTAT on the fd instead of STAT on the file name. The latter is risky for AUTODB
	 * since it could be concurrently deleted whereas the fd would still be accessible (delete-pending).
	 */
	FSTAT_FILE(udi->fd, &buf, stat_res);
        if (-1 == stat_res)
        {
        	save_errno = errno;
        	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
        }
	set_gdid_from_stat(&udi->fileid, &buf);
	if (prev_reg = gv_match(reg))
	{
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		free(seg->file_cntl->file_info);
		free(seg->file_cntl);
		seg->file_cntl = NULL;
		return prev_reg;
	}
	SYNC_OWNING_GD(reg);
	return reg;
}

void dbsecspc(gd_region *reg, sgmnt_data_ptr_t csd, gtm_uint64_t *sec_size)
{
	gtm_uint64_t	tmp_sec_size;

	/* Ensure that all the various sections that the shared memory contains are actually
	 * aligned at the OS_PAGE_SIZE boundary
	 */
	INIT_NUM_CRIT_ENTRY_IF_NEEDED(csd);
	assert(MIN_NODE_LOCAL_SPACE <= NODE_LOCAL_SPACE(csd));
	assert(0 == NODE_LOCAL_SPACE(csd) % OS_PAGE_SIZE);
	assert(0 == LOCK_SPACE_SIZE(csd) % OS_PAGE_SIZE);
	assert(0 == JNL_SHARE_SIZE(csd) % OS_PAGE_SIZE);
	assert(0 == SHMPOOL_SECTION_SIZE % OS_PAGE_SIZE);
	assert(0 == CACHE_CONTROL_SIZE(csd) % OS_PAGE_SIZE);
	/* First compute the size based on sections common to both MM and BG */
	tmp_sec_size = NODE_LOCAL_SPACE(csd) + JNL_SHARE_SIZE(csd) + SHMPOOL_SECTION_SIZE + LOCK_SPACE_SIZE(csd);
	/* Now, add sections specific to MM and BG */
	if (dba_mm == reg->dyn.addr->acc_meth)
		tmp_sec_size += SIZEOF_FILE_HDR(csd);
	else
	{
		assertpro(dba_bg == reg->dyn.addr->acc_meth);
		tmp_sec_size += CACHE_CONTROL_SIZE(csd) + (LOCK_BLOCK(csd) * DISK_BLOCK_SIZE);
	}
	ADJUST_SHM_SIZE_FOR_HUGEPAGES(tmp_sec_size, *sec_size);	/* *sec_size is adjusted size */
	return;
}

int db_init(gd_region *reg, boolean_t ok_to_bypass)
{
	boolean_t       	is_bg, read_only, need_stacktrace, have_standalone_access;
	boolean_t		shm_setup_ok = FALSE, vermismatch = FALSE, vermismatch_already_printed = FALSE;
	boolean_t		new_shm_ipc, replinst_mismatch, need_shmctl, need_semctl;
	boolean_t		gld_do_crypt_init, db_do_crypt_init, semop_success, statsdb_off;
	char            	machine_name[MAX_MCNAMELEN];
	int			gethostname_res, stat_res, user_id, group_id, perm, save_udi_semid;
	int4            	status, dblksize, save_errno, wait_time, loopcnt, sem_pid;
	sm_long_t       	status_l;
	sgmnt_addrs     	*csa;
	node_local_ptr_t	cnl;
	sgmnt_data		tsdbuff;
	sgmnt_data_ptr_t        csd, tsd;
	jnlpool_addrs_ptr_t	save_jnlpool, local_jnlpool;
	replpool_identifier	replpool_id;
	unsigned int		full_len;	/* for REPL_INST_AVAILABLE */
	gd_id			replfile_gdid, *tmp_gdid;
	boolean_t		need_jnlpool_setup;
	struct sembuf   	sop[3];
	struct stat     	stat_buf;
	union semun		semarg;
	struct semid_ds		semstat;
	struct shmid_ds         shmstat;
	struct statvfs		dbvfs;
	uint4           	fbwsize, sopcnt;
	uint4			max_hrtbt_delta;
	boolean_t		sem_timedout, *sem_timedoutp = NULL,
				sem_stacktrace_time, *sem_stacktrace_timep = NULL,
				indefinite_wait = TRUE;
	unix_db_info    	*udi;
	char			now_running[MAX_REL_NAME];
	int			err_ret, init_status;
	gtm_uint64_t 		sec_size, mmap_sz;
	semwait_status_t	retstat;
	struct perm_diag_data	pdd;
	boolean_t		bypassed_ftok = FALSE, bypassed_access = FALSE, dummy_ftok_counter_halted,
				ftok_counter_halted, access_counter_halted, incr_cnt;
	int			jnl_buffer_size;
	char			s[JNLBUFFUPDAPNDX_SIZE];	/* JNLBUFFUPDAPNDX_SIZE is defined in jnl.h */
	char			*syscall;
	void			*mmapaddr;
	int			ret, secshrstat;
	boolean_t		crypt_warning;
	gd_region		*baseDBreg;
	sgmnt_addrs		*baseDBcsa;
	node_local_ptr_t	baseDBnl;
	DEBUG_ONLY(int		i);
	DEBUG_ONLY(char 	*ptr);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_NOUNWIND(dbinit_ch);
	db_init_region = reg;	/* initialized for dbinit_ch */
	assert(INTRPT_IN_GVCST_INIT == intrpt_ok_state); /* we better be called from gvcst_init */
	read_only = reg->read_only;
	udi = FILE_INFO(reg);
	tsd = udi->fd_opened_with_o_direct
			? (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned : &tsdbuff; /* If O_DIRECT, use aligned buffer */
	memset(machine_name, 0, SIZEOF(machine_name));
	csa = &udi->s_addrs;
	assert(!mutex_per_process_init_pid || mutex_per_process_init_pid == process_id);
	if (!mutex_per_process_init_pid)
		mutex_per_process_init();
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, gethostname_res))
		RTS_ERROR(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Unable to get the hostname"), errno);
	if (WBTEST_ENABLED(WBTEST_TAMPER_HOSTNAME))
		STRCPY(machine_name, "s_i_l_l_y");
	assert(strlen(machine_name) < MAX_MCNAMELEN);
	assert(NULL == csa->hdr);	/* dbinit_ch relies on this to unmap the db (if mm) */
	assert((NULL == csa->db_addrs[0]) && (NULL == csa->db_addrs[1]));
	assert((NULL == csa->lock_addrs[0]) && (NULL == csa->lock_addrs[1]));
	reg->opening = TRUE;
	assert(0 <= udi->fd); /* database file must have been already opened by "dbfilopn" done from "gvcst_init" */
	FSTAT_FILE(udi->fd, &stat_buf, stat_res); /* get the stats for the database file */
	if (-1 == stat_res)
		RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
	/* Setup new group and permissions if indicated by the security rules. */
	if (!gtm_permissions(&stat_buf, &user_id, &group_id, &perm, PERM_IPC, &pdd))
	{
		SEND_MSG(VARLSTCNT(6 + PERMGENDIAG_ARG_COUNT)
			ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("ipc resources"), RTS_ERROR_STRING(udi->fn),
			PERMGENDIAG_ARGS(pdd));
		RTS_ERROR(VARLSTCNT(6 + PERMGENDIAG_ARG_COUNT)
			ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("ipc resources"), RTS_ERROR_STRING(udi->fn),
			PERMGENDIAG_ARGS(pdd));
	}
	/* if the process has standalone access, it will have udi->grabbed_access_sem set to TRUE at
	 * this point. Note that down in a local variable as the udi->grabbed_access_sem will be set
	 * to TRUE even for non-standalone access below and hence we can't rely on that later to determine if the process had
	 * standalone access or not when it entered this function.
	 */
	have_standalone_access = udi->grabbed_access_sem;
	init_status = 0;
	crypt_warning = FALSE;
	udi->shm_deleted = udi->sem_deleted = FALSE;
	if (!have_standalone_access)
	{
		gld_do_crypt_init = (IS_ENCRYPTED(reg->dyn.addr->is_encrypted) && !IS_LKE_IMAGE);
		assert(!TO_BE_ENCRYPTED(reg->dyn.addr->is_encrypted));
		INIT_PROC_ENCRYPTION_IF_NEEDED(csa, gld_do_crypt_init, init_status); /* heavyweight so do it before ftok */
		max_hrtbt_delta = TREF(dbinit_max_delta_secs);
		indefinite_wait = (INDEFINITE_WAIT_ON_EAGAIN == max_hrtbt_delta);
		if (!indefinite_wait)
		{
			START_SEM_TIMERS(sem_stacktrace_time, sem_timedout, max_hrtbt_delta);
			sem_timedoutp = &sem_timedout;
			sem_stacktrace_timep = &sem_stacktrace_time;
		}
		/* If the header is uninitalized we assume that mumps_can_bypass is TRUE. After we read the header to confirm our
		 * assumption. If we turn out to be wrong, we error out.
		 */
		bypassed_ftok = ok_to_bypass;
		if (!ftok_sem_get2(reg, sem_stacktrace_timep, sem_timedoutp, &retstat, &bypassed_ftok, &ftok_counter_halted, TRUE))
			ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "ftok");
		assert(ok_to_bypass || !bypassed_ftok);
		assert(udi->grabbed_ftok_sem || bypassed_ftok);
		if (bypassed_ftok)
			SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("FTOK bypassed at database initialization"));
		/* At this point we have ftok_semid semaphore based on ftok key (exception is if "bypassed_ftok" is TRUE).
		 * Any ftok conflicted region will block at this point. For example, if a.dat and b.dat both have same ftok
		 * and process A tries to open or close a.dat and process B tries to open or close b.dat, even though the
		 * database accesses don't conflict, the first one to control the ftok semaphore blocks (makes wait) the other(s).
		 */
		READ_DB_FILE_HEADER(reg, tsd, err_ret); /* file already opened by "dbfilopn" done from "gvcst_init" */
		RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait, sem_stacktrace_time, sem_timedout);
		DO_BADDBVER_CHK(reg, tsd); /* need to do BADDBVER check before de-referencing shmid and semid from file header
					    * as they could be at different offsets if the database is V4-format */
		db_do_crypt_init = (USES_ENCRYPTION(tsd->is_encrypted) && !IS_LKE_IMAGE);
		if (gld_do_crypt_init != db_do_crypt_init)
		{	/* Encryption setting different between global directory and database file header */
			if (db_do_crypt_init)
			{	/* Encryption is turned on in the file header. Need to do encryption initialization. Release ftok
				 * as initialization is heavy-weight. Decrement counter so later increment is correct.
				 */
				assert(udi->counter_ftok_incremented == !ftok_counter_halted);
				/* If we are going to do a "ftok_sem_release", do not decrement the counter that was
				 * potentially incremented in the previous call to "ftok_sem_get2". Hence the 2nd parameter FALSE.
				 */
				if (!bypassed_ftok && !ftok_sem_release(reg, FALSE, FALSE))
					RTS_ERROR(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				INIT_PROC_ENCRYPTION_IF_NEEDED(csa, db_do_crypt_init, init_status); /* redo initialization */
				bypassed_ftok = ok_to_bypass;
				if (!indefinite_wait)
				{	/* restart timer to reflect time lost in encryption initialization */
					CANCEL_SEM_TIMERS(sem_stacktrace_time, sem_timedout);
					START_SEM_TIMERS(sem_stacktrace_time, sem_timedout, max_hrtbt_delta);
				}
				/* Since this is the second call to "ftok_sem_get2", do not increment counter.
				 * Hence FALSE as last parameter.
				 */
				if (!ftok_sem_get2(reg, sem_stacktrace_timep, sem_timedoutp, &retstat, &bypassed_ftok,
									&dummy_ftok_counter_halted, FALSE))
					ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "ftok");
				assert(ok_to_bypass || !bypassed_ftok);
				assert(!udi->counter_ftok_incremented);
				udi->counter_ftok_incremented = !ftok_counter_halted;	/* restore to first "ftok_sem_get2" call */
				assert(udi->grabbed_ftok_sem || bypassed_ftok);
				if (bypassed_ftok)
					SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2,
						 LEN_AND_LIT("bypassed at database encryption initialization"));
				/* Re-read now possibly stale file header and redo the above based on the new header info */
				READ_DB_FILE_HEADER(reg, tsd, err_ret);
				RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait, sem_stacktrace_time, sem_timedout);
				DO_BADDBVER_CHK(reg, tsd); /* need to do BADDBVER check before de-referencing shmid and semid from
							    * file header as they could be at different offsets if the database is
							    * V4-format
							    */
				db_do_crypt_init = (USES_ENCRYPTION(tsd->is_encrypted) && !IS_LKE_IMAGE);
			} /* else encryption is turned off in the file header. Continue as-is. Any encryption initialization done
			   * before is discarded
			   */
		}
		if (mu_reorg_encrypt_in_prog)
			INIT_DB_ENCRYPTION_IF_NEEDED(db_do_crypt_init, init_status, reg, csa, tsd, crypt_warning);
		else
			DO_ERR_PROC_ENCRYPTION_IF_NEEDED(reg, db_do_crypt_init, init_status, crypt_warning);
		if (WBTEST_ENABLED(WBTEST_HOLD_ONTO_FTOKSEM_IN_DBINIT))
		{
			DBGFPF((stderr, "Holding the ftok semaphore.. Sleeping for 30 seconds\n"));
			LONG_SLEEP(30);
			DBGFPF((stderr, "30 second sleep exhausted.. continuing with rest of db_init..\n"));
		}
		access_counter_halted = FALSE;
		/* This loop is primarily to take care of the case where udi->semid is non-zero in the file header and did exist
		 * when we read the file header but was concurrently deleted by a process holding standalone access on the database
		 * file (that does not hold the ftok semaphore) e.g. "db_ipcs_reset". We do not expect the semaphore to be deleted
		 * more than once because for another delete to happen, the deleting process must first open the database and
		 * for that it needs to go through "mu_rndwn_file"/"db_init" which needs the ftok lock that we currently hold.
		 * Note that it is possible we don't hold the ftok lock (e.g. DSE/LKE) but in that case, any errors cause us
		 * to retry in the caller ("gvcst_init") where the final retry is always with holding the ftok lock.
		 */
		for (loopcnt = 0; MAX_ACCESS_SEM_RETRIES > loopcnt; loopcnt++)
		{
			CSD2UDI(tsd, udi); /* sets udi->semid/shmid/sem_ctime/shm_ctime from file header */
			/* We did not create a new ipc resource */
			udi->sem_created = udi->shm_created = FALSE;
			/* In the below code, use RETURN_IF_BYPASSED macro to retry the "db_init" (with "ok_to_bypass" == FALSE)
			 * if ever we are about to issue an error or create a sem/shm, all of which should happen only when
			 * someone has the ftok lock (i.e. have not bypassed that part above).
			 */
			if (INVALID_SEMID == udi->semid)
			{	/* Access control semaphore does not exist. Create one. But if we have bypassed the ftok lock,
				 * then return (sem/shm can only be created by someone with the ftok lock).
				 */
				RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time, sem_timedout);
				if (!bypassed_ftok)
					INIT_DB_ENCRYPTION_IF_NEEDED(db_do_crypt_init, init_status, reg, csa, tsd, crypt_warning);
				if (0 != udi->gt_sem_ctime || INVALID_SHMID != udi->shmid || 0 != udi->gt_shm_ctime)
				{	/* We must have somthing wrong in protocol or, code, if this happens. */
					assert(FALSE);
					PRINT_CRASH_MESSAGE(0, tsd, ERR_TEXT, 2, LEN_AND_STR(REQRUNDOWN_TEXT));
				}
				/* Create new semaphore using IPC_PRIVATE. System guarantees a unique id. */
				if (-1 == (udi->semid = semget(IPC_PRIVATE, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
				{
					udi->semid = INVALID_SEMID;
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_TEXT, 2, LEN_AND_LIT("Error with database control semget"), errno);
				}
				udi->shmid = INVALID_SHMID; /* reset shmid so dbinit_ch does not get confused in case we go there */
				udi->sem_created = TRUE;
				udi->shm_created = !tsd->read_only;
				/* change group and permissions */
				semarg.buf = &semstat;
				if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT1"), errno);
				if ((INVALID_UID != user_id) && (user_id != semstat.sem_perm.uid))
				{
					semstat.sem_perm.uid = user_id;
					need_semctl = TRUE;
				}
				if ((INVALID_GID != group_id) && (group_id != semstat.sem_perm.gid))
				{
					semstat.sem_perm.gid = group_id;
					need_semctl = TRUE;
				}
				if (semstat.sem_perm.mode != perm)
				{
					semstat.sem_perm.mode = perm;
					need_semctl = TRUE;
				}
				if (need_semctl && (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_SET, semarg)))
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_SET"), errno);
				SET_GTM_ID_SEM(udi->semid, status);
				if (-1 == status)
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl SETVAL"), errno);
				/* WARNING: Because SETVAL changes sem_ctime, we must NOT do any SETVAL after this one; code here
				 * and elsewhere uses IPC_STAT to get sem_ctime and relies on sem_ctime as the creation time of the
				 * semaphore.
				 */
				semarg.buf = &semstat;
				if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT2"), errno);
				tsd->gt_sem_ctime.ctime = udi->gt_sem_ctime = semarg.buf->sem_ctime;
			} else
			{	/* "semid" already exists. Need to lock it. Before that do sanity check on "semid" and "shmid" */
				if (INVALID_SHMID != udi->shmid)
				{
					if (WBTEST_ENABLED(WBTEST_HOLD_FTOK_UNTIL_BYPASS))
					{
						if ((4 * DB_COUNTER_SEM_INCR) == semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
						{	/* We are bypasser */
							DBGFPF((stderr, "Waiting for all processes to quit.\n"));
							while (1 < semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
								LONG_SLEEP(1);
						}
					}
					if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
					{
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						if (((MAX_ACCESS_SEM_RETRIES - 1) == loopcnt) || !(SHM_REMOVED(errno)))
							PRINT_CRASH_MESSAGE(1, tsd, ERR_TEXT, 2,
									    LEN_AND_LIT("Error with database control shmctl"),
									    errno);
						/* else */
						READ_DB_FILE_HEADER(reg, tsd, err_ret);
						RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait,
								sem_stacktrace_time, sem_timedout);
						continue;
					} else if (shmstat.shm_ctime != tsd->gt_shm_ctime.ctime)
					{
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						GTM_ATTACH_SHM_AND_CHECK_VERS(vermismatch, shm_setup_ok);
						if (vermismatch)
						{
							GTM_VERMISMATCH_ERROR;
						} else
						{
							PRINT_CRASH_MESSAGE(0, tsd, ERR_TEXT, 2,
								LEN_AND_LIT("IPC creation time indicates a probable prior crash"));
						}
					}
					semarg.buf = &semstat;
					if (-1 == semctl(udi->semid, DB_CONTROL_SEM, IPC_STAT, semarg))
					{	/* file header has valid semid but semaphore does not exist */
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						PRINT_CRASH_MESSAGE(1, tsd, ERR_TEXT, 2,
							LEN_AND_LIT("Error with database control semaphore (IPC_STAT)"), errno);
					} else if (semarg.buf->sem_ctime != tsd->gt_sem_ctime.ctime)
					{
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						GTM_ATTACH_SHM_AND_CHECK_VERS(vermismatch, shm_setup_ok);
						if (vermismatch)
						{
							GTM_VERMISMATCH_ERROR;
						} else
						{
							PRINT_CRASH_MESSAGE(0, tsd, ERR_TEXT, 2,
								LEN_AND_LIT("IPC creation time indicates a probable prior crash"));
						}
					}
				} else
				{	/* else "shmid" is NOT valid. This is possible if -
					 * (a) Another process is holding the access control semaphore for a longer duration of time
					 * but does NOT have the shared memory setup (MUPIP INTEG -FILE or MUPIP RESTORE).
					 *
					 * (b) If a process (like in (a)) were kill -15ed or -9ed and hence did not get a chance to
					 * do db_ipcs_reset which resets "semid"/"shmid" field in the file header to INVALID.
					 *
					 * In either case, try grabbing the semaphore. If not, wait (depending on the user specified
					 * wait time). Eventually, we will either get hold of the semaphore OR will error out.
					 */
					RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time, sem_timedout);
					udi->shm_created = !tsd->read_only; /* Need to create shared memory */
				}
			}
			incr_cnt = !read_only;
			/* We already have ftok semaphore of this region, so all we need is the access control semaphore */
			SET_GTM_SOP_ARRAY(sop, sopcnt, incr_cnt, (SEM_UNDO | IPC_NOWAIT));
			SEMOP(udi->semid, sop, sopcnt, status, NO_WAIT);
			if (-1 != status)
				break;
			save_errno = errno;
			assert(!udi->sem_created); /* If we created the semaphore, we should be able to do the semop */
			if ((EAGAIN == save_errno) || (ERANGE == save_errno))
			{
				if ((EAGAIN == save_errno) && (NO_SEMWAIT_ON_EAGAIN == TREF(dbinit_max_delta_secs)))
				{
					sem_pid = semctl(udi->semid, DB_CONTROL_SEM, GETPID);
					if (-1 != sem_pid)
					{
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						RTS_ERROR(VARLSTCNT(13) ERR_DBFILERR, 2, DB_LEN_STR(reg),
							ERR_SEMWT2LONG, 7, process_id, 0, LEN_AND_LIT("access control"),
								DB_LEN_STR(reg), sem_pid);
					} else
					{
						save_errno = errno;
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						if (!SEM_REMOVED(save_errno))
							RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
								ERR_SYSCALL, 5,	RTS_ERROR_LITERAL("semop()"), CALLFROM,
								save_errno);
					}
				} else
				{
					bypassed_access = ok_to_bypass;
					semop_success = do_blocking_semop(udi->semid, gtm_access_sem, sem_stacktrace_timep,
										sem_timedoutp, &retstat, reg,
										&bypassed_access, &access_counter_halted, TRUE);
					assert(ok_to_bypass || !bypassed_access);
					if (!semop_success)
					{
						RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time,	\
													sem_timedout);
						if (!SEM_REMOVED(retstat.save_errno))
							ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "access control");
						save_errno = retstat.save_errno;
					} else
					{
						if (bypassed_access)
							SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2,
								 LEN_AND_LIT("Access control bypassed at init"));
						save_errno = status = SS_NORMAL;
						incr_cnt = (incr_cnt && !access_counter_halted);
						break;
					}
				}
			} else if (!SEM_REMOVED(save_errno))
			{
				RETURN_IF_BYPASSED(bypassed_ftok, indefinite_wait, sem_stacktrace_time, sem_timedout);
				RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			}
			assert(SEM_REMOVED(save_errno));
			if ((MAX_ACCESS_SEM_RETRIES - 1) == loopcnt)
				RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
					RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			READ_DB_FILE_HEADER(reg, tsd, err_ret);
			RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait, sem_stacktrace_time, sem_timedout);
		}
		assert((-1 != status) || bypassed_access);
		if (!indefinite_wait)
			CANCEL_SEM_TIMERS(sem_stacktrace_time, sem_timedout);
		if (!bypassed_access)
		{
			udi->grabbed_access_sem = TRUE;
			/* Now that we have the access control semaphore, re-read the file header so we have uptodate information
			 * in case some of the fields (like access method) were modified concurrently by MUPIP SET -FILE.
			 */
			READ_DB_FILE_HEADER(reg, tsd, err_ret);
			RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait, sem_stacktrace_time, sem_timedout);
			UDI2CSD(udi, tsd); /* Since we read the file header again, tsd->semid/shmid and corresponding ctime fields
					    * will not be uptodate. Refresh it with udi copies as they are more uptodate.
					    */
		}
		udi->counter_acc_incremented = incr_cnt;
	} else
	{	/* for have_standalone_access we were already in "mu_rndwn_file" and got "semid" semaphore. Since mu_rndwn_file
		 * would have gotten "ftok" semaphore before acquiring the access control semaphore, no need to get the "ftok"
		 * semaphore as well.
		 */
		incr_cnt = !read_only;
		ftok_counter_halted = FALSE;
		access_counter_halted = FALSE;
		READ_DB_FILE_HEADER(reg, tsd, err_ret); /* file already opened by "dbfilopn" done from "gvcst_init" */
		RETURN_IF_ERROR(err_ret, err_ret, indefinite_wait, sem_stacktrace_time, sem_timedout);
		db_do_crypt_init = (USES_ENCRYPTION(tsd->is_encrypted) && !IS_LKE_IMAGE);
		INIT_PROC_ENCRYPTION_IF_NEEDED(csa, db_do_crypt_init, init_status);
		INIT_DB_ENCRYPTION_IF_NEEDED(db_do_crypt_init, init_status, reg, csa, tsd, crypt_warning);
		CSD2UDI(tsd, udi);
		/* Make sure "mu_rndwn_file" has created semaphore for standalone access */
		assertpro((INVALID_SEMID != udi->semid) && (0 != udi->gt_sem_ctime));
		/* Make sure "mu_rndwn_file" has reset shared memory. In pro, just clear it and proceed. */
		assert((INVALID_SHMID == udi->shmid) && (0 == udi->gt_shm_ctime));
		/* In pro, just clear it and proceed */
		udi->shmid = INVALID_SHMID;	/* reset shmid so dbinit_ch does not get confused in case we go there */
		udi->sem_created = TRUE;
		udi->shm_created = !tsd->read_only;
	}
	assert(udi->grabbed_access_sem || bypassed_access);
	if (udi->fd_opened_with_o_direct)
	{	/* "tsd" points to dio_buff.aligned, a global variable buffer that will likely be reused by other functions
		 * inside "db_init" (e.g. "recover_truncate" invoking "db_write_eof_block"). Point it back to tsdbuff
		 * after copying the contents as we need access to "tsd" even after the "recover_truncate" invocation.
		 */
		assert(tsd == (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned);
		memcpy(&tsdbuff, (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned, SGMNT_HDR_LEN);
		tsd = &tsdbuff;
	}
	DO_DB_HDR_CHECK(reg, tsd); /* Basic sanity check on the file header fields */
	if (WBTEST_ENABLED(WBTEST_HOLD_ONTO_ACCSEM_IN_DBINIT))
	{
		DBGFPF((stderr, "Holding the access control semaphore.. Sleeping for 30 seconds\n"));
		LONG_SLEEP(30);
		DBGFPF((stderr, "30 second sleep exhausted.. continuing with rest of db_init..\n"));
	}
	if (WBTEST_ENABLED(WBTEST_HOLD_FTOK_UNTIL_BYPASS))
	{
		if ((3 * DB_COUNTER_SEM_INCR) == semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
		{	/* We are ftok semaphore holder */
			DBGFPF((stderr, "Holding the ftok semaphore until a new process comes along.\n"));
			while (3 == semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
				LONG_SLEEP(1);
		}
	}
	/* Now that the access control lock is obtained and file header passed all sanity checks, update the acc_meth of the
	 * region from the one in the file header (in case they are different). This way, any later code that relies on the
	 * acc_meth dereferenced from the region will work correctly. Instead of checking if they are different, do the assignment
	 * unconditionally
	 */
	reg->dyn.addr->acc_meth = tsd->acc_meth;
	reg->dyn.addr->read_only = tsd->read_only;
	COPY_AIO_SETTINGS(reg->dyn.addr, tsd);	/* copy "asyncio" from tsd to reg->dyn.addr */
	new_shm_ipc = udi->shm_created;
	if (new_shm_ipc && !tsd->read_only)
	{	/* Bypassers are not allowed to create shared memory so we don't end up with conflicting shared memories */
		assert(!bypassed_ftok && !bypassed_access);
		/* Since we are about to allocate new shared memory, if necessary, adjust the journal buffer size right now.
		 * Note that if the process setting up shared memory is a read-only process, then we might not flush updated
		 * jnl_buffer_size to the file header, which is fine because the value in shared memory is what all processes
		 * are looking at. If necessary, the next process to initialize shared memory will repeat the process of
		 * adjusting the jnl_buffer_size value.
		 */
		jnl_buffer_size = tsd->jnl_buffer_size;
		if ((0 != jnl_buffer_size) && (jnl_buffer_size < JNL_BUFFER_MIN))
		{
			ROUND_UP_MIN_JNL_BUFF_SIZE(tsd->jnl_buffer_size, tsd);
			SNPRINTF(s, JNLBUFFUPDAPNDX_SIZE, JNLBUFFUPDAPNDX, JNL_BUFF_PORT_MIN(tsd), JNL_BUFFER_MAX);
			SEND_MSG(VARLSTCNT(10) ERR_JNLBUFFREGUPD, 4, REG_LEN_STR(reg),
				jnl_buffer_size, tsd->jnl_buffer_size, ERR_TEXT, 2, LEN_AND_STR(s));
		}
		dbsecspc(reg, tsd, &sec_size); 	/* Find db segment size */
		/* Create new shared memory using IPC_PRIVATE. System guarantees a unique id */
		GTM_WHITE_BOX_TEST(WBTEST_FAIL_ON_SHMGET, sec_size, GTM_UINT64_MAX);
		if (-1 == (status_l = udi->shmid = shmget(IPC_PRIVATE, sec_size, RWDALL | IPC_CREAT)))
		{
			udi->shmid = (int)INVALID_SHMID;
			status_l = INVALID_SHMID;
			RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database shmget"), errno);
		}
		tsd->shmid = udi->shmid;
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl IPC_STAT1"), errno);
		/* change uid, group-id and permissions if needed */
		need_shmctl = FALSE;
		if ((INVALID_UID != user_id) && (user_id != shmstat.shm_perm.uid))
		{
			shmstat.shm_perm.uid = user_id;
			need_shmctl = TRUE;
		}
		if ((INVALID_GID != group_id) && (group_id != shmstat.shm_perm.gid))
		{
			shmstat.shm_perm.gid = group_id;
			need_shmctl = TRUE;
		}
		if (shmstat.shm_perm.mode != perm)
		{
			shmstat.shm_perm.mode = perm;
			need_shmctl = TRUE;
		}
		if (need_shmctl && (-1 == shmctl(udi->shmid, IPC_SET, &shmstat)))
			RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl IPC_SET"), errno);
		/* Warning: We must read the shm_ctime using IPC_STAT after IPC_SET, which changes it.
		 *	    We must NOT do any more IPC_SET or SETVAL after this. Our design is to use
		 *	    shm_ctime as creation time of shared memory and store it in file header.
		 */
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl IPC_STAT2"), errno);
		tsd->gt_shm_ctime.ctime = udi->gt_shm_ctime = shmstat.shm_ctime;
		GTM_ATTACH_SHM;
		shm_setup_ok = TRUE;
	} else if (tsd->read_only)
	{
		dbsecspc(reg, tsd, &sec_size); 	/* Find db segment size */
		csa->db_addrs[0] = malloc(ROUND_UP2(sec_size, OS_PAGE_SIZE) + OS_PAGE_SIZE);
		/* Init the space to zero; the system assumes shared memory gets init'd to zero, funny errors without this */
		memset(csa->db_addrs[0], 0, ROUND_UP2(sec_size, OS_PAGE_SIZE) + OS_PAGE_SIZE);
		/* Move the pointer above so it falls on a cacheline boundary;
		 *  since this segment will be needed during all of process execution
		 *  we don't need to record the original place for cleanup */
		csa->db_addrs[0] = (sm_uc_ptr_t)((UINTPTR_T)csa->db_addrs[0] + OS_PAGE_SIZE)
			- ((UINTPTR_T)csa->db_addrs[0] % OS_PAGE_SIZE);
		csa->nl = (node_local_ptr_t)csa->db_addrs[0];
		read_only = TRUE;
		reg->read_only = TRUE;
		csa->read_write = FALSE;
		shm_setup_ok = TRUE;
		incr_cnt = 0;
	} else
	{
		GTM_ATTACH_SHM_AND_CHECK_VERS(vermismatch, shm_setup_ok);
		if (vermismatch)
		{
			GTM_VERMISMATCH_ERROR;
		} else if (!shm_setup_ok)
		{
			PRINT_CRASH_MESSAGE(0, tsd, ERR_TEXT, 2, LEN_AND_LIT("shared memory is invalid"));
		}
		/* Compare file header fields against cached values. Restore as necessary to fix shared memory layout calculation */
		if (IS_DSE_IMAGE)
		{
			DSE_VERIFY_AND_RESTORE(csa, tsd, acc_meth);
			DSE_VERIFY_AND_RESTORE(csa, tsd, lock_space_size);
			DSE_VERIFY_AND_RESTORE(csa, tsd, jnl_buffer_size);
			DSE_VERIFY_AND_RESTORE(csa, tsd, blk_size);
		}
	}
	csa->critical = (mutex_struct_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SIZE);
	assert(((INTPTR_T)csa->critical & 0xf) == 0); /* critical should be 16-byte aligned */
#	ifdef CACHELINE_SIZE
	assert(0 == ((INTPTR_T)csa->critical & (CACHELINE_SIZE - 1)));
#	endif
	/* Note: Here we check jnl_state from database file; its value cannot change without stand-alone access.
	 * The jnl_buff should be initialized irrespective of read/write process
	 */
	JNL_INIT(csa, reg, tsd);
	csa->shmpool_buffer = (shmpool_buff_hdr_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SPACE(tsd) + JNL_SHARE_SIZE(tsd));
	/* Initialize memory for snapshot context */
	if (NULL == csa->ss_ctx)					/* May have been allocated if opened/closed/opened */
		csa->ss_ctx = malloc(SIZEOF(snapshot_context_t));
	DEFAULT_INIT_SS_CTX((SS_CTX_CAST(csa->ss_ctx)));
	csa->lock_addrs[0] = (sm_uc_ptr_t)csa->shmpool_buffer + SHMPOOL_SECTION_SIZE;
	csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
	csa->total_blks = tsd->trans_hist.total_blks;   		/* For test to see if file has extended */
	cnl = csa->nl;
	if (new_shm_ipc)
	{
#		ifdef DEBUG
		/* We allocated shared storage -- "shmget" ensures it is null initialized. Assert that. */
		ptr = (char *)cnl;
		for (i = 0; i < SIZEOF(*cnl); i++)
			assert('\0' == ptr[i]);
#		endif
		cnl->sec_size = sec_size;			/* Set the shared memory size 			     */
		if (JNL_ALLOWED(csa))
		{	/* initialize jb->cycle to a value different from initial value of jpc->cycle (0). although this is not
			 * necessary right now, in the future, the plan is to change "jnl_ensure_open" to only do a cycle mismatch
			 * check in order to determine whether to call jnl_file_open() or not. this is in preparation for that.
			 */
			csa->jnl->jnl_buff->cycle = 1;
		}
		GTM_CACHE_INTO_SHM(csa, tsd);
	}
	is_bg = (dba_bg == tsd->acc_meth);
	if (is_bg)
		csd = csa->hdr = (sgmnt_data_ptr_t)(csa->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd));
	else
	{
		FSTAT_FILE(udi->fd, &stat_buf, stat_res);
		if (-1 == stat_res)
			RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(tsd->start_vbn);
		assert(0 < mmap_sz);
		CHECK_LARGEFILE_MMAP(reg, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
#		ifdef _AIX
		mmapaddr = shmat(udi->fd, 0, (read_only ? (SHM_MAP|SHM_RDONLY) : SHM_MAP));
#		else
		mmapaddr = MMAP_FD(udi->fd, mmap_sz, BLK_ZERO_OFF(tsd->start_vbn), read_only);
#		endif
		if (-1 == (INTPTR_T)mmapaddr)
		{
			RTS_ERROR(VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_SYSCALL, 5, LEN_AND_STR(MEM_MAP_SYSCALL), CALLFROM, errno);
		}
#		ifdef _AIX
		csa->db_addrs[0] = (sm_uc_ptr_t)((sm_uc_ptr_t)mmapaddr + BLK_ZERO_OFF(tsd->start_vbn));
#		else
		csa->db_addrs[0] = mmapaddr;
#		endif
		csa->db_addrs[1] = (sm_uc_ptr_t)((sm_uc_ptr_t)csa->db_addrs[0] + mmap_sz - 1);	/* '- 1' due to 0-based indexing */
		assert(csa->db_addrs[1] > csa->db_addrs[0]);
		csd = csa->hdr = (sgmnt_data_ptr_t)((sm_uc_ptr_t)csa->lock_addrs[1] + 1);
	}
	/* At this point, shm_setup_ok is TRUE so we are guaranteed that vermismatch is FALSE.  Therefore, we can safely
	 * dereference cnl->glob_sec_init without worrying about whether or not it could be at a different offset than
	 * the current version. The only exception is DSE which can continue even after the VERMISMATCH error and hence
	 * can have shm_setup_ok set to FALSE at this point.
	 */
	if (shm_setup_ok && !cnl->glob_sec_init && !(bypassed_ftok || bypassed_access))
	{
		assert(udi->shm_created || tsd->read_only);
		assert(new_shm_ipc || tsd->read_only);
		assert(!vermismatch);
		memcpy(csd, tsd, SIZEOF(sgmnt_data));
		READ_DB_FILE_MASTERMAP(reg, csd);
		if (csd->machine_name[0])                  /* crash occurred */
		{
			if (0 != STRNCMP_STR(csd->machine_name, machine_name, MAX_MCNAMELEN))  /* crashed on some other node */
				RTS_ERROR(VARLSTCNT(8) ERR_HOSTCONFLICT, 6, LEN_AND_STR(machine_name), DB_LEN_STR(reg),
					  LEN_AND_STR(csd->machine_name));
			else
			{
				PRINT_CRASH_MESSAGE(0, csd, ERR_TEXT, 2,
					LEN_AND_LIT("machine name in file header is non-null implying possible crash"));
			}
		}
		if (is_bg)
		{
			cnl->cache_off = -CACHE_CONTROL_SIZE(csd);
			db_csh_ini(csa);
			bt_malloc(csa);
		}
		db_csh_ref(csa, TRUE);
		shmpool_buff_init(reg);
		SS_INFO_INIT(csa);
		STRNCPY_STR(cnl->machine_name, machine_name, MAX_MCNAMELEN);				/* machine name */
		assert(MAX_REL_NAME > gtm_release_name_len);
		memcpy(cnl->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
		memcpy(cnl->label, GDS_LABEL, GDS_LABEL_SZ - 1);				/* GDS label */
		memcpy(cnl->fname, reg->dyn.addr->fname, reg->dyn.addr->fname_len);		/* database filename */
		cnl->creation_date_time4 = csd->creation_time4;
		cnl->highest_lbm_blk_changed = GDS_CREATE_BLK_MAX;				/* set to invalid block number */
		cnl->wcs_timers = -1;
		cnl->nbb = BACKUP_NOT_IN_PROGRESS;
		cnl->unique_id.uid = FILE_INFO(reg)->fileid;            /* save what file we initialized this storage for */
		/* save pointers in csa to access shared memory */
		cnl->critical = (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)cnl);
		if (JNL_ALLOWED(csa))
			cnl->jnl_buff = (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)cnl);
		cnl->shmpool_buffer = (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)cnl);
		if (is_bg)
			/* Field is sm_off_t (4 bytes) so only in BG mode is this assurred to be 4 byte capable */
			cnl->hdr = (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)cnl);
		cnl->lock_addrs = (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)cnl);
		if (!read_only || is_bg)
		{
			csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
			csd->max_update_array_size = csd->max_non_bm_update_array_size
				= (int4)(ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE));
			csd->max_update_array_size += (int4)(ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE));
			/* add current db_csh counters into the cumulative counters and reset the current counters */
#			define TAB_DB_CSH_ACCT_REC(COUNTER, DUMMY1, DUMMY2)		\
				csd->COUNTER.cumul_count += csd->COUNTER.curr_count;	\
				csd->COUNTER.curr_count = 0;
#			include "tab_db_csh_acct_rec.h"
#			undef TAB_DB_CSH_ACCT_REC
		}
		cnl->wc_blocked = FALSE; 	/* Since we are creating shared memory, reset wc_blocked to FALSE */
		gvstats_rec_csd2cnl(csa);	/* should be called before "db_auto_upgrade" */
		reg->dyn.addr->ext_blk_count = csd->extension_size;
		mlk_shr_init(csa->lock_addrs[0], csd->lock_space_size, csa, (FALSE == read_only));
		db_auto_upgrade(reg);		/* should be called before "gtm_mutex_init" to ensure NUM_CRIT_ENTRY is nonzero */
		DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		gtm_mutex_init(reg, NUM_CRIT_ENTRY(csd), FALSE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (read_only)
			cnl->remove_shm = TRUE;	/* gds_rundown can remove shmem if first process has read-only access */
		if (FALSE == csd->multi_site_open)
		{	/* first time database is opened after upgrading to a GTM version that supports multi-site
			 * replication
			 */
			csd->zqgblmod_seqno = 0;
			csd->zqgblmod_tn = 0;
			if (csd->pre_multisite_resync_seqno > csd->reg_seqno)
				csd->pre_multisite_resync_seqno = csd->reg_seqno;
			csd->multi_site_open = TRUE;
		}
		cnl->glob_sec_init = TRUE;
		STAT_FILE((char *)cnl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			save_errno = errno;
			RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
		}
		set_gdid_from_stat(&cnl->unique_id.uid, &stat_buf);
		recover_truncate(csa, csd, reg);
		cnl->jnlpool_shmid = INVALID_SHMID;
		cnl->statsdb_fname_len = ARRAYSIZE(cnl->statsdb_fname);
		/* Initialize cnl->statsdb_fname from the basedb name */
		gvcst_set_statsdb_fname(csd, reg, cnl->statsdb_fname, &cnl->statsdb_fname_len);
		cnl->statsdb_created = FALSE;
		cnl->statsdb_rundown_clean = FALSE;
		if (IS_STATSDB_REGNAME(reg))
		{	/* Note that in case this region is a statsdb, its baseDBnl->statsdb_rundown_clean would have been set to
			 * FALSE at baseDB "db_init" time but it is possible the statsdb has been through "gds_rundown" and we
			 * are back in "db_init" for the same statsDB (e.g. VIEW "NOSTATSHARE" VIEW "STATSHARE" sequence). In that
			 * case, we need to reset the flag to FALSE. Hence the code below.
			 */
			STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
			assert(baseDBreg->open);
			baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
			baseDBnl = baseDBcsa->nl;
			baseDBnl->statsdb_rundown_clean = FALSE;
			assert(csd->basedb_fname_len);	/* should have been initialized for statsdb at "mucregini" time */
		}
	} else
	{
		if (STRNCMP_STR(cnl->machine_name, machine_name, MAX_MCNAMELEN))       /* machine names do not match */
		{
			if (cnl->machine_name[0])
				RTS_ERROR(VARLSTCNT(8) ERR_HOSTCONFLICT, 6, LEN_AND_STR(machine_name), DB_LEN_STR(reg),
					  LEN_AND_STR(cnl->machine_name));
			else
			{
				PRINT_CRASH_MESSAGE(0, csd, ERR_TEXT, 2,
					LEN_AND_LIT("machine name in shared memory is non-null implying possible crash"));
			}
		}
		/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
		 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' at least at cnl->fname[MAX_FN_LEN]
		 */
		assert(cnl->fname[MAX_FN_LEN] == '\0');	/* Note: the first '\0' in cnl->fname can be much earlier */
		/* Check whether cnl->fname exists. If not, then it is a serious condition. Error out. */
		STAT_FILE((char *)cnl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			save_errno = errno;
			SEND_MSG(VARLSTCNT(13) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(cnl->machine_name),
				ERR_DBNAMEMISMATCH, 4, DB_LEN_STR(reg), udi->shmid, cnl->fname, save_errno);
			PRINT_CRASH_MESSAGE(3, cnl, ERR_DBNAMEMISMATCH, 4,
				DB_LEN_STR(reg), udi->shmid, cnl->fname, save_errno);
		}
		/* Check whether cnl->fname and cnl->unique_id.uid are in sync. If not error out. */
		if (FALSE == is_gdid_stat_identical(&cnl->unique_id.uid, &stat_buf))
		{
			SEND_MSG(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(cnl->machine_name),
				ERR_DBIDMISMATCH, 4, cnl->fname, DB_LEN_STR(reg), udi->shmid);
			PRINT_CRASH_MESSAGE(2, cnl, ERR_DBIDMISMATCH, 4, cnl->fname, DB_LEN_STR(reg), udi->shmid);
		}
		/* Previously, we used to check for cnl->creation_date_time4 vs csd->creation_time4 and treat it as
		 * an id mismatch situation as well. But later it was determined that as long as the filename and the fileid
		 * match between the database file header and the copy in shared memory, there is no more matching that needs
		 * to be done. It is not possible for the user to create a situation where the filename/fileid matches but
		 * the creation time does not. The only way for this to happen is shared memory corruption in which case we
		 * have a much bigger problem to deal with -- 2011/03/30 --- nars.
		 */
		if (FALSE == is_gdid_gdid_identical(&FILE_INFO(reg)->fileid, &cnl->unique_id.uid))
		{
			SEND_MSG(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(cnl->machine_name),
				ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, cnl->fname);
			PRINT_CRASH_MESSAGE(2, cnl, ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, cnl->fname);
		}
		/* If a regular Recover/Rollback created the shared memory and died (because of a user error or runtime error),
		 * any process that comes up after that should NOT touch the shared memory or database. The user should reissue
		 * Rollback/Recover command that will fix the state of the shared memory and bring the database back to a consistent
		 * state. Note that the reissue of a regular Rollback/Recover command will NOT hit this condition because it invokes
		 * mu_rndwn_file (STANDALONE) that removes the shared memory. The only case in which mu_rndwn_file does NOT remove
		 * shared memory is if it was invoked by an Online Rollback in which case the below check should be bypassed
		 */
		if (cnl->donotflush_dbjnl && !jgbl.onlnrlbk)
		{
			PRINT_CRASH_MESSAGE(0, cnl, ERR_TEXT, 2,
				LEN_AND_LIT("mupip recover/rollback created shared memory. Needs MUPIP RUNDOWN"));
		}
		/* verify pointers from our calculation vs. the copy in shared memory */
		if (cnl->critical != (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)cnl))
		{
			PRINT_CRASH_MESSAGE(2, cnl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("critical"),
					(uint4)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)cnl), (uint4)cnl->critical);
		}
		if ((JNL_ALLOWED(csa)) &&
		    (cnl->jnl_buff != (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)cnl)))
		{
			PRINT_CRASH_MESSAGE(2, cnl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("journal buffer"),
					(uint4)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)cnl), (uint4)cnl->jnl_buff);
		}
		if (cnl->shmpool_buffer != (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)cnl))
		{
			PRINT_CRASH_MESSAGE(2, cnl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("backup buffer"),
				  (uint4)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)cnl), (uint4)cnl->shmpool_buffer);
		}
		if ((is_bg) && (cnl->hdr != (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)cnl)))
		{
			PRINT_CRASH_MESSAGE(2, cnl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("file header"),
					(uint4)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)cnl), (uint4)cnl->hdr);
		}
		if (cnl->lock_addrs != (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)cnl))
		{
			PRINT_CRASH_MESSAGE(2, cnl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("lock address"),
				  (uint4)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)cnl), (uint4)cnl->lock_addrs);
		}
		assert(!udi->shm_created);
		if (is_bg)
			db_csh_ini(csa);
	}
	/* If this is a source server, bind this database to the journal pool shmid & instance file name that the
	 * source server started with. Assert that jnlpool_init has already been done by the source server before
	 * it does db_init.
	 * In addition, if this is not a source server, but has done a "jnlpool_init" (in caller function "gvcst_init"),
	 * and the source server had shut down just before we got the access control lock in db_init(), we would have
	 * had to create db shm afresh. In that case, the source server would have left the jnlpool intact but db shm
	 * would have lost the jnlpool initialization that the source server did. So do it on behalf of the source
	 * server even though this is not a source server.
	 */
	need_jnlpool_setup = REPL_ALLOWED(csd) && is_src_server;
	save_jnlpool = jnlpool;
	if (need_jnlpool_setup)
		assert(jnlpool && jnlpool->pool_init);	/* only one jnlpool for source server */
	else if (!is_src_server && pool_init && REPL_ALLOWED(csd) && gd_header && udi->shm_created
				&& REPL_INST_AVAILABLE(csa->gd_ptr))
	{	/* not source server but db shm created so check proper jnlpool */
		status = filename_to_id(&replfile_gdid, replpool_id.instfilename);	/* set by REPL_INST_AVAILABLE */
		assertpro(SS_NORMAL == status);
		if (jnlpool && jnlpool->pool_init)
		{
			tmp_gdid = &FILE_ID(jnlpool->jnlpool_dummy_reg);
			if (!gdid_cmp(tmp_gdid, &replfile_gdid))
				need_jnlpool_setup = TRUE;	/* current jnlpool is for this region */
		}
		if (!need_jnlpool_setup)
		{	/* need to find right jnlpool */
			for (local_jnlpool = jnlpool_head; local_jnlpool; local_jnlpool = local_jnlpool->next)
			{
				if (local_jnlpool->pool_init)
				{
					tmp_gdid = &FILE_ID(jnlpool->jnlpool_dummy_reg);
					if (!gdid_cmp(tmp_gdid, &replfile_gdid))
					{
						jnlpool = local_jnlpool;
						need_jnlpool_setup = TRUE;
						break;
					}
				}
			}
		}
	}
	if (need_jnlpool_setup)
	{
		assert((NULL != jnlpool) && (NULL != jnlpool->repl_inst_filehdr));
		/* Note: cnl->replinstfilename is changed under control of the init/rundown semaphore only. */
		assert('\0' != jnlpool->jnlpool_ctl->jnlpool_id.instfilename[0]);
		replinst_mismatch = FALSE;
		if ('\0' == cnl->replinstfilename[0])
			STRCPY(cnl->replinstfilename, jnlpool->jnlpool_ctl->jnlpool_id.instfilename);
		else if (STRCMP(cnl->replinstfilename, jnlpool->jnlpool_ctl->jnlpool_id.instfilename))
		{
			assert(!(jnlpool->pool_init && udi->shm_created));
			replinst_mismatch = TRUE;
		}
		/* Note: cnl->jnlpool_shmid is changed under control of the init/rundown semaphore only. */
		assert(INVALID_SHMID != jnlpool->repl_inst_filehdr->jnlpool_shmid);
		if (INVALID_SHMID == cnl->jnlpool_shmid)
			cnl->jnlpool_shmid = jnlpool->repl_inst_filehdr->jnlpool_shmid;
		else if (cnl->jnlpool_shmid != jnlpool->repl_inst_filehdr->jnlpool_shmid)
		{	/* shmid mismatch. Check if the shmid noted down in db filehdr is out-of-date.
			 * Possible if the jnlpool has since been deleted. If so, note the new one down.
			 * If not, then issue an error.
			 */
			assert(!(jnlpool->pool_init && udi->shm_created));
			if (-1 == shmctl(cnl->jnlpool_shmid, IPC_STAT, &shmstat))
			{
				save_errno = errno;
				if (SHM_REMOVED(save_errno))
				{
					replinst_mismatch = FALSE;
					cnl->jnlpool_shmid = jnlpool->repl_inst_filehdr->jnlpool_shmid;
				} else
					replinst_mismatch = TRUE;
			} else
				replinst_mismatch = TRUE;
		}
		/* Replication instance file or jnlpool id mismatch. Issue error. */
		if (replinst_mismatch)
		{
			assert(!(jnlpool->pool_init && udi->shm_created));
			if (INVALID_SHMID == cnl->jnlpool_shmid)
				RTS_ERROR(VARLSTCNT(4) ERR_REPLINSTNOSHM, 2, DB_LEN_STR(reg));
			else
				RTS_ERROR(VARLSTCNT(10) ERR_REPLINSTMISMTCH, 8,
					  LEN_AND_STR(jnlpool->jnlpool_ctl->jnlpool_id.instfilename),
					  jnlpool->repl_inst_filehdr->jnlpool_shmid, DB_LEN_STR(reg),
					  LEN_AND_STR(cnl->replinstfilename), cnl->jnlpool_shmid);
		}
	}
	csa->root_search_cycle = cnl->root_search_cycle;
	csa->onln_rlbk_cycle = cnl->onln_rlbk_cycle;	/* take local copy of the current Online Rollback cycle */
	csa->db_onln_rlbkd_cycle = cnl->db_onln_rlbkd_cycle; /* take local copy of the current Online Rollback mod cycle */
	SYNC_RESERVEDDBFLAGS_REG_CSA_CSD(reg, csa, csd, cnl);
	INITIALIZE_CSA_ENCR_PTR(csa, csd, udi, db_do_crypt_init, crypt_warning, bypassed_ftok);	/* sets csa->encr_ptr */
	/* Record ftok information as soon as shared memory set up is done */
	if (!have_standalone_access && !bypassed_ftok)
		FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_lock, process_id);
	assert(!incr_cnt || !read_only);
	if (incr_cnt)
	{
		if (!cnl->first_writer_seen)
		{
			cnl->first_writer_seen = TRUE;
			cnl->remove_shm = FALSE;
		}
		if (!cnl->first_nonbypas_writer_seen && !bypassed_ftok && !bypassed_access && !FROZEN_CHILLED(csa)
				&& !tsd->read_only)
		{	/* For read-write process flush file header to write machine_name,
			 * semaphore, shared memory id and semaphore creation time to disk.
			 * Note: If first process to open db was read-only, then sem/shm info would have already been flushed
			 * using gtmsecshr but machine_name is flushed only now. It is possible the first writer bypassed
			 * the ftok/access in which case we will not write the machine_name then (because we do not hold a
			 * lock on the database to safely flush the file header) but it is considered acceptable to wait
			 * until the first non-bypassing-writer process attaches since those bypassing processes would be
			 * DSE/LKE only.
			 */
			cnl->first_nonbypas_writer_seen = TRUE;
			STRNCPY_STR(csd->machine_name, machine_name, MAX_MCNAMELEN);
			assert(csd->shmid == tsd->shmid); /* csd already has uptodate sem/shm info from the UDI2CSD call above */
			assert(csd->semid == tsd->semid);
			assert(!memcmp(&csd->gt_sem_ctime, &tsd->gt_sem_ctime, SIZEOF(tsd->gt_sem_ctime)));
			assert(!memcmp(&csd->gt_shm_ctime, &tsd->gt_shm_ctime, SIZEOF(tsd->gt_shm_ctime)));
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, (off_t)0, (sm_uc_ptr_t)csd, SGMNT_HDR_LEN, save_errno);
			if (0 != save_errno)
			{
				if (save_jnlpool != jnlpool)
					jnlpool = save_jnlpool;
				RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					  ERR_TEXT, 2, LEN_AND_LIT("Error with database header flush"), save_errno);
			}
		}
	} else if (read_only && new_shm_ipc)
	{	/* For read-only process if shared memory and semaphore created for first time,
		 * semaphore and shared memory id, and semaphore creation time are written to disk.
		 */
		db_ipcs.open_fd_with_o_direct = udi->fd_opened_with_o_direct;
		db_ipcs.semid = tsd->semid;	/* use tsd instead of csd in order for MM to work too */
		db_ipcs.shmid = tsd->shmid;
		db_ipcs.gt_sem_ctime = tsd->gt_sem_ctime.ctime;
		db_ipcs.gt_shm_ctime = tsd->gt_shm_ctime.ctime;
		db_ipcs.fn_len = reg->dyn.addr->fname_len;
		memcpy(db_ipcs.fn, reg->dyn.addr->fname, reg->dyn.addr->fname_len);
		db_ipcs.fn[reg->dyn.addr->fname_len] = 0;
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
		secshrstat = send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0);
		csa->read_only_fs = (EROFS == secshrstat);
		if ((0 != secshrstat) && !csa->read_only_fs)
		{
			if (save_jnlpool != jnlpool)
				jnlpool = save_jnlpool;
			RTS_ERROR(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("gtmsecshr failed to update database file header"));
		}
	}
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	if (ftok_counter_halted || access_counter_halted)
	{
		if (!csd->mumps_can_bypass)
		{	/* We skipped ftok/access counter increment operation, but after reading the file header, we found out we
			 * are not allowed to do that. Abort. */
			SET_SEMWAIT_FAILURE_RETSTAT(&retstat, ERANGE, op_semctl_or_semop, 0, ERR_CRITSEMFAIL, 0);
			ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, (ftok_counter_halted ? "ftok" : "access control"));
		}
		if (ftok_counter_halted && !cnl->ftok_counter_halted)
		{
			cnl->ftok_counter_halted = TRUE;
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_NOMORESEMCNT, 5,
								LEN_AND_LIT("ftok"), FILE_TYPE_DB, DB_LEN_STR(reg));
		}
		if (access_counter_halted && !cnl->access_counter_halted)
		{
			cnl->access_counter_halted = TRUE;
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_NOMORESEMCNT, 5,
								LEN_AND_LIT("access"), FILE_TYPE_DB, DB_LEN_STR(reg));
		}
	}
	if (udi->counter_acc_incremented && cnl->access_counter_halted)
	{	/* Shared access counter had overflown a while back but is not right now. Undo the counter bump that we
		 * did as otherwise we will later have problems (for example if this is a MUPIP SET -JOURNAL command that
		 * needs standalone access and does a "gds_rundown" followed by a "mu_rndwn_file" later. The "mu_rndwn_file"
		 * call will wait for the counter to become 0 which it never will as we would have bumped it here.
		 */
		SET_SOP_ARRAY_FOR_DECR_CNT(sop, sopcnt, (SEM_UNDO | IPC_NOWAIT));
		SEMOP(udi->semid, sop, sopcnt, status, NO_WAIT);
		udi->counter_acc_incremented = FALSE;
		assert(-1 != status);	/* since we hold the access control lock, we do not expect any errors */
	}
	if (udi->counter_ftok_incremented && cnl->ftok_counter_halted)
	{	/* Do similar cleanup for ftok like we did for access semaphore above */
		SET_SOP_ARRAY_FOR_DECR_CNT(sop, sopcnt, (SEM_UNDO | IPC_NOWAIT));
		SEMOP(udi->ftok_semid, sop, sopcnt, status, NO_WAIT);
		udi->counter_ftok_incremented = FALSE;
		assert(-1 != status);	/* since we hold the access control lock, we do not expect any errors */
	}
	if (gtm_fullblockwrites)
	{	/* We have been asked to do FULL BLOCK WRITES for this database. On *NIX, attempt to get the filesystem
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
		dblksize = csd->blk_size;
		if (0 != fbwsize && (0 == dblksize % fbwsize) && (0 == (BLK_ZERO_OFF(csd->start_vbn)) % fbwsize))
			csa->do_fullblockwrites = gtm_fullblockwrites;		/* This region is fullblockwrite enabled */
		/* Report this length in DSE even if not enabled */
		csa->fullblockwrite_len = fbwsize;		/* Length for rounding fullblockwrite */
	}
	/* Note that the below value is normally incremented/decremented under control of the init/rundown semaphore in
	 * "db_init" and "gds_rundown" but if QDBRUNDOWN is turned ON it could be manipulated without the semaphore in
	 * both callers. Therefore use interlocked INCR_CNT/DECR_CNT.
	 */
	INCR_CNT(&cnl->ref_cnt, &cnl->wc_var_lock);
	assert(!csa->ref_cnt);	/* Increment shared ref_cnt before private ref_cnt increment. */
	csa->ref_cnt++;		/* Currently journaling logic in gds_rundown() in VMS relies on this order to detect last writer */
	if (WBTEST_ENABLED(WBTEST_HOLD_SEM_BYPASS) && !IS_GTM_IMAGE)
	{
		if (0 == cnl->wbox_test_seq_num)
		{
			cnl->wbox_test_seq_num = 1;
			DBGFPF((stderr, "Holding semaphores...\n"));
			while (1 == cnl->wbox_test_seq_num)
				LONG_SLEEP(1);
		}
	}
	if (!have_standalone_access && !jgbl.onlnrlbk && !bypassed_access)
	{
		/* Release control lockout now that it is init'd */
		if (0 != (save_errno = do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO)))
		{
			save_errno = errno;
			RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,	\
					RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
		}
		udi->grabbed_access_sem = FALSE;
	}
	if (WBTEST_ENABLED(WBTEST_SEMTOOLONG_STACK_TRACE) && (1 == cnl->wbox_test_seq_num))
	{
		cnl->wbox_test_seq_num = 2;
		/* Wait till the other process has got some stack traces */
		while (cnl->wbox_test_seq_num != 3)
			LONG_SLEEP(1);
	}
	/* In case of REORG -ENCRYPT the ftok will be released after it has incremented cnl->reorg_encrypt_cycle. */
	if (!have_standalone_access && !bypassed_ftok && !mu_reorg_encrypt_in_prog)
	{	/* Release ftok semaphore lock so that any other ftok conflicted database can continue now */
		if (!ftok_sem_release(reg, FALSE, FALSE))
			RTS_ERROR(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_release, process_id);
		udi->grabbed_ftok_sem = FALSE;
	}
	if (udi->fd_opened_with_o_direct)
	{	/* When we opened the database file we allocated an aligned buffer to hold SGMNT_HDR_LEN bytes.
		 * Now that we have read the db file header (as part of READ_DB_FILE_HEADER above) check if the
		 * database block size is bigger than SGMNT_HDR_LEN. If so, allocate more aligned space in the
		 * global variable "dio_buff" as that will be later used to write either the file header or a GDS block.
		 */
		DIO_BUFF_EXPAND_IF_NEEDED(udi, csd->blk_size, &(TREF(dio_buff)));
	}
	REVERT;
	return 0;
}
