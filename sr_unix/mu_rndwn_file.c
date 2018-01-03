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
#include "gtmcrypt.h"
#include "db_snapshot.h"
#include "shmpool.h"	/* Needed for the shmpool structures */
#include "is_proc_alive.h"
#include "ss_lock_facility.h"
#include "cli.h"
#include "gtm_file_stat.h"
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab.h"		/* needed for muprec.h */
#include "muprec.h"
#include "aio_shim.h"
#include "mu_gv_cur_reg_init.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			process_id;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	mur_opt_struct		mur_options;
GBLREF	mval			dollar_zgbldir;
#ifdef DEBUG
GBLREF	boolean_t		in_mu_rndwn_file;
#endif

/* Any additions/removal from this "static" list requires a similar change in NESTED_MU_RNDWN_FILE_CALL */
static gd_region	*rundown_reg = NULL;
static gd_region	*temp_region;
static sgmnt_data_ptr_t	temp_cs_data;
static sgmnt_addrs	*temp_cs_addrs;
static boolean_t	mu_rndwn_file_standalone;
static boolean_t	sem_created;
static boolean_t	no_shm_exists;
static boolean_t	shm_status_confirmed;

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
error_def(ERR_MUFILRNDWNSUC);
error_def(ERR_MURNDWNOVRD);
error_def(ERR_MUUSERECOV);
error_def(ERR_MUUSERLBK);
error_def(ERR_NOMORESEMCNT);
error_def(ERR_OFRZACTIVE);
error_def(ERR_SEMREMOVED);
error_def(ERR_SHMREMOVED);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

#define	ALIGN_BUFF_IF_NEEDED_FOR_DIO(UDI, BUFF, TSD, TSD_SIZE)	\
MBSTART {							\
	if (UDI->fd_opened_with_o_direct)			\
	{							\
		BUFF = (TREF(dio_buff)).aligned;		\
		memcpy(BUFF, TSD, TSD_SIZE);			\
	} else							\
		BUFF = (char *)TSD;				\
} MBEND

#define RESET_GV_CUR_REGION		\
MBSTART {				\
	gv_cur_region = temp_region;	\
	cs_addrs = temp_cs_addrs;	\
	cs_data = temp_cs_data;		\
} MBEND

#define	MU_RNDWN_FILE_CLNUP(REG, UDI, TSD, SEM_CREATED, SEM_INCREMENTED)		\
MBSTART {										\
	int		rc;								\
											\
	IF_LIBAIO(aio_shim_destroy(UDI->owning_gd);)					\
	if (FD_INVALID != UDI->fd)							\
	{										\
		CLOSEFILE_RESET(UDI->fd, rc);						\
		assert(FD_INVALID == UDI->fd);						\
	}										\
	if (NULL != TSD)								\
	{										\
		free(TSD);								\
		TSD = NULL;								\
	}										\
	if (SEM_INCREMENTED)								\
	{										\
		do_semop(udi->semid, DB_CONTROL_SEM, -1, IPC_NOWAIT | SEM_UNDO);	\
		SEM_INCREMENTED = FALSE;						\
	}										\
	if (SEM_CREATED)								\
	{										\
		if (-1 == sem_rmid(UDI->semid))						\
		{									\
			RNDWN_ERR("!AD -> Error removing semaphore.", REG);		\
		} else									\
			SEM_CREATED = FALSE;						\
	}										\
	REVERT;										\
	assert((NULL == ftok_sem_reg) || (REG == ftok_sem_reg));			\
	if (REG == ftok_sem_reg)							\
		ftok_sem_release(REG, UDI->counter_ftok_incremented, TRUE);		\
	RESET_GV_CUR_REGION;								\
} MBEND

#define SEG_SHMATTACH(addr, reg, udi, tsd, sem_created, sem_incremented)		\
MBSTART {										\
	if (-1 == (sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)			\
				do_shmat(udi->shmid, addr, SHM_RND)))			\
	{										\
		if (EINVAL != errno)							\
			RNDWN_ERR("!AD -> Error attaching to shared memory", (reg));	\
		/* shared memory segment no longer exists */				\
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, sem_incremented);	\
		return FALSE;								\
	}										\
} MBEND

/* Print an error message that, based on whether replication was enabled at the time of the crash, would instruct
 * the user to a more appropriate operation than RUNDOWN, such as RECOVER or ROLLBACK.
 */
#define PRINT_PREVENT_RUNDOWN_MESSAGE(REG, CS_ADDRS, NEED_ROLLBACK)					\
MBSTART {												\
	if (NEED_ROLLBACK)										\
	{												\
		rts_error_csa(CSA_ARG(CS_ADDRS) VARLSTCNT(8) ERR_MUUSERLBK, 2, DB_LEN_STR(REG),		\
			ERR_TEXT, 2, LEN_AND_LIT("Run MUPIP JOURNAL ROLLBACK"));			\
	} else												\
	{												\
		rts_error_csa(CSA_ARG(CS_ADDRS) VARLSTCNT(8) ERR_MUUSERECOV, 2, DB_LEN_STR(REG),	\
			ERR_TEXT, 2, LEN_AND_LIT("Run MUPIP JOURNAL RECOVER"));				\
	}												\
} MBEND

/* This macro is called whenever a "mu_rndwn_file" call is done from within "mu_rndwn_file".
 * In that case, some static variables (relied upon by mu_rndwn_file_ch to handle errors in current
 * "mu_rndwn_file" frame level) are saved off in the stack and restored once the call returns.
 */
#define	NESTED_MU_RNDWN_FILE_CALL(REG, STANDALONE, STATUS)		\
{									\
	gd_region		*save_rundown_reg;			\
	gd_region		*save_temp_region;			\
	sgmnt_data_ptr_t	save_temp_cs_data;			\
	sgmnt_addrs		*save_temp_cs_addrs;			\
	boolean_t		save_mu_rndwn_file_standalone;		\
	boolean_t		save_sem_created;			\
	boolean_t		save_no_shm_exists;			\
	boolean_t		save_shm_status_confirmed;		\
									\
	save_rundown_reg                = rundown_reg;			\
	save_temp_region                = temp_region;			\
	save_temp_cs_data               = temp_cs_data;			\
	save_temp_cs_addrs              = temp_cs_addrs;		\
	save_mu_rndwn_file_standalone   = mu_rndwn_file_standalone;	\
	save_sem_created                = sem_created;			\
	save_no_shm_exists              = no_shm_exists;		\
	save_shm_status_confirmed       = shm_status_confirmed;		\
									\
	STATUS = mu_rndwn_file(REG, STANDALONE);			\
									\
	rundown_reg                = save_rundown_reg;			\
	temp_region                = save_temp_region;			\
	temp_cs_data               = save_temp_cs_data;			\
	temp_cs_addrs              = save_temp_cs_addrs;		\
	mu_rndwn_file_standalone   = save_mu_rndwn_file_standalone;	\
	sem_created                = save_sem_created;			\
	no_shm_exists              = save_no_shm_exists;		\
	shm_status_confirmed       = save_shm_status_confirmed;		\
}

