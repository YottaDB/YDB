/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "ftok_sems.h"

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
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "db_snapshot.h"
#include "lockconst.h"	/* for LOCK_AVAILABLE */

#ifndef GTM_SNAPSHOT
# error "Snapshot facility not supported in this platform"
#endif

#define REQRUNDOWN_TEXT	"semid is invalid but shmid is valid or at least one of sem_ctime or shm_ctime are non-zero"

#define SS_INFO_INIT(CSA)												\
{															\
	shm_snapshot_ptr_t	ss_shm_ptr;										\
	node_local_ptr_t	lcl_cnl;										\
															\
	lcl_cnl = CSA->nl;												\
	lcl_cnl->ss_shmid = INVALID_SHMID;										\
	lcl_cnl->ss_shmcycle = 0;											\
	lcl_cnl->snapshot_in_prog = FALSE;										\
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
		rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),						\
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
		{	/* Copy csa->nl->now_running into a local variable before passing to rts_error() due to the following	\
			 * issue:												\
			 * In VMS, a call to rts_error() copies only the error message and its arguments (as pointers) and	\
			 *  transfers control to the topmost condition handler which is dbinit_ch() in this case. dbinit_ch()	\
			 *  does a PRN_ERROR only for SUCCESS/INFO (VERMISMATCH is neither of them) and in addition		\
			 *  nullifies csa->nl as part of its condition handling. It then transfers control to the next level	\
			 *  condition handler which does a PRN_ERROR but at that point in time, the parameter			\
			 *  csa->nl->now_running is no longer accessible and hence no \parameter substitution occurs (i.e. the	\
			 *  error message gets displayed with plain !ADs).							\
			 * In UNIX, this is not an issue since the first call to rts_error() does the error message		\
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

#define GTM_VERMISMATCH_ERROR											\
{														\
	if (!vermismatch_already_printed)									\
	{													\
		vermismatch_already_printed = TRUE;								\
		/* for DSE, change VERMISMATCH to be INFO (instead of the more appropriate WARNING)		\
		 * as we want the condition handler (dbinit_ch) to do a CONTINUE (which it does			\
		 * only for severity levels SUCCESS or INFO) and resume processing in gvcst_init.c		\
		 * instead of detaching from shared memory.							\
		 */												\
		rts_error(VARLSTCNT(8) MAKE_MSG_TYPE(ERR_VERMISMATCH, (!IS_DSE_IMAGE ? ERROR : INFO)), 6,	\
			DB_LEN_STR(reg), gtm_release_name_len, gtm_release_name, LEN_AND_STR(now_running));	\
	}													\
}

#define ATTACH_TRIES   		10
#define DEFEXT          	"*.dat"
#define MAX_RES_TRIES  	 	620
#define EIDRM_SLEEP_INT		500
#define EIDRM_MAX_SLEEPS	20

GBLREF  uint4                   process_id;
GBLREF  gd_region               *gv_cur_region;
GBLREF  boolean_t               sem_incremented;
GBLREF  boolean_t               mupip_jnl_recover;
GBLREF  boolean_t               have_standalone_access;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	node_local_ptr_t	locknl;
GBLREF	boolean_t		new_dbinit_ipc;
GBLREF	boolean_t		gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
GBLREF	uint4			mutex_per_process_init_pid;

GTMCRYPT_ONLY(
GBLREF	gtmcrypt_key_t		mu_int_encrypt_key_handle;
)
#ifndef MUTEX_MSEM_WAKE
GBLREF	int 	mutex_sock_fd;
#endif

LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

OS_PAGE_SIZE_DECLARE

static  int             errno_save;
error_def(ERR_CLSTCONFLICT);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBFILERR);
error_def(ERR_DBIDMISMATCH);
error_def(ERR_DBNAMEMISMATCH);
error_def(ERR_NLMISMATCHCALC);
error_def(ERR_REQRUNDOWN);
error_def(ERR_SYSCALL);
error_def(ERR_DBSHMNAMEDIFF);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

