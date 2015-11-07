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

#include <sys/shm.h>
#include <errno.h>
#include <sys/sem.h>

#include "gtm_ipc.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_sem.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtmio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "mutex.h"
#include "jnl.h"
#include "repl_sem.h"
#include "eintr_wrappers.h"
#include "mu_rndwn_file.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "gtm_logicals.h"
#include "min_max.h"
#include "util.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_all.h"
#include "dbfilop.h"
#include "ipcrmid.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "cliif.h"
#include "mu_rndwn_repl_instance.h"
#include "send_msg.h"
#include "do_shmat.h"	/* for do_shmat() prototype */
#include "shmpool.h" /* Needed for the shmpool structures */
#include "error.h"
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

#define PRINT_AND_SEND_SHMREMOVED_MSG(MSGBUFF, FNAME_LEN, FNAME, SHMID)							\
{															\
	gtm_putmsg(VARLSTCNT(9) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), ERR_SHMREMOVED, 3, SHMID, FNAME_LEN, FNAME);		\
	send_msg(VARLSTCNT(9) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), ERR_SHMREMOVED, 3, SHMID, FNAME_LEN, FNAME);		\
}

#define PRINT_AND_SEND_REPLPOOL_FAILURE_MSG(MSGBUFF, REPLPOOL_ID, SHMID)						\
{															\
	int		msgid;												\
	unsigned char	ipcs_buff[MAX_IPCS_ID_BUF], *ipcs_ptr;								\
															\
	ipcs_ptr = i2asc(ipcs_buff, SHMID);										\
	*ipcs_ptr = '\0';												\
	msgid = (JNLPOOL_SEGMENT == REPLPOOL_ID->pool_type) ? ERR_MUJPOOLRNDWNFL : ERR_MURPOOLRNDWNFL;			\
	gtm_putmsg(VARLSTCNT(10) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), msgid, 4, LEN_AND_STR(ipcs_buff),			\
			LEN_AND_STR(REPLPOOL_ID->instfilename));							\
	send_msg(VARLSTCNT(10) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), msgid, 4, LEN_AND_STR(ipcs_buff),			\
			LEN_AND_STR(REPLPOOL_ID->instfilename));							\
}

#define PRINT_AND_SEND_DBRNDWN_FAILURE_MSG(MSGBUFF, FNAME, SHMID)							\
{															\
	gtm_putmsg(VARLSTCNT(9) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), ERR_MUFILRNDWNFL2, 3, SHMID, LEN_AND_STR(FNAME));	\
	send_msg(VARLSTCNT(9) ERR_TEXT, 2, LEN_AND_STR(MSGBUFF), ERR_MUFILRNDWNFL2, 3, SHMID, LEN_AND_STR(FNAME));	\
}

GBLREF gd_region        *gv_cur_region;

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

error_def(ERR_DBFILERR);
error_def(ERR_MUFILRNDWNFL2);
error_def(ERR_MUFILRNDWNSUC);
error_def(ERR_MUJPOOLRNDWNFL);
error_def(ERR_MUJPOOLRNDWNSUC);
error_def(ERR_MUNOTALLSEC);
error_def(ERR_MURPOOLRNDWNFL);
error_def(ERR_MURPOOLRNDWNSUC);
error_def(ERR_SEMREMOVED);
error_def(ERR_SHMREMOVED);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

STATICFNDCL boolean_t validate_db_shm_entry(shm_parms *parm_buff, char *fname, int *tmp_exit_status);
STATICFNDCL boolean_t validate_replpool_shm_entry(shm_parms *parm_buff, replpool_id_ptr_t replpool_id, int *tmp_exit_status);
STATICFNDCL shm_parms *get_shm_parm(char *entry);
STATICFNDCL char *parse_shm_entry(char *entry, int which_field);
STATICFNDCL void mu_rndwn_all_helper(shm_parms *parm_buff, char *fname, int *exit_status, int *tmp_exit_status);

STATICDEF	boolean_t	mu_rndwn_all_helper_error = FALSE;

/* This condition handler is necessary so the argumentless "mupip rundown" does not terminate in case of an error
 * while processing one ipc. Instead it moves on to the next ipc. The only exception is fatal errors (SEVERE)
 * where it might not be safe to continue processing so we transfer control to a higher level condition handler.
 */
