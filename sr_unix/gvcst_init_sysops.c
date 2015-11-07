/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include <sys/un.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include "gtm_ipc.h"
#include "gtm_socket.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_sem.h"
#include "gtm_statvfs.h"
#ifdef __linux__
#include "hugetlbfs_overrides.h"
#endif

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

#include "heartbeat_timer.h"
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
#include "wcs_clean_dbsync.h" /* for setting wcs_clean_dbsync pointer */
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "db_snapshot.h"
#include "lockconst.h"	/* for LOCK_AVAILABLE */
#ifdef GTM_TRUNCATE
#include "recover_truncate.h"
#endif

#ifndef GTM_SNAPSHOT
# error "Snapshot facility not supported in this platform"
#endif

#define REQRUNDOWN_TEXT		"semid is invalid but shmid is valid or at least one of sem_ctime or shm_ctime are non-zero"
#define MAX_ACCESS_SEM_RETRIES	2

#define RTS_ERROR(...)		rts_error_csa(CSA_ARG(csa) __VA_ARGS__)
#define SEND_MSG(...)		send_msg_csa(CSA_ARG(csa) __VA_ARGS__)

#define SS_INFO_INIT(CSA)												\
{															\
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
}

#define GTM_ATTACH_CHECK_ERROR												\
{															\
	if (-1 == status_l)												\
	{														\
		RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),						\
			  ERR_TEXT, 2, LEN_AND_LIT("Error attaching to database shared memory"), errno);		\
	}														\
}

#define GTM_ATTACH_SHM													\
{															\
	status_l = (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)do_shmat(udi->shmid, 0, SHM_RND));			\
	GTM_ATTACH_CHECK_ERROR;												\
	csa->nl = (node_local_ptr_t)csa->db_addrs[0];									\
}

#define GTM_ATTACH_SHM_AND_CHECK_VERS(VERMISMATCH, SHM_SETUP_OK)								\
{																\
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
}

#define GTM_VERMISMATCH_ERROR												\
{															\
	if (!vermismatch_already_printed)										\
	{														\
		vermismatch_already_printed = TRUE;									\
		RTS_ERROR(VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len, gtm_release_name,	\
			  LEN_AND_STR(now_running));									\
	}														\
}

#ifdef GTM_CRYPT
#define INIT_DB_ENCRYPTION_IF_NEEDED(DO_CRYPT_INIT, INIT_STATUS, REG, CSA, TSD)							\
{																\
	int			fn_len = 0;											\
	char			*fn;												\
																\
	if (DO_CRYPT_INIT)													\
	{															\
		if (0 == INIT_STATUS)												\
			INIT_DB_ENCRYPTION(CSA, TSD, INIT_STATUS);								\
		if (0 != INIT_STATUS)												\
		{														\
			fn = (char *)(REG->dyn.addr->fname);									\
			fn_len = REG->dyn.addr->fname_len;									\
			if (IS_GTM_IMAGE)											\
			{													\
				GTMCRYPT_REPORT_ERROR(INIT_STATUS, rts_error, fn_len, fn);					\
			} else													\
				GTMCRYPT_REPORT_ERROR(MAKE_MSG_WARNING(INIT_STATUS), gtm_putmsg, fn_len, fn);			\
			CSA->encr_key_handle = GTMCRYPT_INVALID_KEY_HANDLE;							\
		}														\
	}															\
}
#define INIT_PROC_ENCRYPTION_IF_NEEDED(CSA, DO_CRYPT_INIT, INIT_STATUS)								\
{																\
	if (DO_CRYPT_INIT)													\
		INIT_PROC_ENCRYPTION(CSA, INIT_STATUS);										\
}
#else
#define INIT_DB_ENCRYPTION_IF_NEEDED(IS_ENCRYPTED, INIT_STATUS, REG, CSA, TSD)
#define INIT_PROC_ENCRYPTION_IF_NEEDED(CSA, IS_ENCRYPTED, INIT_STATUS)
#endif

#define READ_DB_FILE_HEADER(REG, TSD)			\
{							\
	file_control    	*fc;			\
							\
	fc = REG->dyn.addr->file_cntl;			\
	fc->file_type = REG->dyn.addr->acc_meth;	\
	fc->op = FC_READ;				\
	fc->op_buff = (sm_uc_ptr_t)TSD;			\
	fc->op_pos = 1;					\
	fc->op_len = SIZEOF(sgmnt_data);		\
	dbfilop(fc);					\
}

#define READ_DB_FILE_MASTERMAP(REG, CSD)		\
{							\
	file_control    	*fc;			\
							\
	fc = REG->dyn.addr->file_cntl;			\
	fc->file_type = dba_bg;				\
	fc->op = FC_READ;				\
	fc->op_buff = MM_ADDR(CSD);			\
	fc->op_len = MASTER_MAP_SIZE(CSD);		\
	fc->op_pos = MM_BLOCK;				\
	dbfilop(fc);					\
}

/* Depending on whether journaling and/or replication was enabled at the time of the crash,
 * print REQRUNDOWN, REQRECOV, or REQROLLBACK error message.
 */
