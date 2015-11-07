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

#include "gtm_fcntl.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_permissions.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "mupipbckup.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "sleep_cnt.h"
#include "util.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gtm_file_stat.h"
#include "gtm_tempnam.h"
#include "gds_blk_downgrade.h"
#include "shmpool.h"
#include "wcs_phase2_commit_wait.h"

#define TMPDIR_ACCESS_MODE	R_OK | W_OK | X_OK
#define	COMMAND_ARRAY_SIZE	1024
#define	MV_CMD			"mv "
#define	CP_CMD			"cp "

#define	FREE_COMMAND_STR_IF_NEEDED		\
{						\
	if (command != &cmdarray[0])		\
	{					\
		free(command);			\
		command = &cmdarray[0];		\
	}					\
}

#define	CLEANUP_AND_RETURN_FALSE								\
{												\
	int	rc;										\
												\
	if (FD_INVALID != backup_fd)								\
		CLOSEFILE_RESET(backup_fd, rc);	/* resets "backup_fd" to FD_INVALID */		\
	if (!debug_mupip)									\
		UNLINK(tempfilename);								\
	return FALSE;										\
}

GBLREF	bool			file_backed_up;
GBLREF	gd_region		*gv_cur_region;
GBLREF	bool			record;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	bool			online;
GBLREF	uint4			process_id;
GBLREF	boolean_t		debug_mupip;

error_def(ERR_BCKUPBUFLUSH);
error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_DBCCERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_ERRCALL);
error_def(ERR_PERMGENFAIL);
error_def(ERR_TEXT);
error_def(ERR_TMPFILENOCRE);
ZOS_ONLY(error_def(ERR_BADTAG);)

bool	mubfilcpy (backup_reg_list *list)
{
	mstr			*file, tempfile;
	unsigned char		cmdarray[COMMAND_ARRAY_SIZE], *command = &cmdarray[0];
	sgmnt_data_ptr_t	header_cpy;
	int4			backup_fd = FD_INVALID, counter, hdrsize, rsize, ntries;
	ssize_t                 status;
	int4			blk_num, cmdlen, rv, save_errno, tempfilelen, tmplen;
	struct stat		stat_buf;
	off_t			filesize, offset;
	char 			*inbuf, *zero_blk, *ptr, *errptr;
	char			tempfilename[MAX_FN_LEN + 1], tempdir[MAX_FN_LEN], prefix[MAX_FN_LEN];
	int                     fstat_res;
	uint4			ustatus, size;
	muinc_blk_hdr_ptr_t	sblkh_p;
	ZOS_ONLY(int		realfiletag;)
	int			group_id;
	int			perm;
	struct perm_diag_data	pdd;

	file = &(list->backup_file);
	file->addr[file->len] = '\0';
	header_cpy = list->backup_hdr;
	hdrsize = (int4)ROUND_UP(SIZEOF_FILE_HDR(header_cpy), DISK_BLOCK_SIZE);

	/* the temporary file should be located in the destination directory */
	ptr = file->addr + file->len - 1;
	while (('/' != *ptr) && (ptr > file->addr))
		ptr--;
	if (ptr > file->addr)
	{
		memcpy(tempdir, file->addr, ptr - file->addr + 1);
		tempdir[ptr - file->addr + 1] = '\0';
	}
	else
	{
		tempdir[0] = '.';
		tempdir[1] = '\0';
	}
	memset(prefix, 0, MAX_FN_LEN);
	memcpy(prefix, gv_cur_region->rname, gv_cur_region->rname_len);
	SPRINTF(&prefix[gv_cur_region->rname_len], "_%d_", process_id);

	/* verify that we have access to the temporary directory to avoid /tmp */
	if (0 != ACCESS(tempdir, TMPDIR_ACCESS_MODE))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("access : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("ERROR: Do NOT have full access to directory !AD", TRUE,
			       LEN_AND_STR(tempdir));
		return FALSE;
	}

	/* make this cp a two step process ==> cp followed by mv */
	ntries = 0;
	fstat_res = FILE_NOT_FOUND;
	/* do go into the loop for the first time, irrespective of fstat_res*/
	while ((FILE_NOT_FOUND != fstat_res) || (!ntries))
	{
		gtm_tempnam(tempdir, prefix, tempfilename);
		tempfile.addr = tempfilename;
		tempfile.len = STRLEN(tempfilename);
		if ((FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempfile, NULL, NULL, FALSE, &ustatus))) ||
		    (ntries > MAX_TEMP_OPEN_TRY))
		{
			if (FILE_STAT_ERROR != fstat_res)
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr,
					   ERR_TEXT, 2, LEN_AND_LIT("Tried a maximum number of times, clean-up temporary files " \
								    "in backup directory and retry."));
			else
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr,
						ustatus);
			return FALSE;
		}
		ntries++;
	}
	/* Calculate total line length for "cp" command. If cannot fit in local variable array, malloc space */
	tmplen = gv_cur_region->dyn.addr->fname_len;
	tempfilelen = STRLEN(tempfilename);
	cmdlen = STR_LIT_LEN(CP_CMD) + tmplen + 1 /* space */ + tempfilelen + 1 /* terminating NULL byte */;
	if (cmdlen > SIZEOF(cmdarray))
		command = malloc(cmdlen);	/* allocate memory and use that instead of local array "cmdarray" */
	MEMCPY_LIT(command, CP_CMD);
	cmdlen = STR_LIT_LEN(CP_CMD);
	memcpy(&command[cmdlen], gv_cur_region->dyn.addr->fname, tmplen);
	cmdlen += tmplen;
	command[cmdlen++] = ' ';
	memcpy(&command[cmdlen], tempfilename, tempfilelen);
	cmdlen += tempfilelen;
	command[cmdlen] = 0;
	if (debug_mupip)
		util_out_print("!/MUPIP INFO:   !AD", TRUE, cmdlen, command);
	rv = SYSTEM(((char *)command));
	if (0 != rv)
	{
		if (-1 == rv)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("system : !AZ", TRUE, errptr);
		}
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("Error doing !AD", TRUE, cmdlen, command);
		FREE_COMMAND_STR_IF_NEEDED;
		CLEANUP_AND_RETURN_FALSE;
	}
	FREE_COMMAND_STR_IF_NEEDED;
	assert(command == &cmdarray[0]);

	/* give temporary files the group and permissions as other shared resources - like journal files */
	OPENFILE(tempfilename, O_RDWR, backup_fd);
	if (FD_INVALID == backup_fd)
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("open : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("Error opening backup file !AD.", TRUE, file->len, file->addr);
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE;
	}
	FSTAT_FILE(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fd, &stat_buf, fstat_res);
	if (-1 != fstat_res)
		if (gtm_set_group_and_perm(&stat_buf, &group_id, &perm, PERM_FILE, &pdd) < 0)
		{
			send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
				RTS_ERROR_STRING(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
				PERMGENDIAG_ARGS(pdd));
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
				RTS_ERROR_STRING(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
				PERMGENDIAG_ARGS(pdd));
			if (online)
				cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			CLEANUP_AND_RETURN_FALSE;
		}
	/* setup new group and permissions if indicated by the security rules.
	 */
	if ((-1 == fstat_res) || (-1 == FCHMOD(backup_fd, perm)) || ((-1 != group_id) && (-1 == fchown(backup_fd, -1, group_id))))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("system : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		CLEANUP_AND_RETURN_FALSE;
	}
