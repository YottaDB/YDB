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

#include "gtm_ipc.h"
#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#include "gtm_sem.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gtmio.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "mutex.h"
#include "lockconst.h"
#include "interlock.h"
#include "aswp.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "mu_rndwn_file.h"
#include "performcaslatchcheck.h"
#include "util.h"
#include "send_msg.h"
#include "tp_change_reg.h"
#include "dbfilop.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */
#include "do_semop.h"
#include "ipcrmid.h"
#include "rc_cpt_ops.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "do_shmat.h"
#include "is_file_identical.h"
#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "ftok_sems.h"
#include "mu_rndwn_all.h"
#include "error.h"
#include "anticipatory_freeze.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "db_snapshot.h"
#include "shmpool.h"	/* Needed for the shmpool structures */
#include "is_proc_alive.h"
#include "ss_lock_facility.h"
#include "cli.h"
#include "gtm_file_stat.h"

#ifndef GTM_SNAPSHOT
# error "Snapshot facility not available in this platform"
#endif

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			process_id;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	gd_region		*ftok_sem_reg;
#ifdef DEBUG
GBLREF	boolean_t		in_mu_rndwn_file;
#endif

static gd_region	*rundown_reg = NULL;
static gd_region	*temp_region;
static sgmnt_data_ptr_t	temp_cs_data;
static sgmnt_addrs	*temp_cs_addrs;
static boolean_t	restore_rndwn_gbl;
static boolean_t	mu_rndwn_file_standalone;

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

error_def(ERR_BADDBVER);
error_def(ERR_DBFILERR);
error_def(ERR_DBIDMISMATCH);
error_def(ERR_DBNAMEMISMATCH);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBRDONLY);
error_def(ERR_DBSHMNAMEDIFF);
error_def(ERR_JNLORDBFLU);
error_def(ERR_MURNDWNOVRD);
error_def(ERR_MUUSERECOV);
error_def(ERR_MUUSERLBK);
error_def(ERR_SEMREMOVED);
error_def(ERR_SHMREMOVED);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

#define RESET_GV_CUR_REGION													\
{																\
	gv_cur_region = temp_region;												\
	cs_addrs = temp_cs_addrs;												\
	cs_data = temp_cs_data;													\
}

#define	CLNUP_AND_RETURN(REG, UDI, TSD, SEM_CREATED, SEM_INCREMENTED)								\
{																\
	int		rc;													\
																\
	if (FD_INVALID != UDI->fd)												\
	{															\
		CLOSEFILE_RESET(UDI->fd, rc);											\
		assert(FD_INVALID == UDI->fd);											\
	}															\
	if (NULL != TSD)													\
	{															\
		free(TSD);													\
		TSD = NULL;													\
	}															\
	if (SEM_INCREMENTED)													\
	{															\
		do_semop(udi->semid, DB_CONTROL_SEM, -1, IPC_NOWAIT | SEM_UNDO);						\
		SEM_INCREMENTED = FALSE;											\
	}															\
	if (SEM_CREATED)													\
	{															\
		if (-1 == sem_rmid(UDI->semid))											\
		{														\
			RNDWN_ERR("!AD -> Error removing semaphore.", REG);							\
		} else														\
			SEM_CREATED = FALSE;											\
	}															\
	REVERT;															\
	assert((NULL == ftok_sem_reg) || (REG == ftok_sem_reg));								\
	if (REG == ftok_sem_reg)												\
		ftok_sem_release(REG, TRUE, TRUE);										\
	if (restore_rndwn_gbl)													\
	{															\
		RESET_GV_CUR_REGION;												\
		restore_rndwn_gbl = FALSE;											\
	}															\
	return FALSE;														\
}

#define SEG_SHMATTACH(addr, reg, udi, tsd, sem_created, sem_incremented)							\
{																\
	if (-1 == (sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)								\
				do_shmat(udi->shmid, addr, SHM_RND)))								\
	{															\
		if (EINVAL != errno)												\
			RNDWN_ERR("!AD -> Error attaching to shared memory", (reg));						\
		/* shared memory segment no longer exists */									\
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, sem_incremented);							\
	}															\
}

#define REMOVE_SEMID_IF_ORPHANED(REG, UDI, TSD, SEM_CREATED, SEM_INCREMENTED)							\
{																\
	if (is_orphaned_gtm_semaphore(UDI->semid))										\
	{															\
		if (0 != sem_rmid(UDI->semid))											\
		{														\
			RNDWN_ERR("!AD -> Error removing semaphore.", reg);							\
			CLNUP_AND_RETURN(REG, UDI, TSD, SEM_CREATED, SEM_INCREMENTED);						\
		}														\
		send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(3) ERR_SEMREMOVED, 1, UDI->semid);					\
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(3) ERR_SEMREMOVED, 1, UDI->semid);					\
		UDI->semid = INVALID_SEMID;											\
	}															\
}

/* Print an error message that, based on whether replication was enabled at the time of the crash, would instruct
 * the user to a more appropriate operation than RUNDOWN, such as RECOVER or REQROLLBACK.
 */
#define PRINT_PREVENT_RUNDOWN_MESSAGE(REG)								\
{													\
	if (REPL_ENABLED(tsd) && tsd->jnl_before_image)							\
	{												\
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_MUUSERLBK, 2, DB_LEN_STR(REG),		\
			ERR_TEXT, 2, LEN_AND_LIT("Run MUPIP JOURNAL ROLLBACK"));			\
	} else												\
	{												\
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_MUUSERECOV, 2, DB_LEN_STR(REG),	\
			ERR_TEXT, 2, LEN_AND_LIT("Run MUPIP JOURNAL RECOVER"));				\
	}												\
}