ZOS_ONLY(error_def(ERR_BADTAG);)

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
	int		stat_res, rc;
	ZOS_ONLY(int	realfiletag;)

	seg = reg->dyn.addr;
	assert(seg->acc_meth == dba_bg  ||  seg->acc_meth == dba_mm);
	FILE_CNTL_INIT_IF_NULL(seg);
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
		rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
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
	udi = FILE_INFO(reg);
	udi->raw = raw;
	udi->fn = (char *)fnptr;
	OPENFILE(fnptr, O_RDWR, udi->fd);
	udi->ftok_semid = INVALID_SEMID;
	udi->semid = INVALID_SEMID;
	udi->shmid = INVALID_SHMID;
	udi->gt_sem_ctime = 0;
	udi->gt_shm_ctime = 0;
	reg->read_only = FALSE;		/* maintain csa->read_write simultaneously */
	udi->s_addrs.read_write = TRUE;	/* maintain reg->read_only simultaneously */
	if (FD_INVALID == udi->fd)
	{
		OPENFILE(fnptr, O_RDONLY, udi->fd);
		if (FD_INVALID == udi->fd)
		{
			errno_save = errno;
			if (!IS_GTCM_GNP_SERVER_IMAGE)
			{
				free(seg->file_cntl->file_info);
				free(seg->file_cntl);
				seg->file_cntl = 0;
			}
			rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno_save);
		}
		reg->read_only = TRUE;			/* maintain csa->read_write simultaneously */
		udi->s_addrs.read_write = FALSE;	/* maintain reg->read_only simultaneously */
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(udi->fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_SEND_MSG(fnptr, errno, realfiletag, TAG_BINARY);
#	endif
	STAT_FILE(fnptr, &buf, stat_res);
        if (-1 == stat_res)
        {
        	errno_save = errno;
        	rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno_save);
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
	/* Ensure that all the various sections that the shared memory contains are actually
	 * aligned at the OS_PAGE_SIZE boundary
	 */
	assert(0 == NODE_LOCAL_SPACE % OS_PAGE_SIZE);
	assert(0 == LOCK_SPACE_SIZE(csd) % OS_PAGE_SIZE);
	assert(0 == JNL_SHARE_SIZE(csd) % OS_PAGE_SIZE);
	assert(0 == SHMPOOL_SECTION_SIZE % OS_PAGE_SIZE);
	switch(reg->dyn.addr->acc_meth)
	{
	case dba_mm:
		assert(0 == MMBLK_CONTROL_SIZE(csd) % OS_PAGE_SIZE);
		*sec_size = ROUND_UP(NODE_LOCAL_SPACE + LOCK_SPACE_SIZE(csd) + MMBLK_CONTROL_SIZE(csd) \
					 + JNL_SHARE_SIZE(csd) + SHMPOOL_SECTION_SIZE, OS_PAGE_SIZE);
		break;
	case dba_bg:
		assert(0 == CACHE_CONTROL_SIZE(csd) % OS_PAGE_SIZE);
		*sec_size = ROUND_UP(NODE_LOCAL_SPACE + (LOCK_BLOCK(csd) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(csd) \
					 + CACHE_CONTROL_SIZE(csd) + JNL_SHARE_SIZE(csd) + SHMPOOL_SECTION_SIZE, OS_PAGE_SIZE);
		break;
	default:
		GTMASSERT;
	}
	return;
}

void db_init(gd_region *reg, sgmnt_data_ptr_t tsd)
{
	boolean_t       	is_bg, read_only;
	char            	machine_name[MAX_MCNAMELEN], instfilename[MAX_FN_LEN + 1];
	file_control    	*fc;
	int			gethostname_res, stat_res, mm_prot;
	int4            	status, semval, dblksize, fbwsize;
	sm_long_t       	status_l;
	sgmnt_addrs     	*csa;
	sgmnt_data_ptr_t        csd;
	struct sembuf   	sop[3];
	struct stat     	stat_buf;
	union semun		semarg;
	struct semid_ds		semstat;
	struct shmid_ds         shmstat;
	struct statvfs		dbvfs;
	uint4           	sopcnt;
	unix_db_info    	*udi;
	char			now_running[MAX_REL_NAME];
	unsigned int		full_len;
	GTMCRYPT_ONLY(
		int		init_status;
		boolean_t	do_crypt_init = FALSE;
	)
	boolean_t		shm_setup_ok = FALSE;
	boolean_t		vermismatch = FALSE;
	boolean_t		vermismatch_already_printed = FALSE;
	int			lib_gid = -1;
	int			group_id;
	struct stat		sb;
	int			perm;
	gtm_uint64_t 		sec_size;

	assert(INTRPT_IN_GVCST_INIT == intrpt_ok_state); /* we better be called from gvcst_init */
	assert(tsd->acc_meth == dba_bg  ||  tsd->acc_meth == dba_mm);
	is_bg = (dba_bg == tsd->acc_meth);
	read_only = reg->read_only;
	new_dbinit_ipc = FALSE;	/* we did not create a new ipc resource */
	udi = FILE_INFO(reg);
	memset(machine_name, 0, SIZEOF(machine_name));
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, gethostname_res))
		rts_error(VARLSTCNT(5) ERR_TEXT, 2, LEN_AND_LIT("Unable to get the hostname"), errno);
	assert(strlen(machine_name) < MAX_MCNAMELEN);
	csa = &udi->s_addrs;
	csa->db_addrs[0] = csa->db_addrs[1] = csa->lock_addrs[0] = NULL;   /* to help in dbinit_ch  and gds_rundown */
	reg->opening = TRUE;
	/* We do some basic encryption initializations here.
	 * 1. Call hash check to figure out if the dat file's hash matches with that of the db_key_file
	 * 2. if the dat file is encrypted, then make a call to the encryption plugin with GTMCRYPT_GETKEY to obtain the
	 *    symmetric key which can be used at later stages for encryption and decryption.
	 * 3. malloc csa->encrypted_blk_contents to be used in jnl_write_aimg_rec and dsk_write_nocache
	 */
