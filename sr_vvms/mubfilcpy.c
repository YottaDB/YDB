/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>
#include <ssdef.h>
#include <climsgdef.h>
#include <iodef.h>
#include <descrip.h>
#include <errno.h>
#include <errnodef.h>
#include <efndef.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "sleep_cnt.h"
#include "util.h"
#include "setfileprot.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gtm_tempnam.h"
#include "gds_blk_downgrade.h"
#include "shmpool.h"
#include "min_max.h"
#include "iormdef.h"
#include "wcs_phase2_commit_wait.h"

#define MAX_TEMPFILE_TRY	16
#define	BACKUP_E_OPENOUT	0x10A38012
#define	BACKUP_W_ACCONFLICT	0x10A38410

#define	DELETE_BAD_BACKUP(A)												\
{															\
	if (SS$_NORMAL == (status = sys$dassgn((A).fab$l_stv)))								\
		status = sys$erase(&(A));										\
	if (RMS$_NORMAL != status) 											\
	{														\
		gtm_putmsg(VARLSTCNT(1) status);									\
		util_out_print("Cannot delete the unsuccessful backup file !AD", TRUE, (A).fab$b_fns, (A).fab$l_fna); 	\
	}														\
}

GBLREF	bool			online;
GBLREF	bool			record;
GBLREF	bool			file_backed_up;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF	boolean_t		debug_mupip;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		*mubbuf;
GBLREF	uint4			process_id;