/* Runs down a stats db pointed to by "statsDBreg". Returns TRUE/FALSE depending on whether rundown succeeded or not.
 * Input parameter "standalone" is FALSE if caller is MUPIP RUNDOWN and TRUE otherwise. Used to decide whether to
 *	print the MUFILRNDWNSUC message or not (only mupip rundown should print it).
 * Modifies "*statsdb_exists" to TRUE or FALSE depending on whether the file exists or not.
 */
STATICFNDEF boolean_t mu_rndwn_file_statsdb(gd_region *statsDBreg, boolean_t *statsdb_exists, boolean_t standalone)
{
	boolean_t	statsDBrundown_status;
	gd_region	*save_gv_cur_region, *save_ftok_sem_reg;
	gd_segment	*statsDBseg;

	/* We are in "mu_rndwn_file" of a basedb and are about to do a nested call of "mu_rndwn_file" on a statsdb.
	 * But we don't want that to in turn call a "mu_rndwn_file" of the basedb (which it can if IS_RDBF_STATSDB
	 * is TRUE; see NESTED_MU_RNDWN_FILE_CALL usage in "mu_rndwn_file") as that would lead to an indefinite recursion.
	 * Below assert is done to ensure that does not happen.
	 */
	assert(IS_STATSDB_REGNAME(statsDBreg));
	assert(statsDBreg->owning_gd->regions != statsDBreg);	/* A statsdb is never the first region in a gld */
	if (mupfndfil(statsDBreg, NULL, LOG_ERROR_FALSE))
	{	/* statsDB exists. Do its rundown first. */
		*statsdb_exists = TRUE;
		statsDBseg = statsDBreg->dyn.addr;
		if (NULL == statsDBseg->file_cntl)
			FILE_CNTL_INIT(statsDBseg);
		/* Since this function is called only from "mu_rndwn_file", we hold the ftok lock on the basedb at this point.
		 * But the "mu_rndwn_file" call below is going to get the ftok lock on the statsdb and "ftok_sem_get" does not
		 * like holding more than one ftok at any point in time. So fix structures to indicate no ftok lock is held
		 * and restore that after the "mu_rndwn_file" call.
		 */
		save_ftok_sem_reg = ftok_sem_reg;
		ftok_sem_reg = NULL;
		NESTED_MU_RNDWN_FILE_CALL(statsDBreg, FALSE, statsDBrundown_status);
		if (statsDBrundown_status && !standalone)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUFILRNDWNSUC, 2, DB_LEN_STR(statsDBreg));
		ftok_sem_reg = save_ftok_sem_reg;
		save_gv_cur_region = gv_cur_region;
		/* In case "mu_rndwn_file" call on the statsdb failed with SIG-11 or so, it would return with "gv_cur_region"
		 * still pointing to the statsdb so fix it to point back to the basedb and continue with basedb rundown.
		 */
		if (save_gv_cur_region != gv_cur_region)
		{
			gv_cur_region = save_gv_cur_region;
			tp_change_reg();
		}
	} else
	{
		*statsdb_exists = FALSE;
		statsDBrundown_status = TRUE;
	}
	return statsDBrundown_status;
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
	boolean_t		is_gtm_shm, is_statsdb, rc_cpt_removed, statsDBexists;
	boolean_t		glob_sec_init, db_shm_in_sync, remove_shmid, ftok_counter_halted;
	boolean_t		crypt_warning, do_crypt_init, need_statsdb_rundown;
	boolean_t		baseDBrundown_status, statsDBrundown_status;
	sgmnt_data_ptr_t	csd, tsd = NULL;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	struct sembuf		sop[4], *sopptr;
	struct shmid_ds		shm_buf;
	file_control		*fc;
	unix_db_info		*statsDBudi, *udi;
	enum db_acc_method	acc_meth;
        struct stat     	stat_buf;
	struct semid_ds		semstat;
	union semun		semarg;
	uint4			status_msg, ss_pid;
	shm_snapshot_t		*ss_shm_ptr;
	gtm_uint64_t		sec_size, mmap_sz = 0;
	gd_segment		*seg, *baseDBseg;
	int			gtmcrypt_errno;
	boolean_t		cleanjnl_present, override_present, wcs_flu_success, prevent_mu_rndwn, need_rollback;
	unsigned char		*fn;
	mstr 			jnlfile;
	int			jnl_fd;
	jnl_file_header		header;
	int4			status1;
	uint4			status2;
	int			iter, secshrstat;
	char			*buff;
	node_local_ptr_t	cnl;
	char			basedb_fname[MAX_FN_LEN + 1], statsdb_fname[MAX_FN_LEN + 1], *statsdb_fname_ptr;
	int			basedb_fname_len;
	uint4			statsdb_fname_len;
	gd_addr			*owning_gd;
	gd_region		*statsDBreg, *save_ftok_sem_reg;
	gd_segment		*statsDBseg;
#	ifdef DEBUG
	boolean_t		already_grabbed_ftok_sem = FALSE;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_rndwn_file_standalone = standalone;
	rc_cpt_removed = FALSE;
	sem_created = FALSE;
	shm_status_confirmed = FALSE;
	assert(!jgbl.onlnrlbk);
	assert(!mupip_jnl_recover || standalone || IS_STATSDB_REG(reg));
	/* save gv_cur_region, cs_data, cs_addrs, and restore them on return */
	temp_region = gv_cur_region;
	temp_cs_data = cs_data;
	temp_cs_addrs = cs_addrs;
	rundown_reg = gv_cur_region = reg;
#	ifdef GTCM_RC
        rc_cpt_removed = mupip_rundown_cpt();
