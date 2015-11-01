/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "gtm_string.h"
#include <unistd.h>

#include "gtm_sem.h"
#include "gtm_stdio.h"
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
#include "gvcst_init_sysops.h"
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

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region        *gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF uint4		process_id;
GBLREF boolean_t	mupip_jnl_recover;
GBLREF ipcs_mesg	db_ipcs;
GBLREF gd_region	*standalone_reg;

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

#ifdef DEBUG_DB64
/* if debugging large address stuff, make all memory segments allocate above 4G line */
GBLREF	sm_uc_ptr_t	next_smseg;
#endif

bool mu_rndwn_file(gd_region *reg, bool standalone)
{
	int			status, save_errno, sopcnt;
	char                    now_running[MAX_REL_NAME];
	boolean_t		rc_cpt_removed, sem_created = FALSE;
	sgmnt_data_ptr_t	csd, tsd, temp_cs_data;
	struct sembuf		sop[4];
	struct shmid_ds		shm_buf;
	file_control		*fc;
	unix_db_info		*udi;
	enum db_acc_method	acc_meth;
        struct stat     	stat_buf;
	gd_region		*temp_region;
	sgmnt_addrs		*temp_cs_addrs;
	struct semid_ds		semstat;
	union semun		semarg;
	int			semop_res, stat_res;
	uint4			jnl_status, status_msg;
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

	assert(!mupip_jnl_recover || standalone);
	jnl_status = 0;
	temp_region = gv_cur_region; 	/* save gv_cur_region wherever there is scope for it to be changed */
	gv_cur_region = reg;
        rc_cpt_removed = mupip_rundown_cpt();
	tsd = (sgmnt_data_ptr_t)malloc(sizeof(*tsd));
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
	 * if mu_rndwn_file() is called for standalone, it's still possible for
	 * a read only process to succeed, if the db is clean with no orphaned shared memory.
	 * We use gtmsecshr for updating file header for semaphores we create here.
	 */
	if (reg->read_only && !standalone)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		close(udi->fd);
		return FALSE;
	}
	if (!ftok_sem_get(reg, TRUE, GTM_ID, !standalone))
	{
		close(udi->fd);
		return FALSE;
	}
	/*
	 * Now we have locked the database using ftok_sem.
	 * Any other ftok conflicted database will suspend there operation at this point.
	 * At the end of this routine, we release ftok semaphore lock.
	 */
	LSEEKREAD(udi->fd, 0, tsd, sizeof(*tsd), status);
	if (0 != status)
	{
		RNDWN_ERR("!AD -> Error reading from file.", reg);
		close(udi->fd);
		ftok_sem_release(reg, TRUE, TRUE);
		return FALSE;
	}
	udi->shmid = tsd->shmid;
	udi->semid = tsd->semid;
	udi->sem_ctime = tsd->sem_ctime.ctime;
	udi->shm_ctime = tsd->shm_ctime.ctime;
	if (standalone)
	{
		semarg.buf = &semstat;
		if (0 == udi->semid || -1 == semctl(udi->semid, 0, IPC_STAT, semarg) ||
			tsd->sem_ctime.ctime != semarg.buf->sem_ctime)
		{
			/*
			 * if no database access control lock semaphore id is found, or,
			 * id is found but ctime does not match, create new semaphore.
			 */
			if (-1 == (udi->semid = semget(IPC_PRIVATE, 3, RWDALL | IPC_CREAT)))
			{
				RNDWN_ERR("!AD -> Error with semget with IPC_CREAT.", reg);
				close(udi->fd);
				ftok_sem_release(reg, TRUE, TRUE);
				return FALSE;
			}
			tsd->semid = udi->semid;
			sem_created = TRUE;
			if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg))
			{
				RNDWN_ERR("!AD -> Error with semctl with IPC_STAT.", reg);
				close(udi->fd);
				ftok_sem_release(reg, TRUE, TRUE);
				return FALSE;
			}
			udi->sem_ctime = tsd->sem_ctime.ctime = semarg.buf->sem_ctime;
		}
		/* Increment the database access control semaphore after checking it's zero */
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
			}
			else
			{
				RNDWN_ERR("!AD -> File already open by another process.", reg);
			}
			close(udi->fd);
			ftok_sem_release(reg, TRUE, TRUE);
			return FALSE;
		}
	}
	/*
	 * At this point if standalone == TRUE, we have the the database access control lock.
	 * We also incremented number of count semaphore of the semaphore set.
	 */
	if (0 == udi->shmid || -1 == shmctl(udi->shmid, IPC_STAT, &shm_buf) ||
			tsd->shm_ctime.ctime != shm_buf.shm_ctime)
	{
		/* The shared memory segment did not exist, nothing to 'rundown' */
		if (rc_cpt_removed)
		{       /* reset RC values if we've rundown the RC CPT */
			/* attempt to force-write header */
			tsd->rc_srv_cnt = tsd->dsid = tsd->rc_node = 0;
			tsd->owner_node = 0;
		} else
		{
			tsd->shmid = udi->shmid = 0;
			tsd->shm_ctime.ctime = udi->shm_ctime = 0;
			if (standalone)
			{
				if (!reg->read_only)
				{
					LSEEKWRITE(udi->fd, (off_t)0, tsd, sizeof(*tsd), status);
					if (0 != status)
					{
						RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
						CLNUP_RNDWN(udi, reg);
						return FALSE;
					}
				} else
				{
					db_ipcs.semid = tsd->semid;
					db_ipcs.sem_ctime = tsd->sem_ctime.ctime;
					db_ipcs.shmid = tsd->shmid;
					db_ipcs.shm_ctime = tsd->shm_ctime.ctime;
					get_full_path((char *)reg->dyn.addr->fname,  reg->dyn.addr->fname_len,
						db_ipcs.fn, &db_ipcs.fn_len, MAX_TRANS_NAME_LEN);
					db_ipcs.fn[db_ipcs.fn_len] = 0;
					if (0 != send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0))
					{
						RNDWN_ERR("!AD -> gtmsecshr was unable to write header to disk.", reg);
						CLNUP_RNDWN(udi, reg);
						return FALSE;
					}
				}
				close(udi->fd);
				if (!ftok_sem_release(reg, FALSE, TRUE))
					return FALSE;
				standalone_reg = gv_cur_region;
				return TRUE;
			} else
			{
				/* Not standalone */
				memset(tsd->machine_name, 0, MAX_MCNAMELEN);
				tsd->freeze = 0;
				tsd->owner_node = 0;
				tsd->shmid = 0;
				tsd->semid = 0;
				tsd->sem_ctime.ctime = 0;
				tsd->shm_ctime.ctime = 0;
			}
		}
		LSEEKWRITE(udi->fd, (off_t)0, tsd, sizeof(*tsd), status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Unable to write header to disk.", reg);
			CLNUP_RNDWN(udi, reg);
			return FALSE;
		}
		close(udi->fd);
		/*
		 * if (standalone) we have grabbed lock on semid (database access control semaphore).
		 * So now release ftok_semid lock, but do not decrement counter semaphore.
		 * ftok_semid will be still on the system. Specific utilities or, exit handler
		 * must take care of removing it after decrementing counter if possible.
		 */
		if (!ftok_sem_release(reg, !standalone, TRUE))
			return FALSE;
		if (standalone)
			standalone_reg = gv_cur_region;
		return TRUE;
	}
	/*
	 * shared memory is already open, so now find the number of users attached to it
	 * if it is zero, it needs to be flushed to disk
	 */
	/* If we aren't alone in using it, we can do nothing */
	if (0 != shm_buf.shm_nattch)
	{
		util_out_print("!AD -> File is in use by another process.", TRUE, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		return FALSE;
	}
	if (reg->read_only)             /* read only process can't succeed beyond this point */
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
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
		return FALSE;
       	}
	reg->dyn.addr->acc_meth = acc_meth = tsd->acc_meth;
	dbsecspc(reg, tsd);
	if (reg->sec_size != shm_buf.shm_segsz)
	{
		util_out_print("Expected shared memory size is !SL, but found !SL",
			TRUE, reg->sec_size, shm_buf.shm_segsz);
		util_out_print("!AD -> Existing shared memory size do not match.", TRUE, DB_LEN_STR(reg));
		CLNUP_RNDWN(udi, reg);
		return FALSE;
	}
	/*
	 * save gv_cur_region, cs_data, cs_addrs, and restore them on return
	 */
	temp_region = gv_cur_region;
	temp_cs_data = cs_data;
	temp_cs_addrs = cs_addrs;
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
		RESET_GV_CUR_REGION;
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
		RESET_GV_CUR_REGION;
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
		send_msg(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, errno);
		gtm_putmsg(VARLSTCNT(7) ERR_DBNAMEMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid, errno);
		CLNUP_RNDWN(udi, reg);
		RESET_GV_CUR_REGION;
		return FALSE;
	}
	/* Check whether csa->nl->fname and csa->nl->dbfid are in sync. If not its a serious condition. Error out. */
	set_gdid_from_stat(&tmp_dbfid, &stat_buf);
	if (FALSE == is_gdid_gdid_identical(&tmp_dbfid, &cs_addrs->nl->dbfid.u))
	{
		send_msg(VARLSTCNT(6) ERR_DBIDMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid);
		gtm_putmsg(VARLSTCNT(6) ERR_DBIDMISMATCH, 4, cs_addrs->nl->fname, DB_LEN_STR(reg), udi->shmid);
		CLNUP_RNDWN(udi, reg);
		RESET_GV_CUR_REGION;
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
	if (JNL_ALLOWED(tsd))
	{
		cs_addrs->jnl = (jnl_private_control *)malloc(sizeof(*cs_addrs->jnl));
		memset(cs_addrs->jnl, 0, sizeof(*cs_addrs->jnl));
		cs_addrs->jnl->channel = NOJNL;
		cs_addrs->jnl->region = reg;
		cs_addrs->jnl->jnl_buff = (jnl_buffer_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SPACE + JNL_NAME_EXP_SIZE);
	}
	cs_addrs->backup_buffer = (backup_buff_ptr_t)(cs_addrs->db_addrs[0] + NODE_LOCAL_SPACE + JNL_SHARE_SIZE(tsd));
	cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)cs_addrs->backup_buffer + BACKUP_BUFFER_SIZE + 1;
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
			RESET_GV_CUR_REGION;
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
		RESET_GV_CUR_REGION;
		return FALSE;
	}
	if (dba_bg == acc_meth)
		db_csh_ini(cs_addrs);
	else
		cs_addrs->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_ulong_t)csd + (int)(csd->start_vbn - 1) * DISK_BLOCK_SIZE);
	db_common_init(reg, cs_addrs, csd);	/* do initialization common to db_init() and mu_rndwn_file() */

	/* cleanup mutex stuff and grab crit -- jnl_file_close() may need */
	cs_addrs->hdr->image_count = 0;
	gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE); /* it is ensured, this is the only process running */
	cs_addrs->nl->in_crit = 0;
	cs_addrs->now_crit = cs_addrs->read_lock = FALSE;

	reg->open = TRUE;
	grab_crit(reg);
	if (rc_cpt_removed)
		csd->rc_srv_cnt = csd->dsid = csd->rc_node = 0;   /* reset RC values if we've rundown the RC CPT */
	if (cs_addrs->nl->glob_sec_init)
	{
		if JNL_ENABLED(csd)
			jnl_status = jnl_ensure_open();
		/* we are anyway running down the file, so we expect a cleanly terminated journal file. Hence only WCSFLU_NONE */
		wcs_flu(WCSFLU_NONE);
		if (JNL_ENABLED(csd))
		{
			if(0 == jnl_status)
				jnl_file_close(reg, TRUE, FALSE);
			else
				send_msg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
		}
	}
	if (JNL_ALLOWED(csd))
	{	/* If we own it or owner died, clear the lock */
		if (process_id == cs_addrs->jnl->jnl_buff->fsync_in_prog_latch.latch_pid)
		{
			RELEASE_SWAPLOCK(&cs_addrs->jnl->jnl_buff->fsync_in_prog_latch);
		} else
			performCASLatchCheck(&cs_addrs->jnl->jnl_buff->fsync_in_prog_latch);
		free(cs_addrs->jnl);
	}
	rel_crit(reg);
	reg->open = FALSE;
	/* Note: At this point we have write permission */
	memset(csd->machine_name, 0, MAX_MCNAMELEN);
	csd->owner_node = 0;
	csd->freeze = 0;
	csd->shmid = 0;
	csd->shm_ctime.ctime = 0;
	if (!standalone)
	{
		csd->semid = 0;
		csd->sem_ctime.ctime = 0;
	}
	if (dba_bg == acc_meth)
	{
		LSEEKWRITE(udi->fd, (off_t)0, csd, sizeof(*csd), status);
		if (0 != status)
		{
			RNDWN_ERR("!AD -> Error writing header to disk.", reg);
			CLNUP_RNDWN(udi, reg);
			RESET_GV_CUR_REGION;
			return FALSE;
		}
	} else
	{
		if (-1 == msync((caddr_t)cs_addrs->db_addrs[0], (size_t)stat_buf.st_size, MS_SYNC))
		{
			RNDWN_ERR("!AD -> Error synchronizing mapped memory.", reg);
			CLNUP_RNDWN(udi, reg);
			RESET_GV_CUR_REGION;
    			return FALSE;
		}
		if (-1 == munmap((caddr_t)cs_addrs->db_addrs[0], (size_t)stat_buf.st_size))
		{
			RNDWN_ERR("!AD -> Error unmapping mapped memory.", reg);
			CLNUP_RNDWN(udi, reg);
			RESET_GV_CUR_REGION;
			return FALSE;
		}
	}
	if (-1 == shmdt((caddr_t)cs_addrs->db_addrs[0]))
	{
		RNDWN_ERR("!AD -> Error detaching from shared memory.", reg);
		CLNUP_RNDWN(udi, reg);
		RESET_GV_CUR_REGION;
		return FALSE;
	}
	if (0 != shm_rmid(udi->shmid))
	{
		gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("releasing shared memory"));
		return FALSE;
	}
	if (!standalone)
	{
		if (0 != sem_rmid(udi->semid))
		{
			gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("mu_rndwn_file sem_rmid"));
			return FALSE;
		}
	}
	/*
	 * if standalone, we have grabbed lock on database access control semaphore.
	 * So now release ftok_semid lock so other ftok conflicted database can continue.
	 * Also for standalone do not decrement counter semaphore.
	 * Since ftok semaphore set will be needed again during exit, just release the lock.
	 * So ftok semaphore will be still there with non-zero counter.
	 * Exit handlers must take care of removing if necessary.
	 */
	if (!ftok_sem_release(reg, !standalone, !standalone))
		return FALSE;
	if (standalone)
		standalone_reg = gv_cur_region;
	udi->shmid = 0;
	udi->shm_ctime = 0;
	RESET_GV_CUR_REGION;
	close(udi->fd);
	return TRUE;
}
