/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "gtm_stdio.h"

#include <rms.h>
#include <ssdef.h>
#include <iodef.h>
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "stringpool.h"
#include "muextr.h"
#include "murest.h"
#include "mupipbckup.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "sleep_cnt.h"
#include "util.h"
#include "cli.h"
#include "op.h"
#include "io.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gds_blk_upgrade.h"
#include "shmpool.h"
#include "iormdef.h"
#include "iosp.h"
#include "min_max.h"
#include "gvcst_lbm_check.h"
#include "wcs_phase2_commit_wait.h"

#define	MAX_TCP_SEND_RETRY	5

GBLREF 	bool 			record;
GBLREF 	bool			online;
GBLREF 	bool			incremental;
GBLREF 	bool 			file_backed_up;
GBLREF	bool			error_mupip;
GBLREF 	bool 			mu_ctrly_occurred;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF  bool                    mubtomag;
GBLREF 	int4 			mubmaxblk;
GBLREF  spdesc                  stringpool;
GBLREF  gd_region               *gv_cur_region;
GBLREF 	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF 	unsigned char 		*mubbuf;
GBLREF	tcp_library_struct      tcp_routines;
GBLREF  int4                    backup_write_errno;
GBLREF	int4			backup_close_errno;
GBLREF	boolean_t		debug_mupip;
GBLREF	uint4			process_id;

#define COMMON_CLOSE(A)		{					\
					(*common_close)(A);		\
					if (0 != backup_close_errno)	\
						return FALSE;		\
				}

#define COMMON_WRITE(A, B, C)   {                                       \
					(*common_write)(A, B, C);       \
					if (0 != backup_write_errno)    \
						return FALSE;           \
				}

LITREF	mval		mu_bin_datefmt;

/* forward declarations */
static void file_write(char *temp, char *buf, int nbytes);
static void file_close(char *temp);
static void tcp_write(char *temp, char *buf, int nbytes);
static void tcp_close(char *temp);