#define PRINT_CRASH_MESSAGE(CNT, ARG, ...)							\
{												\
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
}

GBLREF	boolean_t		gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
GBLREF	boolean_t		is_src_server;
GBLREF  boolean_t               mupip_jnl_recover;
GBLREF  gd_region               *gv_cur_region;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	node_local_ptr_t	locknl;
GBLREF	uint4			heartbeat_counter;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF  uint4                   process_id;
GBLREF	void			(*wcs_clean_dbsync_fptr)();
GBLREF	jnl_gbls_t		jgbl;
GTMCRYPT_ONLY(
GBLREF	gtmcrypt_key_t		mu_int_encrypt_key_handle;
)
#ifndef MUTEX_MSEM_WAKE
GBLREF	int 	mutex_sock_fd;
#endif

LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

OS_PAGE_SIZE_DECLARE

error_def(ERR_BADDBVER);
ZOS_ONLY(error_def(ERR_BADTAG);)
error_def(ERR_HOSTCONFLICT);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBFILERR);
error_def(ERR_DBFLCORRP);
error_def(ERR_DBIDMISMATCH);
error_def(ERR_DBNAMEMISMATCH);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBSHMNAMEDIFF);
error_def(ERR_JNLBUFFREGUPD);
error_def(ERR_NLMISMATCHCALC);
error_def(ERR_MMNODYNUPGRD);
error_def(ERR_PERMGENFAIL);
error_def(ERR_REQROLLBACK);
error_def(ERR_REQRECOV);
error_def(ERR_REQRUNDOWN);
error_def(ERR_REGOPENRETRY);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