#	endif
	seg = reg->dyn.addr;
	fc = seg->file_cntl;
	SYNC_OWNING_GD(reg);
	owning_gd = reg->owning_gd;
	for (iter = 0; ; iter++)
	{
		fc->op = FC_OPEN;
		status = dbfilop(fc);
		udi = FILE_INFO(reg);
		csa = &(udi->s_addrs);		/* Need valid cs_addrs in is_anticipatory_freeze_needed, which can be called */
		cs_addrs = csa;			/* by gtm_putmsg(), so set it up here. */
		if (SS_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) status, 2, DB_LEN_STR(reg), errno);
			if (0 == iter)
			{
				if (FD_INVALID != udi->fd)	/* Since dbfilop failed, close udi->fd only if it was opened */
					CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
			} else
			{	/* Release ftok semaphore grabbed in first iteration and never released in later iterations */
				assert(reg == ftok_sem_reg);
				ftok_sem_release(reg, udi->counter_ftok_incremented, TRUE);
			}
			return FALSE;
		}
		ESTABLISH_RET(mu_rndwn_file_ch, FALSE);
		if (0 == iter)
		{	/* Get FTOK lock only once in for loop and do not release it as otherwise it introduces complications
			 * related to "ftok_counter_halted" maintenance across all iterations and how much to finally
			 * decrement the counter semaphore at the end (as part of the "ftok_sem_release").
			 * Note that if we are a statsdb it is possible that a parent "mu_rndwn_file" invocation grabbed the
			 * ftok (and in turn invoked "mu_rndwn_file" on the basedb which in turn invoked "mu_rndwn_file"
			 * on the statsdb again). In that case, the nested statsdb rundown invocation will continue to use
			 * the ftok grabbed by the parent statsdb rundown invocation. We do not release the ftok in the parent
			 * invocation as otherwise we run the possibility of deleting the ftok semaphore as part of that
			 * (since at the time we do the basedb rundown invocation we have not yet attached to db shm to know
			 * if the ftok counter semaphore overflowed or not).
			 */
			DEBUG_ONLY(already_grabbed_ftok_sem = udi->grabbed_ftok_sem);
			if (!udi->grabbed_ftok_sem)
			{
				if (!ftok_sem_get(reg, TRUE, GTM_ID, !standalone, &ftok_counter_halted))
				{
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
			} else
			{
				assert(NULL == ftok_sem_reg); /* parent statsdb "mu_rndwn_file" invocation would have done this */
				ftok_sem_reg = reg;
				/* Inherit "ftok_counter_halted" from parent "ftok_sem_get" invocation since we did not do one */
				ftok_counter_halted = !udi->counter_ftok_incremented;
			}
			/* At this point, we have not yet attached to the database shared memory so we do not know if the ftok
			 * counter got halted previously or not. So be safe and assume it has halted in case the db shmid
			 * indicates it is up and running.
			 */
			assert(udi->counter_ftok_incremented == !ftok_counter_halted);
			udi->counter_ftok_incremented = FALSE;
		}
		/* Now we have standalone access of the database using ftok semaphore. Any other ftok conflicted database suspends
		 * their operation at this point. At the end of this routine, we release ftok semaphore lock.
		 */
		tsd_size = ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
		/* Assert that later DB_LSEEKWRITE with DIO will be fine */
		assert(!udi->fd_opened_with_o_direct || (tsd_size == ROUND_UP2(tsd_size, DIO_ALIGNSIZE(udi))));
		tsd = (sgmnt_data_ptr_t)malloc(tsd_size);
		buff = (udi->fd_opened_with_o_direct ? (TREF(dio_buff)).aligned : (char *)tsd);
		DB_LSEEKREAD(udi, udi->fd, 0, buff, tsd_size, status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Error reading from file.", reg);
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		if (udi->fd_opened_with_o_direct)
			memcpy(tsd, buff, tsd_size);
		if (standalone && IS_RDBF_STATSDB(tsd))
		{	/* Only MUPIP RUNDOWN (which has "standalone" set to FALSE) is supported for statsdb files.
			 * All other MUPIP commands which require standalone access cannot directly operate on statsdb files.
			 */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_STATSDBNOTSUPP, 2, DB_LEN_STR(reg));
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		seg->read_only = tsd->read_only;
		SYNC_RESERVEDDBFLAGS_REG_CSA_CSD(reg, csa, tsd, ((node_local_ptr_t)NULL));
		if (!IS_AIO_DBGLDMISMATCH(seg, tsd))
			break;
		/* Close current file and reopen with the correct O_DIRECT setting based on the revised asyncio value */
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		/* asyncio settings differ between segment and db. In this case, copy asyncio setting from db into segment
		 * and retry the db file open.
		 */
		COPY_AIO_SETTINGS(seg, tsd);	/* copies from tsd to seg */
		free(tsd);
		tsd = NULL;
		REVERT;
	}
	CSD2UDI(tsd, udi);	/* copies tsd->semid/tsd->shmid into udi->semid/udi->shmid */
	is_statsdb = IS_RDBF_STATSDB(tsd);
	assert(!already_grabbed_ftok_sem || is_statsdb);
	assert(!is_statsdb || !standalone);	/* or else an ERR_STATSDBNOTSUPP error would have been issued */
	/* If this is a statsdb, then we are guaranteed the caller is a MUPIP RUNDOWN command (standalone is FALSE).
	 * Check if it is a MUPIP RUNDOWN -FILE statsdb call. In that case, we need to instead do a
	 * MUPIP RUNDOWN -FILE basedb call and let that in turn rundown the statsdb (need that to safely cleanup
	 * the .gst file). Similar handling is needed if the caller is argumentless MUPIP RUNDOWN. In all other cases,
	 * we are guaranteed "mu_rndwn_file" will be invoked only on the basedb and so this call for a rundown of the
	 * statsdb is not a direct call and so we can continue the rundown. Thankfully the two desired cases are
	 * easily identified as in that case we would have done a "mu_gv_cur_reg_init" (which would have set
	 * owning_gd->is_dummy_gbldir to TRUE) AND "reg" would be the first region in the gld even though it points
	 * to a statsdb file.
	 */
	if (is_statsdb && owning_gd->is_dummy_gbldir && (reg == owning_gd->regions))
	{	/* Copy basedb file name to local variable before freeing up tsd, releasing ftok lock etc. */
		assert(tsd->basedb_fname_len);
		assert(ARRAYSIZE(tsd->basedb_fname) <= ARRAYSIZE(basedb_fname));
		basedb_fname_len = MIN(tsd->basedb_fname_len, ARRAYSIZE(basedb_fname) - 1);
		assert('\0' == tsd->basedb_fname[basedb_fname_len]);
		memcpy(basedb_fname, tsd->basedb_fname, basedb_fname_len + 1);	/* copy trailing '\0' too */
		/* Record "ftok_counter_halted" value from "ftok_sem_get" invocation for child statsdb "mu_rndwn_file" invocation */
		udi->counter_ftok_incremented = !ftok_counter_halted;
		/* Create fresh gld/region structure and point basedb to it and invoke mu_rndwn_file on this */
		mu_gv_cur_reg_init();	/* changes "gv_cur_region" */
		baseDBseg = gv_cur_region->dyn.addr;
		assert(ARRAYSIZE(baseDBseg->fname) >= ARRAYSIZE(basedb_fname));
		memcpy(baseDBseg->fname, basedb_fname, basedb_fname_len + 1);	/* copy trailing '\0' too */
		baseDBseg->fname_len = basedb_fname_len;
		/* Check if the basedb exists. If not, skip nested "mu_rndwn_file" invocation altogether */
		if (mupfndfil(gv_cur_region, NULL, LOG_ERROR_FALSE))
		{
			/* We have gotten the ftok semaphore on the statsdb. Releasing it here (and reobtaining it in the nested
			 * statsdb "mu_rndwn_file" invocation) runs the risk of deleting the ftok semaphore while processes are
			 * still attached. But we also do not want to leak the ftok ipcs. So hold on to the ftok sem of the statsdb
			 * here and use that in the nested statsdb "mu_rndwn_file" invocation. And do the release of this there when
			 * we have access to statsdb shm (and can therefore correctly decide whether counter semaphore got halted or
			 * not). Save "ftok_sem_reg" before it is modified (a few lines later).
			 */
			save_ftok_sem_reg = ftok_sem_reg;
			/* Set "ftok_sem_reg" to NULL since the nested "mu_rndwn_file" call is going to get ftok lock on the
			 * basedb and that would assert fail otherwise.
			 */
			ftok_sem_reg = NULL;
			/* Now that statsdb region has been setup, transfer parent "mu_rndwn_file" udi info to it */
			BASEDBREG_TO_STATSDBREG(gv_cur_region, statsDBreg);
			statsDBseg = statsDBreg->dyn.addr;
			if (NULL == statsDBseg->file_cntl)
				FILE_CNTL_INIT(statsDBseg);
			statsDBudi = FILE_INFO(statsDBreg);
			memcpy(statsDBudi, udi, SIZEOF(*udi));
			NESTED_MU_RNDWN_FILE_CALL(gv_cur_region, FALSE, baseDBrundown_status);
			/* If above "mu_rndwn_file" invocation of the basedb did a rundown of the statsdb too, then we would have
			 * released the ftok lock on the statsdb. But if an error happened in "mu_rndwn_file" of the basedb before
			 * it even attempted "mu_rndwn_file" of statsdb, then we would still be holding the ftok sem on the statsdb.
			 * So use that as a means of distinguishing the two scenarios. We can return right away if the statsdb
			 * rundown is already happened in the nested call and should continue with the rundown in this call
			 * otherwise.
			 */
			need_statsdb_rundown = statsDBudi->grabbed_ftok_sem;
			/* Note: It is possible that need_statsdb_rundown is TRUE and baseDBrundown_status is also TRUE
			 * in case the baseDB has already been run down but statsDB has not been rundown and
			 * the gtm_statsdir env var used to create this statsDB is different from the current value of
			 * the gtm_statsdir env var. Therefore continue running down the statsDB as long as that is
			 * needed, irrespective of the rundown status of the baseDB.
			 */
			if (!need_statsdb_rundown)
			{
				mu_gv_cur_reg_free();
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return baseDBrundown_status;
			}
			ftok_sem_reg = save_ftok_sem_reg;
		}
		/* else : basedb does not exist. Continue rundown of statsdb without going through basedb rundown */
	}
	/* read_only process cannot rundown database.
	 * read only process can succeed in getting standalone access of the database,
	 *	if the db is clean with no orphaned shared memory.
	 * Note: we use gtmsecshr for updating file header for semaphores id.
	 * Note: We place this check AFTER the LSEEKREAD so an argumentless rundown caller will have access to
	 *	udi->semid and make sure that does not later get removed if we are not running down shm due to DBRDONLY.
	 */
	if (reg->read_only && !standalone)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		return FALSE;
	}
	override_present = (cli_present("OVERRIDE") == CLI_PRESENT);
	csa->hdr = tsd;
	csa->region = gv_cur_region;
	/* At this point, we have not yet attached to the database shared memory so we do not know if the ftok counter got halted
	 * previously or not. So be safe and assume it has halted in case the db shmid indicates it is up and running.
	 * We will set udi->counter_ftok_incremented back to an accurate value after we attach to the db shm.
	 * This means we might not delete the ftok semaphore in some cases of error codepaths but it should be rare
	 * and is better than incorrectly deleting it while live processes are concurrently using it.
	 */
	udi->counter_ftok_incremented = !ftok_counter_halted && (INVALID_SHMID == udi->shmid);
	if (USES_ENCRYPTION(tsd->is_encrypted))
	{
		INIT_PROC_ENCRYPTION(csa, gtmcrypt_errno);
		if (0 == gtmcrypt_errno)
			INIT_DB_OR_JNL_ENCRYPTION(csa, tsd, seg->fname_len, seg->fname, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
	}
	semarg.buf = &semstat;
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
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
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
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
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
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
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
	sop[3].sem_num = DB_COUNTER_SEM; sop[3].sem_op = DB_COUNTER_SEM_INCR; /* increment the counter semaphore */
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
	shm_status_confirmed = TRUE;	/* Now we have ascertained the status of shared memory. */
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
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		return FALSE;
	}
	udi->grabbed_access_sem = TRUE;
	udi->counter_acc_incremented = no_shm_exists;
