/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include <sys/stat.h>
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

#define BACKUP_CHUNK_SIZE	(32*1024)
#define BLK_SZ			512
#define BACKUP_BLOCKS		(BACKUP_CHUNK_SIZE/BLK_SZ)
#define MAX_GBL_NAME_LEN	15
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
	mstr			*file;
	unsigned char		command[MAX_FN_LEN * 2 + 5]; /* 5 == max(sizeof("cp"),sizeof("mv")) + 2 (space) + 1 (NULL) */
	sgmnt_data_ptr_t	header_cpy;
	int4			backup_fd = -1, size, vbn, status, counter, rsize;
	int4			handled, save_errno, adjust, blk_num, temp, rv, tempfilelen;
	struct stat		stat_buf;
	off_t			filesize;
	char 			*inbuf, *zero_blk, *ptr, *errptr;
	boolean_t		done;
	char			*tempfilename, tempdir[MAX_FN_LEN], prefix[MAX_FN_LEN], *tempnam();
	int                     fstat_res, save_no;

	error_def(ERR_BCKUPBUFLUSH);

	file = &(list->backup_file);
	file->addr[file->len] = '\0';
	header_cpy = list->backup_hdr;
	size = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);

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
		save_no = errno;
		errptr = (char *)STRERROR(save_no);
                util_out_print("access : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("ERROR: Do NOT have full access to directory !AD", TRUE,
			LEN_AND_STR(tempdir));
		return FALSE;
	}

	/* make this cp a two step process ==> cp followed by mv */
	if (NULL == (tempfilename = TEMPNAM(tempdir, prefix)))
	{
		save_no = errno;
		errptr = (char *)STRERROR(save_no);
                util_out_print("tempname() : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("ERROR: Cannot create the temporary filename needed for backup.", TRUE);
		return FALSE;
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
			save_no = errno;
			errptr = (char *)STRERROR(save_no);
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
		save_no = errno;
		errptr = (char *)STRERROR(save_no);
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
			save_no = errno;
			errptr = (char *)STRERROR(save_no);
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
				save_no = errno;
				errptr = (char *)STRERROR(save_no);
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
		}

		/* restore from the temp file */
		grab_crit(gv_cur_region);
		rel_crit(gv_cur_region);
		counter = 1;
		while ((cs_addrs->backup_buffer->free != cs_addrs->backup_buffer->disk) && (0 == cs_addrs->backup_buffer->failed))
		{
			backup_buffer_flush(gv_cur_region);
			if (counter++ > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				CLEANUP_AND_RETURN_FALSE;
			}
			wcs_sleep(counter);
		}

		if (0 != cs_addrs->backup_buffer->failed)
		{
			util_out_print("Process !UL encountered the following error.", TRUE, cs_addrs->backup_buffer->failed);
			if (0 != cs_addrs->backup_buffer->backup_errno)
				gtm_putmsg(VARLSTCNT(1) cs_addrs->backup_buffer->backup_errno);
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE;
		}

		FSTAT_FILE(list->backup_fd, &stat_buf, fstat_res);
		if (-1 == fstat_res)
		{
			save_no = errno;
			errptr = (char *)STRERROR(save_no);
                	util_out_print("fstat : !AZ", TRUE, errptr);
			util_out_print("Error obtaining status of temporary file !AD.",
				TRUE, LEN_AND_STR(list->backup_tempfile));
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE;
		}

		if (0 < (filesize = stat_buf.st_size))
		{
			inbuf = (char *)malloc(sizeof(int4) + sizeof(block_id) + header_cpy->blk_size);
			LSEEKREAD(list->backup_fd, 0, &rsize, sizeof(rsize), status);
			if (0 != status)
			{
				if (0 < status)
				{
					save_no = errno;
					errptr = (char *)STRERROR(save_no);
                			util_out_print("read : ", TRUE, errptr);
					util_out_print("Error reading the temporary file !AD.",
						TRUE, LEN_AND_STR(list->backup_tempfile));
				}
				else
					util_out_print("Premature end of temporary file !AD.",
						TRUE, LEN_AND_STR(list->backup_tempfile));
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				free(inbuf);
				CLEANUP_AND_RETURN_FALSE;
			}
			adjust = 0;
			for (handled = 0, done = FALSE; ; )
			{
				if (handled + rsize + sizeof(rsize) >= filesize)
				{
					if (handled + rsize == filesize)
					{
						adjust = sizeof(rsize);
						done = TRUE;
					}
					else
					{
						util_out_print("Invalid information in temporary file !AD.",
							TRUE, LEN_AND_STR(list->backup_tempfile));
						util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len,
							file->addr);
						free(inbuf);
						CLEANUP_AND_RETURN_FALSE;
					}
				}
				DOREADRC(list->backup_fd, inbuf, rsize - adjust, save_errno);
				if (0 != save_errno)
				{
					if (0 < save_errno)
					{
						save_no = errno;
						util_out_print("Error accessing temporary file !AD.",
						TRUE, LEN_AND_STR(list->backup_tempfile));
						errptr = (char *)STRERROR(save_no);
                				util_out_print("read : !AZ", TRUE, errptr);
					}
					else
						util_out_print("Premature end of temporary file !AD.",
							TRUE, LEN_AND_STR(list->backup_tempfile));
					free(inbuf);
					CLEANUP_AND_RETURN_FALSE;
				}
				blk_num = *(block_id *)inbuf;
				if (debug_mupip)
					util_out_print("MUPIP INFO:     Restoring block 0x!8XL from temporary file.",
						TRUE, blk_num);
				if (blk_num < header_cpy->trans_hist.total_blks)
				{
                        		vbn = header_cpy->start_vbn - 1 + (header_cpy->blk_size/ DISK_BLOCK_SIZE * blk_num);
                        		LSEEKWRITE(backup_fd,
                        		           (off_t)vbn * DISK_BLOCK_SIZE,
                        		           inbuf + sizeof(block_id),
                        		           rsize - sizeof(block_id) - sizeof(int4),
                        		           save_errno);
                        		if (0 != save_errno)
                        		{
						save_no = errno;
                        		        util_out_print("Error accessing output file !AD. Aborting restore.",
                        		                TRUE, file->len, file->addr);
						errptr = (char *)STRERROR(save_no);
                				util_out_print("write : !AZ", TRUE, errptr);
						free(inbuf);
						CLEANUP_AND_RETURN_FALSE;
                        		}
				}
				if (TRUE == done)
				{
					free(inbuf);
					break;
				}
				handled += rsize;
                        	GET_LONG(temp, (inbuf + rsize - sizeof(int4)));
                        	rsize = temp;
			}
		}
	}

	header_cpy->last_com_backup = header_cpy->trans_hist.curr_tn;
	if (record)
		header_cpy->last_rec_backup = header_cpy->trans_hist.curr_tn;
	LSEEKWRITE(backup_fd, 0, header_cpy, size, status);
	if (0 != status)
	{
		save_no = errno;
		errptr = (char *)STRERROR(save_no);
               	util_out_print("write : !AZ", TRUE, errptr);
		util_out_print("Error writing data to backup file !AD.", TRUE, file->len, file->addr);
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE;
	}
	close(backup_fd);
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
			save_no = errno;
			errptr = (char *)STRERROR(save_no);
               		util_out_print("system : !AZ", TRUE, errptr);
		}
		util_out_print("Error doing !AD", TRUE, 4 + gv_cur_region->dyn.addr->fname_len + tempfilelen, command);
		CLEANUP_AND_RETURN_FALSE;
	}


	util_out_print("DB file !AD backed up in file !AD", TRUE, gv_cur_region->dyn.addr->fname_len,
		gv_cur_region->dyn.addr->fname, file->len, file->addr);
	util_out_print("Transactions up to 0x!8XL are backed up.", TRUE, header_cpy->trans_hist.curr_tn);
	cs_addrs->hdr->last_com_backup = header_cpy->trans_hist.curr_tn;
	if (record)
		cs_addrs->hdr->last_rec_backup = header_cpy->trans_hist.curr_tn;
	file_backed_up = TRUE;

	return TRUE;
}