gd_region *dbfilopn (gd_region *reg)
{
	unix_db_info    *udi;
	parse_blk       pblk;
	mstr            file;
	char            *fnptr, fbuff[MAX_FBUFF + 1];
	struct stat     buf;
	gd_region       *prev_reg;
	gd_segment      *seg;
	int             status;
	bool            raw;
	int		stat_res, rc, save_errno;
	sgmnt_addrs	*csa;
	ZOS_ONLY(int	realfiletag;)

	seg = reg->dyn.addr;
	assert(seg->acc_meth == dba_bg  ||  seg->acc_meth == dba_mm);
	FILE_CNTL_INIT_IF_NULL(seg);
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	file.addr = (char *)seg->fname;
	file.len = seg->fname_len;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = fbuff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	memcpy(fbuff,file.addr,file.len);
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
	if (!(status & 1))
	{
		if (!IS_GTCM_GNP_SERVER_IMAGE)
		{
			free(seg->file_cntl->file_info);
			free(seg->file_cntl);
			seg->file_cntl = 0;
		}
		RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
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
	OPENFILE(fnptr, O_RDWR, udi->fd);
	if (!udi->grabbed_access_sem)
	{	/* If the process already has standalone access, these fields are initialized in mu_rndwn_file */
		udi->ftok_semid = INVALID_SEMID;
		udi->semid = INVALID_SEMID;
		udi->shmid = INVALID_SHMID;
		udi->gt_sem_ctime = 0;
		udi->gt_shm_ctime = 0;
	}
	reg->read_only = FALSE;		/* maintain csa->read_write simultaneously */
	csa->read_write = TRUE;	/* maintain reg->read_only simultaneously */
	if (FD_INVALID == udi->fd)
	{
		OPENFILE(fnptr, O_RDONLY, udi->fd);
		if (FD_INVALID == udi->fd)
		{
			save_errno = errno;
			if (!IS_GTCM_GNP_SERVER_IMAGE)
			{
				free(seg->file_cntl->file_info);
				free(seg->file_cntl);
				seg->file_cntl = 0;
			}
			RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
		}
		reg->read_only = TRUE;			/* maintain csa->read_write simultaneously */
		csa->read_write = FALSE;	/* maintain reg->read_only simultaneously */
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(udi->fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_SEND_MSG(fnptr, errno, realfiletag, TAG_BINARY);
#	endif
	STAT_FILE(fnptr, &buf, stat_res);
        if (-1 == stat_res)
        {
        	save_errno = errno;
        	RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
        }
	set_gdid_from_stat(&udi->fileid, &buf);
	if (prev_reg = gv_match(reg))
	{
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		free(seg->file_cntl->file_info);
		free(seg->file_cntl);
		seg->file_cntl = 0;
		return prev_reg;
	}
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
#	ifdef HUGETLB_SUPPORTED
	*sec_size = ROUND_UP(tmp_sec_size, OS_HUGEPAGE_SIZE);
#	else
	*sec_size = ROUND_UP(tmp_sec_size, OS_PAGE_SIZE);
#	endif
	return;
}

int db_init(gd_region *reg)
{
	boolean_t       	is_bg, read_only, sem_created = FALSE, need_stacktrace, have_standalone_access;
	boolean_t		shm_setup_ok = FALSE, vermismatch = FALSE, vermismatch_already_printed = FALSE;
	boolean_t		new_shm_ipc, do_crypt_init = FALSE, replinst_mismatch;
	char            	machine_name[MAX_MCNAMELEN];
	int			gethostname_res, stat_res, group_id, perm, save_udi_semid;
	int4            	status, semval, dblksize, fbwsize, save_errno, wait_time, loopcnt, sem_pid;
	sm_long_t       	status_l;
	sgmnt_addrs     	*csa;
	sgmnt_data		tsdbuff;
	sgmnt_data_ptr_t        csd, tsd;
	struct sembuf   	sop[3];
	struct stat     	stat_buf;
	union semun		semarg;
	struct semid_ds		semstat;
	struct shmid_ds         shmstat;
	struct statvfs		dbvfs;
	uint4           	sopcnt, start_hrtbt_cntr;
	unix_db_info    	*udi;
	char			now_running[MAX_REL_NAME];
	int			init_status;
	gtm_uint64_t 		sec_size, mmap_sz;
	semwait_status_t	retstat;
	struct perm_diag_data	pdd;
	boolean_t		bypassed_ftok = FALSE, bypassed_access = FALSE;
	int			jnl_buffer_size;
	char			s[JNLBUFFUPDAPNDX_SIZE];	/* JNLBUFFUPDAPNDX_SIZE is defined in jnl.h */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_NOUNWIND(dbinit_ch);
	assert(INTRPT_IN_GVCST_INIT == intrpt_ok_state); /* we better be called from gvcst_init */
	wcs_clean_dbsync_fptr = &wcs_clean_dbsync;
	tsd = &tsdbuff;
	read_only = reg->read_only;
	udi = FILE_INFO(reg);
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
	assert(0 <= udi->fd); /* database file must have been already opened by dbfilopn() done from gvcst_init() */
	FSTAT_FILE(udi->fd, &stat_buf, stat_res); /* get the stats for the database file */
	if (-1 == stat_res)
		RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
	/* Setup new group and permissions if indicated by the security rules. */
	if (gtm_set_group_and_perm(&stat_buf, &group_id, &perm, PERM_IPC, &pdd) < 0)
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
	if (!have_standalone_access)
	{
		do_crypt_init = (reg->dyn.addr->is_encrypted && !IS_LKE_IMAGE);
		INIT_PROC_ENCRYPTION_IF_NEEDED(csa, do_crypt_init, init_status); /* heavy-weight so needs to be done before ftok */
		start_hrtbt_cntr = heartbeat_counter;
		if (!ftok_sem_get2(reg, start_hrtbt_cntr, &retstat, &bypassed_ftok))
			ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "ftok");
		if (bypassed_ftok)
			SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("FTOK bypassed at database initialization"));
		/* At this point we have ftok_semid semaphore based on ftok key. Any ftok conflicted region will block at this
		 * point. For example, if a.dat and b.dat both have same ftok and process A tries to open or close a.dat and
		 * process B tries to open or close b.dat, even though the database accesses don't conflict, the first one to
		 * control the ftok semaphore blocks (makes wait) the other(s).
		 */
		READ_DB_FILE_HEADER(reg, tsd); /* file already opened by dbfilopn() done from gvcst_init() */
		DO_BADDBVER_CHK(reg, tsd); /* need to do BADDBVER check before de-referencing shmid and semid from file header
					    * as they could be at different offsets if the database is V4-format */
		if (reg->dyn.addr->is_encrypted != tsd->is_encrypted)
		{	/* Encryption setting different between global directory and database file header */
			reg->dyn.addr->is_encrypted = tsd->is_encrypted; /* override with the value in file header */
			do_crypt_init = (tsd->is_encrypted && !IS_LKE_IMAGE);
			if (do_crypt_init)
			{	/* Encryption is turned on in the file header. Need to do encryption initialization. Release ftok
				 * as initialization is heavy-weight.
				 */
				if (!ftok_sem_release(reg, TRUE, FALSE)) /* decrement counter so later increment is correct */
					RTS_ERROR(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
				INIT_PROC_ENCRYPTION_IF_NEEDED(csa, do_crypt_init, init_status); /* redo initialization */
				start_hrtbt_cntr = heartbeat_counter; /* update to reflect time lost in encryption initialization */
				if (!ftok_sem_get2(reg, start_hrtbt_cntr, &retstat, &bypassed_ftok))
					ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "ftok");
				if (bypassed_ftok)
					SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2,
						 LEN_AND_LIT("bypassed at database encryption initialization"));
			} /* else encryption is turned off in the file header. Continue as-is. Any encryption initialization done
			   * before is discarded
			   */
		}
		INIT_DB_ENCRYPTION_IF_NEEDED(do_crypt_init, init_status, reg, csa, tsd);
		if (WBTEST_ENABLED(WBTEST_HOLD_ONTO_FTOKSEM_IN_DBINIT))
		{
			DBGFPF((stderr, "Holding the ftok semaphore.. Sleeping for 30 seconds\n"));
			LONG_SLEEP(30);
			DBGFPF((stderr, "30 second sleep exhausted.. continuing with rest of db_init..\n"));
		}
		for (loopcnt = 0; MAX_ACCESS_SEM_RETRIES > loopcnt; loopcnt++)
		{
			CSD2UDI(tsd, udi); /* sets udi->semid/shmid/sem_ctime/shm_ctime from file header */
			/* we did not create a new ipc resource */
			udi->new_sem = udi->new_shm = FALSE;
			sem_created = FALSE;
			if (INVALID_SEMID == udi->semid)
			{	/* access control semaphore does not exist. Create one */
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
				udi->new_sem = udi->new_shm = TRUE;
				sem_created = TRUE;
				/* change group and permissions */
				semarg.buf = &semstat;
				if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
					RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT1"), errno);
				if ((-1 != group_id) && (group_id != semstat.sem_perm.gid))
					semstat.sem_perm.gid = group_id;
				semstat.sem_perm.mode = perm;
				if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_SET, semarg))
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
						if (4 == semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
						{	/* We are bypasser */
							DBGFPF((stderr, "Waiting for all processes to quit.\n"));
							while (1 < semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
								LONG_SLEEP(1);
						}
					}
					if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
					{
						if (bypassed_ftok)
						{
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_REGOPENRETRY, 4,
								       REG_LEN_STR(reg), DB_LEN_STR(reg));
							REVERT;
							return -1; /* Retry calling db_init. Cleanup in gvcst_init() */
						}
						PRINT_CRASH_MESSAGE(1, tsd, ERR_TEXT, 2,
								    LEN_AND_LIT("Error with database control shmctl"), errno);
					} else if (shmstat.shm_ctime != tsd->gt_shm_ctime.ctime)
					{
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
						PRINT_CRASH_MESSAGE(1, tsd, ERR_TEXT, 2,
							LEN_AND_LIT("Error with database control semaphore (IPC_STAT)"), errno);
					} else if (semarg.buf->sem_ctime != tsd->gt_sem_ctime.ctime)
					{
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
					udi->new_shm = TRUE; /* Need to create shared memory */
				}
			}
			/* We already have ftok semaphore of this region, so all we need is the access control semaphore */
			SET_GTM_SOP_ARRAY(sop, sopcnt, !read_only, (SEM_UNDO | IPC_NOWAIT));
			SEMOP(udi->semid, sop, sopcnt, status, NO_WAIT);
			if (-1 != status)
				break;
			else
			{
				assert(!sem_created); /* if we created the semaphore, we should be able to do the semop */
				save_errno = errno;
				if (EAGAIN == save_errno)
				{
					if (NO_SEMWAIT_ON_EAGAIN == TREF(dbinit_max_hrtbt_delta))
					{
						sem_pid = semctl(udi->semid, DB_CONTROL_SEM, GETPID);
						if (-1 != sem_pid)
						{
							RTS_ERROR(VARLSTCNT(13) ERR_DBFILERR, 2, DB_LEN_STR(reg),
								ERR_SEMWT2LONG, 7, process_id, 0, LEN_AND_LIT("access control"),
									DB_LEN_STR(reg), sem_pid);
						} else
						{
							save_errno = errno;
							if (!SEM_REMOVED(save_errno))
							{
								RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
									ERR_SYSCALL, 5,	RTS_ERROR_LITERAL("semop()"), CALLFROM,
									save_errno);
							} /* else semaphore was removed. Fall-through */
						}
					} else if (!do_blocking_semop(udi->semid, gtm_access_sem, start_hrtbt_cntr,
								      &retstat, reg, &bypassed_access))
					{
						if (!SEM_REMOVED(retstat.save_errno))
							ISSUE_SEMWAIT_ERROR((&retstat), reg, udi, "access control");
						save_errno = retstat.save_errno;
					} else
					{
						if (bypassed_access)
							SEND_MSG(VARLSTCNT(4) ERR_TEXT, 2,
								 LEN_AND_LIT("Access control bypassed at init"));
						save_errno = status = SS_NORMAL;
						break;
					}
				} else if (!SEM_REMOVED(save_errno))
				{
					RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,	\
							RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
				}
				/* this is possible if a concurrent gds_rundown removed the access control semaphore (if
				 * it was the last writer). Another possibility is if the user did an ipcrm which removed
				 * the access control semaphore from the system. Instead of issuing an error right-away,
				 * retry by reading the file header again. Note, it is not possible for another gds_rundown
				 * removing the access control semaphore because any other process has to first get the
				 * ftok lock at startup and since we hold it, they will wait for us to release the ftok.
				 */
				assert(SEM_REMOVED(save_errno));
				if (1 == loopcnt)
				{
					RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,	\
						RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
				}
				READ_DB_FILE_HEADER(reg, tsd);
			}
		}
		assert(-1 != status || bypassed_access);
		if (!bypassed_access)
			udi->grabbed_access_sem = TRUE;
		if(!read_only)
			udi->counter_acc_incremented = TRUE;
		/* Now that we have the access control semaphore, re-read the file header so we have the uptodate information
		 * in case some of the fields (like access method) were modified concurrently by MUPIP SET -FILE
		 */
		READ_DB_FILE_HEADER(reg, tsd);
		UDI2CSD(udi, tsd); /* Since we read the file header again, tsd->semid/shmid and corresponding ctime fields
				    * will not be uptodate. Refresh it with the udi copies as they are the ones used above */
	} else
	{	/* for have_standalone_access we were already in "mu_rndwn_file" and got "semid" semaphore. Since mu_rndwn_file
		 * would have gotten "ftok" semaphore before acquiring the access control semaphore, no need to get the "ftok"
		 * semaphore as well.
		 */
		READ_DB_FILE_HEADER(reg, tsd); /* file already opened by dbfilopn() done from gvcst_init() */
		do_crypt_init = (tsd->is_encrypted && !IS_LKE_IMAGE);
		INIT_PROC_ENCRYPTION_IF_NEEDED(csa, do_crypt_init, init_status);
		INIT_DB_ENCRYPTION_IF_NEEDED(do_crypt_init, init_status, reg, csa, tsd);
		CSD2UDI(tsd, udi);
		/* Make sure "mu_rndwn_file" has created semaphore for standalone access */
		if (INVALID_SEMID == udi->semid || 0 == udi->gt_sem_ctime)
			GTMASSERT;
		/* Make sure "mu_rndwn_file" has reset shared memory. In pro, just clear it and proceed. */
		assert((INVALID_SHMID == udi->shmid) && (0 == udi->gt_shm_ctime));
		/* In pro, just clear it and proceed */
		udi->shmid = INVALID_SHMID;	/* reset shmid so dbinit_ch does not get confused in case we go there */
		udi->new_shm = udi->new_sem = TRUE;
	}
	assert(udi->grabbed_access_sem || bypassed_access);
	DO_DB_HDR_CHECK(reg, tsd); /* Basic sanity check on the file header fields */
	if (WBTEST_ENABLED(WBTEST_HOLD_ONTO_ACCSEM_IN_DBINIT))
	{
		DBGFPF((stderr, "Holding the access control semaphore.. Sleeping for 30 seconds\n"));
		LONG_SLEEP(30);
		DBGFPF((stderr, "30 second sleep exhausted.. continuing with rest of db_init..\n"));
	}
	if (WBTEST_ENABLED(WBTEST_HOLD_FTOK_UNTIL_BYPASS))
	{
		if (3 == semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))
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
	new_shm_ipc = udi->new_shm;
	if (new_shm_ipc)
	{	/* Bypassers are not allowed to create shared memory so we don't end up with conflicting shared memories */
		if (bypassed_ftok || bypassed_access)
		{
			gtm_putmsg_csa(CSA_ARG(csa) ERR_REGOPENRETRY, 2, REG_LEN_STR(reg), DB_LEN_STR(reg));
			REVERT;
			return -1; /* Retry calling db_init. Cleanup in gvcst_init() */
		}
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
		/* change group and permissions */
		if ((-1 != group_id) && (group_id != shmstat.shm_perm.gid))
			shmstat.shm_perm.gid = group_id;
		shmstat.shm_perm.mode = perm;
		if (-1 == shmctl(udi->shmid, IPC_SET, &shmstat))
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
	/* Initialize memory for snapshot context */									\
	csa->ss_ctx = malloc(SIZEOF(snapshot_context_t));
	DEFAULT_INIT_SS_CTX((SS_CTX_CAST(csa->ss_ctx)));
	csa->lock_addrs[0] = (sm_uc_ptr_t)csa->shmpool_buffer + SHMPOOL_SECTION_SIZE;
	csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
	csa->total_blks = tsd->trans_hist.total_blks;   		/* For test to see if file has extended */
	if (new_shm_ipc)
	{
		memset(csa->nl, 0, SIZEOF(*csa->nl));			/* We allocated shared storage -- we have to init it */
		csa->nl->sec_size = sec_size;				/* Set the shared memory size 			     */
		if (JNL_ALLOWED(csa))
		{	/* initialize jb->cycle to a value different from initial value of jpc->cycle (0). although this is not
			 * necessary right now, in the future, the plan is to change jnl_ensure_open() to only do a cycle mismatch
			 * check in order to determine whether to call jnl_file_open() or not. this is in preparation for that.
			 */
			csa->jnl->jnl_buff->cycle = 1;
		}
	}
	is_bg = (dba_bg == tsd->acc_meth);
	if (is_bg)
		csd = csa->hdr = (sgmnt_data_ptr_t)(csa->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd));
	else
	{
		FSTAT_FILE(udi->fd, &stat_buf, stat_res);
		if (-1 == stat_res)
			RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(tsd);
		assert(0 < mmap_sz);
		CHECK_LARGEFILE_MMAP(reg, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
		if (-1 == (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)MMAP_FD(udi->fd, mmap_sz, BLK_ZERO_OFF(tsd), read_only)))
		{
			RTS_ERROR(VARLSTCNT(12) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_SYSCALL, 5, LEN_AND_LIT("mmap()"), CALLFROM, errno);
		}
		csa->db_addrs[1] = csa->db_addrs[0] + mmap_sz - 1;	/* '- 1' due to 0-based indexing */
		assert(csa->db_addrs[1] > csa->db_addrs[0]);
		csd = csa->hdr = (sgmnt_data_ptr_t)((sm_uc_ptr_t)csa->lock_addrs[1] + 1);
	}
	/* At this point, shm_setup_ok is TRUE so we are guaranteed that vermismatch is FALSE.  Therefore, we can safely
	 * dereference csa->nl->glob_sec_init without worrying about whether or not it could be at a different offset than
	 * the current version. The only exception is DSE which can continue even after the VERMISMATCH error and hence
	 * can have shm_setup_ok set to FALSE at this point.
	 */
	if (shm_setup_ok && !csa->nl->glob_sec_init && !(bypassed_ftok || bypassed_access))
	{
		assert(new_shm_ipc);
		assert(!vermismatch);
		csa->dbinit_shm_created = TRUE;
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
			csa->nl->cache_off = -CACHE_CONTROL_SIZE(csd);
			db_csh_ini(csa);
			bt_malloc(csa);
		}
		db_csh_ref(csa, TRUE);
		shmpool_buff_init(reg);
		SS_INFO_INIT(csa);
		STRNCPY_STR(csa->nl->machine_name, machine_name, MAX_MCNAMELEN);				/* machine name */
		assert(MAX_REL_NAME > gtm_release_name_len);
		memcpy(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
		memcpy(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1);				/* GDS label */
		memcpy(csa->nl->fname, reg->dyn.addr->fname, reg->dyn.addr->fname_len);		/* database filename */
		csa->nl->creation_date_time4 = csd->creation_time4;
		csa->nl->highest_lbm_blk_changed = -1;
		csa->nl->wcs_timers = -1;
		csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		csa->nl->unique_id.uid = FILE_INFO(reg)->fileid;            /* save what file we initialized this storage for */
		/* save pointers in csa to access shared memory */
		csa->nl->critical = (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl);
		if (JNL_ALLOWED(csa))
			csa->nl->jnl_buff = (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl);
		csa->nl->shmpool_buffer = (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl);
		if (is_bg)
			/* Field is sm_off_t (4 bytes) so only in BG mode is this assurred to be 4 byte capable */
			csa->nl->hdr = (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl);
		csa->nl->lock_addrs = (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl);
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
		csa->nl->wc_blocked = FALSE; 	/* Since we are creating shared memory, reset wc_blocked to FALSE */
		gvstats_rec_csd2cnl(csa);	/* should be called before "db_auto_upgrade" */
		reg->dyn.addr->ext_blk_count = csd->extension_size;
		mlk_shr_init(csa->lock_addrs[0], csd->lock_space_size, csa, (FALSE == read_only));
		db_auto_upgrade(reg);		/* should be called before "gtm_mutex_init" to ensure NUM_CRIT_ENTRY is nonzero */
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		gtm_mutex_init(reg, NUM_CRIT_ENTRY(csd), FALSE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (read_only)
			csa->nl->remove_shm = TRUE;	/* gds_rundown can remove shmem if first process has read-only access */
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
		csa->nl->glob_sec_init = TRUE;
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			save_errno = errno;
			RTS_ERROR(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), save_errno);
		}
		set_gdid_from_stat(&csa->nl->unique_id.uid, &stat_buf);