#	ifdef DEBUG
	cleanjnl_present = (cli_present("CLEANJNL") == CLI_PRESENT);
#	else
	cleanjnl_present = FALSE;
#	endif
	/* Proceed with rundown if either journaling is off or we got here as a result of MUPIP JOURNAL -RECOVER or
	 * MUPIP JOURNAL -ROLLBACK, unless the OVERRIDE qualifier is present (see the following code).
	 */
	/* If journaling and/or replication is enabled, prevent rundown on this database. Instead force caller to use
	 * MUPIP JOURNAL RECOVER BACKWARD or MUPIP JOURNAL ROLLBACK BACKWARD respectively. Note though that those commands
	 * too can call this function so allow them in those respective cases.
	 */
	if (JNL_ENABLED(tsd))
	{
		if (REPL_ENABLED(tsd))
		{
			need_rollback = TRUE;
			prevent_mu_rndwn = !mur_options.rollback;
		} else
		{
			need_rollback = FALSE;
			prevent_mu_rndwn = !mur_options.update;	/* Allow MUPIP JOURNAL RECOVER or MUPIP JOURNAL ROLLBACK */
		}
	} else
	{
		prevent_mu_rndwn = FALSE;
		need_rollback = FALSE;
	}
	statsDBrundown_status = TRUE;
	/* Now rundown database if shared memory segment exists. We try this for both values of 'standalone'. */
	if (no_shm_exists)
	{	/* Since we know no shm exists, we can safely say the counter has not halted so restore counter_ftok_incremented */
		udi->counter_ftok_incremented = !ftok_counter_halted;
		assert(udi->counter_ftok_incremented);
		/* If this is a basedb, check for statsdb existence and if so call mu_rndwn_file on it */
		if (!is_statsdb)
		{
			assert(udi->fn == (char *)&seg->fname[0]);
			statsdb_fname_len = ARRAYSIZE(statsdb_fname);
			gvcst_set_statsdb_fname(tsd, reg, statsdb_fname, &statsdb_fname_len);
			if (statsdb_fname_len)
			{
				BASEDBREG_TO_STATSDBREG(reg, statsDBreg);
				COPY_STATSDB_FNAME_INTO_STATSREG(statsDBreg, statsdb_fname, statsdb_fname_len);
				/* Note: Error status of statsdb rundown is factored into final basedb rundown status
				 * by storing the return value in "statsDBrundown_status".
				 */
				statsDBrundown_status = mu_rndwn_file_statsdb(statsDBreg, &statsDBexists, standalone);
				/* If statsDB exists and rundown was successful, remove it. It is okay to do so since
				 * we hold an ftok on the basedb at this point.
				 */
				if (statsDBexists && statsDBrundown_status)
				{
					rc = UNLINK(statsdb_fname);
					assert(0 == rc);
				}
			}
		}
		if (prevent_mu_rndwn)
		{
			if (override_present)
			{	/* If the rundown should normally be prevented, but the operator specified an OVERRIDE qualifier,
				 * record the fact of the usage in the syslog and continue.
				 */
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_MURNDWNOVRD, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
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
											(char *)seg->fname) && header.crash)
								{
									PRINT_PREVENT_RUNDOWN_MESSAGE(reg, csa, need_rollback);
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
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
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
					ALIGN_BUFF_IF_NEEDED_FOR_DIO(udi, buff, tsd, tsd_size);	/* sets "buff" */
					DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, (off_t)0, buff, tsd_size, status);
					if (0 != status)
					{
						RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
						MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
						return FALSE;
					}
				} else
				{
					db_ipcs.open_fd_with_o_direct = udi->fd_opened_with_o_direct;
					db_ipcs.semid = tsd->semid;
					db_ipcs.gt_sem_ctime = tsd->gt_sem_ctime.ctime;
					db_ipcs.shmid = tsd->shmid;
					db_ipcs.gt_shm_ctime = tsd->gt_shm_ctime.ctime;
					if (!get_full_path((char *)DB_STR_LEN(reg), db_ipcs.fn, &db_ipcs.fn_len,
											GTM_PATH_MAX, &status_msg))
					{
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(1) status_msg);
						RNDWN_ERR("!AD -> get_full_path failed.", reg);
						MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
						return FALSE;
					}
					db_ipcs.fn[db_ipcs.fn_len] = 0;
					WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
					secshrstat = send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0);
					csa->read_only_fs = (EROFS == secshrstat);
					if ((0 != secshrstat) && !csa->read_only_fs)
					{
						RNDWN_ERR("!AD -> gtmsecshr was unable to write header to disk.", reg);
						MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
						return FALSE;
					}
				}
				if (!ftok_sem_release(reg, FALSE, FALSE))
				{
					RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
				REVERT;
				free(tsd);
				assert(udi->grabbed_access_sem);
				RESET_GV_CUR_REGION;
				return statsDBrundown_status; /* For "standalone" and "no shared memory existing", we exit here */
			} else
			{	/* We are here for not standalone (basically the "mupip rundown" command). */
				if (0 != do_semop(udi->semid, DB_CONTROL_SEM, -1, IPC_NOWAIT | SEM_UNDO))
				{
					assert(FALSE); /* We incremented the semaphore, so we should be able to decrement it */
					RNDWN_ERR("!AD -> Error decrementing semaphore.", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				udi->counter_acc_incremented = FALSE;
				if (0 != sem_rmid(udi->semid))
				{
					assert(FALSE); /* We've created the semaphore, so we should be able to remove it */
					RNDWN_ERR("!AD -> Error removing semaphore.", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				if (!sem_created)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(3) ERR_SEMREMOVED, 1, udi->semid);
					gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(3) ERR_SEMREMOVED, 1, udi->semid);
				}
				sem_created = FALSE;
				udi->grabbed_access_sem = FALSE;
				udi->semid = INVALID_SEMID; /* "orphaned" and "newly" created semaphores are now removed */
				/* Reset IPC fields in the file header and exit */
				memset(tsd->machine_name, 0, MAX_MCNAMELEN);
				RESET_IPC_FIELDS(tsd);
			}
		}
		assert(!standalone);
		ALIGN_BUFF_IF_NEEDED_FOR_DIO(udi, buff, tsd, tsd_size);	/* sets "buff" */
		if (!tsd->read_only)
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, (off_t)0, buff, tsd_size, status);
		else
			status = 0;
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
		free(tsd);
		REVERT;
		/* For mupip rundown (standalone = FALSE), we release/remove ftok semaphore here. */
		if (!ftok_sem_release(reg, udi->counter_ftok_incremented, TRUE))
		{
			RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
			return FALSE;
		}
		RESET_GV_CUR_REGION;
		return statsDBrundown_status; /* For "!standalone" and "no shared memory existing", we exit here */
	}
	/* Now that we know shared memory exists, make sure FORWARD RECOVER or FORWARD ROLLBACK are not allowed to recover
	 * the database. Only backward rollback and/or recover should be allowed.
	 */
	prevent_mu_rndwn = prevent_mu_rndwn || mur_options.forward;
	if (reg->read_only)             /* read only process can't succeed beyond this point */
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		return FALSE;
	}
	/* Now we have a pre-existing shared memory section. Do some setup */
	if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
       	{
		if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBNOTGDS, 2, DB_LEN_STR(reg));
		else
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_BADDBVER, 2, DB_LEN_STR(reg));
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		return FALSE;
       	}
	seg->acc_meth = acc_meth = tsd->acc_meth;
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
	gv_cur_region = reg;
	tp_change_reg();
	SEG_SHMATTACH(0, reg, udi, tsd, sem_created, udi->counter_acc_incremented);
	assert(csa == cs_addrs);
	csa->nl = cnl = (node_local_ptr_t)csa->db_addrs[0];
	/* The following checks for GDS_LABEL_GENERIC, gtm_release_name, and cnl->glob_sec_init ensure that the
	 * shared memory under consideration is valid.  First, since cnl->label is in the same place for every
	 * version, a failing check means it is most likely NOT a GT.M created shared memory, so no attempt will be
	 * made to delete it. Next a successful match of the currently running release will guarantee that all fields in
	 * the structure are where they're expected to be -- without this check the glob_sec_init check should not be done
	 * as this field is at different offsets in different versions. Finally, we can check the glob_sec_init flag to
	 * verify that shared memory has been completely initialized. If not, we should delete it right away. [C9D07-002355]
	 */
	for ( ; ; )	/* for loop only there to let us break from error cases without having a deep if-then-else structure */
	{
		if (MEMCMP_LIT(cnl->label, GDS_LABEL_GENERIC))
		{
			is_gtm_shm = FALSE;
			assert(FALSE);
			break;
		}
		is_gtm_shm = TRUE;
		remove_shmid = TRUE;
		memcpy(now_running, cnl->now_running, MAX_REL_NAME);
		if (memcmp(now_running, gtm_release_name, gtm_release_name_len + 1))
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len,
				   gtm_release_name, LEN_AND_STR(now_running));
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		if (!cnl->glob_sec_init)
		{
			glob_sec_init = FALSE;
			break;
		}
		glob_sec_init = TRUE;
		if (memcmp(cnl->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		{
			if (memcmp(cnl->label, GDS_LABEL, GDS_LABEL_SZ - 3))
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBNOTGDS, 2, DB_LEN_STR(reg),
					   ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
			else
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_BADDBVER, 2, DB_LEN_STR(reg),
					   ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
		 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' at least at
		 * cnl->fname[MAX_FN_LEN]
		 */
		assert(cnl->fname[MAX_FN_LEN] == '\0');  /* First '\0' in cnl->fname can be earlier */
		db_shm_in_sync = TRUE;	/* assume this unless proven otherwise */
		/* Check whether cnl->fname exists. If not, then db & shm are not in sync. */
		STAT_FILE((char *)cnl->fname, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			save_errno = errno;
			db_shm_in_sync = FALSE;
			if (ENOENT == save_errno)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
						DB_LEN_STR(reg), udi->shmid, cnl->fname, save_errno);
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
						DB_LEN_STR(reg),
					udi->shmid, cnl->fname);
				/* In this case, the shared memory no longer points to a valid db file in the filesystem.
				 * So it is best that we remove this shmid. But remove_shmid is already TRUE. Assert that.
				 */
				assert(remove_shmid);
			} else /* Could be permission issue */
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
						DB_LEN_STR(reg),
					 udi->shmid, cnl->fname, save_errno);
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_INFO(ERR_DBNAMEMISMATCH), 4,
						DB_LEN_STR(reg),
					udi->shmid, cnl->fname, save_errno);
				remove_shmid = FALSE; /* Shared memory might be pointing to valid database */
			}
		}
		if (db_shm_in_sync)
		{	/* Check if cnl->fname and cnl->dbfid are in sync. If not, then db & shm are not in sync */
			if (FALSE == is_gdid_stat_identical(&cnl->unique_id.uid, &stat_buf))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(10) MAKE_MSG_INFO(ERR_DBIDMISMATCH), 4,
				      cnl->fname, DB_LEN_STR(reg), udi->shmid, ERR_TEXT, 2,
				      LEN_AND_LIT("[MUPIP] Database filename and fileid in shared memory are not in sync"));
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(10) MAKE_MSG_INFO(ERR_DBIDMISMATCH), 4,
				      cnl->fname, DB_LEN_STR(reg), udi->shmid, ERR_TEXT, 2,
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
		/* Check if seg->fname and cnl->fname are identical filenames.
		 * If not, then db that we want to rundown points to a shared memory which in turn points
		 * to a DIFFERENT db file. In this case, do NOT remove the shared memory as it is possible
		 * some other database file on the system points to it and passes the filename identicality check.
		 */
		if (db_shm_in_sync && !is_file_identical((char *)cnl->fname, (char *)seg->fname))
		{
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBSHMNAMEDIFF), 4, DB_LEN_STR(reg),
				udi->shmid, cnl->fname);
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_INFO(ERR_DBSHMNAMEDIFF), 4, DB_LEN_STR(reg),
				udi->shmid, cnl->fname);
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
			/* Now that we have attached to db shm and verified it is usable, fix udi->counter_ftok_incremented
			 * to take into account the "ftok_counter_halted" flag. This will make sure we do not incorrectly
			 * delete the ftok semaphore in any error codepaths from now on until the end. In the end when we
			 * are sure it is safe to rundown this shm, we will fix udi->counter_ftok_incremented to what it
			 * should be to correctly remove the ftok semaphore.
			 */
			udi->counter_ftok_incremented = udi->counter_ftok_incremented && !cnl->ftok_counter_halted;
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
					RNDWN_ERR("!AD -> File already open by another process (2).", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				udi->counter_acc_incremented = TRUE;
			}
			/* If db & shm are in sync AND we aren't alone in using it, we can do nothing */
			if (0 != shm_buf.shm_nattch)
			{
				util_out_print("!AD [!UL]-> File is in use by another process.",
						TRUE, DB_LEN_STR(reg), udi->shmid);
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return FALSE;
			}
			if (prevent_mu_rndwn && cnl->jnl_file.u.inode)
			{
				if (override_present)
				{	/* If the rundown should normally be prevented, but the operator specified an
					 * OVERRIDE qualifier, record the fact of the usage in the syslog and continue.
					 */
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_MURNDWNOVRD, 2, DB_LEN_STR(reg),
						ERR_TEXT, 2,
						LEN_AND_LIT("Overriding OPEN journal file state in shared memory"));
				} else
				{	/* Journal file state being still open in shared memory implies a crashed state,
					 * so error out.
					 */
					PRINT_PREVENT_RUNDOWN_MESSAGE(reg, csa, need_rollback);
				}
			}
			/* The shared section is valid and up-to-date with respect to the database file header;
			 * ignore the temporary storage and use the shared section from here on. Now that the
			 * shared section is valid, check if there is a statsdb associated with this (that we created
			 * in this lifetime of the basedb shm or even before) and if so attempt a rundown of that first.
			 */
			if (cnl->statsdb_created)
			{
				assert(!IS_STATSDB_REG(reg));
				statsdb_fname_len = cnl->statsdb_fname_len;
				assert(statsdb_fname_len);
				statsdb_fname_ptr = cnl->statsdb_fname;
			} else if (!is_statsdb)
			{
				assert(!IS_STATSDB_REG(reg));
				assert(udi->fn == (char *)&seg->fname[0]);
				statsdb_fname_len = ARRAYSIZE(statsdb_fname);
				gvcst_set_statsdb_fname(tsd, reg, statsdb_fname, &statsdb_fname_len);
				statsdb_fname_ptr = &statsdb_fname[0];
			} else
				statsdb_fname_len = 0;
			if (statsdb_fname_len)
			{
				BASEDBREG_TO_STATSDBREG(reg, statsDBreg);
				COPY_STATSDB_FNAME_INTO_STATSREG(statsDBreg, statsdb_fname_ptr, statsdb_fname_len);
				/* Note: Error status of statsdb rundown is factored into final basedb rundown status
				 * by storing the return value in "statsDBrundown_status".
				 */
				statsDBrundown_status = mu_rndwn_file_statsdb(statsDBreg, &statsDBexists, standalone);
				assert(!cnl->statsdb_created || statsDBexists);
				assert(csa == cs_addrs);	/* cs_addrs should not have changed in above call */
				if (cnl->statsdb_created)
				{	/* If basedb shm exists, note statsdb rundown status in basedb shm too */
					cnl->statsdb_rundown_clean = statsDBrundown_status;
				}
			}
			csa->critical = (mutex_struct_ptr_t)(csa->db_addrs[0] + NODE_LOCAL_SIZE);
			assert(((INTPTR_T)csa->critical & 0xf) == 0);	/* critical should be 16-byte aligned */
