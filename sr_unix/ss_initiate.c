/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "error.h"
#include "cli.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "repl_sp.h"
#include "gtm_file_stat.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "gt_timer.h"
#include "is_proc_alive.h"
#include "wcs_flu.h"
#include "jnl.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "mu_file_size.h"
#include "trans_log_name.h"
#include "mupint.h"
#include "memcoherency.h"
#include "gtm_logicals.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "shmpool.h"
#include "db_snapshot.h"
#include "gtm_c_stack_trace.h"

GBLREF	uint4				process_id;
GBLREF	uint4				mu_int_errknt;
GBLREF	boolean_t			ointeg_this_reg;
GBLREF  boolean_t			online_specified;

static int init_shm_and_sparse_file(int, uint4 *, uint4 *, int *, long *, void **, gd_region*, char *, int);

#define SNAPSHOT_TMP_PREFIX	"gtm_snapshot_"
#define ISSUE_WRITE_ERROR_AND_EXIT(reg, RC, csa, tempfilename)								\
{															\
	error_def(ERR_SSFILOPERR);											\
															\
	gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("write"), LEN_AND_STR(tempfilename), RC);		\
	if (csa->now_crit)												\
		rel_crit(csa->region);											\
	UNFREEZE_REGION_IF_NEEDED(csa->hdr, reg);										\
	return FALSE;													\
}

#define TOT_BYTES_REQUIRED(BLKS)	DIVIDE_ROUND_UP(BLKS, 8) /* One byte can represent 8 blocks' before image state */
#define EOF_MARKER_SIZE			DISK_BLOCK_SIZE
#define MAX_TRY_FOR_TOT_BLKS		CDB_STAGNATE

#define FILL_SS_FILHDR_INFO(ss_shm_ptr, ss_filhdr_ptr)						\
{												\
	MEMCPY_LIT(ss_filhdr_ptr->label, SNAPSHOT_HDR_LABEL);					\
	ss_filhdr_ptr->ss_info.ss_pid = ss_shm_ptr->ss_info.ss_pid;				\
	ss_filhdr_ptr->ss_info.snapshot_tn = ss_shm_ptr->ss_info.snapshot_tn;			\
	ss_filhdr_ptr->ss_info.db_blk_size = ss_shm_ptr->ss_info.db_blk_size;			\
	ss_filhdr_ptr->ss_info.free_blks = ss_shm_ptr->ss_info.free_blks;			\
	ss_filhdr_ptr->ss_info.total_blks = ss_shm_ptr->ss_info.total_blks;			\
	STRCPY(ss_filhdr_ptr->ss_info.shadow_file, ss_shm_ptr->ss_info.shadow_file);		\
	ss_filhdr_ptr->shadow_file_len = STRLEN((ss_shm_ptr->ss_info.shadow_file));		\
	ss_filhdr_ptr->ss_info.shadow_vbn = ss_shm_ptr->ss_info.shadow_vbn;			\
	ss_filhdr_ptr->ss_info.ss_shmsize = ss_shm_ptr->ss_info.ss_shmsize;			\
}

#define GET_CRIT_AND_DECR_INHIBIT_KILLS(reg, csa, cnl)			\
{									\
	if (!csa->now_crit)						\
		grab_crit(reg);						\
	DECR_INHIBIT_KILLS(cnl);					\
	rel_crit(reg);							\
}

#define SS_INIT_SHM(TOT_BLKS, SS_SHMSIZE, SS_SHMADDR, SS_SHMID, RC)					\
{													\
	long	lcl_shmid = INVALID_SHMID;								\
	int	lcl_status;										\
													\
	SS_SHMSIZE = (int)(ROUND_UP((SNAPSHOT_HDR_SIZE + TOT_BYTES_REQUIRED(TOT_BLKS)), OS_PAGE_SIZE));	\
	SS_SHMADDR = ss_attach_shmseg(SS_SHMSIZE, &lcl_shmid, &lcl_status, TRUE);			\
	RC = lcl_status;										\
	SS_SHMID = lcl_shmid;										\
	if (0 == lcl_status)										\
		memset(SS_SHMADDR, 0, SS_SHMSIZE);							\
}