CONDITION_HANDLER(mu_rndwn_all_helper_ch)
{
	START_CH;
	mu_rndwn_all_helper_error = TRUE;
	PRN_ERROR;
	if (SEVERITY == SEVERE)
	{
		NEXTCH;
	} else
		UNWIND(NULL, NULL);
}

STATICFNDEF void mu_rndwn_all_helper(shm_parms *parm_buff, char *fname, int *exit_status, int *tmp_exit_status)
{
	replpool_identifier	replpool_id;
	boolean_t 		ret_status, jnlpool_sem_created;
	unsigned char		ipcs_buff[MAX_IPCS_ID_BUF], *ipcs_ptr;

	ESTABLISH(mu_rndwn_all_helper_ch);
	if (validate_db_shm_entry(parm_buff, fname, tmp_exit_status))
	{
		if (SS_NORMAL == *tmp_exit_status)
		{	/* shm still exists */
			mu_gv_cur_reg_init();
			gv_cur_region->dyn.addr->fname_len = strlen(fname);
			STRNCPY_STR(gv_cur_region->dyn.addr->fname, fname, gv_cur_region->dyn.addr->fname_len);
			if (mu_rndwn_file(gv_cur_region, FALSE))
				gtm_putmsg(VARLSTCNT(4) ERR_MUFILRNDWNSUC, 2, DB_LEN_STR(gv_cur_region));
			else
				*exit_status = ERR_MUNOTALLSEC;
			mu_gv_cur_reg_free();
		} else
		{	/* shm has been cleaned up by "validate_db_shm_entry" so no need of any more cleanup here */
			assert(ERR_SHMREMOVED == *tmp_exit_status);
			*tmp_exit_status = SS_NORMAL;	/* reset tmp_exit_status for below logic to treat this as normal */
		}
	} else if ((SS_NORMAL == *tmp_exit_status)
		&& validate_replpool_shm_entry(parm_buff, (replpool_id_ptr_t)&replpool_id, tmp_exit_status))
	{
		if (SS_NORMAL == *tmp_exit_status)
		{
			assert(JNLPOOL_SEGMENT == replpool_id.pool_type || RECVPOOL_SEGMENT == replpool_id.pool_type);
			ret_status = mu_rndwn_repl_instance(&replpool_id, TRUE, FALSE, &jnlpool_sem_created);
			ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, parm_buff->shmid);
			*ipcs_ptr = '\0';
			gtm_putmsg(VARLSTCNT(6) (JNLPOOL_SEGMENT == replpool_id.pool_type) ?
				(ret_status ? ERR_MUJPOOLRNDWNSUC : ERR_MUJPOOLRNDWNFL) :
				(ret_status ? ERR_MURPOOLRNDWNSUC : ERR_MURPOOLRNDWNFL),
				4, LEN_AND_STR(ipcs_buff), LEN_AND_STR(replpool_id.instfilename));
			if (!ret_status)
				*exit_status = ERR_MUNOTALLSEC;
		} else
		{	/* shm has been cleaned up by "validate_replpool_shm_entry" so no need of any more cleanup here */
			assert(ERR_SHMREMOVED == *tmp_exit_status);
			*tmp_exit_status = SS_NORMAL;	/* reset tmp_exit_status for below logic to treat this as normal */
		}
	}
	REVERT;
}

int mu_rndwn_all(void)
{
	int 			save_errno, fname_len, exit_status = SS_NORMAL, shmid, tmp_exit_status;
	char			entry[MAX_ENTRY_LEN];
	FILE			*pf;
	char			*fname, *fgets_res;
	shm_parms		*parm_buff;

	if (NULL == (pf = POPEN(IPCS_CMD_STR ,"r")))
        {
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("POPEN()"), CALLFROM, save_errno);
                return ERR_MUNOTALLSEC;
        }
	fname = (char *)malloc(MAX_FN_LEN + 1);
	while (NULL != (FGETS(entry, SIZEOF(entry), pf, fgets_res)) && entry[0] != '\n')
	{
		tmp_exit_status = SS_NORMAL;
		parm_buff = get_shm_parm(entry);
		if (NULL == parm_buff)
		{
			exit_status = ERR_MUNOTALLSEC;
			continue;
		}
		mu_rndwn_all_helper(parm_buff, fname, &exit_status, &tmp_exit_status);
		if ((SS_NORMAL == exit_status) && (SS_NORMAL != tmp_exit_status))
			exit_status = tmp_exit_status;
		if (mu_rndwn_all_helper_error)
		{	/* Encountered a runtime error while processing this ipc. Make sure we return with
			 * MUNOTALLSEC and reset this static variable before starting processing on next ipc.
			 */
			mu_rndwn_all_helper_error = FALSE;
			if (SS_NORMAL == exit_status)
				exit_status = ERR_MUNOTALLSEC;
		}
		if (NULL != parm_buff)
			free(parm_buff);
	}
	pclose(pf);
	free(fname);
	return exit_status;
}