/* Description:
 *	This routine is used for two reasons
 *		1) get standalone access:
 *			First uses/creates ftok semaphore.
 *			Then create a new semaphore for that region.
 *			Releases ftok semaphore (does not remove).
 *		2) rundown shared memory
 *			Uses/creates ftok semaphore.
 * Parameters:
 *	standalone = TRUE  => create semaphore to get standalone access
 *	standalone = FALSE => rundown shared memory
 *	Note:	Currently there are no callers with standalone == FALSE other
 *		than MUPIP RUNDOWN.
 * Return Value:
 *	TRUE for success
 *	FALSE for failure
 */
boolean_t mu_rndwn_file(gd_region *reg, boolean_t standalone)
{
	int			status, save_errno, sopcnt, tsd_size, save_udi_semid = INVALID_SEMID, semop_res, stat_res, rc;
	int			csd_size;
	char                    now_running[MAX_REL_NAME];
	boolean_t		rc_cpt_removed = FALSE, sem_created = FALSE, is_gtm_shm;
	boolean_t		glob_sec_init, db_shm_in_sync, remove_shmid, no_shm_exists;
	sgmnt_data_ptr_t	csd, tsd = NULL;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	struct sembuf		sop[4], *sopptr;
	struct shmid_ds		shm_buf;
	file_control		*fc;
	unix_db_info		*udi;
	enum db_acc_method	acc_meth;
        struct stat     	stat_buf;
	struct semid_ds		semstat;
	union semun		semarg;
	uint4			status_msg, ss_pid;
	shm_snapshot_t		*ss_shm_ptr;
	gtm_uint64_t		sec_size, mmap_sz = 0;
#	ifdef GTM_CRYPT
	gd_segment		*seg;
	int			gtmcrypt_errno;
#	endif
	boolean_t		override_present, wcs_flu_success, prevent_mu_rndwn;
	unsigned char		*fn;
	mstr 			jnlfile;
	int			jnl_fd;
	jnl_file_header		header;
	int4			status1;
	uint4			status2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_rndwn_file_standalone = standalone;
	restore_rndwn_gbl = FALSE;
	assert(!jgbl.onlnrlbk);
	assert(!mupip_jnl_recover || standalone);
	temp_region = gv_cur_region; 	/* save gv_cur_region wherever there is scope for it to be changed */
	rundown_reg = gv_cur_region = reg;
#	ifdef GTCM_RC
        rc_cpt_removed = mupip_rundown_cpt();
#	endif
	fc = reg->dyn.addr->file_cntl;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	udi = FILE_INFO(reg);
	csa = &(udi->s_addrs);		/* Need valid cs_addrs in is_anticipatory_freeze_needed, which can be called */
	cs_addrs = csa;			/* by gtm_putmsg(), so set it up here. */
	if (SS_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) status, 2, DB_LEN_STR(reg), errno);
		if (FD_INVALID != udi->fd)	/* Since dbfilop failed, close udi->fd only if it was opened */
			CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
                return FALSE;
	}
	/* read_only process cannot rundown database.
	 * read only process can succeed to get standalone access of the database,
	 *	if the db is clean with no orphaned shared memory.
	 * Note: we use gtmsecshr for updating file header for semaphores id.
	 */
	if (reg->read_only && !standalone)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		return FALSE;
	}
	ESTABLISH_RET(mu_rndwn_file_ch, FALSE);
	if (!ftok_sem_get(reg, TRUE, GTM_ID, !standalone))
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	/* Now we have standalone access of the database using ftok semaphore. Any other ftok conflicted database suspends
	 * their operation at this point. At the end of this routine, we release ftok semaphore lock.
	 */
	tsd_size = ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
	tsd = (sgmnt_data_ptr_t)malloc(tsd_size);
	LSEEKREAD(udi->fd, 0, tsd, tsd_size, status);
	if (0 != status)
	{
		RNDWN_ERR("!AD -> Error reading from file.", reg);
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	}
	csa->hdr = tsd;
	csa->region = gv_cur_region;