bool mubinccpy(backup_reg_list *list)
{
	static readonly mval	null_str = {MV_STR, 0, 0 , 0 , 0, 0};

	int			backup_socket;
	int4                    size, size1, bsize, bm_num, hint, lmsize, save_blks, rsize, match, timeout, outsize;
	uint4                   status, total_blks, bplmap, gds_ratio, blks_per_buff, counter, i, lcnt, read_size;
	uchar_ptr_t		bm_blk_buff, ptr1, ptr1_top, ptr, ptr_top;
	char_ptr_t		outptr, data_ptr;
	unsigned short		rd_iosb[4], port;
	enum db_acc_method	access;
	blk_hdr			*bp, *bptr;
	struct FAB		*fcb, temp_fab, mubincfab;
	struct RAB		temp_rab, mubincrab;
	inc_header		*outbuf;
	mval			val;
	mstr                    *file;
	sgmnt_data_ptr_t        header;
	char			*common, addr[SA_MAXLEN + 1];
	void			(*common_write)();
	void			(*common_close)();
	muinc_blk_hdr_ptr_t	sblkh_p;
	trans_num		blk_tn;
	block_id		blk_num_base, blk_num;
	boolean_t		is_bitmap_blk, backup_this_blk;
	enum db_ver		dummy_odbv;
	int4			blk_bsiz;

	error_def(ERR_BCKUPBUFLUSH);
	error_def(ERR_COMMITWAITSTUCK);
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);

	assert(list->reg == gv_cur_region);
	assert(incremental);
	/* Make sure inc_header  can be same size on all platforms. Some platforms pad 8 byte aligned structures
	   that end on a 4 byte boundary and some do not. It is critical that this structure is the same size on
	   all platforms as it is sent across TCP connections when doing TCP backup.
	*/
	assert(0 == (SIZEOF(inc_header) % 8));

	/* ================= Initialization and some checks ======================== */

	header  =       list->backup_hdr;
	file    =       &(list->backup_file);

	if (!mubtomag)
		mubmaxblk = BACKUP_TEMPFILE_BUFF_SIZE;
	fcb = ((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fab;
	if (list->tn >= header->trans_hist.curr_tn)
	{
		util_out_print("!/TRANSACTION number is greater than or equal to current transaction,", TRUE);
		util_out_print("No blocks backed up from database !AD", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
		return TRUE;
	}

	/* =========== open backup destination and define common_write ================= */
	backup_write_errno = 0;
	backup_close_errno = 0;
	switch(list->backup_to)
	{
		case backup_to_file:
			/* open the file and define the common_write function */
			mubincfab = cc$rms_fab;
			mubincfab.fab$b_fac = FAB$M_PUT;
			mubincfab.fab$l_fop = FAB$M_CBT | FAB$M_MXV | FAB$M_TEF | FAB$M_POS & (~FAB$M_RWC) & (~FAB$M_RWO);
			mubincfab.fab$l_fna = file->addr;
			mubincfab.fab$b_fns = file->len;
			mubincfab.fab$l_alq = cs_addrs->hdr->start_vbn +
				STARTING_BLOCKS * cs_addrs->hdr->blk_size / DISK_BLOCK_SIZE;
			mubincfab.fab$w_mrs = mubmaxblk;
			mubincfab.fab$w_deq = EXTEND_SIZE;
			switch (status = sys$create(&mubincfab))
			{
				case RMS$_NORMAL:
				case RMS$_CREATED:
				case RMS$_SUPERSEDE:
				case RMS$_FILEPURGED:
					break;
				default:
					gtm_putmsg(status, 0, mubincfab.fab$l_stv);
					util_out_print("Error: Cannot create backup file !AD.",
						       TRUE, mubincfab.fab$b_fns, mubincfab.fab$l_fna);
					return FALSE;
			}

			mubincrab = cc$rms_rab;
			mubincrab.rab$l_fab = &mubincfab;
			mubincrab.rab$l_rop = RAB$M_WBH;
			if (RMS$_NORMAL != (status = sys$connect(&mubincrab)))
			{
				gtm_putmsg(status, 0, mubincrab.rab$l_stv);
				util_out_print("Error: Cannot connect to backup file !AD.",
					       TRUE, mubincfab.fab$b_fns, mubincfab.fab$l_fna);
				mubincfab.fab$l_fop |= FAB$M_DLT;
				sys$close(&mubincfab);
				return FALSE;
			}
			common = (char *)(&mubincrab);
			common_write = file_write;
			common_close = file_close;
			break;
		case backup_to_exec:
			util_out_print("Error: Backup to pipe is yet to be implemented.", TRUE);
			util_out_print("Error: Your request to backup database !AD to !AD is currently not valid.", TRUE,
				       fcb->fab$b_fns, fcb->fab$l_fna, file->len, file->addr);
			return FALSE;
		case backup_to_tcp:
			iotcp_fillroutine();
			/* parse it first */
			switch (match = SSCANF(file->addr, "%[^:]:%hu", addr, &port))
			{
				case 1 :
					port = DEFAULT_BKRS_PORT;
				case 2 :
					break;
				default :
					util_out_print("ERROR: A hostname has to be specified to backup through a TCP connection.",
						       TRUE);
					return FALSE;
			}
			if ((0 == cli_get_int("NETTIMEOUT", &timeout)) || (0 > timeout))
				timeout = DEFAULT_BKRS_TIMEOUT;
			if (0 > (backup_socket = tcp_open(addr, port, timeout, FALSE)))
			{
				util_out_print("ERROR: Cannot open tcp connection due to the above error.", TRUE);
				return FALSE;
			}
			common_write = tcp_write;
			common_close = tcp_close;
			common = (char *)(&backup_socket);
			break;
		default :
			util_out_print("ERROR: Backup format !UL not supported.", TRUE, list->backup_to);
			util_out_print("Error: Your request to backup database !AD to !AD is not valid.", TRUE,
				       fcb->fab$b_fns, fcb->fab$l_fna, file->len, file->addr);
			return FALSE;
	}

	/* ============================= write inc_header =========================================== */

	outptr = malloc(SIZEOF(inc_header));
	outbuf = (inc_header *)outptr;
	MEMCPY_LIT(&outbuf->label[0], INC_HEADER_LABEL);
	stringpool.free = stringpool.base;
	op_horolog(&val);
	stringpool.free = stringpool.base;
	op_fnzdate(&val, &mu_bin_datefmt, &null_str, &null_str, &val);
	memcpy(&outbuf->date[0], val.str.addr, val.str.len);
	memcpy(&outbuf->reg[0], gv_cur_region->rname, MAX_RN_LEN);
	outbuf->start_tn = list->tn;
	outbuf->end_tn = header->trans_hist.curr_tn;
	outbuf->db_total_blks = header->trans_hist.total_blks;
	outbuf->blk_size = header->blk_size;
	outbuf->blks_to_upgrd = header->blks_to_upgrd;
	COMMON_WRITE(common, outptr, SIZEOF(inc_header));
	free(outptr);

	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		error_mupip = TRUE;
		COMMON_CLOSE(common);
		util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
		return FALSE;
	}

	/* ============================ read/write appropriate blocks =============================== */

	bsize		= header->blk_size;
	gds_ratio	= bsize / DISK_BLOCK_SIZE;
	blks_per_buff	= BACKUP_READ_SIZE / bsize;
	read_size	= blks_per_buff * bsize;
	outsize		= SIZEOF(muinc_blk_hdr) + bsize;
	outptr		= (char_ptr_t)malloc(MAX(outsize, mubmaxblk));
	sblkh_p		= (muinc_blk_hdr_ptr_t)outptr;
	data_ptr	= (char_ptr_t)(sblkh_p + 1);
	bp		= (blk_hdr_ptr_t)mubbuf;
	bm_blk_buff	= (uchar_ptr_t)malloc(SIZEOF(blk_hdr) + (BLKS_PER_LMAP * BML_BITS_PER_BLK / BITS_PER_UCHAR));
	mubincrab.rab$l_rbf = outptr;
	save_blks	= 0;
	access = header->acc_meth;
	memset(sblkh_p, 0, SIZEOF(*sblkh_p));

	if (access == dba_bg)
		bp = mubbuf;
	else
	{
		ptr = cs_addrs->db_addrs[0] + (cs_addrs->hdr->start_vbn - 1) * DISK_BLOCK_SIZE;
		ptr_top = cs_addrs->db_addrs[1] + 1;
	}

	sblkh_p->use.bkup.ondsk_blkver = GDSNOVER;
	for (blk_num_base = 0; blk_num_base < header->trans_hist.total_blks; blk_num_base += blks_per_buff)
	{
		if (online && (0 != cs_addrs->shmpool_buffer->failed))
			break;
		if (header->trans_hist.total_blks - blk_num_base < blks_per_buff)
		{
			blks_per_buff = header->trans_hist.total_blks - blk_num_base;
			read_size = blks_per_buff * bsize;
		}

		if (access == dba_bg)
		{
			if ((SS$_NORMAL != (status = sys$qiow(EFN$C_ENF, fcb->fab$l_stv, IO$_READVBLK, &rd_iosb, 0, 0, bp,
							      read_size, cs_addrs->hdr->start_vbn + (gds_ratio * blk_num_base),
							      0, 0, 0)))
			    || (SS$_NORMAL != (status = rd_iosb[0])))
			{
				gtm_putmsg(VARLSTCNT(1) status);
				util_out_print("Error reading data from database !AD.", TRUE,
					       fcb->fab$b_fns, fcb->fab$l_fna);
				free(outptr);
				free(bm_blk_buff);
				error_mupip = TRUE;
				COMMON_CLOSE(common);
				return FALSE;
			}
		} else
		{
			assert(dba_mm == access);
			bp = ptr + blk_num_base * bsize;
		}

		bptr = (blk_hdr *)bp;
		/* The blocks we back up will be whatever version they are. There is no implicit conversion in this
		   part of the backup/restore. Since we aren't even looking at the blocks (and indeed some of these blocks
		   could potentially contain unintialized garbage data), we set the block version to GDSNOVER to signal
		   that the block version is unknown. The above applies to "regular" blocks but not to bitmap blocks which
		   we know are initialized. Because we have to read the bitmap blocks, they will be converted as necessary.
		*/
		for (i = 0;
		     i < blks_per_buff && ((blk_num_base + i) < header->trans_hist.total_blks);
		     i++, bptr = (blk_hdr *)((char *)bptr + bsize))
		{
			blk_num = blk_num_base + i;
			if (mu_ctrly_occurred  ||  mu_ctrlc_occurred)
			{
				free(outptr);
				free(bm_blk_buff);
				error_mupip = TRUE;
				COMMON_CLOSE(common);
				util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
				return FALSE;
			}
			/* Before we check if this block needs backing up, check if this is a new bitmap block or not. If it is,
			   we can fall through and back it up as normal. But if this is NOT a bitmap block, use the
			   existing bitmap to determine if this block has ever been allocated or not. If not, we don't want to
			   even look at this block. It could be uninitialized which will just make things run slower if we
			   go to read it and back it up.
			*/
			if (0 != ((BLKS_PER_LMAP - 1) & blk_num))
			{	/* Not a local bitmap block */
				if (!gvcst_blk_ever_allocated(bm_blk_buff + SIZEOF(blk_hdr),
							      ((blk_num * BML_BITS_PER_BLK)
							       % (BLKS_PER_LMAP * BML_BITS_PER_BLK))))
					continue;		/* Bypass never-set blocks to avoid conversion problems */
				is_bitmap_blk = FALSE;
				if (SIZEOF(v15_blk_hdr) <= (blk_bsiz = ((v15_blk_hdr_ptr_t)bptr)->bsiz))
				{	/* We have either a V4 block or uninitialized garbage */
					if (blk_bsiz > bsize)
						/* This is not a valid V4 block so ignore it */
						continue;
					blk_tn = ((v15_blk_hdr_ptr_t)bptr)->tn;
				} else
				{	/* Assume V5 block */
					if ((blk_bsiz = bptr->bsiz) > bsize)
						/* Not a valid V5 block either */
						continue;
					blk_tn = bptr->tn;
				}
			} else
			{	/* This is a bitmap block so save it into our bitmap block buffer. It is used as the
				   basis of whether or not we have to process a given block or not. We process allocated and
				   recycled blocks leaving free (never used) blocks alone as they have no data worth saving.
				   But after saving it, upgrade it to the current format if necessary.
				*/
				is_bitmap_blk = TRUE;
				memcpy(bm_blk_buff, bptr, BM_SIZE(header->bplmap));
				if (SIZEOF(v15_blk_hdr) <= ((v15_blk_hdr_ptr_t)bm_blk_buff)->bsiz)
				{	/* This is a V4 format block -- needs upgrading */
					status = gds_blk_upgrade(bm_blk_buff, bm_blk_buff, bsize, &dummy_odbv);
					if (SS_NORMAL != status)
					{
						free(outptr);
						free(bm_blk_buff);
						error_mupip = TRUE;
						COMMON_CLOSE(common);
						util_out_print("Error: Block 0x!XL is too large for automatic upgrade", TRUE,
							       sblkh_p->blkid);
						return FALSE;
					}
				}
				assert(BM_SIZE(header->bplmap) == ((blk_hdr_ptr_t)bm_blk_buff)->bsiz);
				assert(LCL_MAP_LEVL == ((blk_hdr_ptr_t)bm_blk_buff)->levl);
				assert(gvcst_blk_is_allocated(bm_blk_buff + SIZEOF(blk_hdr),
							      ((blk_num * BML_BITS_PER_BLK)
							       % (BLKS_PER_LMAP * BML_BITS_PER_BLK))));
				blk_bsiz = BM_SIZE(header->bplmap);
				blk_tn = ((blk_hdr_ptr_t)bm_blk_buff)->tn;
			}
			/* The conditions for backing up a block or ignoring it (in order of evaluation):

			   1) If blk is larger than size of db at time backup was initiated, we ignore the block.
			   2) Always backup blocks 0, 1, and 2 as these are the only blocks that can contain data
			      and still have a transaction number of 0.
			   3) For bitmap blocks, if blks_to_upgrd != 0 and the TN is 0 and the block number >=
			      last_blk_at_last_bkup, then backup the block. This way we get the correct version of
			      the bitmap block in the restore (otherwise have no clue what version to create them in
			      as bitmaps are created with a TN of 0 when before image journaling is enabled).
			   4) If the block TN is below our TN threshold, ignore the block.
			   5) Else if none of the above conditions, backup the block.
			*/
			if (online && (header->trans_hist.curr_tn <= blk_tn))
				backup_this_blk = FALSE;
			else if (3 > blk_num || (is_bitmap_blk && 0 != header->blks_to_upgrd && (trans_num)0 == blk_tn
						 && blk_num >= list->last_blk_at_last_bkup))
				backup_this_blk = TRUE;
			else if ((blk_tn < list->tn))
				backup_this_blk = FALSE;
			else
				backup_this_blk = TRUE;
			if (!backup_this_blk)
			{
				if (online)
					cs_addrs->nl->nbb = blk_num;
				continue; /* not applicable */
			}
			sblkh_p->blkid = blk_num;
			memcpy(data_ptr, bptr, blk_bsiz);
			sblkh_p->valid_data = TRUE;	/* Validation marker */
			COMMON_WRITE(common, outptr, outsize);
			if (online)
			{
				if (0 != cs_addrs->shmpool_buffer->failed)
					break;
				cs_addrs->nl->nbb = blk_num;
			}
			save_blks++;
		}
	}

	/* ============================= write saved information for online backup ========================== */

	if (online && (0 == cs_addrs->shmpool_buffer->failed))
	{
		/* -------- make sure everyone involved finishes -------- */
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
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
				free(outptr);
				free(bm_blk_buff);
				error_mupip = TRUE;
				COMMON_CLOSE(common);
				return FALSE;
			}
		}
		if (debug_mupip)
		{
			util_out_print("MUPIP INFO:   Current Transaction # at end of backup is 0x!16@XQ", TRUE,
				&cs_data->trans_hist.curr_tn);
		}
		rel_crit(gv_cur_region);
		counter = 0;
		while (0 != cs_addrs->shmpool_buffer->backup_cnt)
		{
			if (0 != cs_addrs->shmpool_buffer->failed)
			{
				util_out_print("Process !UL encountered the following error.", TRUE,
					       cs_addrs->shmpool_buffer->failed);
				if (0 != cs_addrs->shmpool_buffer->backup_errno)
					gtm_putmsg(VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
				free(outptr);
				free(bm_blk_buff);
				error_mupip = TRUE;
				COMMON_CLOSE(common);
				return FALSE;
			}
			backup_buffer_flush(gv_cur_region);
			if (++counter > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				free(outptr);
				free(bm_blk_buff);
				error_mupip = TRUE;
				COMMON_CLOSE(common);
				return FALSE;
			}
			if (counter & 0xF)
				wcs_sleep(counter);
			else
			{	/* Force shmpool recovery to see if it can find the lost blocks */
				if (!shmpool_lock_hdr(gv_cur_region))
				{
					gtm_putmsg(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					free(outptr);
					free(bm_blk_buff);
					error_mupip = TRUE;
					COMMON_CLOSE(common);
					assert(FALSE);
					return FALSE;;
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}

		/* -------- Open the temporary file -------- */
		temp_fab = cc$rms_fab;
		temp_fab.fab$b_fac = FAB$M_GET;
		temp_fab.fab$l_fna = list->backup_tempfile;
		temp_fab.fab$b_fns = strlen(list->backup_tempfile);
		temp_rab = cc$rms_rab;
		temp_rab.rab$l_fab = &temp_fab;

		for (lcnt = 1;  MAX_OPEN_RETRY >= lcnt;  lcnt++)
		{
			if (RMS$_FLK != (status = sys$open(&temp_fab, NULL, NULL)))
				break;
			wcs_sleep(lcnt);
		}

		if (RMS$_NORMAL != status)
		{
			gtm_putmsg(status, 0, temp_fab.fab$l_stv);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			free(outptr);
			free(bm_blk_buff);
			error_mupip = TRUE;
			COMMON_CLOSE(common);
			return FALSE;
		}

		if (RMS$_NORMAL != (status = sys$connect(&temp_rab)))
		{
			gtm_putmsg(status, 0, temp_rab.rab$l_stv);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			free(outptr);
			free(bm_blk_buff);
			error_mupip = TRUE;
			COMMON_CLOSE(common);
			return FALSE;
		}

		/* -------- read and write every record in the temporary file -------- */
		while (1)
		{
			temp_rab.rab$w_usz = outsize;
			temp_rab.rab$l_ubf = outptr;
			status = sys$get(&temp_rab);
			if (RMS$_NORMAL != status)
			{
				if (RMS$_EOF == status)
					status = RMS$_NORMAL;
				break;
			}
			assert(outsize == temp_rab.rab$w_rsz);
			/* Still validly sized blk? */
			assert((outsize - SIZEOF(shmpool_blk_hdr)) >= ((blk_hdr_ptr_t)(outptr + SIZEOF(shmpool_blk_hdr)))->bsiz);
			COMMON_WRITE(common, outptr, temp_rab.rab$w_rsz);
		}

		if (RMS$_NORMAL != status)
		{
			gtm_putmsg(status, 0, temp_rab.rab$l_stv);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			free(outptr);
			free(bm_blk_buff);
			error_mupip = TRUE;
			COMMON_CLOSE(common);
			return FALSE;
		}

		/* ---------------- Close the temporary file ----------------------- */
		if (RMS$_NORMAL != (status = sys$close(&temp_fab)))
		{
			gtm_putmsg(status, 0, temp_fab.fab$l_stv);
			util_out_print("WARNING:  DB file !AD backup aborted.", TRUE, fcb->fab$b_fns, fcb->fab$l_fna);
			free(outptr);
			free(bm_blk_buff);
			error_mupip = TRUE;
			COMMON_CLOSE(common);
			return FALSE;
		}
	}

	/* ============================= write end_msg and fileheader ======================================= */

	if ((!online) || (0 == cs_addrs->shmpool_buffer->failed))
	{
		MEMCPY_LIT(outptr, END_MSG);
		/* Although the write only need be of length SIZEOF(END_MSG) - 1 for file IO, if the write is going
		   to TCP we have to write all these records with common length so just write the "regular" sized
		   buffer. The extra garbage left over from the last write will be ignored as we key only on the
		   this end text.
		*/
		COMMON_WRITE(common, outptr, outsize);

		ptr1 = header;
		size1 = ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
		ptr1_top = ptr1 + size1;
		for (;ptr1 < ptr1_top ; ptr1 += size1)
		{
			if ((size1 = ptr1_top - ptr1) > mubmaxblk)
				size1 = (mubmaxblk / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE;
			COMMON_WRITE(common, ptr1, size1);
		}

		MEMCPY_LIT(outptr, HDR_MSG);
		COMMON_WRITE(common, outptr, SIZEOF(HDR_MSG));
		ptr1 = MM_ADDR(header);
		size1 = ROUND_UP(MASTER_MAP_SIZE(header), DISK_BLOCK_SIZE);
		ptr1_top = ptr1 + size1;
		for (;ptr1 < ptr1_top ; ptr1 += size1)
		{
			if ((size1 = ptr1_top - ptr1) > mubmaxblk)
				size1 = (mubmaxblk / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE;
			COMMON_WRITE(common, ptr1, size1);
		}

		MEMCPY_LIT(outptr, MAP_MSG);
		COMMON_WRITE(common, outptr, SIZEOF(MAP_MSG));
	}


	/* ================== close backup destination, output and return ================================== */

	if (online && (0 != cs_addrs->shmpool_buffer->failed))
	{
		util_out_print("Process !UL encountered the following error.", TRUE,
			       cs_addrs->shmpool_buffer->failed);
		if (0 != cs_addrs->shmpool_buffer->backup_errno)
			gtm_putmsg(VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
		free(outptr);
		free(bm_blk_buff);
		error_mupip = TRUE;
		COMMON_CLOSE(common);
		return FALSE;
	}

	COMMON_CLOSE(common);
	free(outptr);
	free(bm_blk_buff);

	util_out_print("DB file !AD incrementally backed up in !AD", TRUE,
		       fcb->fab$b_fns, fcb->fab$l_fna, file->len, file->addr);
	util_out_print("!UL blocks saved.", TRUE, save_blks);
	util_out_print("Transactions from 0x!16@XQ to 0x!16@XQ are backed up.", TRUE,
		       &cs_addrs->shmpool_buffer->inc_backup_tn, &header->trans_hist.curr_tn);
	cs_addrs->hdr->last_inc_backup = header->trans_hist.curr_tn;
	if (record)
		cs_addrs->hdr->last_rec_backup = header->trans_hist.curr_tn;
	file_backed_up = TRUE;
	return TRUE;
}

static void tcp_write(char *temp, char *buf, int nbytes)
{
	int 	socket, nwritten, iostatus, send_retry;

	socket = *(int *)(temp);

	nwritten = 0;
	send_retry = MAX_TCP_SEND_RETRY;

	do
	{
		if (-1 != (iostatus = tcp_routines.aa_send(socket, buf + nwritten, nbytes - nwritten, 0)))
		{
			nwritten += iostatus;
			if (nwritten == nbytes)
				break;
		} else if (EINTR != errno)
			break;
	} while (0 < send_retry--);

	if ((nwritten != nbytes) && (-1 == iostatus))
	{
		gtm_putmsg(VARLSTCNT(1) errno);
		tcp_routines.aa_close(socket);
		backup_write_errno = errno;
	}

	return;
}

static void tcp_close(char *temp)
{
	int 	socket;

	socket = *((int *)(temp));
	tcp_routines.aa_close(socket);

	return;
}

static void file_write(char *temp, char *buf, int nbytes)
{
	uint4		status;
	struct RAB	*rab;
	void		gtm_putmsg();

	assert(nbytes > 4);
	rab = (struct RAB *)(temp);
	rab->rab$w_rsz = nbytes;
	rab->rab$l_rbf = buf;
	if (RMS$_NORMAL != (status = sys$put(rab)))
	{
		backup_write_errno = status;
		gtm_putmsg(status, 0, rab->rab$l_stv);
		(rab->rab$l_fab)->fab$l_fop |= FAB$M_DLT;
		sys$close(rab->rab$l_fab);
	}

	return;
}

static void file_close(char *temp)
{
	uint4		status;
	struct RAB	*rab;
	void 		gtm_putmsg();

	rab = (struct RAB *)(temp);
	if (error_mupip)
		rab->rab$l_fab->fab$l_fop |= FAB$M_DLT;
	status = sys$close(rab->rab$l_fab);
	if (status != RMS$_NORMAL)
	{
		backup_close_errno = status;
		gtm_putmsg(status);
		util_out_print("FATAL ERROR: System Service failure.",TRUE);
	}

	return;
}