/* Takes an entry from 'ipcs -m' and checks for its validity to be a GT.M db segment.
 * Returns TRUE if the shared memory segment is a valid GT.M db segment
 * (based on a check on some fields in the shared memory) else FALSE.
 * If the segment belongs to GT.M it returns the database file name by the second argument.
 * Sets exit_stat to ERR_MUNOTALLSEC if appropriate.
 */
boolean_t validate_db_shm_entry(shm_parms *parm_buff, char *fname, int *exit_stat)
{
	boolean_t		remove_shmid;
	file_control		*fc;
	int			fname_len, save_errno, status, shmid;
	node_local_ptr_t	nl_addr;
	sm_uc_ptr_t		start_addr;
	struct stat		st_buff;
	struct shmid_ds		shmstat;
	sgmnt_data		tsd;
	unix_db_info		*udi;
	char			msgbuff[OUT_BUFF_SIZE];

	if (NULL == parm_buff)
		return FALSE;
	/* check for the bare minimum size of the shared memory segment that we expect
	 * (with no fileheader related information at hand) */
	if (MIN_NODE_LOCAL_SPACE + SHMPOOL_SECTION_SIZE > parm_buff->sgmnt_siz)
		return FALSE;
	if (IPC_PRIVATE != parm_buff->key)
		return FALSE;
	shmid = parm_buff->shmid;
	/* we do not need to lock the shm for reading the rundown information as
	 * the other rundowns (if any) can also be allowed to share reading the
	 * same info concurrently.
	 */
	if (-1 == (sm_long_t)(start_addr = (sm_uc_ptr_t) do_shmat(shmid, 0, SHM_RND)))
		return FALSE;
	nl_addr = (node_local_ptr_t)start_addr;
	memcpy(fname, nl_addr->fname, MAX_FN_LEN + 1);
	fname[MAX_FN_LEN] = '\0';			/* make sure the fname is null terminated */
	fname_len = STRLEN(fname);
	msgbuff[0] = '\0';
	if (memcmp(nl_addr->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(nl_addr->label, GDS_LABEL, GDS_LABEL_SZ - 3))
		{
			util_out_print("Cannot rundown shmid = !UL for database !AD as it has format !AD "
				"but this mupip uses format !AD", TRUE, shmid,
				fname_len, fname, GDS_LABEL_SZ - 1, nl_addr->label, GDS_LABEL_SZ - 1, GDS_LABEL);
			*exit_stat = ERR_MUNOTALLSEC;
		}
		shmdt((void *)start_addr);
		return FALSE;
	}
	if (memcmp(nl_addr->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		SNPRINTF(msgbuff, OUT_BUFF_SIZE, "Cannot rundown database %s. Attempt to access with version %s, "
				"while already using %s", fname, gtm_release_name, nl_addr->now_running);
		PRINT_AND_SEND_DBRNDWN_FAILURE_MSG(msgbuff, fname, shmid);
		*exit_stat = ERR_MUNOTALLSEC;
		shmdt((void *)start_addr);
		return FALSE;
	}
	if (-1 == shmctl(shmid, IPC_STAT, &shmstat))
	{
		save_errno = errno;
		assert(FALSE);/* we were able to attach to this shmid before so should be able to get stats on it */
		util_out_print("!AD -> Error with shmctl for shmid = !UL",
			TRUE, fname_len, fname, shmid);
		gtm_putmsg(VARLSTCNT(1) save_errno);
		*exit_stat = ERR_MUNOTALLSEC;
		shmdt((void *)start_addr);
		return FALSE;
	}
	remove_shmid = FALSE;
	/* Check if db filename reported in shared memory still exists. If not, clean this shared memory section
	 * without even invoking "mu_rndwn_file" as that expects the db file to exist. Same case if shared memory
	 * points back to a database whose file header does not have this shmid.
	 */
	if (-1 == Stat(fname, &st_buff))
	{
		if (ENOENT == errno)
		{
			SNPRINTF(msgbuff, OUT_BUFF_SIZE, "File %s does not exist", fname);
			if (1 < shmstat.shm_nattch)
			{
				PRINT_AND_SEND_DBRNDWN_FAILURE_MSG(msgbuff, fname, shmid);
				*exit_stat = ERR_MUNOTALLSEC;
				shmdt((void *)start_addr);
				return FALSE;
			}
			remove_shmid = TRUE;
		} else
		{	/* Stat errored out e.g. due to file permissions. Log that */
			save_errno = errno;
			util_out_print("Cannot rundown shmid !UL for database file !AD as stat() on the file"
				" returned the following error", TRUE, shmid, fname_len, fname);
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			shmdt((void *)start_addr);
			return FALSE;
		}
	} else
	{
		mu_gv_cur_reg_init();
		gv_cur_region->dyn.addr->fname_len = strlen(fname);
		STRNCPY_STR(gv_cur_region->dyn.addr->fname, fname, gv_cur_region->dyn.addr->fname_len);
		fc = gv_cur_region->dyn.addr->file_cntl;
		fc->op = FC_OPEN;
		status = dbfilop(fc);
		if (SS_NORMAL != status)
		{
			util_out_print("!AD -> Error with dbfilop for shmid = !UL", TRUE, fname_len, fname, shmid);
			gtm_putmsg(VARLSTCNT(5) status, 2, DB_LEN_STR(gv_cur_region), errno);
			*exit_stat = ERR_MUNOTALLSEC;
			shmdt((void *)start_addr);
			return FALSE;
		}
		udi = FILE_INFO(gv_cur_region);
		LSEEKREAD(udi->fd, 0, &tsd, SIZEOF(sgmnt_data), status);
		if (0 != status)
		{
			save_errno = errno;
			util_out_print("!AD -> Error with LSEEKREAD for shmid = !UL", TRUE, fname_len, fname, shmid);
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			shmdt((void *)start_addr);
			return FALSE;
		}
		mu_gv_cur_reg_free();
		if (tsd.shmid != shmid)
		{
			SNPRINTF(msgbuff, OUT_BUFF_SIZE, "Shared memory ID (%d) in the DB file header does not match with the one"
					" reported by \"ipcs\" command (%d)", tsd.shmid, shmid);
			if (1 < shmstat.shm_nattch)
			{
				PRINT_AND_SEND_DBRNDWN_FAILURE_MSG(msgbuff, fname, shmid);
				*exit_stat = ERR_MUNOTALLSEC;
				shmdt((void *)start_addr);
				return FALSE;
			}
			remove_shmid = TRUE;
		} else if (tsd.gt_shm_ctime.ctime != shmstat.shm_ctime)
		{
			SNPRINTF(msgbuff, OUT_BUFF_SIZE, "Shared memory creation time in the DB file header does not match with"
					" the one reported by shmctl");
			if (1 < shmstat.shm_nattch)
			{
				PRINT_AND_SEND_DBRNDWN_FAILURE_MSG(msgbuff, fname, shmid);
				*exit_stat = ERR_MUNOTALLSEC;
				shmdt((void *)start_addr);
				return FALSE;
			}
			remove_shmid = TRUE;
		}
	}
	shmdt((void *)start_addr);
	if (remove_shmid)
	{
		assert('\0' != msgbuff[0]);
		if (0 != shm_rmid(shmid))
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, fname_len, fname,
				   ERR_TEXT, 2, RTS_ERROR_TEXT("Error removing shared memory"));
			util_out_print("!AD -> Error removing shared memory for shmid = !UL", TRUE, fname_len, fname, shmid);
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			return FALSE;
		}
		PRINT_AND_SEND_SHMREMOVED_MSG(msgbuff, fname_len, fname, shmid);
		*exit_stat = ERR_SHMREMOVED;
	} else
		*exit_stat = SS_NORMAL;
	return TRUE;
}

