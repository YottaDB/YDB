/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include <sys/ipc.h>
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
#include "eintr_wrappers.h"
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
#include "ftok_sems.h"
#include "mu_rndwn_all.h"
#include "error.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			process_id;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	gd_region		*standalone_reg;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		mu_rndwn_file_dbjnl_flush;

static gd_region	*rundown_reg = NULL;
static gd_region	*temp_region;
static sgmnt_data_ptr_t	temp_cs_data;
static sgmnt_addrs	*temp_cs_addrs;
static boolean_t	restore_rndwn_gbl;

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

#ifdef DEBUG_DB64
/* if debugging large address stuff, make all memory segments allocate above 4G line */
GBLREF	sm_uc_ptr_t	next_smseg;
#endif

/*
 * Description:
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
 * Return Value:
 *	TRUE for success
 *	FALSE for failure
 */
bool mu_rndwn_file(gd_region *reg, bool standalone)
{
	int			status, save_errno, sopcnt, tsd_size;
	char                    now_running[MAX_REL_NAME];
	boolean_t		rc_cpt_removed = FALSE, sem_created = FALSE;
	sgmnt_data_ptr_t	csd, tsd = NULL;
	jnl_private_control	*jpc;
	struct sembuf		sop[4];
	struct shmid_ds		shm_buf;
	file_control		*fc;
	unix_db_info		*udi;
	enum db_acc_method	acc_meth;
        struct stat     	stat_buf;
	struct semid_ds		semstat;
	union semun		semarg;
	int			semop_res, stat_res;
	uint4			status_msg;
	gd_id			tmp_dbfid;

	error_def (ERR_BADDBVER);
	error_def (ERR_DBFILERR);
	error_def (ERR_DBNOTGDS);
	error_def (ERR_DBRDONLY);
	error_def (ERR_DBNAMEMISMATCH);
	error_def (ERR_DBIDMISMATCH);
	error_def (ERR_TEXT);
	error_def (ERR_VERMISMATCH);
	error_def (ERR_CRITSEMFAIL);

	restore_rndwn_gbl = FALSE;
	assert(!mupip_jnl_recover || standalone);
	temp_region = gv_cur_region; 	/* save gv_cur_region wherever there is scope for it to be changed */
	rundown_reg = gv_cur_region = reg;
#ifdef GTCM_RC
        rc_cpt_removed = mupip_rundown_cpt();
#endif
	fc = reg->dyn.addr->file_cntl;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	gv_cur_region = temp_region;
	udi = FILE_INFO(reg);
	if (SS_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(5) status, 2, DB_LEN_STR(reg), errno);
		close(udi->fd);
                return FALSE;
	}
	/*
	 * read_only process cannot rundown database.
	 * read only process can succeed to get standalone access of the database,
	 *	if the db is clean with no orphaned shared memory.
	 * Note: we use gtmsecshr for updating file header for semaphores id.
	 */
	if (reg->read_only && !standalone)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		close(udi->fd);
		return FALSE;
	}
	ESTABLISH_RET(mu_rndwn_file_ch, FALSE);
	if (!ftok_sem_get(reg, TRUE, GTM_ID, !standalone))
	{
		close(udi->fd);
		REVERT;
		return FALSE;
	}
	/*
	 * Now we have standalone access of the database using ftok semaphore.
	 * Any other ftok conflicted database will suspend there operation at this point.
	 * At the end of this routine, we release ftok semaphore lock.
	 */
	/* We only need to read sizeof(sgmnt_data) here */
	tsd_size = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
	tsd = (sgmnt_data_ptr_t)malloc(tsd_size);
	LSEEKREAD(udi->fd, 0, tsd, tsd_size, status);
	if (0 != status)
	{
		RNDWN_ERR("!AD -> Error reading from file.", reg);
		close(udi->fd);
		free(tsd);
		REVERT;
		ftok_sem_release(reg, TRUE, TRUE);
		return FALSE;
	}
	udi->shmid = tsd->shmid;
	udi->semid = tsd->semid;
	udi->gt_sem_ctime = tsd->gt_sem_ctime.ctime;
	udi->gt_shm_ctime = tsd->gt_shm_ctime.ctime;
	if (standalone)
	{
		semarg.buf = &semstat;
		if (INVALID_SEMID == udi->semid || -1 == semctl(udi->semid, 0, IPC_STAT, semarg) ||
			tsd->gt_sem_ctime.ctime != semarg.buf->sem_ctime)
		{
			/*
			 * if no database access control semaphore id is found, or,
			 * if semaphore id is found but ctime does not match, create new semaphore.
			 */
			if (-1 == (udi->semid = semget(IPC_PRIVATE, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
			{
				udi->semid = INVALID_SEMID;
				RNDWN_ERR("!AD -> Error with semget with IPC_CREAT.", reg);
				close(udi->fd);
				free(tsd);
				REVERT;
				ftok_sem_release(reg, TRUE, TRUE);
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
				close(udi->fd);
				if (sem_created && -1 == semctl(udi->semid, 0, IPC_RMID))
						RNDWN_ERR("!AD -> Error removing semaphore.", reg);
				free(tsd);
				REVERT;
				ftok_sem_release(reg, TRUE, TRUE);
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
				close(udi->fd);
				if (sem_created && -1 == semctl(udi->semid, 0, IPC_RMID))
						RNDWN_ERR("!AD -> Error removing semaphore.", reg);
				free(tsd);
				REVERT;
				ftok_sem_release(reg, TRUE, TRUE);
				return FALSE;
			}
			udi->gt_sem_ctime = tsd->gt_sem_ctime.ctime = semarg.buf->sem_ctime;
		}
		/* Now lock the database using access control semaphore and increment counter */
		sop[0].sem_num = 0; sop[0].sem_op = 0;
		sop[1].sem_num = 0; sop[1].sem_op = 1;
		sop[2].sem_num = 1; sop[2].sem_op = 0;
		sop[3].sem_num = 1; sop[3].sem_op = 1;
		sopcnt = 4;
		sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = SEM_UNDO | IPC_NOWAIT;
		SEMOP(udi->semid, sop, sopcnt, semop_res);
		if (-1 == semop_res)
		{
			if (sem_created)
			{
				RNDWN_ERR("!AD -> Error doing SEMOP.", reg);
				if (-1 == semctl(udi->semid, 0, IPC_RMID))
					RNDWN_ERR("!AD -> Error removing semaphore.", reg);
			}
			else
				RNDWN_ERR("!AD -> File already open by another process.", reg);
			close(udi->fd);
			free(tsd);
			REVERT;
			ftok_sem_release(reg, TRUE, TRUE);
			return FALSE;
		}
		/* At this point, we have the the database access control lock.
		 * We also incremented the counter semaphore.
		 * We also have the ftok semaphore lock acquired.
		 */
	}

	/* Now rundown database if shared memory segment exists.
	 * We try this for both values of "standalone"
	 */
	if (INVALID_SHMID == udi->shmid || -1 == shmctl(udi->shmid, IPC_STAT, &shm_buf) ||
		tsd->gt_shm_ctime.ctime != shm_buf.shm_ctime) /* if no shared memory exists */
	{
		if (rc_cpt_removed)
		{       /* reset RC values if we've rundown the RC CPT */
			/* attempt to force-write header */
			tsd->rc_srv_cnt = tsd->dsid = tsd->rc_node = 0;
			tsd->owner_node = 0;
			free(tsd);
			REVERT;
			ftok_sem_release(reg, TRUE, TRUE);
			return FALSE; /* This is safer, when this code is not complete */
		} else
		{	/* Note that if creation time does not match, we ignore that shared memory segment.
			 * It might result in orphaned shared memory segment.
			 */
			tsd->shmid = udi->shmid = INVALID_SHMID;
			tsd->gt_shm_ctime.ctime = udi->gt_shm_ctime = 0;
			if (standalone)
			{
				/* Reset ipc fields in file header */
				if (!reg->read_only)
				{
					if (mupip_jnl_recover)
					{
						memset(tsd->machine_name, 0, MAX_MCNAMELEN);
						tsd->freeze = 0;
						tsd->owner_node = 0;
					}
					LSEEKWRITE(udi->fd, (off_t)0, tsd, tsd_size, status);
					if (0 != status)
					{
						RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
						CLNUP_RNDWN(udi, reg);
						free(tsd);
						return FALSE;
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
						gtm_putmsg(VARLSTCNT(1) status_msg);
						RNDWN_ERR("!AD -> get_full_path failed.", reg);
						CLNUP_RNDWN(udi, reg);
						free(tsd);
						return FALSE;
					}
					db_ipcs.fn[db_ipcs.fn_len] = 0;
					if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
					{
						RNDWN_ERR("!AD -> gtmsecshr was unable to write header to disk.", reg);
						CLNUP_RNDWN(udi, reg);
						free(tsd);
						return FALSE;
					}
				}
				close(udi->fd);
				REVERT;
				if (!ftok_sem_release(reg, FALSE, FALSE))
				{
					RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
					if (sem_created && -1 == semctl(udi->semid, 0, IPC_RMID))
						RNDWN_ERR("!AD -> Error removing semaphore.", reg);
					free(tsd);
					return FALSE;
				}
				free(tsd);
				standalone_reg = reg;
				return TRUE; /* For "standalone" and "no shared memory existing", we exit here */
			} else
			{	/* We are here for not standalone (basically the "mupip rundown" command).
				 * We have not created any new semaphore and no shared memory was existing.
				 * So just reset the file header ipc fields and exit.
				 */
				assert(!sem_created);
				memset(tsd->machine_name, 0, MAX_MCNAMELEN);
				tsd->freeze = 0;
				tsd->owner_node = 0;
				tsd->shmid = INVALID_SHMID;
				tsd->semid = INVALID_SEMID;
				tsd->gt_sem_ctime.ctime = 0;
				tsd->gt_shm_ctime.ctime = 0;
			}
		}
		assert(!standalone);
		LSEEKWRITE(udi->fd, (off_t)0, tsd, tsd_size, status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			CLNUP_RNDWN(udi, reg);
			free(tsd);
			return FALSE;
		}
		close(udi->fd);
		free(tsd);
		REVERT;
		/* For mupip rundown (standalone = FALSE), we release/remove ftok semaphore here. */
		if (!ftok_sem_release(reg, TRUE, TRUE))
		{
			RNDWN_ERR("!AD -> Error from ftok_sem_release.", reg);
			assert(!sem_created);
			return FALSE;
		}
		return TRUE; /* For "!standalone" and "no shared memory existing", we exit here */
	}
	/*
	 * shared memory already exists, so now find the number of users attached to it
	 * if it is zero, it needs to be flushed to disk, otherwise, we cannot help.
	 */
	/* If we aren't alone in using it, we can do nothing */
	if (0 != shm_buf.shm_nattch)
	{
		util_out_print("!AD -> File is in use by another process.", TRUE, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	if (reg->read_only)             /* read only process can't succeed beyond this point */
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	/* Now we have a pre-existing shared memory section with no other processes attached.  Do some setup */
	if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
       	{
		if (memcmp(tsd->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg(VARLSTCNT(4) ERR_DBNOTGDS, 2, DB_LEN_STR(reg));
		else
			gtm_putmsg(VARLSTCNT(4) ERR_BADDBVER, 2, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
       	}
	reg->dyn.addr->acc_meth = acc_meth = tsd->acc_meth;
	dbsecspc(reg, tsd);
#ifdef __MVS__
	/* match gvcst_init_sysops.c shmget with __IPC_MEGA */
	if (ROUND_UP(reg->sec_size, MEGA_BOUND) != shm_buf.shm_segsz)
#else
	if (reg->sec_size != shm_buf.shm_segsz)
#endif
	{
		util_out_print("Expected shared memory size is !SL, but found !SL",
			TRUE, reg->sec_size, shm_buf.shm_segsz);
		util_out_print("!AD -> Existing shared memory size do not match.", TRUE, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	/*
	 * save gv_cur_region, cs_data, cs_addrs, and restore them on return
	 */
	temp_region = gv_cur_region;
	temp_cs_data = cs_data;
	temp_cs_addrs = cs_addrs;
	restore_rndwn_gbl = TRUE;
	gv_cur_region = reg;
	tp_change_reg();
#ifdef DEBUG_DB64
	SEG_SHMATTACH(next_smseg, reg);
	next_smseg = (sm_uc_ptr_t)ROUND_UP((sm_long_t)(next_smseg + reg->sec_size), SHMAT_ADDR_INCS);
#else
	SEG_SHMATTACH(0, reg);
#endif
	cs_addrs->nl = (node_local_ptr_t)cs_addrs->db_addrs[0];
	memcpy(now_running, cs_addrs->nl->now_running, MAX_REL_NAME);
	if (memcmp(now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		gtm_putmsg(VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len,
				gtm_release_name, LEN_AND_STR(now_running));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg(VARLSTCNT(8) ERR_DBNOTGDS, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
		else
			gtm_putmsg(VARLSTCNT(8) ERR_BADDBVER, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_LITERAL("(from shared segment - nl)"));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	/* Since nl is memset to 0 initially and then fname is copied over from gv_cur_region and since "fname" is
	 * guaranteed to not exceed MAX_FN_LEN, we should have a terminating '\0' atleast at cs_addrs->nl->fname[MAX_FN_LEN]
	 */
	assert(cs_addrs->nl->fname[MAX_FN_LEN] == '\0');	/* Note the first '\0' in cs_addrs->nl->fname can be much earlier */
	/* Check whether cs_addrs->nl->fname exists. If not, then it is a serious condition. Error out. */
	STAT_FILE((char *)cs_addrs->nl->fname, &stat_buf, stat_res);
	if (-1 == stat_res)
	{
		save_errno = errno;
		send_msg(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, save_errno);
		gtm_putmsg(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, save_errno);
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	/* Check whether csa->nl->fname and csa->nl->dbfid are in sync. If not its a serious condition. Error out. */
	set_gdid_from_stat(&tmp_dbfid, &stat_buf);
	if (FALSE == is_gdid_gdid_identical(&tmp_dbfid, &cs_addrs->nl->unique_id.uid))
	{
		send_msg(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid,
			ERR_TEXT, 2, LEN_AND_LIT("[MUPIP] Database filename and fileid in shared memory are not in sync"));
		rts_error(VARLSTCNT(10) ERR_DBIDMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid,
			ERR_TEXT, 2, LEN_AND_LIT("[MUPIP] Database filename and fileid in shared memory are not in sync"));
		CLNUP_RNDWN(udi, reg);
		free(tsd);
		return FALSE;
	}
	/* The shared section is valid and up-to-date with respect to the database file header;
	 * ignore the temporary storage and use the shared section from here on
	 */
	cs_addrs->critical = (mutex_struct_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SIZE);
        assert(((int)cs_addrs->critical & 0xf) == 0);                        /* critical should be 16-byte aligned */
#ifdef CACHELINE_SIZE
	assert(0 == ((int)cs_addrs->critical & (CACHELINE_SIZE - 1)));
#endif
	JNL_INIT(cs_addrs, reg, tsd);
	cs_addrs->shmpool_buffer = (shmpool_buff_hdr_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SPACE + JNL_SHARE_SIZE(tsd));
	cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)cs_addrs->shmpool_buffer + SHMPOOL_BUFFER_SIZE;
	cs_addrs->lock_addrs[1] = cs_addrs->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;

	if (dba_bg == acc_meth)
		cs_data = csd = cs_addrs->hdr = (sgmnt_data_ptr_t)(cs_addrs->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd));
	else
	{
		cs_addrs->acc_meth.mm.mmblk_state = (mmblk_que_heads_ptr_t)(cs_addrs->lock_addrs[1] + 1);
		FSTAT_FILE(udi->fd, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			RNDWN_ERR("!AD -> Error with fstat.", reg);
			CLNUP_RNDWN(udi, reg);
			free(tsd);
			return FALSE;
		}
#ifdef DEBUG_DB64
		SEG_MEMMAP(next_smseg, reg);
		next_smseg = (sm_uc_ptr_t)ROUND_UP((sm_long_t)(next_smseg + stat_buf.st_size), SHMAT_ADDR_INCS);
#else
		SEG_MEMMAP(NULL, reg);
#endif
		csd = cs_addrs->hdr = (sgmnt_data_ptr_t)cs_addrs->db_addrs[0];
		cs_addrs->db_addrs[1] = cs_addrs->db_addrs[0] + stat_buf.st_size - 1;
	}
	if (sem_created)
	{
		csd->semid = tsd->semid;
		csd->gt_sem_ctime.ctime = tsd->gt_sem_ctime.ctime;
	}
	assert(JNL_ALLOWED(csd) == JNL_ALLOWED(tsd));
	free(tsd);
	tsd = NULL;
	/* Check to see that the fileheader in the shared segment is valid, so we won't endup flushing garbage to db file */
	/* Check the label in the header - keep adding any further appropriate checks in future here */
	if (memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 3))
		{
			status_msg = ERR_DBNOTGDS;
			if (0 != shm_rmid(udi->shmid))
				gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error removing shared memory"));
		} else
			status_msg = ERR_BADDBVER;
		gtm_putmsg(VARLSTCNT(8) status_msg, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_LITERAL("(File header in the shared segment seems corrupt)"));
		CLNUP_RNDWN(udi, reg);
		return FALSE;
	}
	if (dba_bg == acc_meth)
		db_csh_ini(cs_addrs);
	else
		cs_addrs->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_ulong_t)csd + (int)(csd->start_vbn - 1) * DISK_BLOCK_SIZE);
	db_common_init(reg, cs_addrs, csd);	/* do initialization common to db_init() and mu_rndwn_file() */

	/* cleanup mutex stuff */
	cs_addrs->hdr->image_count = 0;
	gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE); /* it is ensured, this is the only process running */
	cs_addrs->nl->in_crit = 0;
	cs_addrs->now_crit = cs_addrs->read_lock = FALSE;

	reg->open = TRUE;
	if (rc_cpt_removed)
		csd->rc_srv_cnt = csd->dsid = csd->rc_node = 0;   /* reset RC values if we've rundown the RC CPT */
	mu_rndwn_file_dbjnl_flush = TRUE;	/* indicate to wcs_recover() no need to write inctn or increment db curr_tn */
	/* If csa->nl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and therefore we should
	 * 	not flush shared memory contents to disk as they might be in an inconsistent state.
	 * In this case, we will go ahead and remove shared memory (without flushing the contents) in this routine.
	 * A reissue of the recover/rollback command will restore the database to a consistent state.
	 */
	if (!cs_addrs->nl->donotflush_dbjnl)
	{
		if (cs_addrs->nl->glob_sec_init)
		{	/* WCSFLU_NONE only is done here, as we aren't sure of the state, so no EPOCHs are written.
			 * If we write an EPOCH record, recover may get confused
			 * Note that for journaling we do not call jnl_file_close() with TRUE for second parameter.
			 * As a result journal file might not have an EOF record.
			 * So, a new process will switch the journal file and cut the journal file link,
			 * though it might be a good journal without an EOF
			 */
			JNL_SHORT_TIME(jgbl.gbl_jrec_time); /* needed for jnl_put_jrt_pini() and jnl_write_inctn_rec() */
			wcs_flu(WCSFLU_NONE);
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
				performCASLatchCheck(&jpc->jnl_buff->fsync_in_prog_latch, 0);
			if (NOJNL != jpc->channel)
				jnl_file_close(gv_cur_region, FALSE, FALSE);
			free(jpc);
			cs_addrs->jnl = NULL;
			rel_crit(gv_cur_region);
		}
	}
	mu_rndwn_file_dbjnl_flush = FALSE;
	reg->open = FALSE;
	/* Note: At this point we have write permission */
	memset(csd->machine_name, 0, MAX_MCNAMELEN);
	csd->owner_node = 0;
	csd->freeze = 0;
	csd->shmid = INVALID_SHMID;
	csd->gt_shm_ctime.ctime = 0;
	if (!standalone)
	{
		csd->semid = INVALID_SEMID;
		csd->gt_sem_ctime.ctime = 0;
	}
	if (dba_bg == acc_meth)
	{
		LSEEKWRITE(udi->fd, (off_t)0, csd, SIZEOF_FILE_HDR(csd), status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Error writing header to disk.", reg);
			CLNUP_RNDWN(udi, reg);
			return FALSE;
		}
	} else
	{
		if (-1 == msync((caddr_t)cs_addrs->db_addrs[0], (size_t)stat_buf.st_size, MS_SYNC))
		{
			RNDWN_ERR("!AD -> Error synchronizing mapped memory.", reg);
			CLNUP_RNDWN(udi, reg);
    			return FALSE;
		}
		if (-1 == munmap((caddr_t)cs_addrs->db_addrs[0], (size_t)stat_buf.st_size))
		{
			RNDWN_ERR("!AD -> Error unmapping mapped memory.", reg);
			CLNUP_RNDWN(udi, reg);
			return FALSE;
		}
	}
	if (-1 == shmdt((caddr_t)cs_addrs->db_addrs[0]))
	{
		RNDWN_ERR("!AD -> Error detaching from shared memory.", reg);
		CLNUP_RNDWN(udi, reg);
		return FALSE;
	}
	if (0 != shm_rmid(udi->shmid))
	{
		RNDWN_ERR("!AD -> Error removing shared memory.", reg);
		CLNUP_RNDWN(udi, reg);
		return FALSE;
	}
	if (!standalone)
	{
		if (is_orphaned_gtm_semaphore(udi->semid) && 0 != sem_rmid(udi->semid))
		{
			RNDWN_ERR("!AD -> Error removing semaphore.", reg);
			CLNUP_RNDWN(udi, reg);
			return FALSE;
		}
	}
	udi->shmid = INVALID_SHMID;
	udi->gt_shm_ctime = 0;
	REVERT;
	RESET_GV_CUR_REGION;
	restore_rndwn_gbl = TRUE;
	/* For mupip rundown, standalone == TRUE and we want to rlease/remove ftok semaphore.
	 * Otherwise, just release ftok semaphore lock. counter will be one more for this process.
	 * Exit handlers must take care of removing if necessary.
	 */
	if (!ftok_sem_release(reg, !standalone, !standalone))
		return FALSE;
	if (standalone)
		standalone_reg = reg;
	close(udi->fd);
	return TRUE;
}

CONDITION_HANDLER(mu_rndwn_file_ch)
{
	unix_db_info	*udi;

	START_CH;
	mu_rndwn_file_dbjnl_flush = FALSE;
	udi = FILE_INFO(rundown_reg);
	if (udi->grabbed_ftok_sem)
		ftok_sem_release(rundown_reg, FALSE, TRUE);
	standalone_reg = NULL;
	if (restore_rndwn_gbl)
	{
		RESET_GV_CUR_REGION;
	}
	NEXTCH;
}