#	ifdef GTM_CRYPT
	/* Since LKE will never look at the encrypted contents of the database file, it won't need the initialize
	 * encryption. */
	do_crypt_init = (tsd->is_encrypted && !IS_LKE_IMAGE);
	if (do_crypt_init)
	{
		/* Encryption initialization is a heavy operation due to the initalization of gpgme. Hence, we do
		 * it before acquiring the lock. We do rest of the operations like hash check after the lock. */
		INIT_PROC_ENCRYPTION(init_status);
	}
#	endif
	/*
	 * Create ftok semaphore for this region.
	 * We do not want to make ftok counter semaphore to be 2 for on mupip journal recover process.
	 */
	if (!ftok_sem_get(reg, !mupip_jnl_recover, GTM_ID, FALSE))
		rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	/*
	 * At this point we have ftok_semid sempahore based on ftok key.
	 * Any ftok conflicted region will block at this point.
	 * Say, a.dat and b.dat both has same ftok and we have process A to access a.dat and
	 * process B to access b.dat. In this case only one can continue to do db_init()
	 */
	fc = reg->dyn.addr->file_cntl;
	fc->file_type = reg->dyn.addr->acc_meth;
	fc->op = FC_READ;
	fc->op_buff = (sm_uc_ptr_t)tsd;
	fc->op_len = SIZEOF(*tsd);
	fc->op_pos = 1;
	dbfilop(fc);		/* Read file header */