/* Takes an entry from 'ipcs -am' and checks for its validity to be a GT.M replication segment.
 * Returns TRUE if the shared memory segment is a valid GT.M replication segment
 * (based on a check on some fields in the shared memory) else FALSE.
 * If the segment belongs to GT.M, it returns the replication id of the segment
 * by the second argument.
 * Sets exit_stat to ERR_MUNOTALLSEC if appropriate.
 */
boolean_t validate_replpool_shm_entry(shm_parms *parm_buff, replpool_id_ptr_t replpool_id, int *exit_stat)
{
	boolean_t		remove_shmid, jnlpool_segment;
	int			fd;
	repl_inst_hdr		repl_instance;
	sm_uc_ptr_t		start_addr;
	int			save_errno, status, shmid;
	struct shmid_ds		shmstat;
	char			msgbuff[OUT_BUFF_SIZE], *instfilename;

	if (NULL == parm_buff)
		return FALSE;
	/* Check for the bare minimum size of the replic shared segment that we expect */
	/* if (parm_buff->sgmnt_siz < (SIZEOF(replpool_identifier) + MIN(MIN_JNLPOOL_SIZE, MIN_RECVPOOL_SIZE))) */
	if (parm_buff->sgmnt_siz < MIN(MIN_JNLPOOL_SIZE, MIN_RECVPOOL_SIZE))
		return FALSE;
	if (IPC_PRIVATE != parm_buff->key)
		return FALSE;
	shmid = parm_buff->shmid;
	/* we do not need to lock the shm for reading the rundown information as
	 * the other rundowns (if any) can also be allowed to share reading the
	 * same info concurrently.
	 */
	if (-1 == (sm_long_t)(start_addr = (sm_uc_ptr_t) do_shmat(shmid, 0, SHM_RND)))
		return FALSE;
	memcpy((void *)replpool_id, (void *)start_addr, SIZEOF(replpool_identifier));
	instfilename = replpool_id->instfilename;
	/* Even though we could be looking at a replication pool structure that has been created by an older version
	 * or newer version of GT.M, the format of the "replpool_identifier" structure is expected to be the same
	 * across all versions so we can safely dereference the "label" and "instfilename" fields in order to generate
	 * user-friendly error messages. Asserts for the layout are in "mu_rndwn_repl_instance" (not here) with a
	 * comment there as to why that location was chosen.
	 */
	if (memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 3))
		{
			util_out_print("Cannot rundown replpool shmid = !UL as it has format !AD "
				"created by !AD but this mupip is version and uses format !AD",
				TRUE, shmid, GDS_LABEL_SZ - 1, replpool_id->label,
				LEN_AND_STR(replpool_id->now_running), gtm_release_name_len, gtm_release_name,
				GDS_LABEL_SZ - 1, GDS_RPL_LABEL);
			*exit_stat = ERR_MUNOTALLSEC;
		}
		shmdt((void *)start_addr);
		return FALSE;
	}
	assert(JNLPOOL_SEGMENT == replpool_id->pool_type || RECVPOOL_SEGMENT == replpool_id->pool_type);
	if(JNLPOOL_SEGMENT != replpool_id->pool_type && RECVPOOL_SEGMENT != replpool_id->pool_type)
	{
		shmdt((void *)start_addr);
		return FALSE;
	}
	jnlpool_segment = (JNLPOOL_SEGMENT == replpool_id->pool_type);
	if (-1 == shmctl(shmid, IPC_STAT, &shmstat))
	{
		save_errno = errno;
		assert(FALSE);/* we were able to attach to this shmid before so should be able to get stats on it */
		util_out_print("!AD -> Error with shmctl for shmid = !UL",
				TRUE, LEN_AND_STR(instfilename), shmid);
		gtm_putmsg(VARLSTCNT(1) save_errno);
		*exit_stat = ERR_MUNOTALLSEC;
		shmdt((void *)start_addr);
		return FALSE;
	}
	/* Check if instance filename reported in shared memory still exists. If not, clean this
	 * shared memory section without even invoking "mu_rndwn_repl_instance" as that expects
	 * the instance file to exist. Same case if shared memory points back to an instance file
	 * whose file header does not have this shmid.
	 */
	OPENFILE(instfilename, O_RDONLY, fd);	/* check if we can open it */
	msgbuff[0] = '\0';
	remove_shmid = FALSE;
	if (FD_INVALID == fd)
	{
		if (ENOENT == errno)
		{
			SNPRINTF(msgbuff, OUT_BUFF_SIZE, "File %s does not exist", instfilename);
			if (1 < shmstat.shm_nattch)
			{
				PRINT_AND_SEND_REPLPOOL_FAILURE_MSG(msgbuff, replpool_id, shmid);
				*exit_stat = ERR_MUNOTALLSEC;
				shmdt((void *)start_addr);
				return FALSE;
			}
			remove_shmid = TRUE;
		} else
		{	/* open() errored out e.g. due to file permissions. Log that */
			save_errno = errno;
			util_out_print("Cannot rundown replpool shmid !UL for instance file"
				" !AD as open() on the file returned the following error",
				TRUE, shmid, LEN_AND_STR(instfilename));
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			shmdt((void *)start_addr);
			return FALSE;
		}
	} else
	{
		LSEEKREAD(fd, 0, &repl_instance, SIZEOF(repl_inst_hdr), status);
		if (0 != status)
		{
			save_errno = errno;
			util_out_print("!AD -> Error with LSEEKREAD for shmid = !UL", TRUE,
				LEN_AND_STR(instfilename), shmid);
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			shmdt((void *)start_addr);
			return FALSE;
		}
		if ((jnlpool_segment && (repl_instance.jnlpool_shmid != shmid))
			|| (!jnlpool_segment && (repl_instance.recvpool_shmid != shmid)))
		{
			SNPRINTF(msgbuff, OUT_BUFF_SIZE, "%s SHMID (%d) in the instance file header does not match with the"
					" one reported by \"ipcs\" command (%d)", jnlpool_segment ? "Journal Pool" : "Receive Pool",
					jnlpool_segment ? repl_instance.jnlpool_shmid : repl_instance.recvpool_shmid, shmid);
			if (1 < shmstat.shm_nattch)
			{
				PRINT_AND_SEND_REPLPOOL_FAILURE_MSG(msgbuff, replpool_id, shmid);
				*exit_stat = ERR_MUNOTALLSEC;
				shmdt((void *)start_addr);
				return FALSE;
			}
			remove_shmid = TRUE;
		}
		CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	}
	shmdt((void *)start_addr);
	if (remove_shmid)
	{
		assert('\0' != msgbuff[0]);
		if (0 != shm_rmid(shmid))
		{
			save_errno = errno;
			util_out_print("!AD -> Error removing shared memory for shmid = !UL",
				TRUE, LEN_AND_STR(instfilename), shmid);
			gtm_putmsg(VARLSTCNT(1) save_errno);
			*exit_stat = ERR_MUNOTALLSEC;
			return FALSE;
		}
		PRINT_AND_SEND_SHMREMOVED_MSG(msgbuff, STRLEN(instfilename), instfilename, shmid);
		*exit_stat = ERR_SHMREMOVED;
	} else
		*exit_stat = SS_NORMAL;
	return TRUE;
}