/* In case of an error, un-freeze the region before returning if we had done the region_freeze
 * in mu_int_reg for a read_only process
 */
#define UNFREEZE_REGION_IF_NEEDED(CSD, REG)			\
{								\
	if (process_id == CSD->freeze)				\
	{							\
		assert(reg->read_only);				\
		region_freeze(reg, FALSE, FALSE, FALSE);	\
	}							\
}

boolean_t	ss_initiate(gd_region *reg, 			/* Region in which snapshot has to be started */
			    util_snapshot_ptr_t	util_ss_ptr, 	/* Utility specific snapshot structure */
			    snapshot_context_ptr_t *ss_ctx, 	/* Snapshot context */
			    boolean_t preserve_snapshot, 	/* Should the snapshots be preserved ? */
			    char *calling_utility)		/* Who called ss_initiate */
{
	int			shdw_fd, tmpfd, fclose_res, fstat_res, status, perm, group_id, pwrite_res, dsk_addr = 0;
	int			retries, idx, this_snapshot_idx, save_errno, ss_shmsize, ss_shm_vbn;
	long			ss_shmid = INVALID_SHMID;
	char			tempnamprefix[MAX_FN_LEN + 1], tempdir_full_buffer[GTM_PATH_MAX];
	char			tempdir_trans_buffer[GTM_PATH_MAX];
	char			*tempfilename;
	struct stat		stat_buf;
	enum db_acc_method	acc_meth;
	void			*ss_shmaddr;
	gtm_int64_t		db_file_size;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	uint4			tempnamprefix_len, crit_counter, tot_blks;
	uint4			rem_blks, native_size, bytes2write, fstat_status;
	mstr			tempdir_log, tempdir_full, tempdir_trans;
	boolean_t		debug_mupip = FALSE, wait_for_zero_kip;
	shm_snapshot_ptr_t	ss_shm_ptr;
	snapshot_context_ptr_t	lcl_ss_ctx, ss_orphan_ctx;
	snapshot_filhdr_ptr_t	ss_filhdr_ptr;
	char			*time_ptr, time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	now_t			now;
	boolean_t		was_crit;
	ZOS_ONLY(int		realfiletag;)
	uint4			*kip_pids_arr_ptr;

	error_def(ERR_BUFFLUFAILED);
	error_def(ERR_FILEPARSE);
	error_def(ERR_MAXSSREACHED);
	error_def(ERR_SSTMPDIRSTAT);
	error_def(ERR_SSTMPFILOPEN);
	error_def(ERR_SYSCALL);
	error_def(ERR_SSTMPCREATE);
	error_def(ERR_SSV4NOALLOW);
	ZOS_ONLY(error_def(ERR_BADTAG);)
	ZOS_ONLY(error_def(ERR_TEXT);)

	assert(NULL != calling_utility);
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	acc_meth = csd->acc_meth;
	debug_mupip = (CLI_PRESENT == cli_present("DBG"));

	/* Create a context containing default information pertinent to this initiate invocation */
	lcl_ss_ctx = malloc(SIZEOF(snapshot_context_t)); /* should be free'd by ss_release */
	DEFAULT_INIT_SS_CTX(lcl_ss_ctx);
	/* Snapshot context created. Any error below before the next cur_state assignment and a later call to ss_release will
	 * have to free the malloc'ed snapshot context instance
	 */
	lcl_ss_ctx->cur_state = BEFORE_SHADOW_FIL_CREAT;
	*ss_ctx = lcl_ss_ctx;
	was_crit = csa->now_crit;
	if (!was_crit)
		grab_crit(reg);
	ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(csa);
	if (MAX_SNAPSHOTS == cnl->num_snapshots_in_effect)
	{
		/* SS_MULTI: If multiple snapshots are supported, then we should run through each of the "possibly" running
		 * snapshots
		 */
		assert(1 == MAX_SNAPSHOTS);
		/* Check if the existing snapshot is still alive. If not, go ahead and cleanup that for us to continue  */
		if ((0 != ss_shm_ptr->ss_info.ss_pid) && !is_proc_alive(ss_shm_ptr->ss_info.ss_pid, 0))
			ss_release(NULL);
		else
		{
			gtm_putmsg(VARLSTCNT(5) ERR_MAXSSREACHED, 3, MAX_SNAPSHOTS, REG_LEN_STR(reg));
			if (!was_crit)
				rel_crit(reg);
			UNFREEZE_REGION_IF_NEEDED(csd, reg);
			return FALSE;
		}
	}
	assert(csa->now_crit);
	ss_shm_ptr->ss_info.ss_pid = process_id;
	cnl->num_snapshots_in_effect++;
	if (!was_crit)
		rel_crit(reg);

	/* For a readonly database for the current process, we better have the region frozen */
	assert(!reg->read_only || csd->freeze);
	/* ============================ STEP 1 : Shadow file name construction ==============================
	 *
	 * --> use $gtm_tmp (denoted by SNAPSHOT_TMP_DIR) if available, else use current directory
	 * --> use the template - gtm_snapshot_<region_name>_<pid_in_hex>_XXXXXX
	 * --> the last six characters will be replaced by MKSTEMP below
	 * Note: MUPIP RUNDOWN will rely on the above template to do the cleanup if there were a
	 * crash that led to the improper shutdown of the snapshot process. So, if the above template
	 * changes make sure it's taken care of in MUPIP RUNDOWN
	 * Note: most of the shadow file name construction and the creation of shadow file is been
	 * borrowed from mupip_backup.c. This code should be modularized and should be used in both mupip_backup
	 * as well as here.
	 */

	/* set up a prefix for the temporary file. */
	tempnamprefix_len = 0;
	memset(tempnamprefix, 0, MAX_FN_LEN);
	memcpy(tempnamprefix, SNAPSHOT_TMP_PREFIX, STR_LIT_LEN(SNAPSHOT_TMP_PREFIX));
	tempnamprefix_len += STR_LIT_LEN(SNAPSHOT_TMP_PREFIX);
	memcpy(tempnamprefix + tempnamprefix_len, reg->rname, reg->rname_len);
	tempnamprefix_len += reg->rname_len;
	SNPRINTF(&tempnamprefix[tempnamprefix_len], MAX_FN_LEN, "_%x", process_id);

	tempdir_log.addr = SNAPSHOT_TMP_DIR;
	tempdir_log.len = STR_LIT_LEN(SNAPSHOT_TMP_DIR);
	tempfilename = tempdir_full.addr = tempdir_full_buffer;
	/* check if the SNAPSHOT_TMP_DIR environment variable is defined (actually $gtm_tmp)
	 * side-effect: tempdir_trans.addr = tempdir_trans_buffer irrespective of whether TRANS_LOG_NAME
	 * succeeded or not.
	 */
	status = TRANS_LOG_NAME(&tempdir_log,
				&tempdir_trans,
				tempdir_trans_buffer,
				SIZEOF(tempdir_trans_buffer),
				do_sendmsg_on_log2long);

	if (SS_NORMAL == status && (NULL != tempdir_trans.addr) && (0 != tempdir_trans.len))
		*(tempdir_trans.addr + tempdir_trans.len) = 0;
	else
	{
		tempdir_trans_buffer[0] = '.';
		tempdir_trans_buffer[1] = '\0';
		tempdir_trans.len = 1;
	}

	/* Verify if we can stat the temporary directory */
	if (FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempdir_trans, NULL, &tempdir_full, FALSE, &fstat_status)))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_SSTMPDIRSTAT, 2, tempdir_trans.len, tempdir_trans.addr, fstat_status);
		UNFREEZE_REGION_IF_NEEDED(csd, reg);
		return FALSE;
	}
	SNPRINTF(tempfilename + tempdir_full.len, GTM_PATH_MAX, "/%s_XXXXXX", tempnamprefix);

	/* ========================== STEP 2 : Create the shadow file ======================== */
	/* get a unique temporary file name. The file gets created on success */
	MKSTEMP(tempfilename, tmpfd);
	if (FD_INVALID == tmpfd)
	{
		status = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_SSTMPCREATE, 2, tempdir_trans.len, tempdir_trans.addr, status);
		UNFREEZE_REGION_IF_NEEDED(csd, reg);
		return FALSE;
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_set_tag(tmpfd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(tempfilename, realfiletag, TAG_BINARY, errno);
#	endif
	/* Shadow file created. Any error below before the next cur_state assignment and a later call to ss_release will have
	 * to delete at least the shadow file apart from free'ing the malloc'ed snapshot context instance
	 */
	lcl_ss_ctx->cur_state = AFTER_SHADOW_FIL_CREAT;
	/* Temporary file for backup was created above using "mkstemp" which on AIX opens the file without
	 * large file support enabled. Work around that by closing the file descriptor returned and reopening
	 * the file with the "open" system call (which gets correctly translated to "open64"). We need to do
	 * this because the temporary file can get > 2GB. Since it is not clear if mkstemp on other Unix platforms
	 * will open the file for large file support, we use this solution for other Unix flavours as well.
	 */
	OPENFILE(tempfilename, O_RDWR, shdw_fd);
	if (FD_INVALID == shdw_fd)
	{
		status = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_SSTMPFILOPEN, 2, tempdir_full.len, tempdir_full.addr, status);
		UNFREEZE_REGION_IF_NEEDED(csd, reg);
		return FALSE;
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_set_tag(shdw_fd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(tempfilename, realfiletag, TAG_BINARY, errno);
#	endif
	/* Now that the temporary file has been opened successfully, close the fd returned by mkstemp */
	F_CLOSE(tmpfd, fclose_res);
	tempdir_full.len = STRLEN(tempdir_full.addr); /* update the length */
	assert(GTM_PATH_MAX >= tempdir_full.len);
	STRCPY(lcl_ss_ctx->shadow_file, tempfilename);

	/* give temporary files the group and permissions as other shared resources - like journal files */
	FSTAT_FILE(((unix_db_info *)(reg->dyn.addr->file_cntl->file_info))->fd, &stat_buf, fstat_res);
	assert(-1 != fstat_res);
	if (-1 != fstat_res)
		gtm_set_group_and_perm(&stat_buf, &group_id, &perm, (0770 & stat_buf.st_mode));

	if ((-1 == fstat_res) || (-1 == FCHMOD(shdw_fd, perm))
		|| ((-1 != group_id) && (-1 == fchown(shdw_fd, -1, group_id))))
	{
		status = errno;
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fchmod/fchown"), CALLFROM, status);
		UNFREEZE_REGION_IF_NEEDED(csd, reg);
		return FALSE;
	}
	if (debug_mupip)
	{
		util_out_print("!/MUPIP INFO: Successfully created the shadow file: !AD",
				TRUE,
				tempdir_full.len,
				tempdir_full.addr);
	}

	/* STEP 3: Wait for kills-in-progress and initialize snapshot shadow file
	 *
	 * Snapshot Shadow File Layout -
	 *
	 *  -----------------------
	 * |   RESERVE_SHM_SPACE   | <-- Variable sized space reserved in case we would like the snapshots to be preserved
	 *  -----------------------
	 * |     DB_FILE_HEADER    | <-- Copy of the database file header taken at the snapshot time (in crit) (SGMNT_HDR_LEN)
	 *  -----------------------
	 * |      DB_MASTER_MAP    | <-- MASTER_MAP_SIZE_MAX
	 *  -----------------------  <-- offset of block zero
	 * |	BLOCK_ZERO_BIMG    | <-- csd->blk_size
	 *  -----------------------
	 * |     BLOCK_ONE_BIMG    | <-- csd->blk_size
	 *  -----------------------
	 * .
	 * .
	 *  -----------------------
	 * |      EOF_MARKER       | <-- 512 bytes (DISK_BLOCK_SIZE)
	 *  -----------------------
	 */
	/* Below, master map and the file header will be copied and hence check if we have the right
	 * pointers setup by the caller
	 */
	assert(NULL != util_ss_ptr);
	assert(NULL != util_ss_ptr->master_map);
	assert(NULL != util_ss_ptr->header);
	grab_crit(reg);
	INCR_INHIBIT_KILLS(cnl);
	kip_pids_arr_ptr = cnl->kip_pid_array;
	for (retries = -1; MAX_TRY_FOR_TOT_BLKS > retries; ++retries)
	{
		/* On all but the 5th retry (inclusive of retries = -1), release crit */
		if (MAX_TRY_FOR_TOT_BLKS != retries)
			rel_crit(reg);
		/*
		 * Calculations outside crit (on the final retry, we will be holding crit while calculating the below):
		 * 1. total blocks
		 * 2. native size (total number of blocks in terms of DISK_BLOCK_SIZE
		 * 3. write 0x00 at an offset equal to the current database file size
		 * 4. create shared memory segment of size just enough to represent the before image state of total
		 * blocks in the database
		 */
		if (!init_shm_and_sparse_file(shdw_fd, &tot_blks, &native_size, &ss_shmsize, &ss_shmid, &ss_shmaddr, reg,
			tempfilename, retries))
		{
			GET_CRIT_AND_DECR_INHIBIT_KILLS(reg, csa, cnl);
			UNFREEZE_REGION_IF_NEEDED(csd, reg);
			return FALSE;
		}
		wait_for_zero_kip = csd->kill_in_prog;
		/* Wait for existing kills-in-progress to be reset. Since a database file extension is possible as
		 * we don't hold crit while wcs_sleep below, we will retry if the total blocks have changed since
		 * we last initialized the shared memory. However, in the final retry, the shared memory and shadow
		 * file initialization will happen in crit. Also, in the final retry, we won't be waiting for the
		 * kills in progress as we would be holding crit and cannot afford to wcs_sleep. So, in such a case,
		 * MUKILLIP should be issued by the caller of ss_initiate if csd->kill_in_prog is set to TRUE. But,
		 * such a case should be rare.*/
		for (crit_counter = 1; wait_for_zero_kip && (MAX_TRY_FOR_TOT_BLKS != retries); )
		{
			/* Release crit before going into the wait loop */
			if (csa->now_crit)
				rel_crit(reg);
			if (debug_mupip)
			{
				GET_CUR_TIME;
				util_out_print("!/MUPIP INFO: !AD : Start kill-in-prog wait for database !AD", TRUE,
					CTIME_BEFORE_NL, time_ptr, DB_LEN_STR(reg));
			}
			while (csd->kill_in_prog && (MAX_CRIT_TRY > crit_counter++))
			{
				GET_C_STACK_FOR_KIP(kip_pids_arr_ptr, crit_counter, MAX_CRIT_TRY, 1, MAX_KIP_PID_SLOTS);
				wcs_sleep(crit_counter);
			}
			if (debug_mupip)
			{
				GET_CUR_TIME;
				util_out_print("!/MUPIP INFO: !AD : Done with kill-in-prog wait on !AD", TRUE,
					CTIME_BEFORE_NL, time_ptr, DB_LEN_STR(reg));
			}
			wait_for_zero_kip = (MAX_CRIT_TRY > crit_counter); /* if TRUE, we can wait for some more time on
									    * this region */
			grab_crit(reg);
			if (csd->kill_in_prog)
			{
				/* It is possible for this to happen in case a concurrent GT.M process is in its 4th retry.
				 * In that case, it will not honor the inhibit_kills flag since it holds crit and therefore
				 * could have set kill-in-prog to a non-zero value while we were outside of crit.
				 * Since we have waited for 1 minute already, proceed with snapshot. The reasoning is that
				 * once the GT.M process that is in the final retry finishes off the second part of the M-kill,
				 * it will not start a new transaction in the first try which is outside of crit so will honor
				 * the inhibit-kills flag and therefore not increment the kill_in_prog counter any more until
				 * this crit is released.  So we could be waiting for at most 1 kip increment per concurrent process
				 * that is updating the database. We expect these kills to be complete within 1 minute.
				 */
				if (!wait_for_zero_kip)
					break;
			} else
				break;
		}
		if (!csa->now_crit)
			grab_crit(reg);
		/* After we have written the bitmap and before we grab crit, another process can add new blocks to the database
		 * in which case csd->trans_hist.total_blks is no longer the value as we grabbed. To work around this, note down
		 * the total_blks inside the crit once again, release the crit, update the bitmap and grab crit once more to see
		 * if the total blks changed. Repeat this process until the total_blks inside and outside crit are the same or we
		 * exhaust our retires and do the whole thing in crit. Note that change in total blocks can result in the existing
		 * shared memory not enough. In such a case, detach the existing shared memory and create new shared memory so
		 * that it can hold the newly calculated total blocks. It is also possible that csd->kill_in_prog was FALSE before
		 * we grab crit above and hence we decided not to do wcs_sleep but before we could grab crit, csd->kill_in_prog was
		 * set to TRUE by a parallel GT.M process. In such a case, we would continue again and wait for the
		 * kill-in-progress to reset to FALSE.
		 */
		if ((tot_blks == csd->trans_hist.total_blks) && !csd->kill_in_prog)
		{
			assert(native_size == ((tot_blks * (csd->blk_size / DISK_BLOCK_SIZE)) + (csd->start_vbn)));
			break;
		}
	}
	/* At this point, we are MOST likely guaranteed that kill-in-prog is set to zero for this region and CERTAINLY
	 * guaranteed that no more kills will be started for this region. Now, we are still holding crit, so any error
	 * that occurs henceforth should do a DECR_INHIBIT_KILLS and rel_crit before exiting to let the other processes
	 * proceed gracefully
	 */
	assert(csa->now_crit);
	assert(native_size == ((tot_blks * (csd->blk_size / DISK_BLOCK_SIZE)) + (csd->start_vbn)));
	assert(NULL != ss_shmaddr);
	assert(0 == ((long)ss_shmaddr % OS_PAGE_SIZE));
	assert(0 == ss_shmsize % OS_PAGE_SIZE);
	assert(INVALID_SHMID != ss_shmid);
	assert(0 == (ss_shmsize % DISK_BLOCK_SIZE));
	ss_shm_vbn = ss_shmsize / DISK_BLOCK_SIZE; /* # of DISK_BLOCK_SIZEs the shared memory spans across */
	/* At this point, we are done with allocating shared memory (aligned with OS_PAGE_SIZE) enough to hold
	 * the total number of blocks. Any error below before the next cur_state assignment and a later call to ss_release will have
	 * to detach/rmid the shared memory identifier apart from deleting the shadow file and free'ing the malloc'ed snapshot
	 * context instance
	 */
	lcl_ss_ctx->cur_state = AFTER_SHM_CREAT;
	/* It is possible that we did saw csd->full_upgraded TRUE before invoking ss_initiate but MUPIP SET -VER=V4 was done after
	 * ss_initiate was called. Handle appropriately.
	 */
	if (!csd->fully_upgraded)
	{
		/* If -ONLINE was specified explicitly, then it is an ERROR. Issue the error and return FALSE. */
		if (online_specified)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_SSV4NOALLOW, 2, DB_LEN_STR(reg));
			util_out_print(NO_ONLINE_ERR_MSG, TRUE);
			mu_int_errknt++;
			GET_CRIT_AND_DECR_INHIBIT_KILLS(reg, csa, cnl);
			UNFREEZE_REGION_IF_NEEDED(csd, reg);
			return FALSE;
		} else
		{	/* If -ONLINE was assumed implicitly (default of INTEG -REG), then set ointeg_this_reg to FALSE,
			 * relinquish the resources and continue as if it is -NOONLINE
			 */
			ointeg_this_reg = FALSE;
			assert(NULL != *ss_ctx);
			ss_release(ss_ctx);
			GET_CRIT_AND_DECR_INHIBIT_KILLS(reg, csa, cnl);
			UNFREEZE_REGION_IF_NEEDED(csd, reg);
			return TRUE;
		}
	}
	/* ===================== STEP 5: Flush the pending updates in the global cache =================== */

	/* For a readonly database for the current process, we cannot do wcs_flu. We would have waited for the active queues
	 * to complete in mu_int_reg after doing a freeze. Now that we have crit, unfreeze the region
	 */
	if (reg->read_only)
	{
		region_freeze(reg, FALSE, FALSE, FALSE);
	}
	else if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH)) /* wcs_flu guarantees that all the pending phase 2 commits
								   * are done with before returning */
	{
		assert(process_id != csd->freeze); /* We would not have frozen the region if the database is read-write */
		gtm_putmsg(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_STR(calling_utility), DB_LEN_STR(reg));
		GET_CRIT_AND_DECR_INHIBIT_KILLS(reg, csa, cnl);
		return FALSE;
	}

	assert(0 < cnl->num_snapshots_in_effect || (!SNAPSHOTS_IN_PROG(cnl)));
	assert(csa->now_crit);
	/* ========== STEP 6: Copy the file header, master map and the native file size into a private structure =========== */

	memcpy(util_ss_ptr->header, csd, SGMNT_HDR_LEN);
	memcpy(util_ss_ptr->master_map, MM_ADDR(csd), MASTER_MAP_SIZE_MAX);
	util_ss_ptr->native_size = native_size;

	/* == STEP 7: Populate snapshot context, database shared memory snapshot structure and snapshot file header structure == */
	ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(csa);
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	/* Fill in the information for this snapshot in database shared memory */
	ss_shm_ptr->ss_info.snapshot_tn = csd->trans_hist.curr_tn;
	ss_shm_ptr->ss_info.free_blks = csd->trans_hist.free_blocks;
	ss_shm_ptr->ss_info.total_blks = csd->trans_hist.total_blks;
	ss_shm_ptr->ss_info.db_blk_size = csd->blk_size;
	ss_shm_ptr->failure_errno = 0;
	ss_shm_ptr->failed_pid = 0;
	STRCPY(ss_shm_ptr->ss_info.shadow_file, tempfilename);
	ss_shm_ptr->ss_info.shadow_vbn = ss_shm_vbn + csd->start_vbn;
	ss_shm_ptr->ss_info.ss_shmid = cnl->ss_shmid = ss_shmid;
	ss_shm_ptr->ss_info.ss_shmsize = ss_shmsize;
	ss_shm_ptr->in_use = 1;
	/* Fill in the information for the process specific snapshot context information */
	lcl_ss_ctx->shdw_fd = shdw_fd;
	lcl_ss_ctx->ss_shmid = ss_shmid;
	lcl_ss_ctx->ss_shm_ptr = (shm_snapshot_ptr_t)(ss_shm_ptr);
	lcl_ss_ctx->start_shmaddr = (sm_uc_ptr_t)ss_shmaddr;
	lcl_ss_ctx->bitmap_addr = ((sm_uc_ptr_t)ss_shmaddr + SNAPSHOT_HDR_SIZE);
	lcl_ss_ctx->shadow_vbn = ss_shm_ptr->ss_info.shadow_vbn;
	lcl_ss_ctx->last_ss_read_progress = ss_shm_ptr->ss_read_progress = 0;
	lcl_ss_ctx->total_blks = ss_shm_ptr->ss_info.total_blks;
	/* Fill snapshot file header information. This data will be persisted if -PRESERVE option is given */
	ss_filhdr_ptr = (snapshot_filhdr_ptr_t)(ss_shmaddr);
	FILL_SS_FILHDR_INFO(ss_shm_ptr, ss_filhdr_ptr)
	ss_shm_ptr->preserve_snapshot = preserve_snapshot;
	SET_LATCH_GLOBAL(&ss_shm_ptr->bitmap_latch, LOCK_AVAILABLE);
	DECR_INHIBIT_KILLS(cnl);
	/* announce GT.M that it's now ok to write the before images */
	if (!SNAPSHOTS_IN_PROG(cnl))
	{
		cnl->snapshot_in_prog = TRUE;
	}
	csa->snapshot_in_prog = TRUE;
	/* Having a write memory barrier here ensures that whenever GT.M reads a newer value of cnl->ss_shmcycle, it is guaranteed
	 * that the remaining fields that it reads from the shared memory will be the latest ones.
	 */
	SHM_WRITE_MEMORY_BARRIER;
	cnl->ss_shmcycle++;	/* indicate that the ss_shmid field of cnl is being reused */
	lcl_ss_ctx->ss_shmcycle = cnl->ss_shmcycle;
	rel_crit(reg);
	/* ============= STEP 8: Write the database file header and the master map =============
	 * Write the database file header at an offset equal to the database shared memory size.
	 * This is because if we want to preserve the snapshot then we would want to write the
	 * shared memory information at the beginning of the file. */
	LSEEKWRITE(shdw_fd, (off_t)ss_shmsize, (sm_uc_ptr_t)util_ss_ptr->header, SGMNT_HDR_LEN, pwrite_res);
	if (0 != pwrite_res)
		ISSUE_WRITE_ERROR_AND_EXIT(reg, pwrite_res, csa, tempfilename);
	dsk_addr += ((int)ss_shmsize + (int)SGMNT_HDR_LEN);
	/* write the database master map to the shadow file */
	LSEEKWRITE(shdw_fd, dsk_addr, (sm_uc_ptr_t)util_ss_ptr->master_map, MASTER_MAP_SIZE_MAX, pwrite_res);
	if (0 != pwrite_res)
		ISSUE_WRITE_ERROR_AND_EXIT(reg, pwrite_res, csa, tempfilename);
	dsk_addr += MASTER_MAP_SIZE_MAX;
	assert(0 == (dsk_addr % OS_PAGE_SIZE));
	lcl_ss_ctx->cur_state = SNAPSHOT_INIT_DONE; /* Same as AFTER_SHM_CREAT but set for clarity of the snapshot state */
	return TRUE;
}