#	ifdef GTM_CRYPT
	if (do_crypt_init)
	{	/* The below macro if encounters an error will note the error code and will defer the error
		 * handling if the image_type is not MUMPS. This is done so that a user who will be using
		 * any of the GT.M  utility(MUPIP, DSE) on a non encrypted task, might not be forced to setup
		 * an encrypted environment. For instance, MUPIP JOURNAL -EXTRACT -SHOW=HEADER -FORWARD should
		 * not error out during the above encryption initialization. Moreover, we shouldn't even be doing
		 * the below INIT_DB_ENCRYPTION if the above encryption initialization failed. */
		if (0 == init_status)
			INIT_DB_ENCRYPTION(reg->dyn.addr->fname, csa, tsd, init_status);
		if ((0 != init_status) && IS_GTM_IMAGE)
			GC_RTS_ERROR(init_status, reg->dyn.addr->fname);
		csa->encrypt_init_status = init_status;
	}
#	endif
	udi->shmid = tsd->shmid;
	udi->semid = tsd->semid;
	udi->gt_sem_ctime = tsd->gt_sem_ctime.ctime;
	udi->gt_shm_ctime = tsd->gt_shm_ctime.ctime;
	dbsecspc(reg, tsd, &sec_size); 	/* Find db segment size */
	/* get the stats for the database file */
	FSTAT_FILE(udi->fd, &sb, stat_res);
	if (-1 == stat_res)
		rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
	/* setup new group and permissions if indicated by the security rules.  Use 660 for
	 * the new mode if it is world writeable.
	 */
	gtm_set_group_and_perm(&sb, &group_id, &perm, 0660);
	if (!have_standalone_access)
	{
		if (INVALID_SEMID == udi->semid)
		{
			if (0 != udi->gt_sem_ctime || INVALID_SHMID != udi->shmid || 0 != udi->gt_shm_ctime)
			{	/* We must have somthing wrong in protocol or, code, if this happens. */
				assert(FALSE);
				rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
					ERR_TEXT, 2, LEN_AND_LIT(REQRUNDOWN_TEXT));
			}
			/* Create new semaphore using IPC_PRIVATE. System guarantees a unique id. */
			if (-1 == (udi->semid = semget(IPC_PRIVATE, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
			{
				udi->semid = INVALID_SEMID;
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semget"), errno);
			}
			udi->shmid = INVALID_SHMID;	/* reset shmid so dbinit_ch does not get confused in case we go there */
			new_dbinit_ipc = TRUE;
			tsd->semid = udi->semid;
			/* change group and permissions */
			semarg.buf = &semstat;
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					  ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT1"), errno);
			if ((-1 != group_id) && (group_id != semstat.sem_perm.gid))
				semstat.sem_perm.gid = group_id;
			semstat.sem_perm.mode = perm;
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_SET, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					  ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_SET"), errno);
			semarg.val = GTM_ID;
			/*
			 * Following will set semaphore number 2 (=FTOK_SEM_PER_ID - 1)  value as GTM_ID.
			 * In case we have orphaned semaphore for some reason, mupip rundown will be
			 * able to identify GTM semaphores from the value and can remove.
			 */
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, SETVAL, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl SETVAL"), errno);
			/* Warning: We must read the sem_ctime using IPC_STAT after SETVAL, which changes it.
			 *	    We must NOT do any more SETVAL after this. Our design is to use
			 *	    sem_ctime as creation time of semaphore.
			 */
			semarg.buf = &semstat;
			if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
				rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control semctl IPC_STAT2"), errno);
			tsd->gt_sem_ctime.ctime = udi->gt_sem_ctime = semarg.buf->sem_ctime;
		} else
		{
			if (INVALID_SHMID == udi->shmid)
				/* if mu_rndwn_file gets standalone access of this region and
				 * somehow mupip process crashes, we can have semid != -1 but shmid == -1
				 */
				rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
						ERR_TEXT, 2, LEN_AND_LIT("semid is valid but shmid is invalid"));
			if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
				rts_error(VARLSTCNT(11) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
					ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl"), errno);
			else if (shmstat.shm_ctime != tsd->gt_shm_ctime.ctime)
			{
				GTM_ATTACH_SHM_AND_CHECK_VERS(vermismatch, shm_setup_ok);
				if (vermismatch)
				{
					GTM_VERMISMATCH_ERROR;
				} else
					rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
						ERR_TEXT, 2, LEN_AND_LIT("shm_ctime does not match"));
			}
			semarg.buf = &semstat;
			if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg))
				/* file header has valid semid but semaphore does not exist */
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name));
			else if (semarg.buf->sem_ctime != tsd->gt_sem_ctime.ctime)
			{
				GTM_ATTACH_SHM_AND_CHECK_VERS(vermismatch, shm_setup_ok);
				if (vermismatch)
				{
					GTM_VERMISMATCH_ERROR;
				} else
					rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
						ERR_TEXT, 2, LEN_AND_LIT("sem_ctime does not match"));
			}
		}
		/* We already have ftok semaphore of this region, so just plainly do semaphore operation */
		/* This is the database access control semaphore for any region */
		sop[0].sem_num = 0; sop[0].sem_op = 0;	/* Wait for 0 */
		sop[1].sem_num = 0; sop[1].sem_op = 1;	/* Lock */
		sopcnt = 2;
		if (!read_only)
		{
			sop[2].sem_num = 1; sop[2].sem_op  = 1;	 /* increment r/w access counter */
			sopcnt = 3;
		}
		sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
		SEMOP(udi->semid, sop, sopcnt, status, NO_WAIT);
		if (-1 == status)
		{
			errno_save = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, errno_save);
		}
	} else /* for have_standalone_access we were already in "mu_rndwn_file" and got "semid" semaphore  */
	{	/* Make sure "mu_rndwn_file" has created semaphore for standalone access */
		if (INVALID_SEMID == udi->semid || 0 == udi->gt_sem_ctime)
			GTMASSERT;
		/* Make sure "mu_rndwn_file" has reset shared memory. In pro, just clear it and proceed. */
		assert((INVALID_SHMID == udi->shmid) && (0 == udi->gt_shm_ctime));
		udi->shmid = INVALID_SHMID;	/* reset shmid so dbinit_ch does not get confused in case we go there */
		new_dbinit_ipc = TRUE;
	}
	sem_incremented = TRUE;
	if (new_dbinit_ipc)
	{	/* Create new shared memory using IPC_PRIVATE. System guarantees a unique id */
		GTM_WHITE_BOX_TEST(WBTEST_FAIL_ON_SHMGET, sec_size, GTM_UINT64_MAX);
		if (-1 == (status_l = udi->shmid = shmget(IPC_PRIVATE, sec_size, RWDALL | IPC_CREAT)))
		{
			udi->shmid = (int)INVALID_SHMID;
			status_l = INVALID_SHMID;
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database shmget"), errno);
		}
		tsd->shmid = udi->shmid;
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl IPC_STAT1"), errno);
		/* change group and permissions */
		if ((-1 != group_id) && (group_id != shmstat.shm_perm.gid))
			shmstat.shm_perm.gid = group_id;
		shmstat.shm_perm.mode = perm;
		if (-1 == shmctl(udi->shmid, IPC_SET, &shmstat))
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database control shmctl IPC_SET"), errno);
		/* Warning: We must read the shm_ctime using IPC_STAT after IPC_SET, which changes it.
		 *	    We must NOT do any more IPC_SET or SETVAL after this. Our design is to use
		 *	    shm_ctime as creation time of shared memory and store it in file header.
		 */
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
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
			rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(tsd->machine_name),
				  ERR_TEXT, 2, LEN_AND_LIT("shared memory is invalid"));
	}
	csa->critical = (mutex_struct_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SIZE);
	assert(((INTPTR_T)csa->critical & 0xf) == 0); 			/* critical should be 16-byte aligned */