/* Gets all the required fields in shm_parms struct for a given shm entry */

shm_parms *get_shm_parm(char *entry)
{
	char 		*parm;
	shm_parms	*parm_buff;
	struct shmid_ds	shm_buf;

	parm_buff = (shm_parms *)malloc(SIZEOF(shm_parms));

	parm = parse_shm_entry(entry, SHMID);
	CONVERT_TO_NUM(shmid);
	parm = parse_shm_entry(entry, KEY);
	CONVERT_TO_NUM(key);

	/* get shm segment size directly from shmid_ds instead of parsing ipcs
	 * output (thus avoiding the -a option for ipcs in mu_rndwn_all()
	 */
	if (-1 == shmctl(parm_buff->shmid, IPC_STAT, &shm_buf))
	{
		free(parm_buff);
		return NULL;
	}
	parm_buff->sgmnt_siz = shm_buf.shm_segsz;
	return parm_buff;
}

/* Parses the output of 'IPCS_CMD_STR' command. Returns the value of the
 * specified field.
 */

/* NOTE : Even though the standard says that every column in the output
 * of 'ipcs' command should be separated by atleast one space, we have
 * observed a case where there is no space between the first (T) and
 * second (ID) fields on AIX under certain conditions.
 * The workaround is to always insert a blank space for all UNIX platforms
 * after the first (T field) character assuming the entry always starts with a
 * character describing the type of the ipc resource ('m' for shared memory).
 * On linux, the ipcs output starts with a KEY field (a hexadecimal number).
 * See the definition of IPCS_CMD_STR for this handling
 */