#		ifdef RELEASE_LATCH_GLOBAL
		/* On HP-UX, it is possible that mucregini/cs_data is not aligned at the same address
		 * boundary as csd would be in shared memory. This may lead to the initialization and
		 * usage of different elements of hp_latch_space. This may lead to the latch being
		 * "in-use" permanently. To resolve this, shm-initialer re-initializes the global latch
		 * to the "available" state.
		 * Although Solaris doesn't have the same issue of alignment, we'll cover the case of
		 * a corrupt latch (say in case of abnormal process termination).
		 */
		RELEASE_LATCH_GLOBAL(&csd->next_upgrd_warn.time_latch);
#		endif
		GTM_TRUNCATE_ONLY(recover_truncate(csa, csd, reg);)
		csa->nl->jnlpool_shmid = INVALID_SHMID;
	} else
	{
		if (STRNCMP_STR(csa->nl->machine_name, machine_name, MAX_MCNAMELEN))       /* machine names do not match */
		{
			if (csa->nl->machine_name[0])
				RTS_ERROR(VARLSTCNT(8) ERR_HOSTCONFLICT, 6, LEN_AND_STR(machine_name), DB_LEN_STR(reg),
					  LEN_AND_STR(csa->nl->machine_name));
			else
			{
				PRINT_CRASH_MESSAGE(0, csd, ERR_TEXT, 2,
					LEN_AND_LIT("machine name in shared memory is non-null implying possible crash"));
			}
		}
		/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
		 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' atleast at csa->nl->fname[MAX_FN_LEN]
		 */
		assert(csa->nl->fname[MAX_FN_LEN] == '\0');	/* Note: the first '\0' in csa->nl->fname can be much earlier */
		/* Check whether csa->nl->fname exists. If not, then it is a serious condition. Error out. */
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			save_errno = errno;
			SEND_MSG(VARLSTCNT(13) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBNAMEMISMATCH, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname, save_errno);
			PRINT_CRASH_MESSAGE(3, csa->nl, ERR_DBNAMEMISMATCH, 4,
				DB_LEN_STR(reg), udi->shmid, csa->nl->fname, save_errno);
		}
		/* Check whether csa->nl->fname and csa->nl->unique_id.uid are in sync. If not error out. */
		if (FALSE == is_gdid_stat_identical(&csa->nl->unique_id.uid, &stat_buf))
		{
			SEND_MSG(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid);
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid);
		}
		/* Previously, we used to check for csa->nl->creation_date_time4 vs csd->creation_time4 and treat it as
		 * an id mismatch situation as well. But later it was determined that as long as the filename and the fileid
		 * match between the database file header and the copy in shared memory, there is no more matching that needs
		 * to be done. It is not possible for the user to create a situation where the filename/fileid matches but
		 * the creation time does not. The only way for this to happen is shared memory corruption in which case we
		 * have a much bigger problem to deal with -- 2011/03/30 --- nars.
		 */
		if (FALSE == is_gdid_gdid_identical(&FILE_INFO(reg)->fileid, &csa->nl->unique_id.uid))
		{
			SEND_MSG(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname);
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname);
		}
		/* If a regular Recover/Rollback created the shared memory and died (because of a user error or runtime error),
		 * any process that comes up after that should NOT touch the shared memory or database. The user should reissue
		 * Rollback/Recover command that will fix the state of the shared memory and bring the database back to a consistent
		 * state. Note that the reissue of a regular Rollback/Recover command will NOT hit this condition because it invokes
		 * mu_rndwn_file (STANDALONE) that removes the shared memory. The only case in which mu_rndwn_file does NOT remove
		 * shared memory is if it was invoked by an Online Rollback in which case the below check should be bypassed
		 */
		if (csa->nl->donotflush_dbjnl && !jgbl.onlnrlbk)
		{
			assert(FALSE);
			PRINT_CRASH_MESSAGE(0, csa->nl, ERR_TEXT, 2,
				LEN_AND_LIT("mupip recover/rollback created shared memory. Needs MUPIP RUNDOWN"));
		}
		/* verify pointers from our calculation vs. the copy in shared memory */
		if (csa->nl->critical != (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("critical"),
					(uint4)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->critical);
		}
		if ((JNL_ALLOWED(csa)) &&
		    (csa->nl->jnl_buff != (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl)))
		{
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("journal buffer"),
					(uint4)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->jnl_buff);
		}
		if (csa->nl->shmpool_buffer != (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("backup buffer"),
				  (uint4)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->shmpool_buffer);
		}
		if ((is_bg) && (csa->nl->hdr != (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl)))
		{
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("file header"),
					(uint4)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->hdr);
		}
		if (csa->nl->lock_addrs != (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE(2, csa->nl, ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("lock address"),
				  (uint4)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->lock_addrs);
		}
		csa->dbinit_shm_created = FALSE;
		if (is_bg)
			db_csh_ini(csa);
	}
	if (REPL_ALLOWED(csd) && is_src_server)
	{	/* Bind this database to the journal pool shmid & instance file name that the source server started with.
		 * Assert that jnlpool_init has already been done by the source server before it does db_init.
		 */
		assert(NULL != jnlpool.repl_inst_filehdr);
		/* Note: csa->nl->replinstfilename is changed under control of the init/rundown semaphore only. */
		assert('\0' != jnlpool.jnlpool_ctl->jnlpool_id.instfilename[0]);
		replinst_mismatch = FALSE;
		if ('\0' == csa->nl->replinstfilename[0])
			STRCPY(csa->nl->replinstfilename, jnlpool.jnlpool_ctl->jnlpool_id.instfilename);
		else if (STRCMP(csa->nl->replinstfilename, jnlpool.jnlpool_ctl->jnlpool_id.instfilename))
			replinst_mismatch = TRUE;
		/* Note: csa->nl->jnlpool_shmid is changed under control of the init/rundown semaphore only. */
		assert(INVALID_SHMID != jnlpool.repl_inst_filehdr->jnlpool_shmid);
		if (INVALID_SHMID == csa->nl->jnlpool_shmid)
			csa->nl->jnlpool_shmid = jnlpool.repl_inst_filehdr->jnlpool_shmid;
		else if (csa->nl->jnlpool_shmid != jnlpool.repl_inst_filehdr->jnlpool_shmid)
		{	/* shmid mismatch. Check if the shmid noted down in db filehdr is out-of-date.
			 * Possible if the jnlpool has since been deleted. If so, note the new one down.
			 * If not, then issue an error.
			 */
			if (-1 == shmctl(csa->nl->jnlpool_shmid, IPC_STAT, &shmstat))
			{
				save_errno = errno;
				if ((EINVAL == save_errno) || (EIDRM == save_errno)) /* EIDRM is only on Linux */
				{
					replinst_mismatch = FALSE;
					csa->nl->jnlpool_shmid = jnlpool.repl_inst_filehdr->jnlpool_shmid;
				} else
					replinst_mismatch = TRUE;
			} else
				replinst_mismatch = TRUE;
		}
		/* Replication instance file or jnlpool id mismatch. Issue error. */
		if (replinst_mismatch)
			RTS_ERROR(VARLSTCNT(10) ERR_REPLINSTMISMTCH, 8,
				LEN_AND_STR(jnlpool.jnlpool_ctl->jnlpool_id.instfilename), jnlpool.repl_inst_filehdr->jnlpool_shmid,
				DB_LEN_STR(reg), LEN_AND_STR(csa->nl->replinstfilename), csa->nl->jnlpool_shmid);
	}
	csa->root_search_cycle = csa->nl->root_search_cycle;
	csa->onln_rlbk_cycle = csa->nl->onln_rlbk_cycle;	/* take local copy of the current Online Rollback cycle */
	csa->db_onln_rlbkd_cycle = csa->nl->db_onln_rlbkd_cycle; /* take local copy of the current Online Rollback mod cycle */
	/* Record  ftok information as soon as shared memory set up is done */
	if (!have_standalone_access && !bypassed_ftok)
		FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_lock, process_id);
	if (-1 == (semval = semctl(udi->semid, DB_COUNTER_SEM, GETVAL))) /* semval = number of process attached */
	{
		save_errno = errno;
		RTS_ERROR(VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,	\
				RTS_ERROR_LITERAL("semctl()"), CALLFROM, save_errno);
	}
	if (!read_only && (1 == semval) && !bypassed_ftok && !bypassed_access)
	{	/* For read-write process flush file header to write machine_name,
		 * semaphore, shared memory id and semaphore creation time to disk.
		 */
		csa->nl->remove_shm = FALSE;
		STRNCPY_STR(csd->machine_name, machine_name, MAX_MCNAMELEN);
		if (!is_bg)
		{
			csd->shmid = tsd->shmid;
			csd->semid = tsd->semid;
			csd->gt_sem_ctime = tsd->gt_sem_ctime;
			csd->gt_shm_ctime = tsd->gt_shm_ctime;
		}
		DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, (sm_uc_ptr_t)csd, SIZEOF(sgmnt_data), save_errno);
		if (0 != save_errno)
		{
			RTS_ERROR(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database header flush"), save_errno);
		}
	} else if (read_only && new_shm_ipc)
	{	/* For read-only process if shared memory and semaphore created for first time,
		 * semaphore and shared memory id, and semaphore creation time are written to disk.
		 */
		db_ipcs.semid = tsd->semid;	/* use tsd instead of csd in order for MM to work too */
		db_ipcs.shmid = tsd->shmid;
		db_ipcs.gt_sem_ctime = tsd->gt_sem_ctime.ctime;
		db_ipcs.gt_shm_ctime = tsd->gt_shm_ctime.ctime;
		db_ipcs.fn_len = reg->dyn.addr->fname_len;
		memcpy(db_ipcs.fn, reg->dyn.addr->fname, reg->dyn.addr->fname_len);
		db_ipcs.fn[reg->dyn.addr->fname_len] = 0;
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
		if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
			RTS_ERROR(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("gtmsecshr failed to update database file header"));

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
		FSTATVFS_FILE(udi->fd, &dbvfs, status);
		if (-1 != status)
		{
			dblksize = csd->blk_size;
			fbwsize = (int4)dbvfs.f_bsize;
			if (0 != fbwsize && (0 == dblksize % fbwsize) && (0 == ((csd->start_vbn - 1) * DISK_BLOCK_SIZE) % fbwsize))
				csa->do_fullblockwrites = TRUE;		/* This region is fullblockwrite enabled */
			/* Report this length in DSE even if not enabled */
			csa->fullblockwrite_len = fbwsize;		/* Length for rounding fullblockwrite */
		} else
		{
			save_errno = errno;
			SEND_MSG(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fstatvfs"), CALLFROM, save_errno);
		}
	}
	++csa->nl->ref_cnt;	/* This value is changed under control of the init/rundown semaphore only */
	assert(!csa->ref_cnt);	/* Increment shared ref_cnt before private ref_cnt increment. */
	csa->ref_cnt++;		/* Currently journaling logic in gds_rundown() in VMS relies on this order to detect last writer */
	if (WBTEST_ENABLED(WBTEST_HOLD_SEM_BYPASS) && !IS_GTM_IMAGE)
	{
		if (0 == csa->nl->wbox_test_seq_num)
		{
			csa->nl->wbox_test_seq_num = 1;
			DBGFPF((stderr, "Holding semaphores...\n"));
			while (1 == csa->nl->wbox_test_seq_num)
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
	if (WBTEST_ENABLED(WBTEST_SEMTOOLONG_STACK_TRACE) && (1 == csa->nl->wbox_test_seq_num))
	{
		csa->nl->wbox_test_seq_num = 2;
		/* Wait till the other process has got some stack traces */
		while (csa->nl->wbox_test_seq_num != 3)
			LONG_SLEEP(10);
	}
	if (!have_standalone_access && !bypassed_ftok)
	{	/* Release ftok semaphore lock so that any other ftok conflicted database can continue now */
		if (!ftok_sem_release(reg, FALSE, FALSE))
			RTS_ERROR(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_release, process_id);
		udi->grabbed_ftok_sem = FALSE;
	}
	REVERT;
	return 0;
}