#	ifdef CACHELINE_SIZE
	assert(0 == ((INTPTR_T)csa->critical & (CACHELINE_SIZE - 1)));
#	endif
	/* Note: Here we check jnl_sate from database file and its value cannot change without standalone access.
	 * The jnl_buff buffer should be initialized irrespective of read/write process */
	JNL_INIT(csa, reg, tsd);
	csa->shmpool_buffer = (shmpool_buff_hdr_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SPACE + JNL_SHARE_SIZE(tsd));
	/* Initialize memory for snapshot context */									\
	csa->ss_ctx = malloc(SIZEOF(snapshot_context_t));
	DEFAULT_INIT_SS_CTX((SS_CTX_CAST(csa->ss_ctx)));
	csa->lock_addrs[0] = (sm_uc_ptr_t)csa->shmpool_buffer + SHMPOOL_SECTION_SIZE;
	csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
	csa->total_blks = tsd->trans_hist.total_blks;   		/* For test to see if file has extended */
	if (new_dbinit_ipc)
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
	if (is_bg)
		csd = csa->hdr = (sgmnt_data_ptr_t)(csa->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd));
	else
	{
		csa->acc_meth.mm.mmblk_state = (mmblk_que_heads_ptr_t)(csa->lock_addrs[1] + 1);
		FSTAT_FILE(udi->fd, &stat_buf, stat_res);
		if (-1 == stat_res)
			rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		mm_prot = read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
		if (-1 == (sm_long_t)(csa->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)NULL,
									   (size_t)stat_buf.st_size,
									   mm_prot,
									   GTM_MM_FLAGS, udi->fd, (off_t)0)))
			rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
		csa->db_addrs[1] = csa->db_addrs[0] + stat_buf.st_size - 1;
		csd = csa->hdr = (sgmnt_data_ptr_t)csa->db_addrs[0];
	}
	/* If shm_setup_ok is TRUE, we are guaranteed that vermismatch is FALSE.  Therefore, we can safely
	 * dereference csa->nl->glob_sec_init without worrying about whether or not it could be at a different
	 * offset than the current version.
	 */
	if (shm_setup_ok && !csa->nl->glob_sec_init)
	{
		assert(new_dbinit_ipc);
		assert(!vermismatch);
		if (is_bg)
		{
			memcpy(csd, tsd, SIZEOF(sgmnt_data));
			fc->file_type = dba_bg;
			fc->op = FC_READ;
			fc->op_buff = MM_ADDR(csd);
			fc->op_len = MASTER_MAP_SIZE(csd);
			fc->op_pos = MM_BLOCK;
			dbfilop(fc);
		}
		if (csd->machine_name[0])                  /* crash occured */
		{
			if (0 != STRNCMP_STR(csd->machine_name, machine_name, MAX_MCNAMELEN))  /* crashed on some other node */
				rts_error(VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg), LEN_AND_STR(csd->machine_name));
			else
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csd->machine_name));
		}
		if (is_bg)
		{
			bt_malloc(csa);
			csa->nl->cache_off = -CACHE_CONTROL_SIZE(tsd);
			db_csh_ini(csa);
		}
		db_csh_ref(csa);
		shmpool_buff_init(reg);
		SS_INFO_INIT(csa);
		STRNCPY_STR(csa->nl->machine_name, machine_name, MAX_MCNAMELEN);			/* machine name */
		assert(MAX_REL_NAME > gtm_release_name_len);
		memcpy(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M release name */
		memcpy(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1);				/* GDS label */
		memcpy(csa->nl->fname, reg->dyn.addr->fname, reg->dyn.addr->fname_len);		/* database filename */
		if (REPL_ALLOWED(csd))
		{
			if (!repl_inst_get_name(instfilename, &full_len, MAX_FN_LEN + 1, issue_rts_error))
				GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
			assert(full_len);
			memcpy(csa->nl->replinstfilename, instfilename, full_len);
		}
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
		gvstats_rec_csd2cnl(csa);	/* should be called before "db_auto_upgrade" */
		if (!read_only)
		{
			if (is_bg)
			{
				assert(memcmp(csd, GDS_LABEL, GDS_LABEL_SZ - 1) == 0);
				LSEEKWRITE(udi->fd, (off_t)0, (sm_uc_ptr_t)csd, SIZEOF(sgmnt_data), errno_save);
				if (0 != errno_save)
				{
					rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						  ERR_TEXT, 2, LEN_AND_LIT("Error with database write"), errno_save);
				}
			}
		}
		reg->dyn.addr->ext_blk_count = csd->extension_size;
		mlk_shr_init(csa->lock_addrs[0], csd->lock_space_size, csa, (FALSE == read_only));
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (read_only)
			csa->nl->remove_shm = TRUE;	/* gds_rundown can remove shmem if first process has read-only access */
		db_auto_upgrade(reg);
		if (FALSE == csd->multi_site_open)
		{	/* first time database is opened after upgrading to a GTM version that supports multi-site replication */
			csd->zqgblmod_seqno = 0;
			csd->zqgblmod_tn = 0;
			csd->dualsite_resync_seqno = csd->pre_multisite_resync_seqno;
			assert(csd->dualsite_resync_seqno);
			assert(csd->dualsite_resync_seqno <= csd->reg_seqno);
			if (!csd->dualsite_resync_seqno)
				csd->dualsite_resync_seqno = 1;
			else if (csd->dualsite_resync_seqno > csd->reg_seqno)
				csd->dualsite_resync_seqno = csd->pre_multisite_resync_seqno = csd->reg_seqno;
			csd->multi_site_open = TRUE;
		}
		csa->nl->glob_sec_init = TRUE;
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
			rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno_save);
		set_gdid_from_stat(&csa->nl->unique_id.uid, &stat_buf);