char *parse_shm_entry(char *entry, int which_field)
{
	char 	*parm;
	int 	iter, indx1 = 0, indx2 = 0;

	for(iter = 1; iter < which_field; iter++)
	{
		/* Strip leading spaces */
		while(entry[indx1] == ' ')
			indx1++;
		/* Accept until spaces or NULL */
		while(entry[indx1] && entry[indx1] != ' ')
			indx1++;
	}
	/* Strip leading spaces */
	while(entry[indx1] == ' ')
		indx1++;
	if ('\0' == entry[indx1])
	{
		assert(FALSE);
		return NULL;
	}
	parm = (char *)malloc(MAX_PARM_LEN);
	/* Copy value from entry until NULL or a space character */
	while(entry[indx1] && (entry[indx1] != ' ') && (indx2 < (MAX_PARM_LEN - 1)))
		parm[indx2++] = entry[indx1++];
	parm[indx2] = '\0';

	return parm;
}

int parse_sem_id(char *entry)
{
	char 	*parm;
	int 	iter, indx1 = 0, indx2;

	while(entry[indx1] == ' ')
		indx1++;
	while(entry[indx1] && entry[indx1] != ' ')
		indx1++;
	while(entry[indx1] == ' ')
		indx1++;
	if ('\0' == entry[indx1])
	{
		assert(FALSE);
		return -1;
	}
	indx2 = indx1;
	parm = &entry[indx1];
	while(entry[indx2] && entry[indx2] != ' ')
		indx2++;
	entry[indx2] = '\0';
	if (cli_is_dcm(parm))
		return (int)STRTOUL(parm, NULL, 10);
	else if (cli_is_hex(parm + 2))
		return (int)STRTOUL(parm, NULL, 16);
	else
	{
		assert(FALSE);
		return -1;
	}
}