static int init_shm_and_sparse_file(int	shdw_fd,
				    uint4 *tot_blks,
				    uint4 *native_size,
	   			    int *ss_shmsize,
				    long *ss_shmid,
				    void **ss_shmaddr,
				    gd_region *reg,
				    char *tempfilename,	/* For error reporting */
				    int retries)
{
	static boolean_t	first_time = TRUE;
	long			lcl_shmid = INVALID_SHMID;
	char			eof_marker[EOF_MARKER_SIZE];
	int			status;
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	gtm_int64_t		db_file_size;

	if (first_time)
	{
		memset(eof_marker, 0, EOF_MARKER_SIZE);
		first_time = FALSE;
	}
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	*tot_blks = csd->trans_hist.total_blks;
	*native_size = mu_file_size(reg->dyn.addr->file_cntl);
	db_file_size = (gtm_int64_t)(*native_size) * DISK_BLOCK_SIZE; /* Size of database file in bytes */
	if (-1 == retries) /* First time we called this function. So, initialize shared memory anyways. */
	{
		SS_INIT_SHM(*tot_blks, *ss_shmsize, *ss_shmaddr, *ss_shmid, status);
		if (0 != status)
			return FALSE;
	} else if ((*ss_shmsize - SNAPSHOT_HDR_SIZE) < TOT_BYTES_REQUIRED(*tot_blks))
	{ 	/* The existing shared memory size is not enough for the newly added blocks. So, detach and re-attach */
		if (0 != ss_detach_shmseg(*ss_shmaddr, *ss_shmid, TRUE, TRUE)) /* Error will be reported inside the function */
			return FALSE;
		SS_INIT_SHM(*tot_blks, *ss_shmsize, *ss_shmaddr, *ss_shmid, status);
		if (0 != status)
			return FALSE;
	}
	LSEEKWRITE(shdw_fd, ((off_t)db_file_size + *ss_shmsize), eof_marker, EOF_MARKER_SIZE, status);
	if (0 != status)
		ISSUE_WRITE_ERROR_AND_EXIT(reg, status, csa, tempfilename);
	return TRUE;
}