#	ifdef GTM_CRYPT
	if (tsd->is_encrypted)
	{
		csa = &(udi->s_addrs);
		INIT_PROC_ENCRYPTION(csa, gtmcrypt_errno);
		if (0 == gtmcrypt_errno)
			INIT_DB_ENCRYPTION(csa, tsd, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = reg->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
	}
#	endif
	CSD2UDI(tsd, udi);
	semarg.buf = &semstat;
	REMOVE_SEMID_IF_ORPHANED(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	if (INVALID_SEMID == udi->semid || (-1 == semctl(udi->semid, DB_CONTROL_SEM, IPC_STAT, semarg)) ||
#		ifdef GTM64
		(((tsd->gt_sem_ctime.ctime & 0xffffffff) == 0) && ((tsd->gt_sem_ctime.ctime >> 32) != semarg.buf->sem_ctime)) ||
#		endif
		tsd->gt_sem_ctime.ctime != semarg.buf->sem_ctime)
	{	/* Access control semaphore doesn't exist. Create one. Note that if creation time does not match, we ignore that
		 * semaphore and assume it never existed. Argumentless mupip rundown will remove such a semaphore later.
		 */
		if (-1 == (udi->semid = semget(IPC_PRIVATE, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
		{
			udi->semid = INVALID_SEMID;
			RNDWN_ERR("!AD -> Error with semget with IPC_CREAT.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		sem_created = TRUE;
		tsd->semid = udi->semid;
		/*
		 * Set value of semaphore number 2 ( = FTOK_SEM_PER_ID - 1) as GTM_ID.
		 * In case we have orphaned semaphore for some reason, mupip rundown will be
		 * able to identify GTM semaphores from the value and can remove.
		 */
		semarg.val = GTM_ID;
		if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, SETVAL, semarg))
		{
			RNDWN_ERR("!AD -> Error with semctl with SETVAL.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		/*
		 * Warning: We must read the sem_ctime after SETVAL, which changes it.
		 *	    We must not do any more SETVAL after this.
		 *	    We consider sem_ctime as creation time of semaphore.
		 */
		semarg.buf = &semstat;
		if (-1 == semctl(udi->semid, FTOK_SEM_PER_ID - 1, IPC_STAT, semarg))
		{
			RNDWN_ERR("!AD -> Error with semctl with IPC_STAT.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		udi->gt_sem_ctime = tsd->gt_sem_ctime.ctime = semarg.buf->sem_ctime;
	}
#	if defined(GTM64) && defined(BIGENDIAN)
	/* If the semaphore was created by a 32-bit big-endian version of GT.M the correct ctime will be in the
	 * upper 32 bits and the lower 32 bits will be zero. Detect this case and adjust the time. We expect it to
	 * error out later due to version mismatch or other conflict.
	 */
	if (((tsd->gt_sem_ctime.ctime & 0xffffffff) == 0) && ((tsd->gt_sem_ctime.ctime >> 32) == semarg.buf->sem_ctime))
		tsd->gt_sem_ctime.ctime >>= 32;
#	endif
	/* Now lock the database using access control semaphore and increment counter. Typically, multiple statements are not
	 * specified in a single line. However, each of the 4 lines below represent "one" semaphore operation and hence an
	 * acceptible exception to the coding guidelines.
	 */
	sop[0].sem_num = DB_CONTROL_SEM; sop[0].sem_op = 0; /* wait for access control semaphore to be available */
	sop[1].sem_num = DB_CONTROL_SEM; sop[1].sem_op = 1; /* lock it */
	sop[2].sem_num = DB_COUNTER_SEM; sop[2].sem_op = 0; /* wait for counter semaphore to become 0 */
	sop[3].sem_num = DB_COUNTER_SEM; sop[3].sem_op = 1; /* increment the counter semaphore */
#	if defined(GTM64) && defined(BIGENDIAN)
	/* If the shared memory was created by a 32-bit big-endian version of GT.M the correct ctime will be in the
	 * upper 32 bits and the lower 32 bits will be zero. Detect this case and adjust the time. We expect it to
	 * error out later due to version mismatch or other conflict.
	 */
	no_shm_exists = (INVALID_SHMID == udi->shmid || -1 == shmctl(udi->shmid, IPC_STAT, &shm_buf));
	if (!no_shm_exists && ((tsd->gt_shm_ctime.ctime & 0xffffffff) == 0) &&
				((tsd->gt_shm_ctime.ctime >> 32) == shm_buf.shm_ctime))
		tsd->gt_shm_ctime.ctime >>= 32;
	no_shm_exists |= (tsd->gt_shm_ctime.ctime != shm_buf.shm_ctime);
#	else
	no_shm_exists = (INVALID_SHMID == udi->shmid || -1 == shmctl(udi->shmid, IPC_STAT, &shm_buf) ||
				tsd->gt_shm_ctime.ctime != shm_buf.shm_ctime);
#	endif
	/* Whether we want to wait for the counter semaphore to become zero NOW (or defer it) depends on the following conditions :
	 * a) MUPIP RUNDOWN : If shared memory exists, then it is possible that the database we are attempting to rundown does not
	 * match the existing shared memory (DBSHMNAMEDIFF case). If so, we should not wait for the counter semaphore, but proceed
	 * with the rest of the rundown (!db_shm_in_sync case). So, we should NOT wait for the counter semaphore at THIS stage. If
	 * we find later that the db and shared memory is in sync, we can then wait for the counter semaphore before doing the
	 * actual rundown.
	 * If shared memory doesn't exist, it is not possible to have DBSHMNAMEDIFF case. So, wait for the counter semaphore to
	 * become zero NOW before proceeding with LSEEKWRITEs of the file header to nullify shmid/semid and other fields.
	 *
	 * b) STANDALONE ACCESS : In this case (like Regular NOONLINE ROLLBACK or MUPIP INTEG -FILE), the scenario is similar to
	 * MUPIP RUNDOWN. So, wait for counter semaphore if no shared memory exists. If shared memory exists, defer waiting for
	 * counter semaphore until later (when db_shm_in_sync is TRUE).
	 */
	sopcnt = no_shm_exists ? 4 : 2;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(udi->semid, sop, sopcnt, semop_res, NO_WAIT);
	if (-1 == semop_res)
	{
		RNDWN_ERR("!AD -> File already open by another process.", reg);
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	}
	udi->grabbed_access_sem = TRUE;
	udi->counter_acc_incremented = no_shm_exists;
	override_present = (cli_present("OVERRIDE") == CLI_PRESENT);
	/* Proceed with rundown if either journaling is off or we got here as a result of MUPIP JOURNAL -RECOVER or
	 * MUPIP JOURNAL -ROLLBACK, unless the OVERRIDE qualifier is present (see the following code).
	 */
	prevent_mu_rndwn = JNL_ENABLED(tsd) && !standalone;
	/* Now rundown database if shared memory segment exists. We try this for both values of 'standalone'. */
	if (no_shm_exists)
	{
		if (prevent_mu_rndwn)
		{
			if (override_present)
			{	/* If the rundown should normally be prevented, but the operator specified an OVERRIDE qualifier,
				 * record the fact of the usage in the syslog and continue.
				 */
				send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_MURNDWNOVRD, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
					LEN_AND_LIT("Overriding enabled journaling state"));
			} else
			{	/* Issue error if the crash bit in the journal file is set thereby preventing the user from doing a
				 * less appropriate operation than RECOVER or ROLLBACK.
				 */
				jnlfile.addr = (char *)tsd->jnl_file_name;
				jnlfile.len = tsd->jnl_file_len;
				if (FILE_PRESENT & gtm_file_stat(&jnlfile, NULL, NULL, TRUE, &status2))
				{	/* The journal file exists. */
					assert('\0' == jnlfile.addr[jnlfile.len]);
					jnlfile.addr[jnlfile.len] = '\0';	/* In case the above assert fails. */
					OPENFILE(jnlfile.addr, O_RDONLY, jnl_fd);
					if (0 <= jnl_fd)
					{
						DO_FILE_READ(jnl_fd, 0, &header, SIZEOF(header), status1, status2);
						if (SS_NORMAL == status1)
						{	/* FALSE in the call below is to skip gtm_putmsgs even on errors. */
							CHECK_JNL_FILE_IS_USABLE(&header, status1, FALSE, 0, NULL);
							if ((SS_NORMAL == status1)
							    && (ARRAYSIZE(header.data_file_name) > header.data_file_name_length))
							{
								assert('\0' == header.data_file_name[header.data_file_name_length]);
								header.data_file_name[header.data_file_name_length] = '\0';
								if (is_file_identical((char *)header.data_file_name,
								    (char *)gv_cur_region->dyn.addr->fname) && header.crash)
								{
									PRINT_PREVENT_RUNDOWN_MESSAGE(reg);
								}
							}
						}
					}
				}
			}
		}
		if (rc_cpt_removed)
		{       /* reset RC values if we've rundown the RC CPT */
			/* attempt to force-write header */
			tsd->rc_srv_cnt = tsd->dsid = tsd->rc_node = 0;
			assert(FALSE);	/* not sure what to do here. handle it if/when it happens */
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		} else
		{	/* Note that if creation time does not match, we ignore that shared memory segment. It might result in
			 * orphaned shared memory segment which can be later removed with argument-less MUPIP RUNDOWN.
			 */
			tsd->shmid = udi->shmid = INVALID_SHMID;
			tsd->gt_shm_ctime.ctime = udi->gt_shm_ctime = 0;
			if (standalone)
			{
				if (!reg->read_only)
				{
					if (mupip_jnl_recover)
						memset(tsd->machine_name, 0, MAX_MCNAMELEN);
					DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, tsd, tsd_size, status);
					if (0 != status)
					{
						RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
				} else
				{
					db_ipcs.semid = tsd->semid;
					db_ipcs.gt_sem_ctime = tsd->gt_sem_ctime.ctime;
					db_ipcs.shmid = tsd->shmid;
					db_ipcs.gt_shm_ctime = tsd->gt_shm_ctime.ctime;
					if (!get_full_path((char *)DB_STR_LEN(reg), db_ipcs.fn, &db_ipcs.fn_len,
											MAX_TRANS_NAME_LEN, &status_msg))
					{
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(1) status_msg);
						RNDWN_ERR("!AD -> get_full_path failed.", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
					db_ipcs.fn[db_ipcs.fn_len] = 0;
					WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
					if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
					{
						RNDWN_ERR("!AD -> gtmsecshr was unable to write header to disk.", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
				}
				if (!ftok_sem_release(reg, FALSE, FALSE))
				{
					RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
				REVERT;
				free(tsd);
				assert(udi->grabbed_access_sem);
				return TRUE; /* For "standalone" and "no shared memory existing", we exit here */
			} else
			{	/* We are here for not standalone (basically the "mupip rundown" command). */
				if (0 != do_semop(udi->semid, DB_CONTROL_SEM, -1, IPC_NOWAIT | SEM_UNDO))
				{
					assert(FALSE); /* We incremented the semaphore, so we should be able to decrement it */
					RNDWN_ERR("!AD -> Error decrementing semaphore.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				udi->counter_acc_incremented = FALSE;
				if (sem_created && (0 != sem_rmid(udi->semid)))
				{
					assert(FALSE); /* We've created the semaphore, so we should be able to remove it */
					RNDWN_ERR("!AD -> Error removing semaphore.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				sem_created = FALSE;
				udi->grabbed_access_sem = FALSE;
				udi->semid = INVALID_SEMID; /* "orphaned" and "newly" created semaphores are now removed */
				/* Reset IPC fields in the file header and exit */
				memset(tsd->machine_name, 0, MAX_MCNAMELEN);
				tsd->freeze = 0;
				RESET_IPC_FIELDS(tsd);
			}
		}
		assert(!standalone);
		DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, tsd, tsd_size, status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		free(tsd);
		REVERT;
		/* For mupip rundown (standalone = FALSE), we release/remove ftok semaphore here. */
		if (!ftok_sem_release(reg, TRUE, TRUE))
		{
			RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
			return FALSE;
		}
		return TRUE; /* For "!standalone" and "no shared memory existing", we exit here */
	}
	if (reg->read_only)             /* read only process can't succeed beyond this point */
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	}
	/* Now we have a pre-existing shared memory section. Do some setup */
	if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
       	{
		if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBNOTGDS, 2, DB_LEN_STR(reg));
		else
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_BADDBVER, 2, DB_LEN_STR(reg));
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
       	}
	reg->dyn.addr->acc_meth = acc_meth = tsd->acc_meth;
	dbsecspc(reg, tsd, &sec_size);
#	ifdef __MVS__
	/* match gvcst_init_sysops.c shmget with __IPC_MEGA or _LP64 */
	if (ROUND_UP(sec_size, MEGA_BOUND) != shm_buf.shm_segsz)
#	else
	if (sec_size != shm_buf.shm_segsz)
#	endif
	{
		util_out_print("Expected shared memory size is !SL, but found !SL",
			TRUE, sec_size, shm_buf.shm_segsz);
		util_out_print("!AD -> Existing shared memory size do not match.", TRUE, DB_LEN_STR(reg));
		/* fall through to get VERMISMATCH message */
	}
	/* save gv_cur_region, cs_data, cs_addrs, and restore them on return */
	temp_region = gv_cur_region;
	temp_cs_data = cs_data;
	temp_cs_addrs = cs_addrs;
	restore_rndwn_gbl = TRUE;
	gv_cur_region = reg;
	tp_change_reg();
	SEG_SHMATTACH(0, reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	cs_addrs->nl = (node_local_ptr_t)cs_addrs->db_addrs[0];
	if (prevent_mu_rndwn && cs_addrs->nl->jnl_file.u.inode)
	{
		if (override_present)
		{	/* If the rundown should normally be prevented, but the operator specified an OVERRIDE qualifier, record
			 * the fact of the usage in the syslog and continue.
			 */
			send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_MURNDWNOVRD, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
				LEN_AND_LIT("Overriding OPEN journal file state in shared memory"));
		} else
		{	/* Journal file state being still open in shared memory implies a crashed state, so error out. */
			PRINT_PREVENT_RUNDOWN_MESSAGE(reg);
		}
	}
	/* The following checks for GDS_LABEL_GENERIC, gtm_release_name, and cs_addrs->nl->glob_sec_init ensure that the
	 * shared memory under consideration is valid.  First, since cs_addrs->nl->label is in the same place for every
	 * version, a failing check means it is most likely NOT a GT.M created shared memory, so no attempt will be
	 * made to delete it. Next a successful match of the currently running release will guarantee that all fields in
	 * the structure are where they're expected to be -- without this check the glob_sec_init check should not be done
	 * as this field is at different offsets in different versions. Finally, we can check the glob_sec_init flag to
	 * verify that shared memory has been completely initialized. If not, we should delete it right away. [C9D07-002355]
	 */
	if (!MEMCMP_LIT(cs_addrs->nl->label, GDS_LABEL_GENERIC))
	{
		is_gtm_shm = TRUE;
		remove_shmid = TRUE;
		memcpy(now_running, cs_addrs->nl->now_running, MAX_REL_NAME);
		if (memcmp(now_running, gtm_release_name, gtm_release_name_len + 1))
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len,
				   gtm_release_name, LEN_AND_STR(now_running));
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		if (cs_addrs->nl->glob_sec_init)
		{
			glob_sec_init = TRUE;
			if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1))
			{
				if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 3))
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBNOTGDS, 2, DB_LEN_STR(reg),
						   ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
				else
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_BADDBVER, 2, DB_LEN_STR(reg),
						   ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
				CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			}
			/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
			 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' at least at
			 * cs_addrs->nl->fname[MAX_FN_LEN]
			 */
			assert(cs_addrs->nl->fname[MAX_FN_LEN] == '\0');  /* First '\0' in cs_addrs->nl->fname can be earlier */
			db_shm_in_sync = TRUE;	/* assume this unless proven otherwise */
			/* Check whether cs_addrs->nl->fname exists. If not, then db & shm are not in sync. */
			STAT_FILE((char *)cs_addrs->nl->fname, &stat_buf, stat_res);
			if (-1 == stat_res)
			{
				save_errno = errno;
				db_shm_in_sync = FALSE;
				if (ENOENT == save_errno)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
							DB_LEN_STR(reg), udi->shmid, cs_addrs->nl->fname, save_errno);
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
							DB_LEN_STR(reg),
						udi->shmid, cs_addrs->nl->fname);
					/* In this case, the shared memory no longer points to a valid db file in the filesystem.
					 * So it is best that we remove this shmid. But remove_shmid is already TRUE. Assert that.
					 */
					assert(remove_shmid);
				} else /* Could be permission issue */
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
							DB_LEN_STR(reg),
						 udi->shmid, cs_addrs->nl->fname, save_errno);
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
							DB_LEN_STR(reg),
						udi->shmid, cs_addrs->nl->fname, save_errno);
					remove_shmid = FALSE; /* Shared memory might be pointing to valid database */
				}
			}
			if (db_shm_in_sync)
			{	/* Check if csa->nl->fname and csa->nl->dbfid are in sync. If not, then db & shm are not in sync */
				if (FALSE == is_gdid_stat_identical(&cs_addrs->nl->unique_id.uid, &stat_buf))
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(10) MAKE_MSG_INFO(ERR_DBIDMISMATCH), 4,
					      cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, ERR_TEXT, 2,
					      LEN_AND_LIT("[MUPIP] Database filename and fileid in shared memory are not in sync"));
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(10) MAKE_MSG_INFO(ERR_DBIDMISMATCH), 4,
					      cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, ERR_TEXT, 2,
					      LEN_AND_LIT("[MUPIP] Database filename and fileid in shared memory are not in sync"));
					db_shm_in_sync = FALSE;
					/* In this case, the shared memory points to a file that exists in the filesystem but
					 * the fileid of the currently present file does not match the file id recorded in
					 * shared memory at shm creation time. Therefore the database file has since been
					 * overwritten. In this case, we have no other option but to remove this shmid.
					 * remove_shmid is already TRUE. Assert that.
					 */
					assert(remove_shmid);
				}
			}
			/* Check if reg->dyn.addr->fname and csa->nl->fname are identical filenames.
			 * If not, then db that we want to rundown points to a shared memory which in turn points
			 * to a DIFFERENT db file. In this case, do NOT remove the shared memory as it is possible
			 * some other database file on the system points to it and passes the filename identicality check.
			 */
			if (db_shm_in_sync && !is_file_identical((char *)cs_addrs->nl->fname, (char *)reg->dyn.addr->fname))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBSHMNAMEDIFF), 4, DB_LEN_STR(reg),
					udi->shmid, cs_addrs->nl->fname);
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBSHMNAMEDIFF), 4, DB_LEN_STR(reg),
					udi->shmid, cs_addrs->nl->fname);
				db_shm_in_sync = FALSE;
				remove_shmid = FALSE;
			}
			wcs_flu_success = TRUE;
			/* If db & shm are not in sync at this point, skip the part of flushing shm to db file on disk.
			 * We still need to reset the fields (shmid, semid etc.) in db file header.
			 * About deleting the shmid, it depends on the type of out-of-sync between db & shm. This is handled
			 * by setting the variable "remove_shmid" appropriately in each case.
			 */
			if (db_shm_in_sync)
			{
				if (!udi->counter_acc_incremented)
				{	/* Now that we have ensured db & shm are in sync and will be doing the "actual"
					 * rundown, we need to ensure that no one is attached to the database (counter
					 * sempahore is 0)
					 */
					assert(!no_shm_exists);
					assert(udi->grabbed_access_sem);
					assert(INVALID_SEMID != udi->semid);
					sopcnt = 2; /* Need only the last 2 "sop" array */
					sopptr = &sop[2];
					SEMOP(udi->semid, sopptr, sopcnt, semop_res, NO_WAIT);
					if (-1 == semop_res)
					{
						RNDWN_ERR("!AD -> File already open by another process.", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
					udi->counter_acc_incremented = TRUE;
				}
				/* If db & shm are in sync AND we aren't alone in using it, we can do nothing */
				if (0 != shm_buf.shm_nattch)
				{
					util_out_print("!AD [!UL]-> File is in use by another process.",
							TRUE, DB_LEN_STR(reg), udi->shmid);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				/* The shared section is valid and up-to-date with respect to the database file header;
				 * ignore the temporary storage and use the shared section from here on
				 */
				cs_addrs->critical = (mutex_struct_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SIZE);
				assert(((INTPTR_T)cs_addrs->critical & 0xf) == 0);	/* critical should be 16-byte aligned */
#				ifdef CACHELINE_SIZE
				assert(0 == ((INTPTR_T)cs_addrs->critical & (CACHELINE_SIZE - 1)));
#				endif
				JNL_INIT(cs_addrs, reg, tsd);
				cs_addrs->shmpool_buffer = (shmpool_buff_hdr_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SPACE(tsd) +
					JNL_SHARE_SIZE(tsd));
				cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)cs_addrs->shmpool_buffer + SHMPOOL_SECTION_SIZE;
				cs_addrs->lock_addrs[1] = cs_addrs->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
				if (dba_bg == acc_meth)
				{
					cs_data = csd = csa->hdr = (sgmnt_data_ptr_t)(cs_addrs->lock_addrs[1] + 1
									+ CACHE_CONTROL_SIZE(tsd));
					assert(csa->nl->cache_off == -CACHE_CONTROL_SIZE(csd));
					db_csh_ini(csa);
				} else
				{
					cs_data = csd = csa->hdr = (sgmnt_data_ptr_t)((sm_uc_ptr_t)csa->lock_addrs[1] + 1);
					FSTAT_FILE(udi->fd, &stat_buf, stat_res);
					if (-1 == stat_res)
					{
						RNDWN_ERR("!AD -> Error with fstat.", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
					mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(csd);
					CHECK_LARGEFILE_MMAP(reg, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
					cs_addrs->db_addrs[0] = (sm_uc_ptr_t) MMAP_FD(udi->fd, mmap_sz, BLK_ZERO_OFF(csd), FALSE);
					if (-1 == (sm_long_t)(cs_addrs->db_addrs[0]))
					{
						RNDWN_ERR("!AD -> Error mapping memory", reg);
						CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					}
					cs_addrs->db_addrs[1] = cs_addrs->db_addrs[0] + mmap_sz - 1;
				}
				assert(sem_created ||
					((csd->semid == tsd->semid) && (csd->gt_sem_ctime.ctime == tsd->gt_sem_ctime.ctime)));
				if (sem_created && standalone)
				{
					csd->semid = tsd->semid;
					csd->gt_sem_ctime.ctime = tsd->gt_sem_ctime.ctime;
				}
				assert(JNL_ALLOWED(csd) == JNL_ALLOWED(tsd));
				free(tsd);
				tsd = NULL;
				/* Check to see that the fileheader in the shared segment is valid, so we wont end up
				 * flushing garbage to the db file.
				 * Check the label in the header - keep adding any further appropriate checks in future here.
				 */
				if (memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
				{
					if (memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 3))
					{
						status_msg = ERR_DBNOTGDS;
						if (0 != shm_rmid(udi->shmid))
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
								   ERR_TEXT, 2, RTS_ERROR_TEXT("Error removing shared memory"));
						else
						{
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_SHMREMOVED, 3, udi->shmid,
									DB_LEN_STR(reg));
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_SHMREMOVED, 3, udi->shmid,
									DB_LEN_STR(reg));
						}
					} else
						status_msg = ERR_BADDBVER;
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) status_msg, 2, DB_LEN_STR(reg),
					       ERR_TEXT, 2, RTS_ERROR_LITERAL("(File header in the shared segment seems corrupt)"));
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				db_common_init(reg, cs_addrs, csd); /* do initialization common to "db_init" and "mu_rndwn_file" */
				/* cleanup mutex stuff */
				cs_addrs->hdr->image_count = 0;
				gtm_mutex_init(reg, NUM_CRIT_ENTRY(cs_addrs->hdr), FALSE); /* this is the only process running */
				assert(!cs_addrs->hold_onto_crit); /* so it is safe to do unconditional grab_crit/rel_crit below */
				cs_addrs->nl->in_crit = 0;
				cs_addrs->now_crit = FALSE;
				reg->open = TRUE;
				if (rc_cpt_removed) /* reset RC values if we've rundown the RC CPT */
					csd->rc_srv_cnt = csd->dsid = csd->rc_node = 0;
				/* At this point we are holding standalone access and are about to invoke wcs_recover/wcs_flu. If
				 * one or more GT.M processes were at the midst of phase 2 commit, wcs_recover/wcs_flu invokes
				 * wcs_phase2_commit_wait to wait for the processes to complete the phase 2 commit. But, if we have
				 * standalone access, there is NO point waiting for the phase 2 commits to complete as the processes
				 * might have been killed. So, set wcs_phase2_commit_pidcnt to 0 so wcs_recover/wcs_flu skips
				 * invoking wcs_phase2_commit_wait
				 */
				cs_addrs->nl->wcs_phase2_commit_pidcnt = 0;
				DEBUG_ONLY(in_mu_rndwn_file = TRUE);
				TREF(donot_write_inctn_in_wcs_recover) = TRUE;
				/* If an orphaned snapshot is lying around, then clean it up */
				/* SS_MULTI: If multiple snapshots are supported, then we must run-through each of the
				 * "possible" snapshots in progress and clean them up
				 */
				assert(1 == MAX_SNAPSHOTS);
				ss_get_lock(gv_cur_region); /* Snapshot initiator will wait until this cleanup is done */
				ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(cs_addrs);
				ss_pid = ss_shm_ptr->ss_info.ss_pid;
				if (ss_pid && !is_proc_alive(ss_pid, 0))
					ss_release(NULL);
				ss_release_lock(gv_cur_region);
				/* If csa->nl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and
				 * therefore we need not flush shared memory contents to disk as they might be in an inconsistent
				 * state. Moreover, any more flushing will only cause the future rollback/recover to undo more
				 * journal records (PBLKs). In this case, we will go ahead and remove shared memory (without
				 * flushing the contents) in this routine. A reissue of the recover/rollback command will restore
				 * the database to a consistent state.
				 */
				if (!cs_addrs->nl->donotflush_dbjnl)
				{
					if (cs_addrs->nl->glob_sec_init)
					{	/* WCSFLU_NONE only is done here, as we aren't sure of the state, so no EPOCHs are
						 * written. If we write an EPOCH record, recover may get confused. Note that for
						 * journaling we do not call jnl_file_close() with TRUE for second parameter.
						 * As a result journal file might not have an EOF record.
						 * So, a new process will switch the journal file and cut the journal file link,
						 * though it might be a good journal without an EOF
						 */
						wcs_flu_success = wcs_flu(WCSFLU_NONE);
						if (!wcs_flu_success)
						{
							if (override_present && !standalone)
							{	/* Case of MUPIP RUNDOWN with OVERRIDE flag; continue. */
								send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_MURNDWNOVRD, 2,
									DB_LEN_STR(reg), ERR_TEXT, 2, LEN_AND_LIT(
									"Overriding error during database block flush"));
								csd = cs_addrs->hdr;
							} else
							{	/* In case of MUPIP RUNDOWN append a suggestion to use OVERRIDE flag
								 * to bypass the error.
								 */
								if (standalone)
									rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4)
										ERR_JNLORDBFLU, 2, DB_LEN_STR(reg));
								else
									rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(8)
										ERR_JNLORDBFLU, 2, DB_LEN_STR(reg),
										ERR_TEXT, 2, LEN_AND_LIT(
										"To force the operation to proceed, use the "
										"OVERRIDE qualifier"));
								assert(FALSE);	/* The above rts_error should not return. */
								return FALSE;
							}
						}
					}
					jpc = cs_addrs->jnl;
					if (NULL != jpc)
					{
						grab_crit(gv_cur_region);
						/* If we own it or owner died, clear the fsync lock */
						if (process_id == jpc->jnl_buff->fsync_in_prog_latch.u.parts.latch_pid)
						{
							RELEASE_SWAPLOCK(&jpc->jnl_buff->fsync_in_prog_latch);
						} else
							performCASLatchCheck(&jpc->jnl_buff->fsync_in_prog_latch, FALSE);
						if (NOJNL != jpc->channel)
							jnl_file_close(gv_cur_region, FALSE, FALSE);
						free(jpc);
						cs_addrs->jnl = NULL;
						rel_crit(gv_cur_region);
					}
				}
				/* Write master-map (in addition to file header) as wcs_flu done above would not have updated it */
				csd_size = SIZEOF_FILE_HDR(csd); 	/* SIZEOF(sgmnt_data) + master-map */
				TREF(donot_write_inctn_in_wcs_recover) = FALSE;
			} else
			{	/* In this case, we just want to clear a few fields in the file header but we do NOT want to
				 * write the master map which we dont have access to at this point so set csd_size accordingly.
				 */
				csd = tsd;
				csd_size = tsd_size;	/* SIZEOF(sgmnt_data) */
			}
			reg->open = FALSE;
			/* If wcs_flu returned FALSE, it better be because of MUPIP RUNDOWN run with OVERRIDE qualifier. */
			assert(wcs_flu_success || (override_present && !standalone));
			/* In case MUPIP RUNDOWN is invoked with OVERRIDE qualifier and we ignored a FALSE return from wcs_flu, do
			 * not update the database header, thus forcing the operator to either use a ROLLBACK/RECOVER or supply the
			 * OVERRIDE qualifier with RUNDOWN before GT.M could again be used to access the database.
			 */
			if (wcs_flu_success)
			{	/* Note: At this point we have write permission */
				memset(csd->machine_name, 0, MAX_MCNAMELEN);
				if (!mupip_jnl_recover)
					csd->freeze = 0;
				RESET_SHMID_CTIME(csd);
				if (!standalone)
				{	/* Invalidate semid in the file header as part of rundown. The actual semaphore still
					 * exists and we'll remove that just before releasing the ftok semaphore. However, if the
					 * MUPIP RUNDOWN command gets killed AFTER we write the file header but BEFORE we remove
					 * the semaphore from the system, we can have an orphaned semaphore. But, this is okay
					 * since an arugment-less MUPIP RUNDOWN, if invoked, will remove those orphaned semaphores
					 */
					RESET_SEMID_CTIME(csd);
				}
				DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, csd, csd_size, status);
				if (0 != status)
				{
					RNDWN_ERR("!AD -> Error writing header to disk.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
			}
			if (NULL != tsd)
			{
				assert(!db_shm_in_sync);
				free(tsd);
				tsd = NULL;
			}
			if (dba_mm == acc_meth)
			{
				assert(0 != mmap_sz);
				if (-1 == msync((caddr_t)cs_addrs->db_addrs[0], mmap_sz, MS_SYNC))
				{
					RNDWN_ERR("!AD -> Error synchronizing mapped memory.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
				if (-1 == munmap((caddr_t)cs_addrs->db_addrs[0], mmap_sz))
				{
					RNDWN_ERR("!AD -> Error unmapping mapped memory.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
			}
		} else
			glob_sec_init = FALSE;
	} else
	{
		is_gtm_shm = FALSE;
		assert(FALSE);
	}
	/* Detach from shared memory whether it is a GT.M shared memory or not */
	if (-1 == shmdt((caddr_t)cs_addrs->nl))
	{
		assert(FALSE);
		RNDWN_ERR("!AD -> Error detaching from shared memory.", reg);
		CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	}
	cs_addrs->nl = NULL;
	/* Remove the shared memory only if it is a GT.M created one. */
	if (is_gtm_shm)
	{
		if (remove_shmid) /* Note: remove_shmid is defined only if is_gtm_shm is TRUE */
		{
			if (0 != shm_rmid(udi->shmid))
			{
				save_errno = errno;
				if ((EINVAL != save_errno) && (EIDRM != save_errno))
				{
					assert(FALSE);
					RNDWN_ERR("!AD -> Error removing shared memory.", reg);
					CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				}
			} else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SHMREMOVED, 3, udi->shmid, DB_LEN_STR(reg));
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SHMREMOVED, 3, udi->shmid, DB_LEN_STR(reg));
			}
		}
		udi->shmid = INVALID_SHMID;
		udi->gt_shm_ctime = 0;
	}
	/* If the shared memory was found to a valid GT.M created segment and if it was properly initialized, we would
	 * have cleared shmid in the database file header to INVALID_SHMID. If not, do the reset now as we nevertheless
	 * dont want the database to connect to that non-GT.M-shmid or uninitialized-GT.M-shmid anymore.
	 */
	if (!is_gtm_shm || !glob_sec_init)
	{
		assert(NULL != tsd);	/* should not have been freed */
		RESET_SHMID_CTIME(tsd);
		if (mupip_jnl_recover)
			memset(tsd->machine_name, 0, MAX_MCNAMELEN);
		DB_LSEEKWRITE(csa, udi->fn, udi->fd, (off_t)0, tsd, tsd_size, status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		free(tsd);
		tsd = NULL;
	}
	assert(INVALID_SHMID == udi->shmid);
	assert(0 == udi->gt_shm_ctime);
	assert(udi->grabbed_access_sem);
	assert(!db_shm_in_sync || udi->counter_acc_incremented);
	assert(INVALID_SEMID != udi->semid);
	if (!standalone && (db_shm_in_sync || sem_created))
	{
		if (0 != sem_rmid(udi->semid))
		{
			assert(FALSE); /* We've created the semaphore, so we should be able to remove it */
			RNDWN_ERR("!AD -> Error removing semaphore.", reg);
			CLNUP_AND_RETURN(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		}
		sem_created = FALSE;
		udi->grabbed_access_sem = FALSE;
		udi->counter_acc_incremented = FALSE;
		udi->semid = INVALID_SEMID;
	}
	REVERT;
	RESET_GV_CUR_REGION;
	restore_rndwn_gbl = FALSE;
	/* For mupip rundown, standalone == FALSE and we want to release/remove ftok semaphore.
	 * Otherwise, just release ftok semaphore lock. counter will be one more for this process.
	 * Exit handlers must take care of removing if necessary.
	 */
	if (!ftok_sem_release(reg, !standalone, !standalone))
		return FALSE;
	/* if "standalone" we better leave this function with standalone access */
	assert(!standalone || udi->grabbed_access_sem);
	CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
	DEBUG_ONLY(in_mu_rndwn_file = FALSE);
	return TRUE;
}

CONDITION_HANDLER(mu_rndwn_file_ch)
{
	unix_db_info	*udi;
	sgmnt_addrs	*csa;

	START_CH;
	PRN_ERROR;
	assert(NULL != rundown_reg);
	if (NULL != rundown_reg)
	{
		udi = FILE_INFO(rundown_reg);
		csa = &udi->s_addrs;
		DEBUG_ONLY(in_mu_rndwn_file = FALSE);
		TREF(donot_write_inctn_in_wcs_recover) = FALSE;
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(rundown_reg, FALSE, TRUE);
	}
	if (restore_rndwn_gbl)
	{
		RESET_GV_CUR_REGION;
	}
	rundown_reg->open = FALSE;
	/* We want to proceed to the next condition handler in case we have stand-alone access, because if an error happens on one
	 * region, we should signal an issue and not proceed to the next region. Otherwise, we try to rundown the next region.
	 */
	if (mu_rndwn_file_standalone)
	{
		NEXTCH;
	} else
	{
		UNWIND(NULL, NULL);
	}
}