int mu_rndwn_sem_all(void)
{
	int 			save_errno, exit_status = SS_NORMAL, semid;
	char			entry[MAX_ENTRY_LEN];
	FILE			*pf;
	char			fname[MAX_FN_LEN + 1], *fgets_res;
	boolean_t 		rem_sem;
	shm_parms		*parm_buff;

	if (NULL == (pf = POPEN(IPCS_SEM_CMD_STR ,"r")))
        {
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("POPEN()"), CALLFROM, save_errno);
                return ERR_MUNOTALLSEC;
        }
	while (NULL != (FGETS(entry, SIZEOF(entry), pf, fgets_res)) && entry[0] != '\n')
	{
		if (-1 != (semid = parse_sem_id(entry)))
		{
			if (is_orphaned_gtm_semaphore(semid))
			{
				if (-1 != semctl(semid, 0, IPC_RMID))
				{
					gtm_putmsg(VARLSTCNT(3) ERR_SEMREMOVED, 1, semid);
					send_msg(VARLSTCNT(3) ERR_SEMREMOVED, 1, semid);
				}
			}
		}
	}
	pclose(pf);
	return exit_status;
}
boolean_t is_orphaned_gtm_semaphore(int semid)
{
	int			semno, semval;
	struct semid_ds		semstat;
	union semun		semarg;

	semarg.buf = &semstat;
	if (-1 != semctl(semid, 0, IPC_STAT, semarg))
	{
		if (-1 == (semval = semctl(semid, semarg.buf->sem_nsems - 1, GETVAL)) || GTM_ID != semval)
			return FALSE;
		else
		{
			/* Make sure all has value = 0 */
			for (semno = 0; semno < semarg.buf->sem_nsems - 1; semno++)
				if (-1 == (semval = semctl(semid, semno, GETVAL)) || semval)
					return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}