#			ifdef CACHELINE_SIZE
			assert(0 == ((INTPTR_T)csa->critical & (CACHELINE_SIZE - 1)));
#			endif
			JNL_INIT(csa, reg, tsd);
			csa->shmpool_buffer = (shmpool_buff_hdr_ptr_t)(csa->db_addrs[0]
										+ NODE_LOCAL_SPACE(tsd) + JNL_SHARE_SIZE(tsd));
			csa->lock_addrs[0] = (sm_uc_ptr_t)csa->shmpool_buffer + SHMPOOL_SECTION_SIZE;
			csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
			if (dba_bg == acc_meth)
			{
				cs_data = csd = csa->hdr = (sgmnt_data_ptr_t)(csa->lock_addrs[1] + 1
								+ CACHE_CONTROL_SIZE(tsd));
				assert(cnl->cache_off == -CACHE_CONTROL_SIZE(csd));
				db_csh_ini(csa);
			} else
			{
				cs_data = csd = csa->hdr = (sgmnt_data_ptr_t)((sm_uc_ptr_t)csa->lock_addrs[1] + 1);
				FSTAT_FILE(udi->fd, &stat_buf, stat_res);
				if (-1 == stat_res)
				{
					RNDWN_ERR("!AD -> Error with fstat.", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				mmap_sz = stat_buf.st_size - BLK_ZERO_OFF(csd->start_vbn);
				CHECK_LARGEFILE_MMAP(reg, mmap_sz); /* can issue rts_error MMFILETOOLARGE */
				csa->db_addrs[0] = (sm_uc_ptr_t) MMAP_FD(udi->fd, mmap_sz,
										BLK_ZERO_OFF(csd->start_vbn), FALSE);
				if (-1 == (sm_long_t)(csa->db_addrs[0]))
				{
					RNDWN_ERR("!AD -> Error mapping memory", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
				csa->db_addrs[1] = csa->db_addrs[0] + mmap_sz - 1;
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
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return FALSE;
			}
			if (FROZEN_CHILLED(csa) && !override_present)
			{	/* If there is an online freeze, we can't do the file writes, so autorelease or give up. */
				DO_CHILLED_AUTORELEASE(csa, csd);
				if (FROZEN_CHILLED(csa))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_OFRZACTIVE, 2, DB_LEN_STR(reg));
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
				}
			}
			/* If there was an online freeze and it was autoreleased, we don't want to take down the shared memory
			 * and lose the freeze_online state.
			 */
			if (CHILLED_AUTORELEASE(csa) && !override_present)
				remove_shmid = FALSE;
			db_common_init(reg, csa, csd); /* do initialization common to "db_init" and "mu_rndwn_file" */
			do_crypt_init = USES_ENCRYPTION(csd->is_encrypted);
			crypt_warning = FALSE;
			INITIALIZE_CSA_ENCR_PTR(csa, csd, udi, do_crypt_init, crypt_warning, FALSE);	/* sets csa->encr_ptr */
			/* cleanup mutex stuff */
			csa->hdr->image_count = 0;
			gtm_mutex_init(reg, NUM_CRIT_ENTRY(csa->hdr), FALSE); /* this is the only process running */
			assert(!csa->hold_onto_crit); /* so it is safe to do unconditional grab_crit/rel_crit below */
			cnl->in_crit = 0;
			csa->now_crit = FALSE;
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
			cnl->wcs_phase2_commit_pidcnt = 0;
			DEBUG_ONLY(in_mu_rndwn_file = TRUE);
			TREF(donot_write_inctn_in_wcs_recover) = TRUE;
			/* If an orphaned snapshot is lying around, then clean it up */
			/* SS_MULTI: If multiple snapshots are supported, then we must run-through each of the
			 * "possible" snapshots in progress and clean them up
			 */
			assert(1 == MAX_SNAPSHOTS);
			ss_get_lock(gv_cur_region); /* Snapshot initiator will wait until this cleanup is done */
			ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(csa);
			ss_pid = ss_shm_ptr->ss_info.ss_pid;
			if (ss_pid && !is_proc_alive(ss_pid, 0))
				ss_release(NULL);
			ss_release_lock(gv_cur_region);
			/* If cnl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and
			 * therefore we need not flush shared memory contents to disk as they might be in an inconsistent
			 * state. Moreover, any more flushing will only cause the future rollback/recover to undo more
			 * journal records (PBLKs). In this case, we will go ahead and remove shared memory (without
			 * flushing the contents) in this routine. A reissue of the recover/rollback command will restore
			 * the database to a consistent state.
			 */
			if (!cnl->donotflush_dbjnl)
			{
				if (cnl->glob_sec_init)
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
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_MURNDWNOVRD, 2,
								DB_LEN_STR(reg), ERR_TEXT, 2, LEN_AND_LIT(
								"Overriding error during database block flush"));
							csd = csa->hdr;
						} else
						{	/* In case of MUPIP RUNDOWN append a suggestion to use OVERRIDE flag
							 * to bypass the error.
							 */
							if (standalone)
								rts_error_csa(CSA_ARG(csa) VARLSTCNT(4)
									ERR_JNLORDBFLU, 2, DB_LEN_STR(reg));
							else
								rts_error_csa(CSA_ARG(csa) VARLSTCNT(8)
									ERR_JNLORDBFLU, 2, DB_LEN_STR(reg),
									ERR_TEXT, 2, LEN_AND_LIT(
									"To force the operation to proceed, use the "
									"OVERRIDE qualifier"));
							assert(FALSE);	/* The above rts_error should not return. */
							return FALSE;
						}
					}
				}
				jpc = csa->jnl;
				if (NULL != jpc)
				{	/* this swaplock should probably be a mutex */
					grab_crit(gv_cur_region);
					/* If we own it or owner died, clear the fsync lock */
					if (process_id == jpc->jnl_buff->fsync_in_prog_latch.u.parts.latch_pid)
					{
						RELEASE_SWAPLOCK(&jpc->jnl_buff->fsync_in_prog_latch);
					} else
						performCASLatchCheck(&jpc->jnl_buff->fsync_in_prog_latch, FALSE);
					if (NOJNL != jpc->channel)
						jnl_file_close(gv_cur_region, cleanjnl_present, FALSE);
					free(jpc);
					csa->jnl = NULL;
					rel_crit(gv_cur_region);
				}
			}
			/* Write master-map (in addition to file header) as wcs_flu done above would not have updated it */
			csd_size = SIZEOF_FILE_HDR(csd); 	/* SIZEOF(sgmnt_data) + master-map */
			TREF(donot_write_inctn_in_wcs_recover) = FALSE;
		} else
		{	/* In this case, we just want to clear a few fields in the file header but we do NOT want to
			 * write the master map which we don't have access to at this point so set csd_size accordingly.
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
			/* If "db_shm_in_sync" is TRUE, csd points to shared memory which is already aligned so we do
			 * not need to do any more alignment if fd is opened with O_DIRECT.
			 */
			if (db_shm_in_sync)
				buff = (char *)csd;
			else
			{
				assert(!udi->fd_opened_with_o_direct || DIO_BUFF_NO_OVERFLOW((TREF(dio_buff)), csd_size));
				ALIGN_BUFF_IF_NEEDED_FOR_DIO(udi, buff, csd, csd_size);	/* sets "buff" */
			}
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, (off_t)0, buff, csd_size, status);
			if (0 != status)
			{
				RNDWN_ERR("!AD -> Error writing header to disk.", reg);
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return FALSE;
			}
		}
		if (NULL != tsd)
		{
			assert(!db_shm_in_sync);
			free(tsd);
			tsd = NULL;
		}