bool mubfilcpy(backup_reg_list *list)
{
	mstr			*file;
	char			*errptr, *temp_ptr, tempfilename[MAX_FN_LEN + 1], tempdir[MAX_FN_LEN], prefix[MAX_FN_LEN];
	char			backup_ign[] = "BACKUP/IGNORE=INTERLOCK ", rename[] = "RENAME ";
	char                    command_buff[SIZEOF(backup_ign)+2*MAX_FN_LEN+1]; /*= SIZEOF(backup_ign)-1+2(filenames)+2(spaces)*/
	unsigned short 		wt_iosb[4];
	uint4 			status, lcnt, backup_status, vbn;
	int4 			size, tempfilelen, command_len, errlen, read_size, read_len;
	off_t			filesize_tobe, filesize_curr;
	struct FAB 		*fcb, fab, temp_fab;
	struct RAB		temp_rab;
	unsigned short		old_perm;
	sgmnt_addrs             *csa;
	sgmnt_data              *header;
	struct XABFHC 		xabfhc;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	sm_uc_ptr_t		read_ptr, inbuf;
	block_id		blk_num;

	$DESCRIPTOR(command, command_buff);
	$DESCRIPTOR(nl, "nl:");

	error_def(ERR_BACKUPCTRL);
	error_def(ERR_BCKUPBUFLUSH);
	error_def(ERR_COMMITWAITSTUCK);
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);
	error_def(ERR_TEXT);
	error_def(ERR_TRUNCATEFAIL);

	/* ============================================ initialization =================================================== */
	file = &(list->backup_file);
	header = list->backup_hdr;
	fcb = ((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fab;

	/* ======= construct temporary filename in destination directory with region name and process id as prefix ======= */
	temp_ptr = file->addr + file->len - 1;
	while ((']' != *temp_ptr) && (temp_ptr > file->addr))
		temp_ptr--;
	if (temp_ptr > file->addr)
	{
		memcpy(tempdir, file->addr, temp_ptr - file->addr + 1);
		tempdir[temp_ptr - file->addr + 1] = '\0';
	} else
	{
		assert(FALSE);
		tempdir[0] = '\0';
	}
	memset(prefix, 0, MAX_FN_LEN);
	memcpy(prefix, gv_cur_region->rname, gv_cur_region->rname_len);
	SPRINTF(&prefix[gv_cur_region->rname_len], "_%x", process_id);
	gtm_tempnam(tempdir, prefix, tempfilename);
	tempfilelen = strlen(tempfilename);

	/* ================= construct the command to backup the database to the temporary file ========================== */
	MEMCPY_LIT(command_buff, backup_ign);
	command_len = SIZEOF(backup_ign) - 1;
	memcpy(&command_buff[command_len], fcb->fab$l_fna, fcb->fab$b_fns);
	command_len += fcb->fab$b_fns;
	command_buff[command_len++] = ' ';
	memcpy(&command_buff[command_len], tempfilename, tempfilelen);
	command_len += tempfilelen;
	command_buff[command_len] = '\0';
	command.dsc$w_length = command_len;

	/* ============================ Issue the command and check the return status ==================================== */
	lcnt = 0;
	do
	{
		if (debug_mupip)
			util_out_print("!/MUPIP INFO:	!AD", TRUE, command_len, command_buff);
		if (SS$_NORMAL != (status = lib$spawn(&command, 0, &nl, 0, 0, 0, &backup_status, 0, 0, 0, 0, 0, 0)))
		{
			gtm_putmsg(VARLSTCNT(1) status);
			util_out_print("Unable to spawn the command: 	!AD", TRUE, command_len, command_buff);
			return FALSE;
		}
		if ((backup_status & 1) || (BACKUP_W_ACCONFLICT == backup_status))
			break;
		else if ((BACKUP_E_OPENOUT == backup_status) && (lcnt++ < MAX_TEMPFILE_TRY))
		{
			command_len -= tempfilelen;
			gtm_tempnam(tempdir, prefix, tempfilename);
			tempfilelen = strlen(tempfilename);
			memcpy(&command_buff[command_len], tempfilename, tempfilelen);
			command_len += tempfilelen;
			command_buff[command_len] = '\0';
			command.dsc$w_length = command_len;
		}
		else
		{
			gtm_putmsg(VARLSTCNT(1) backup_status);
			util_out_print("Execution of command:	!AD failed.", TRUE, command_len, command_buff);
			assert(FALSE);
			return FALSE;
		}
	} while (TRUE);

	/* ==================== we need to apply header and for online backup, tempfile then rename =================== */
	if (online)
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS; /* stop everyone from writing to tempfile */

	/* -------------------------- open the temp copy of the backup file ---------------------- */
	fab = cc$rms_fab;
	xabfhc = cc$rms_xabfhc;
	fab.fab$b_fac = FAB$M_BIO | FAB$M_PUT;
	fab.fab$l_fop = FAB$M_UFO;
	fab.fab$l_fna = tempfilename;
	fab.fab$b_fns = tempfilelen;
	fab.fab$l_xab = &xabfhc;			/* to obtain the filesize info */
	if (!(1 & (status = sys$open(&fab))))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("ERROR: Cannot open temporary backup file !AD.", TRUE,
			       fab.fab$b_fns, fab.fab$l_fna);
		util_out_print("Please delete it manually.", TRUE);
		return FALSE;
	}

	if (online)
	{
		/* ---------------------- calculate the to-be filesize ----------------------------------- */
		filesize_tobe = header->start_vbn +
			(off_t)header->blk_size / DISK_BLOCK_SIZE * header->trans_hist.total_blks;
		filesize_curr = xabfhc.xab$l_ebk;
		/* By getting crit here, we ensure that there is no process still in transaction logic that sees
		   (nbb != BACKUP_NOT_IN_PRORESS). After rel_crit(), any process that enters transaction logic will
		   see (nbb == BACKUP_NOT_IN_PRORESS) because we just set it to that value. At this point, backup
		   buffer is complete and there will not be any more new entries in the backup buffer until the next
		   backup.
		*/
		grab_crit(gv_cur_region);
		assert(cs_data == cs_addrs->hdr);
		if (dba_bg == cs_data->acc_meth)
		{	/* Now that we have crit, wait for any pending phase2 updates to finish. Since phase2 updates happen
			 * outside of crit, we dont want them to keep writing to the backup temporary file even after the
			 * backup is complete and the temporary file has been deleted.
			 */
			if (cs_addrs->nl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(cs_addrs, NULL))
			{
				gtm_putmsg(VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1,
					cs_addrs->nl->wcs_phase2_commit_pidcnt, DB_LEN_STR(gv_cur_region));
				rel_crit(gv_cur_region);
				DELETE_BAD_BACKUP(fab);
				return FALSE;
			}
		}
		if (debug_mupip)
		{
			util_out_print("MUPIP INFO:   Current Transaction # at end of backup is 0x!16@XQ", TRUE,
				&cs_data->trans_hist.curr_tn);
		}
		rel_crit(gv_cur_region);
		/* ------------------------------- write saved blocks ------------------------------------ */
		lcnt = 0;
		while ((0 != cs_addrs->shmpool_buffer->backup_cnt) && (0 == cs_addrs->shmpool_buffer->failed))
		{
			if (0 != cs_addrs->shmpool_buffer->failed)
				break;
			backup_buffer_flush(gv_cur_region);
			if (++lcnt > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				DELETE_BAD_BACKUP(fab);
				return FALSE;
			}
			if (lcnt & 0xF)
				wcs_sleep(lcnt);
			else
			{	/* Force recovery every few retries - this should not be happening */
				if (FALSE == shmpool_lock_hdr(gv_cur_region))
				{
					assert(FALSE);
					gtm_putmsg(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					DELETE_BAD_BACKUP(fab);
					return FALSE;
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}

		/* --- Verify that no errors from M processes during the backup --- */
		if (0 != cs_addrs->shmpool_buffer->failed)
		{
			util_out_print("Process !XL encountered the following error.", TRUE,
				       cs_addrs->shmpool_buffer->failed);
			if (0 != cs_addrs->shmpool_buffer->backup_errno)
				gtm_putmsg(VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
			DELETE_BAD_BACKUP(fab);
			return FALSE;
		}

		/* --- Open the temporary file (identical to mubinccpy.c) --- */
		temp_fab                = cc$rms_fab;
		temp_fab.fab$b_fac      = FAB$M_GET;
		temp_fab.fab$l_fna      = list->backup_tempfile;
		temp_fab.fab$b_fns      = strlen(list->backup_tempfile); /* double check here */
		temp_rab                = cc$rms_rab;
		temp_rab.rab$l_fab      = &temp_fab;

		for (lcnt = 1;  MAX_OPEN_RETRY >= lcnt;  lcnt++)
		{
			if (RMS$_FLK != (status = sys$open(&temp_fab, NULL, NULL)))
				break;
			wcs_sleep(lcnt);
		}
		if ((RMS$_NORMAL != status) || (RMS$_NORMAL != (status = sys$connect(&temp_rab))))
		{
			gtm_putmsg(VARLSTCNT(1) status);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			DELETE_BAD_BACKUP(fab);
			return FALSE;
		}

		/* --- read and write every record in the temporary file (different from mubinccpy.c) --- */
		sblkh_p = (shmpool_blk_hdr_ptr_t)mubbuf;
		inbuf = (sm_uc_ptr_t)(sblkh_p + 1);
		while (TRUE)
		{	/* Due to RMS restrictions we may have to do more than one read to pull in entire record/blk */
			read_size = SIZEOF(*sblkh_p) + header->blk_size;
			read_ptr = mubbuf;
			while (read_size)
			{
				read_len = MIN(MAX_RMS_RECORDSIZE, read_size);
				temp_rab.rab$w_usz = read_len;
				temp_rab.rab$l_ubf = read_ptr;
				status = sys$get(&temp_rab);
				if (RMS$_NORMAL != status)
				{
					if (RMS$_EOF == status)
						break;
					else
					{
						gtm_putmsg(VARLSTCNT(1) status);
						util_out_print("WARNING:  DB file !AD backup aborted.", TRUE,
							       fcb->fab$b_fns, fcb->fab$l_fna);
						DELETE_BAD_BACKUP(fab);
						return FALSE;
					}
				}
				read_ptr += read_len;
				read_size -= read_len;
			}
			if (RMS$_EOF == status)
				break;
			/* Update block in database backup if it exists */
			blk_num = sblkh_p->blkid;
			if (header->trans_hist.total_blks <= blk_num)
				/* Ignore block outside of db range at time of backup initiation */
				continue;
			if (debug_mupip)
				util_out_print("MUPIP INFO:	Restoring block 0x!XL from temporary file.",
					       TRUE, blk_num);
			vbn = header->start_vbn + blk_num * (header->blk_size / DISK_BLOCK_SIZE);
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
			if (SS$_NORMAL != (status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &wt_iosb[0], 0, 0,
							     inbuf, size, vbn,0,0,0))
			    || SS$_NORMAL != (status = wt_iosb[0]))
			{
				gtm_putmsg(VARLSTCNT(1) status);
				util_out_print("ERROR: Failed writing data to backup file !AD.", TRUE,
					       fab.fab$b_fns, fab.fab$l_fna);
				DELETE_BAD_BACKUP(fab);
				return FALSE;
			}
			if (wt_iosb[1] != size)
			{
				util_out_print("ERROR: !UL bytes, instead of !UL bytes, were written to !AD.", TRUE,
					       wt_iosb[1], size, fab.fab$b_fns, fab.fab$l_fna);
				DELETE_BAD_BACKUP(fab);
				return FALSE;
			}
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
				util_out_print("WARNING:  DB file !AD backup aborted.", TRUE,
					       fcb->fab$b_fns, fcb->fab$l_fna);
				DELETE_BAD_BACKUP(fab);
				return FALSE;
			}
		}

		/* --- close the temporary file (identical to mubinccpy.c) --- */
		if (RMS$_NORMAL != (status = sys$close(&temp_fab)))
		{
			gtm_putmsg(VARLSTCNT(1) status);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			DELETE_BAD_BACKUP(fab);
			return FALSE;
		}
	} /* if (online) */

	/* -------------------------------- write header ----------------------------------------- */
	size = ROUND_UP(SIZEOF_FILE_HDR(header), DISK_BLOCK_SIZE);
	assert(size <= 64 * 1024);	/* Max we can write testing "short" iosb fields */
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &wt_iosb[0], 0, 0, header, size, 1, 0, 0, 0);
	if (SS$_NORMAL != status || SS$_NORMAL != (status = wt_iosb[0]))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("ERROR: Failed writing database header to backup file !AD.", TRUE,
			       fab.fab$b_fns, fab.fab$l_fna);
		DELETE_BAD_BACKUP(fab);
		return FALSE;
	}
	if (wt_iosb[1] != size)
	{
		util_out_print("ERROR: !UL bytes, instead of !UL bytes, were written to !AD.", TRUE,
			       wt_iosb[1], size, fab.fab$b_fns, fab.fab$l_fna);
		DELETE_BAD_BACKUP(fab);
		return FALSE;
	}
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
		util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
		DELETE_BAD_BACKUP(fab);
		return FALSE;
	}

	/* --------------- close the temp copy of the backup file ---------------------------------------------- */
	if (SS$_NORMAL != (status = sys$dassgn(fab.fab$l_stv)))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("ERROR: System Service SYS$DASSGN() failed.", TRUE);
		return FALSE;
	}

	/* ---------- if file has extended since backup started, truncate it ------------------------------------ */
	if (online && (filesize_tobe != filesize_curr))
	{
		/* truncate it */
		assert(filesize_tobe < filesize_curr);
		if (0 != truncate(tempfilename, (off_t)(filesize_tobe - 1) * DISK_BLOCK_SIZE))
		{
			errptr = (char *)strerror(errno);
			errlen = strlen(errptr);
			gtm_putmsg(VARLSTCNT(6) ERR_TRUNCATEFAIL, 4, tempfilelen, tempfilename,
				   filesize_curr, filesize_tobe - 1);
			gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, errlen, errptr);
			return FALSE;
		}
	}

	/* ======================= rename the tempfilename to the real backup filename ========================== */

	/* --- if we don't have delete permission on the temporary file, give it --- */
	old_perm = ((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->xabpro->xab$w_pro;
	if (0x0080 & old_perm)
	{
		if (FALSE == setfileprot(tempfilename, tempfilelen,
					 (old_perm & (~((XAB$M_NODEL << XAB$V_SYS) | (XAB$M_NODEL << XAB$V_OWN))))))
		{
			util_out_print("Failed to set protection mask of !AD, to 0x!4XW", TRUE, tempfilelen, tempfilename,
				       (~((XAB$M_NODEL << XAB$V_SYS) | (XAB$M_NODEL << XAB$V_OWN))) & old_perm);
			return FALSE;
		}
	}

	/* --- construct and issue the command to rename the temporary file to backup file --- */
	MEMCPY_LIT(command_buff, rename);
	command_len = SIZEOF(rename) - 1;
	memcpy(&command_buff[command_len], tempfilename, tempfilelen);
	command_len += tempfilelen;
	command_buff[command_len++] = ' ';
	memcpy(&command_buff[command_len], file->addr, file->len);
	command_len += file->len;
	command_buff[command_len] = '\0';
	command.dsc$w_length = command_len;

	if (debug_mupip)
		util_out_print("MUPIP INFO:	!AD", TRUE, command_len, command_buff);
	if ((SS$_NORMAL != (status = lib$spawn(&command, 0, &nl, 0, 0, 0, &backup_status, 0, 0, 0, 0, 0, 0)))
	    || (!((status = backup_status) & 1)))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("Command:        !AD not executed successfully.", TRUE, command_len, command_buff);
		return FALSE;
	}

	/* --- if the original database doesn't have delete permission, neither should the backup copy --- */
	if (0x0080 & old_perm)
	{
		if (FALSE == setfileprot(file->addr, file->len, old_perm))
		{
			util_out_print("Failed to set protection mask of !AD, to 0x!4XW", TRUE,
				       file->addr, file->len, old_perm);
			return FALSE;
		}
	}

	/* =============================== Output Information =================================================== */
	util_out_print("DB file !AD backed up in file !AD", TRUE,
		       fcb->fab$b_fns, fcb->fab$l_fna, file->len, file->addr);
	util_out_print("Transactions up to 0x!16@XQ are backed up.", TRUE, &header->trans_hist.curr_tn);
	cs_addrs->hdr->last_com_backup = header->trans_hist.curr_tn;
	cs_addrs->hdr->last_com_bkup_last_blk = header->trans_hist.total_blks;
	if (record)
	{
		cs_addrs->hdr->last_rec_backup = header->trans_hist.curr_tn;
		cs_addrs->hdr->last_rec_bkup_last_blk = header->trans_hist.total_blks;
	}
	file_backed_up = TRUE;

	return TRUE;
}