#if defined(__MVS__)
	if (-1 == gtm_zos_tag_to_policy(backup_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG( tempfilename, realfiletag, TAG_BINARY, errno);
#endif
	if (online)
	{
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		if ((cs_addrs->onln_rlbk_cycle != cs_addrs->nl->onln_rlbk_cycle) ||
			(0 != cs_addrs->nl->onln_rlbk_pid))
		{	/* A concurrent online rollback happened since we did the gvcst_init. The backup is not reliable.
			 * Cleanup and exit
			 */
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DBROLLEDBACK);
			CLEANUP_AND_RETURN_FALSE;
		}
		/* if there has been an extend, truncate it */
		FSTAT_FILE(backup_fd, &stat_buf, fstat_res);
		if (-1 == fstat_res)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("fstat : !AZ", TRUE, errptr);
			util_out_print("Error obtaining status of backup file !AD.", TRUE, file->len, file->addr);
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE;
		}
		filesize = header_cpy->start_vbn * DISK_BLOCK_SIZE +
			(off_t)header_cpy->trans_hist.total_blks * header_cpy->blk_size;
		if (filesize != stat_buf.st_size)
		{	/* file has been extended, so truncate it and set the end of database block */
			int ftruncate_res;
			FTRUNCATE(backup_fd, filesize, ftruncate_res);
			if (-1 == ftruncate_res)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("ftruncate : !AZ", TRUE, errptr);
				util_out_print("Error truncating backup file !AD.", TRUE, file->len, file->addr);
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE;
			}
			zero_blk = (char *)malloc(DISK_BLOCK_SIZE);
			memset(zero_blk, 0, DISK_BLOCK_SIZE);
			LSEEKWRITE(backup_fd, filesize - DISK_BLOCK_SIZE, zero_blk, DISK_BLOCK_SIZE, status);
			if (0 != status)
			{
				util_out_print("Error writing the last block in database !AD.", TRUE, file->len, file->addr);
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE;
			}
			free(zero_blk);
		}
		/* By getting crit here, we ensure that there is no process still in transaction logic that sees
		   (nbb != BACKUP_NOT_IN_PRORESS). After rel_crit(), any process that enters transaction logic will
		   see (nbb == BACKUP_NOT_IN_PRORESS) because we just set it to that value. At this point, backup
		   buffer is complete and there will not be any more new entries in the backup buffer until the next
		   backup.
		*/
		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(gv_cur_region);
		assert(cs_data == cs_addrs->hdr);
		if (dba_bg == cs_data->acc_meth)
		{	/* Now that we have crit, wait for any pending phase2 updates to finish. Since phase2 updates happen
			 * outside of crit, we dont want them to keep writing to the backup temporary file even after the
			 * backup is complete and the temporary file has been deleted.
			 */
			if (cs_addrs->nl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(cs_addrs, NULL))
			{
				assert(FALSE);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1,
					cs_addrs->nl->wcs_phase2_commit_pidcnt, DB_LEN_STR(gv_cur_region));
				rel_crit(gv_cur_region);
				CLEANUP_AND_RETURN_FALSE;
			}
		}
		if (debug_mupip)
		{
			util_out_print("MUPIP INFO:   Current Transaction # at end of backup is 0x!16@XQ", TRUE,
				&cs_data->trans_hist.curr_tn);
		}
		rel_crit(gv_cur_region);
		counter = 0;
		while ((0 != cs_addrs->shmpool_buffer->backup_cnt) && (0 == cs_addrs->shmpool_buffer->failed))
		{
			backup_buffer_flush(gv_cur_region);
			if (++counter > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				CLEANUP_AND_RETURN_FALSE;
			}
			if (counter & 0xF)
				wcs_sleep(counter);
			else
			{	/* Force recovery every few retries - this should not be happening */
				if (FALSE == shmpool_lock_hdr(gv_cur_region))
				{
					assert(FALSE);
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					CLEANUP_AND_RETURN_FALSE;
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}

		if (0 != cs_addrs->shmpool_buffer->failed)
		{
			assert(EACCES == cs_addrs->shmpool_buffer->backup_errno);
			util_out_print("Process !UL encountered the following error.", TRUE, cs_addrs->shmpool_buffer->failed);
			if (0 != cs_addrs->shmpool_buffer->backup_errno)
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE;
		}

		FSTAT_FILE(list->backup_fd, &stat_buf, fstat_res);
		if (-1 == fstat_res)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("fstat : !AZ", TRUE, errptr);
			util_out_print("Error obtaining status of temporary file !AD.",
				       TRUE, LEN_AND_STR(list->backup_tempfile));
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE;
		}

		if (0 < (filesize = stat_buf.st_size))
		{
			rsize = (int4)(SIZEOF(muinc_blk_hdr) + header_cpy->blk_size);
			sblkh_p = (muinc_blk_hdr_ptr_t)malloc(rsize);
			/* Do not use LSEEKREAD macro here because of dependence on setting filepointer for
			   subsequent reads.
			*/
			if (-1 != (status = (ssize_t)lseek(list->backup_fd, 0, SEEK_SET)))
			{
				DOREADRC(list->backup_fd, (sm_uc_ptr_t)sblkh_p, rsize, status);
			} else
				status = errno;
			if (0 != status)
			{
				if (0 < status)
				{
					errptr = (char *)STRERROR((int)status);
					util_out_print("read : ", TRUE, errptr);
					util_out_print("Error reading the temporary file !AD.",
						       TRUE, LEN_AND_STR(list->backup_tempfile));
				}
				else
					util_out_print("Premature end of temporary file !AD.",
						       TRUE, LEN_AND_STR(list->backup_tempfile));
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				free(sblkh_p);
				CLEANUP_AND_RETURN_FALSE;
			}
			while (TRUE)
			{
				assert(sblkh_p->valid_data);
				blk_num = sblkh_p->blkid;
				if (debug_mupip)
					util_out_print("MUPIP INFO:     Restoring block 0x!XL from temporary file.",
						       TRUE, blk_num);
				if (blk_num < header_cpy->trans_hist.total_blks)
				{
					inbuf = (char_ptr_t)(sblkh_p + 1);
					/* If the incoming block has an ondisk version of V4, convert it back to that
					   version before writing it out so it is the same as the block in the original
					   database.
					*/
					if (GDSV4 == sblkh_p->use.bkup.ondsk_blkver)
					{	/* Need to downgrade this block back to a previous format. Downgrade in place. */
						gds_blk_downgrade((v15_blk_hdr_ptr_t)inbuf, (blk_hdr_ptr_t)inbuf);
						size = (((v15_blk_hdr_ptr_t)inbuf)->bsiz + 1) & ~1;
					} else
						size = (((blk_hdr_ptr_t)inbuf)->bsiz + 1) & ~1;

					if (cs_addrs->do_fullblockwrites)
						size = ROUND_UP(size, cs_addrs->fullblockwrite_len);
					assert((uint4)cs_addrs->hdr->blk_size >= size);
					offset = (header_cpy->start_vbn - 1) * DISK_BLOCK_SIZE
						+ ((off_t)header_cpy->blk_size * blk_num);
					LSEEKWRITE(backup_fd,
						   offset,
						   inbuf,
						   size,
						   save_errno);
					if (0 != save_errno)
					{
						util_out_print("Error accessing output file !AD. Aborting restore.",
							       TRUE, file->len, file->addr);
						errptr = (char *)STRERROR(save_errno);
						util_out_print("write : !AZ", TRUE, errptr);
						free(sblkh_p);
						CLEANUP_AND_RETURN_FALSE;
					}
				} /* Else ignore block that is larger than file was at time backup initiated */
				DOREADRC(list->backup_fd, (sm_uc_ptr_t)sblkh_p, rsize, save_errno);
				if (0 != save_errno)
				{
					if (0 < save_errno)
					{
						util_out_print("Error accessing temporary file !AD.",
							       TRUE, LEN_AND_STR(list->backup_tempfile));
						errptr = (char *)STRERROR(save_errno);
						util_out_print("read : !AZ", TRUE, errptr);
						free(sblkh_p);
						CLEANUP_AND_RETURN_FALSE;
					} else
						/* End of file .. Note this does not detect the difference between
						   clean end of file and partial record end of file.
						 */
						break;
				}
			}
			free(sblkh_p);
		}
	}

	LSEEKWRITE(backup_fd, 0, header_cpy, hdrsize, save_errno);
	if (0 != save_errno)
	{
		errptr = (char *)STRERROR(save_errno);
		util_out_print("write : !AZ", TRUE, errptr);
		util_out_print("Error writing data to backup file !AD.", TRUE, file->len, file->addr);
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE;
	}
	CLOSEFILE_RESET(backup_fd, status);	/* resets "backup_fd" to FD_INVALID */

	/* mv it to destination */
	/* Calculate total line length for "mv" command. If cannot fit in local variable array, malloc space */
	assert(command == &cmdarray[0]);
	tmplen = file->len;
	cmdlen = STR_LIT_LEN(MV_CMD) + tempfilelen + 1 /* space */ + tmplen + 1 /* terminating NULL byte */;
	if (cmdlen > SIZEOF(cmdarray))
		command = malloc(cmdlen);	/* allocate memory and use that instead of local array "cmdarray" */
	MEMCPY_LIT(command, MV_CMD);
	cmdlen = STR_LIT_LEN(MV_CMD);
	memcpy(&command[cmdlen], tempfilename, tempfilelen);
	cmdlen += tempfilelen;
	command[cmdlen++] = ' ';
	memcpy(&command[cmdlen], file->addr, tmplen);
	cmdlen += tmplen;
	command[cmdlen] = 0;
	if (debug_mupip)
		util_out_print("MUPIP INFO:   !AD", TRUE, cmdlen, command);
	rv = SYSTEM(((char *)command));
	if (0 != rv)
	{
		if (-1 == rv)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("system : !AZ", TRUE, errptr);
		}
		util_out_print("Error executing command : !AD", TRUE, cmdlen, command);
		FREE_COMMAND_STR_IF_NEEDED;
		CLEANUP_AND_RETURN_FALSE;
	}
	FREE_COMMAND_STR_IF_NEEDED;
	assert(command == &cmdarray[0]);
	util_out_print("DB file !AD backed up in file !AD", TRUE, gv_cur_region->dyn.addr->fname_len,
		       gv_cur_region->dyn.addr->fname, file->len, file->addr);
	util_out_print("Transactions up to 0x!16@XQ are backed up.", TRUE, &header_cpy->trans_hist.curr_tn);
	cs_addrs->hdr->last_com_backup = header_cpy->trans_hist.curr_tn;
	cs_addrs->hdr->last_com_bkup_last_blk = header_cpy->trans_hist.total_blks;
	if (record)
	{
		cs_addrs->hdr->last_rec_backup = header_cpy->trans_hist.curr_tn;
		cs_addrs->hdr->last_rec_bkup_last_blk = header_cpy->trans_hist.total_blks;
	}
	file_backed_up = TRUE;
	return TRUE;
}