#		if !defined(_AIX)
		if ((dba_mm == acc_meth) && db_shm_in_sync)
		{
			assert(0 != mmap_sz);
			if (-1 == msync((caddr_t)csa->db_addrs[0], mmap_sz, MS_SYNC))
			{
				RNDWN_ERR("!AD -> Error synchronizing mapped memory.", reg);
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return FALSE;
			}
			if (-1 == munmap((caddr_t)csa->db_addrs[0], mmap_sz))
			{
				RNDWN_ERR("!AD -> Error unmapping mapped memory.", reg);
				MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
				return FALSE;
			}
		}
#		endif
		IF_LIBAIO(aio_shim_destroy(udi->owning_gd);)
		/* If this is a BASEDB and we are the last one out, unlink/remove the corresponding STASDB if one exists */
		/* Note that if "is_statsdb" is TRUE and basedb was not found, we cannot safely delete the statsdb (deletion
		 * of statsdb requires ftok lock on the basedb). In that case, we leave it as is. And when the basedb is next
		 * opened, we will remove this statsdb and create a new one.
		 */
		UNLINK_STATSDB_AT_BASEDB_RUNDOWN(cnl);
		break;
	}
	/* Detach from shared memory whether it is a GT.M shared memory or not */
	if (-1 == shmdt((caddr_t)cnl))
	{
		assert(FALSE);
		RNDWN_ERR("!AD -> Error detaching from shared memory.", reg);
		MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
		return FALSE;
	}
	csa->nl = cnl = NULL;
	csa->hdr = NULL;
	/* Remove the shared memory only if it is a GT.M created one. */
	if (is_gtm_shm)
	{
		if (remove_shmid) /* Note: remove_shmid is defined only if is_gtm_shm is TRUE */
		{
			if (0 != shm_rmid(udi->shmid))
			{
				save_errno = errno;
				if (!SHM_REMOVED(save_errno))
				{
					assert(FALSE);
					RNDWN_ERR("!AD -> Error removing shared memory.", reg);
					MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
					return FALSE;
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
	 * don't want the database to connect to that non-GT.M-shmid or uninitialized-GT.M-shmid anymore.
	 */
	if (!is_gtm_shm || !glob_sec_init)
	{
		assert(NULL != tsd);	/* should not have been freed */
		RESET_SHMID_CTIME(tsd);
		if (mupip_jnl_recover)
			memset(tsd->machine_name, 0, MAX_MCNAMELEN);
		ALIGN_BUFF_IF_NEEDED_FOR_DIO(udi, buff, tsd, tsd_size);	/* sets "buff" */
		DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, (off_t)0, buff, tsd_size, status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
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
			MU_RNDWN_FILE_CLNUP(reg, udi, tsd, sem_created, udi->counter_acc_incremented);
			return FALSE;
		}
		if (!sem_created)
		{
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(3) ERR_SEMREMOVED, 1, udi->semid);
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(3) ERR_SEMREMOVED, 1, udi->semid);
		}
		udi->grabbed_access_sem = FALSE;
		udi->counter_acc_incremented = FALSE;
		udi->semid = INVALID_SEMID;
		sem_created = FALSE;
	}
	REVERT;
	RESET_GV_CUR_REGION;
	/* We successfully ran down the database. So restore udi->counter_ftok_incremented before "ftok_sem_release"
	 * that way the ftok semaphore gets deleted correctly as part of the counter decrement.
	 */
	udi->counter_ftok_incremented = !ftok_counter_halted;
	if (!ftok_sem_release(reg, udi->counter_ftok_incremented && !standalone, !standalone))
		return FALSE;
	/* if "standalone" we better leave this function with standalone access */
	assert(!standalone || udi->grabbed_access_sem);
	CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID */
	DEBUG_ONLY(in_mu_rndwn_file = FALSE);
	return statsDBrundown_status;
}

CONDITION_HANDLER(mu_rndwn_file_ch)
{
	unix_db_info	*udi;
	sgmnt_addrs	*csa;

	START_CH(TRUE);
	PRN_ERROR;
	assert(NULL != rundown_reg);
	if (NULL != rundown_reg)
	{
		udi = FILE_INFO(rundown_reg);
		csa = &udi->s_addrs;
		DEBUG_ONLY(in_mu_rndwn_file = FALSE);
		TREF(donot_write_inctn_in_wcs_recover) = FALSE;
		if (udi->counter_acc_incremented)
		{	/* Decrement the access control semaphore in case we incremented it. */
			do_semop(udi->semid, DB_CONTROL_SEM, -1, IPC_NOWAIT | SEM_UNDO);
			udi->counter_acc_incremented = FALSE;
		}
		if (sem_created || (shm_status_confirmed && no_shm_exists))
		{	/* Remove the access control semaphore if either we just created it, or we know for a fact that there is
			 * no shared memory */
			sem_rmid(udi->semid);
			udi->semid = INVALID_SEMID;
		}
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(rundown_reg, udi->counter_ftok_incremented, !mu_rndwn_file_standalone);
	}
	RESET_GV_CUR_REGION;
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
