/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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

#ifdef __MVS__
#define TMPDIR_ACCESS_MODE	R_OK | W_OK | X_OK
#else
#define TMPDIR_ACCESS_MODE	R_OK | W_OK | X_OK | F_OK
#endif

#define	CLEANUP_AND_RETURN_FALSE	{					\
						if (-1 != backup_fd)		\
							close(backup_fd);	\
						if (!debug_mupip)		\
							UNLINK(tempfilename);	\
						return FALSE;			\
					}

GBLREF	bool		file_backed_up;
GBLREF	gd_region	*gv_cur_region;
GBLREF	bool		record;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	bool		online;
GBLREF	uint4           process_id;
GBLREF	boolean_t	debug_mupip;

bool	mubfilcpy (backup_reg_list *list)
{
	mstr			*file, tempfile;
	unsigned char		command[MAX_FN_LEN * 2 + 5]; /* 5 == max(sizeof("cp"),sizeof("mv")) + 2 (space) + 1 (NULL) */
	sgmnt_data_ptr_t	header_cpy;
	int4			backup_fd = -1, size, vbn, status, counter, hdrsize, rsize, ntries;
	int4			save_errno, adjust, blk_num, temp, rv, tempfilelen;
	struct stat		stat_buf;
	off_t			filesize, handled, offset;
	char 			*inbuf, *zero_blk, *ptr, *errptr;
	boolean_t		done;
	char			tempfilename[MAX_FN_LEN + 1], tempdir[MAX_FN_LEN], prefix[MAX_FN_LEN], *tempnam();
	int                     fstat_res;
	uint4			ustatus;
	shmpool_blk_hdr_ptr_t	sblkh_p;

	error_def(ERR_BCKUPBUFLUSH);
	error_def(ERR_TMPFILENOCRE);
	error_def(ERR_TEXT);
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	file = &(list->backup_file);
	file->addr[file->len] = '\0';
	header_cpy = list->backup_hdr;
	hdrsize = ROUND_UP(SIZEOF_FILE_HDR(header_cpy), DISK_BLOCK_SIZE);

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
		tempfile.len = strlen(tempfilename);
		if ((FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempfile, NULL, NULL, FALSE, &ustatus))) ||
		    (ntries > MAX_TEMP_OPEN_TRY))
		{
			if (FILE_STAT_ERROR != fstat_res)
				gtm_putmsg(VARLSTCNT(8) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr,
					   ERR_TEXT, 2, LEN_AND_LIT("Tried a maximum number of times, clean-up temporary files " \
								    "in backup directory and retry."));
			else
				gtm_putmsg(VARLSTCNT(5) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr, ustatus);
			return FALSE;
		}
		ntries++;
	}
	tempfilelen = strlen(tempfilename);
	memcpy(command, "cp ", 3);
	memcpy(command + 3, gv_cur_region->dyn.addr->fname, gv_cur_region->dyn.addr->fname_len);
	command[3 + gv_cur_region->dyn.addr->fname_len] = ' ';
	memcpy(command + 4 + gv_cur_region->dyn.addr->fname_len, tempfilename, tempfilelen);
	command[4 + gv_cur_region->dyn.addr->fname_len + tempfilelen] = 0;

	if (debug_mupip)
		util_out_print("!/MUPIP INFO:   !AD", TRUE,
			       sizeof("cp  ") - 1 + gv_cur_region->dyn.addr->fname_len + tempfilelen, command);
	if (0 != (rv = SYSTEM((char *)command)))
	{
		if (-1 == rv)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
                	util_out_print("system : !AZ", TRUE, errptr);
		}
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("Error doing !AD", TRUE, 4 + gv_cur_region->dyn.addr->fname_len + tempfilelen, command);
		CLEANUP_AND_RETURN_FALSE;
	}

	OPENFILE(tempfilename, O_RDWR, backup_fd);
	if (-1 == backup_fd)
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

	if (online)
	{
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;

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
		grab_crit(gv_cur_region);
		rel_crit(gv_cur_region);
		counter = 0;
		while ((0 != cs_addrs->shmpool_buffer->backup_cnt) && (0 == cs_addrs->shmpool_buffer->failed))
		{
			backup_buffer_flush(gv_cur_region);
			if (++counter > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				CLEANUP_AND_RETURN_FALSE;
			}
			if (counter & 0xF)
				wcs_sleep(counter);
			else
			{	/* Force recovery every few retries - this should not be happening */
				if (FALSE == shmpool_lock_hdr(gv_cur_region))
				{
					assert(FALSE);
					gtm_putmsg(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					CLEANUP_AND_RETURN_FALSE;
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}

		if (0 != cs_addrs->shmpool_buffer->failed)
		{
			util_out_print("Process !UL encountered the following error.", TRUE, cs_addrs->shmpool_buffer->failed);
			if (0 != cs_addrs->shmpool_buffer->backup_errno)
				gtm_putmsg(VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
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
			rsize = sizeof(shmpool_blk_hdr) + header_cpy->blk_size;
			sblkh_p = (shmpool_blk_hdr_ptr_t)malloc(rsize);
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
					errptr = (char *)STRERROR(status);
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
					util_out_print("MUPIP INFO:     Restoring block 0x!8XL from temporary file.",
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
					assert(cs_addrs->hdr->blk_size >= size);
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
	CLOSEFILE(backup_fd, status);
	backup_fd = -1;

	/* mv it to destination */

	memcpy(command, "mv ", 3);
	memcpy(command + 3, tempfilename, tempfilelen);
	command[3 + tempfilelen] = ' ';
	memcpy(command + 4 + tempfilelen, file->addr, file->len);
	command[4 + tempfilelen + file->len] = 0;
	if (debug_mupip)
		util_out_print("MUPIP INFO:     !AD", TRUE,
			       sizeof("mv  ") - 1 + tempfilelen + file->len, command);
	if (0 != (rv = SYSTEM((char *)command)))
	{
		if (-1 == rv)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
               		util_out_print("system : !AZ", TRUE, errptr);
		}
		util_out_print("Error doing !AD", TRUE, 4 + gv_cur_region->dyn.addr->fname_len + tempfilelen, command);
		CLEANUP_AND_RETURN_FALSE;
	}


	util_out_print("DB file !AD backed up in file !AD", TRUE, gv_cur_region->dyn.addr->fname_len,
		       gv_cur_region->dyn.addr->fname, file->len, file->addr);
	util_out_print("Transactions up to 0x!16@XJ are backed up.", TRUE, &header_cpy->trans_hist.curr_tn);
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
