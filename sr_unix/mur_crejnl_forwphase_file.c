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

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "jnl.h"
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "copy.h"
#include "gtmio.h"
#include "gdskill.h"
#include "collseq.h"
#include "gdscc.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "parse_file.h"
#include "lockconst.h"
#include "aswp.h"
#include "eintr_wrappers.h"
#include "io_params.h"
#include "rename_file_if_exists.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"

#include "gtmmsg.h"
#include "util.h"
#include "is_file_identical.h"
#include "tp_change_reg.h"
#include "gtm_time.h"

#define WRITE_SIZE	(128 * DISK_BLOCK_SIZE)

GBLREF	gd_region		*gv_cur_region;
GBLREF	int4			mur_wrn_count;
GBLREF	char			*log_rollback;
GBLREF	mur_opt_struct		mur_options;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF  sgmnt_data_ptr_t 	cs_data;

boolean_t mur_crejnl_forwphase_file(ctl_list **jnl_files)
{
	ctl_list		*ctl;
	char			*cmd, cmd_string[MAX_LINE], bkup_jnl_file_name[MAX_FN_LEN], rolled_bk_jnl_file_name[MAX_FN_LEN],
				rename_fn[MAX_FN_LEN];
	char			writes[WRITE_SIZE], *errptr, temp_jnl_file_name[MAX_FN_LEN];
	char			zeroes[DISK_BLOCK_SIZE + JNL_REC_START_BNDRY];
					/* + JNL_REC_START_BNDRY to ensure we have 8-byte aligned 512-byte block */
	int			temp_jnl_fd, temp_jnl_file_name_len, ftruncate_res, bkup_jnl_file_name_len, rv,
				rolled_bk_jnl_file_name_len, rename_len, jnl_rename_len, alignsize;
	union
	{
		jnl_file_header	jfh;
		char		jfh_block[HDR_LEN];
	} hdr_buffer;
	uint4			length, ftruncate_len, status, save_errno;
	uint4			lastaddr, forw_phase_eof_addr, src_begin, src_end;
	int4			zero_len, info_status;
	jnl_file_header		*header;
	jnl_record		*eof_record;
	off_t			eof_rec_offset;
	jrec_suffix		*eof_suffix;
	enum jnl_record_type	rectype;
	char			jnl_rename[JNL_NAME_SIZE];
	struct mur_file_control	*fc;
	uint4			jnl_status;
	boolean_t		optcre_forwphase;
	struct stat		stat_buf;
	int			fstat_res;

	error_def(ERR_NEWJNLFILECREATE);
	error_def(ERR_JNLCLOSE);
	error_def(ERR_JNLREADEOF);
	error_def(ERR_RENAMEFAIL);

	for (ctl = *jnl_files;  ctl != NULL;  ctl = ctl->next)
	{
		gv_cur_region = ctl->gd;
		tp_change_reg();	/* needed for jnl_ensure_open below */
		fc = ctl->rab->pvt;
		/* Don't process if journalling is not enabled  or before image journalling is not enabled
		 * If last consist_stop_addr is the last record in the journal file, don't truncate the journal file.
		 * The journal file will be closed as part of run_down after successful recovery
		 */
		if (!JNL_ENABLED(cs_data) || !fc->jfh->before_images)
			continue;
		/* In case the last record is the consist stop record, don't truncate the journal file and create forwphase
		 * file. Close the journal file properly at the end of successful recovery
		 */
		header = &hdr_buffer.jfh;
		if (mur_options.forward)
			ctl->stop_addr = HDR_LEN;
		/* Until now, we were reading the journal in the backward direction (using mur_previous()).
		 * At this point, we can't call mur_next(ctl->rab, 0) since mur_next() assumes that reads until
		 * now have been progressing in the forward direction. Hence we need to call it with an absolute offset
		 * i.e. mur_next(ctl->rab, ctl->stop_addr) and then continue with mur_next(ctl->rab, 0).
		 */
		if (SS_NORMAL != (status = mur_next(ctl->rab, ctl->stop_addr)))
		{
			if (ERR_JNLREADEOF == status)
				GTMASSERT;
			mur_jnl_read_error(ctl, status, TRUE);
			break;
		}
		assert(!ctl->consist_stop_addr || ctl->consist_stop_addr == ctl->stop_addr);
		optcre_forwphase = (ctl->consist_stop_addr && ((ctl->stop_addr + ctl->rab->reclen) == fc->eof_addr)) ? TRUE : FALSE;
		if (SS_NORMAL != (status = mur_next(ctl->rab, 0)) && ERR_JNLREADEOF != status)
		{
			mur_jnl_read_error(ctl, status, TRUE);
			break;
		}
		ftruncate_len = ((ERR_JNLREADEOF == status) ? fc->eof_addr : ctl->rab->dskaddr);
				/* on JNLREADEOF, ctl->rab->dskaddr doesn't get updated, use eof_addr instead for ftruncate_len */
		/* lastaddr is the offset of the last jnl rec after which the jnlfile is ftruncated. Used for EOF backpointering */
		lastaddr = ctl->stop_addr;
		assert(!optcre_forwphase || 0 == fc->jfh->ftruncate_len);
		if (!optcre_forwphase)
		{
			if (0 == fc->jfh->ftruncate_len)
			{
				if (0 == ctl->consist_stop_addr)
				{
					if (mur_options.rollback)
					{
						strcpy(rolled_bk_jnl_file_name, ctl->jnl_fn);
						strcat(rolled_bk_jnl_file_name, "_rolled_bak");
						rolled_bk_jnl_file_name_len = strlen(rolled_bk_jnl_file_name);
						if (RENAME_FAILED == rename_file_if_exists(rolled_bk_jnl_file_name,
							rolled_bk_jnl_file_name_len, &info_status, rename_fn, &rename_len))
							gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, ctl->jnl_fn_len, ctl->jnl_fn,
								rename_len, rename_fn, info_status);
						if (0 != RENAME(ctl->jnl_fn, rolled_bk_jnl_file_name))
						{
							save_errno = errno;
							errptr = (char *)STRERROR(save_errno);
							util_out_print(errptr, TRUE, save_errno);
							util_out_print("MUR-W-ERRRENAME : Failed to rename file !AD to !AD", TRUE,
									ctl->jnl_fn_len, ctl->jnl_fn, rolled_bk_jnl_file_name_len,
									rolled_bk_jnl_file_name);
						}
						continue;
					} else if (TRUE == ctl->concat_prev)
						continue; /* Don't create new journal files for the ones not used,
								rollback to previous generation file */
					else if (mur_options.forward && !is_file_identical(ctl->jnl_fn,
							(char *)cs_data->jnl_file_name))
						continue;
				}
				memcpy(temp_jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
				memcpy((char *)temp_jnl_file_name + ctl->jnl_fn_len, FORWSUFFIX, STR_LIT_LEN(FORWSUFFIX));
				temp_jnl_file_name_len = ctl->jnl_fn_len + STR_LIT_LEN(FORWSUFFIX);
				temp_jnl_file_name[temp_jnl_file_name_len] = '\0';
				if (RENAME_FAILED == rename_file_if_exists(temp_jnl_file_name, temp_jnl_file_name_len,
									&info_status, jnl_rename, &jnl_rename_len))
					gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, ctl->jnl_fn_len, ctl->jnl_fn,
						jnl_rename_len, jnl_rename, info_status);
				OPENFILE3(temp_jnl_file_name, O_CREAT | O_EXCL | O_RDWR, 0600, temp_jnl_fd);
				if (-1 == temp_jnl_fd)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					util_out_print(errptr, TRUE, save_errno);
					util_out_print("MUR-E-JNLOPNERR : Error opening journal file !AD ", TRUE,
								ctl->jnl_fn_len, ctl->jnl_fn);
					return FALSE;
				}
				length = WRITE_SIZE;
				memset(writes, 0, length);
				/* The copy logic here works like this, start copying into the file at alignsize
				 * (basically empty 16k at the begining, to make mur_next happy) and copy the rest
				 * upto next align boundary into the new file (there will be an additional empty file
				 * based on whether the earlier copy started from an align record or not. The stop_addr
				 * is set appropriately for the forward_processing to work later. Please note that we
				 * are overwriting the journal file header as it's just a temporary copy of the old journal file
				 */
				/* We copy an empty 16K chunk just to make sure ctl->stop_addr is non-zero */
				alignsize = fc->jfh->alignsize;
				ctl->stop_addr = alignsize + (ftruncate_len % alignsize);	/* offset to begin reading from
												 * forw_phase file */
				src_begin = ROUND_DOWN(ftruncate_len, alignsize);
				src_end = fc->eof_addr;
				forw_phase_eof_addr = alignsize; /* We leave an empty 16K boundary in the destination file to ensure
							 * ctl->stop_addr is non-zero in case src_begin happens to be at an
							 * alignsize boundary */
				/* Copy all the data from src_begin to src_end of source jnl file to
				 * forw_phase_eof_addr of new journal file */
				while (src_begin < src_end)
				{
					length = (src_end - src_begin) > WRITE_SIZE ? WRITE_SIZE : src_end - src_begin;
					LSEEKREAD(fc->fd, src_begin, writes, length, status);
					if (0 != status)
					{
						util_out_print("Error reading Journal file !AD",
									TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
						return FALSE;
					}
					LSEEKWRITE(temp_jnl_fd, forw_phase_eof_addr, writes, length, status);
					if (0 != status)
					{
						util_out_print("Error Writing to Journal file !AD", TRUE,
									temp_jnl_file_name_len, temp_jnl_file_name);
						return FALSE;
					}
					src_begin += length;
					forw_phase_eof_addr += length;
				}
				if (mur_options.preserve_jnl)
				{	/* Take a backup of this journal file since this is going to be truncated */
					if (mur_options.rollback)
					{
						memcpy(bkup_jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
						memcpy((char *)bkup_jnl_file_name + ctl->jnl_fn_len, RLBKSUFFIX,
												STR_LIT_LEN(RLBKSUFFIX));
						bkup_jnl_file_name_len = ctl->jnl_fn_len + STR_LIT_LEN(RLBKSUFFIX);
						bkup_jnl_file_name[bkup_jnl_file_name_len] = '\0';
					} else
					{
						memcpy(bkup_jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
						memcpy((char *)bkup_jnl_file_name + ctl->jnl_fn_len, RECOVERSUFFIX,
							STR_LIT_LEN(RECOVERSUFFIX));
						bkup_jnl_file_name_len = ctl->jnl_fn_len + STR_LIT_LEN(RECOVERSUFFIX);
						bkup_jnl_file_name[bkup_jnl_file_name_len] = '\0';
					}
					cmd = &cmd_string[0];
					memcpy(cmd, BKUP_CMD, STR_LIT_LEN(BKUP_CMD));
					cmd += STR_LIT_LEN(BKUP_CMD);
					memcpy(cmd, ctl->jnl_fn, ctl->jnl_fn_len);
					cmd += ctl->jnl_fn_len;
					*cmd++ = ' ';
					memcpy(cmd, bkup_jnl_file_name, bkup_jnl_file_name_len);
					cmd += bkup_jnl_file_name_len;
					*cmd = '\0';
					if (0 != (rv = SYSTEM(cmd_string)))
					{
						if (-1 == rv)
						{
							save_errno = errno;
							errptr = (char *)STRERROR(save_errno);
							util_out_print(errptr, TRUE, save_errno);
						}
						util_out_print("MUR-W-ERRCOPY : Error backing up jnl file !AD to !AD", TRUE,
							ctl->jnl_fn_len, ctl->jnl_fn, bkup_jnl_file_name_len, bkup_jnl_file_name);
						/* Even though the copy failed we continue with the file truncation */
					}
				}
			} else
			{	/* Incase rollback/recover was interrupted during forward phase (indicated by a non-zero
				 * 	ftruncate_len), use the fields stored in the jnl-file-header instead of computing them. */
				ftruncate_len = fc->jfh->ftruncate_len; /* set the truncate length to one saved in file header */
				memcpy(header, fc->jfh, HDR_LEN); /* Copy the header from truncated journal file */
				ctl->stop_addr = header->forw_phase_stop_addr; /* This offset used later for EOF backpointering */
				forw_phase_eof_addr = header->forw_phase_eof_addr;
			}
			FSTAT_FILE(fc->fd, &stat_buf, fstat_res);
			if (0 != fstat_res)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print(errptr, TRUE, save_errno);
				util_out_print("MUR-W-ERRFSTAT : Failed to FSTAT file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
			}
			FTRUNCATE(fc->fd, ftruncate_len, ftruncate_res);
			if (0 != ftruncate_res)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print(errptr, TRUE, save_errno);
				util_out_print("MUR-W-ERRTRUNC : Failed to truncate file !AD to length !UL",
							TRUE, ctl->jnl_fn_len, ctl->jnl_fn, ctl->consist_stop_addr);
			}
		}
		ctl->consist_stop_addr = ftruncate_len; /* Make sure that subsequent reads by mur_get_pini_jpv looks
							at correct journal file for pini records in case of extraction */
		eof_rec_offset = ROUND_UP(ftruncate_len, DISK_BLOCK_SIZE);
		zero_len = eof_rec_offset - ftruncate_len;
		assert(zero_len >= 0 && DISK_BLOCK_SIZE > zero_len);
		if (zero_len > 0)
		{
			memset(zeroes, 0, zero_len);
			LSEEKWRITE(fc->fd, ftruncate_len, zeroes, zero_len, status);
			if (0 != status)
				rts_error(VARLSTCNT(6) ERR_JNLCLOSE, 4, ctl->jnl_fn, ctl->jnl_fn_len, DB_LEN_STR(ctl->gd));
		}
		/* Write back the header  and set the fields properly, simulating the jnl_file_close activity */
		if (0 == fc->jfh->ftruncate_len)
		{
			LSEEKREAD(fc->fd, 0, header, HDR_LEN, status);
			if (0 != status)
			{
				util_out_print(" Error reading Journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
				return FALSE;
			}
			assert (0 == header->ftruncate_len);
			/* No forw_phase_file was created. Don't copy any fields into file header related to forw Phase */
			if (!optcre_forwphase)
			{	/* Calculate the last_record offset in the forw_phase file. Do this by subtracting the
				 * length of the last record in the jnl-file from the forw-phase file's eof-addr.
				 * We can do this only because we do aligned copies from the jnl-file to the forw-phase file
				 *  i.e. the jnl records in the jnl-file have the same 16K-modulo offsets in the forw-phase file
				 */
				header->forw_phase_last_record = forw_phase_eof_addr - (fc->eof_addr - fc->last_record);
				header->ftruncate_len = ftruncate_len;
				header->forw_phase_stop_addr = ctl->stop_addr;
				header->forw_phase_eof_addr = forw_phase_eof_addr;
				header->forw_phase_jnl_file_len = temp_jnl_file_name_len;
				memcpy(header->forw_phase_jnl_file_name, temp_jnl_file_name, temp_jnl_file_name_len);
				header->forw_phase_jnl_file_name[temp_jnl_file_name_len] = 0;
				header->data_file_name_length = gv_cur_region->dyn.addr->fname_len;
				memcpy(header->data_file_name, gv_cur_region->dyn.addr->fname, gv_cur_region->dyn.addr->fname_len);
				header->data_file_name[gv_cur_region->dyn.addr->fname_len] = 0;
			}
			JNL_SHORT_TIME(header->eov_timestamp);
			header->eov_tn = cs_addrs->ti->curr_tn;
			header->end_of_data = eof_rec_offset;
			LSEEKWRITE(fc->fd, 0, header, HDR_LEN, status);
			if (0 != status)
			{
				util_out_print(" Error Writing to Journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
				return FALSE;
			}
		}
		if (fc->eof_addr < lastaddr)
			GTMASSERT;
		/* Write an EOF record */
		assert(sizeof(zeroes) >= DISK_BLOCK_SIZE);
		memset(zeroes, 0, sizeof(zeroes));
		assert(EOF_RECLEN <= DISK_BLOCK_SIZE);
		eof_record = (jnl_record *)ROUND_UP((uint4)zeroes, JNL_REC_START_BNDRY);/* ensure 8-byte alignment for EOF record */
		assert(0 == ((int)eof_record) % JNL_REC_START_BNDRY);
		eof_record->jrec_type = JRT_EOF;
		if (lastaddr <= JNL_FILE_FIRST_RECORD)
			eof_record->jrec_backpointer = DISK_BLOCK_SIZE;
		else
			eof_record->jrec_backpointer = eof_rec_offset - lastaddr;
		jnl_prc_vector(&eof_record->val.jrec_eof.process_vector);
		eof_record->val.jrec_eof.tn = cs_addrs->ti->curr_tn;
		QWASSIGN(eof_record->val.jrec_eof.jnl_seqno, cs_data->reg_seqno);
		eof_suffix = (jrec_suffix *)((char *)eof_record + EOF_BACKPTR);
		eof_suffix->backptr = EOF_BACKPTR;
		eof_suffix->suffix_code = JNL_REC_TRAILER;
		LSEEKWRITE(fc->fd, eof_rec_offset, eof_record, DISK_BLOCK_SIZE, status);
			/* write DISK_BLOCK_SIZE above instead of just EOF_RECLEN to make sure we have 512-byte aligned filesize */
		if (0 != status)
		{
			util_out_print(" Error Writing to Journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
			return FALSE;
		}
		if (!optcre_forwphase)
		{	/* restore filesize to what it was before the FTRUNCATE */
			assert(sizeof(zeroes) >= DISK_BLOCK_SIZE);
			memset(zeroes, 0, sizeof(zeroes));
			LSEEKWRITE(fc->fd, (stat_buf.st_size - DISK_BLOCK_SIZE), zeroes, DISK_BLOCK_SIZE, status);
			if (0 != status)
			{
				util_out_print(" Error Writing to End of Journal file !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
				return FALSE;
			}
		}
		if (TRUE == ctl->concat_next)
		{
			if (log_rollback)
				util_out_print("MUR-I-JNLFILECHNG : Database File !AD now uses jnl-file ---> !AD",
					TRUE, header->data_file_name_length, header->data_file_name,
						ctl->jnl_fn_len, ctl->jnl_fn);
			cs_data->jnl_file_len = ctl->jnl_fn_len;
			memcpy(cs_data->jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
			cs_data->jnl_file_name[ctl->jnl_fn_len] = '\0';
		}
		/* We want the journal file jpc->channel to be opened before the first jnl_ensure_open() occurs through
		 * a call from mur_output_record()/t_end(). This is to make sure that any pini_addr manipulations that
		 * we have done in mur_output_record() (like copying pini_addr from the corresponding journal record)
		 * doesn't get reset later in t_end()/jnl_ensure_open() (which can happen if jpc->channel is NOJNL).
		 */
		grab_crit(gv_cur_region);
		jnl_status = jnl_ensure_open();
		if (0 != jnl_status)
			rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
		rel_crit(gv_cur_region);
		if (!optcre_forwphase)
		{
			if (0 == fc->jfh->ftruncate_len)
			{
				ctl->jnl_fn_len = temp_jnl_file_name_len;
				memcpy(ctl->jnl_fn, temp_jnl_file_name, temp_jnl_file_name_len);
				ctl->jnl_fn[temp_jnl_file_name_len] = 0;
			} else
			{
				ctl->jnl_fn_len = header->forw_phase_jnl_file_len;
				memcpy(ctl->jnl_fn, header->forw_phase_jnl_file_name, ctl->jnl_fn_len);
				ctl->jnl_fn[ctl->jnl_fn_len] = 0;
				OPENFILE3(ctl->jnl_fn, O_RDWR, 0600, temp_jnl_fd);
				if (-1 == temp_jnl_fd)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					util_out_print(errptr, TRUE, save_errno);
					util_out_print("MUR-E-JNLOPNERR : Error opening journal file !AD ", TRUE,
								ctl->jnl_fn_len, ctl->jnl_fn);
					return FALSE;
				}
			}
			if (0 != (status = mur_close(ctl->rab))) /* Close the old rab structure */
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print(errptr, TRUE, save_errno);
				util_out_print("MUR-W-JNLCLFAIL : Close failed for journal file !AD\n", TRUE,
							ctl->jnl_fn_len, ctl->jnl_fn);
				mur_wrn_count++;
			}
			ctl->rab = mur_rab_create(MINIMUM_BUFFER_SIZE);
			ctl->found_eof = TRUE;
			ctl->rab->pvt->jfh = (jnl_file_header *)malloc(HDR_LEN);
			memcpy(ctl->rab->pvt->jfh, header, HDR_LEN);
			ctl->rab->pvt->eof_addr = forw_phase_eof_addr;
			ctl->rab->pvt->last_record = header->forw_phase_last_record;
			ctl->rab->pvt->fd = temp_jnl_fd;
		} else
		{
			ctl->rab->pvt->last_record = eof_rec_offset; /* Set rab fields to point to the EOF record written in */
			ctl->rab->pvt->eof_addr = eof_rec_offset + EOF_RECLEN; /* the journal file, since we read same file again */
		}
	}
	return TRUE;
}