#	ifdef RELEASE_LATCH_GLOBAL
		/* On HP-UX, it is possible that mucregini/cs_data is not aligned at the same address
		 * boundary as csd would be in shared memory. This may lead to the initialization and
		 * usage of different elements of hp_latch_space. This may lead to the latch being
		 * "in-use" permanently. To resolve this, shm-initialer re-initializes the global latch
		 * to the "available" state.
		 * Although Solaris doesn't have the same issue of alignment, we'll cover the case of
		 * a corrupt latch (say in case of abnormal process termination).
		 */
		RELEASE_LATCH_GLOBAL(&csd->next_upgrd_warn.time_latch);
#	endif
	} else
	{
		if (STRNCMP_STR(csa->nl->machine_name, machine_name, MAX_MCNAMELEN))       /* machine names do not match */
		{
			if (csa->nl->machine_name[0])
				rts_error(VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name));
			else
				rts_error(VARLSTCNT(6) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name));
		}
		/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
		 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' atleast at csa->nl->fname[MAX_FN_LEN]
		 */
		assert(csa->nl->fname[MAX_FN_LEN] == '\0');	/* Note: the first '\0' in csa->nl->fname can be much earlier */
		/* Check whether csa->nl->fname exists. If not, then it is a serious condition. Error out. */
		STAT_FILE((char *)csa->nl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			errno_save = errno;
			send_msg(VARLSTCNT(13) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBNAMEMISMATCH, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname, errno_save);
			rts_error(VARLSTCNT(13) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBNAMEMISMATCH, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname, errno_save);
		}
		/* Check whether csa->nl->fname and csa->nl->unique_id.uid are in sync. If not error out. */
		if (FALSE == is_gdid_stat_identical(&csa->nl->unique_id.uid, &stat_buf))
		{
			send_msg(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid);
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBIDMISMATCH, 4, csa->nl->fname, DB_LEN_STR(reg), udi->shmid);
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
			send_msg(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname);
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_DBSHMNAMEDIFF, 4, DB_LEN_STR(reg), udi->shmid, csa->nl->fname);
		}
		if (csa->nl->donotflush_dbjnl)
		{
			assert(FALSE);
			rts_error(VARLSTCNT(10) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				ERR_TEXT, 2, LEN_AND_LIT("mupip recover/rollback created shared memory. Needs MUPIP RUNDOWN"));
		}
		/* verify pointers from our calculation vs. the copy in shared memory */
		if (csa->nl->critical != (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("critical"),
					(uint4)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->critical);
		if ((JNL_ALLOWED(csa)) &&
		    (csa->nl->jnl_buff != (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl)))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("journal buffer"),
					(uint4)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->jnl_buff);
		if (csa->nl->shmpool_buffer != (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("backup buffer"),
				  (uint4)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->shmpool_buffer);
		if ((is_bg) && (csa->nl->hdr != (sm_off_t)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl)))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("file header"),
					(uint4)((sm_uc_ptr_t)csd - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->hdr);
		if (csa->nl->lock_addrs != (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl))
			rts_error(VARLSTCNT(12) ERR_REQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(csa->nl->machine_name),
				  ERR_NLMISMATCHCALC, 4, LEN_AND_LIT("lock address"),
				  (uint4)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->lock_addrs);
	}
	/* Record  ftok information as soon as shared memory set up is done */
	FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_lock, process_id);
	if (-1 == (semval = semctl(udi->semid, 1, GETVAL))) /* semval = number of process attached */
	{
		errno_save = errno;
		gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl()"), CALLFROM, errno_save);
	}
	if (!read_only && 1 == semval)
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
		LSEEKWRITE(udi->fd, (off_t)0, (sm_uc_ptr_t)csd, SIZEOF(sgmnt_data), errno_save);
		if (0 != errno_save)
		{
			rts_error(VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("Error with database header flush"), errno_save);
		}
	} else if (read_only && new_dbinit_ipc)
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
		if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
			rts_error(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				  ERR_TEXT, 2, LEN_AND_LIT("gtmsecshr failed to update database file header"));

	}
	if (gtm_fullblockwrites)
	{	/* We have been asked to do FULL BLOCK WRITES for this database. On *NIX, attempt to get the filesystem
		   blocksize from statvfs. This allows a full write of a blockwithout the OS having to fetch the old
		   block for a read/update operation. We will round the IOs to the next filesystem blocksize if the
		   following criteria are met:

		   1) Database blocksize must be a whole multiple of the filesystem blocksize for the above
		      mentioned reason.

		   2) Filesystem blocksize must be a factor of the location of the first data block
		      given by the start_vbn.

		   The saved length (if the feature is enabled) will be the filesystem blocksize and will be the
		   length that a database IO is rounded up to prior to initiation of the IO.
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
		}
		else
		{
			errno_save = errno;
			send_msg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fstatvfs"), CALLFROM, errno_save);
		}
	}
	++csa->nl->ref_cnt;	/* This value is changed under control of the init/rundown semaphore only */
	assert(!csa->ref_cnt);	/* Increment shared ref_cnt before private ref_cnt increment. */
	csa->ref_cnt++;		/* Currently journaling logic in gds_rundown() in VMS relies on this order to detect last writer */
	if (!have_standalone_access)
	{
		/* Release control lockout now that it is init'd */
		if (0 != (errno_save = do_semop(udi->semid, 0, -1, SEM_UNDO)))
		{
			errno_save = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, errno_save);
		}
		sem_incremented = FALSE;
	}
#	ifdef DEBUG
	if (gtm_white_box_test_case_enabled && (WBTEST_SEMTOOLONG_STACK_TRACE == gtm_white_box_test_case_number) \
										&& (1 == csa->nl->wbox_test_seq_num))
	{
		csa->nl->wbox_test_seq_num  = 2;
		/*Wait till the other process has got some stack traces*/
		while (csa->nl->wbox_test_seq_num  != 3)
			LONG_SLEEP(10);
	}
#	endif
	/* Release ftok semaphore lock so that any other ftok conflicted database can continue now */
	if (!ftok_sem_release(reg, FALSE, FALSE))
		rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
	FTOK_TRACE(csa, csd->trans_hist.curr_tn, ftok_ops_release, process_id);
	/* Do the per process initialization of mutex stuff */
	assert(!mutex_per_process_init_pid || mutex_per_process_init_pid == process_id);
	if (!mutex_per_process_init_pid)
		mutex_per_process_init();
	return;
}
