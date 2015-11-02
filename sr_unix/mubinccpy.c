/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mubinccpy.c
 *
 * -- online incremental	online && incremental
 * -- incremental		!online && incremental
 * -------- requires		cs_addrs, cs_data and gv_cur_region be current.
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_socket.h"
#include "gtm_inet.h"

#include <sys/wait.h>
#include <errno.h>

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
#include "iob.h"
#include "error.h"
#include "mupipbckup.h"
#include "gtmio.h"
#include "gtm_pipe.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "iotimer.h"
#include "eintr_wrappers.h"
#include "sleep_cnt.h"
#include "util.h"
#include "cli.h"
#include "op.h"
#include "io.h"
#include "is_proc_alive.h"
#include "is_raw_dev.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gds_blk_upgrade.h"
#include "iosp.h"
#include "shmpool.h"
#include "min_max.h"
#include "gvcst_lbm_check.h"
#include "wcs_phase2_commit_wait.h"
#include "gtm_permissions.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	bool			record;
GBLREF	bool			online;
GBLREF	bool			incremental;
GBLREF	bool			file_backed_up;
GBLREF	bool			mubtomag;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	bool			mu_ctrlc_occurred;
GBLREF	int4			mubmaxblk;
GBLREF	spdesc			stringpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uchar_ptr_t		mubbuf;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	uint4			pipe_child;
GBLREF	uint4			process_id;
GBLREF	boolean_t		debug_mupip;
GBLREF	int4			backup_write_errno;

LITREF	mval			literal_null;

error_def(ERR_BCKUPBUFLUSH);
error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_DBCCERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_ERRCALL);

#ifdef DEBUG_INCBKUP
#  define DEBUG_INCBKUP_ONLY(X) X
#else
#  define DEBUG_INCBKUP_ONLY(X)
#endif

#define	COMMON_WRITE(A, B, C)	{								\
					(*common_write)(A, B, (int)C);				\
					if (0 != backup_write_errno)				\
						return FALSE;					\
					DEBUG_INCBKUP_ONLY(backup_write_offset += (int)C;)	\
				}

#define CLEANUP_AND_RETURN_FALSE {						\
					if (backup_to_file == list->backup_to)	\
					{					\
						if (NULL != backup) 		\
							iob_close(backup);	\
						if (!debug_mupip)		\
							UNLINK(file->addr);	\
					}					\
					return FALSE;				\
				}

#define	MAX_FILENAME_LENGTH	256

static	char			incbackupfile[MAX_FILENAME_LENGTH];
static	BFILE			*backup;

void exec_write(BFILE *bf, char *buf, int nbytes);
void tcp_write(BFILE *bf, char *buf, int nbytes);

error_def(ERR_BCKUPBUFLUSH);
error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_DBCCERR);
error_def(ERR_ERRCALL);
error_def(ERR_IOEOF);
error_def(ERR_PERMGENFAIL);

bool	mubinccpy (backup_reg_list *list)
{
	mstr			*file;
	uchar_ptr_t		bm_blk_buff, ptr1, ptr1_top;
	char_ptr_t		outptr, data_ptr;
	char 			*c, addr[SA_MAXLEN + 1];
	sgmnt_data_ptr_t	header;
	uint4			total_blks, bplmap, gds_ratio, save_blks;
	int4			status;
	int4			size1, bsize, bm_num, hint, lmsize, rsize, timeout, outsize,
				blks_per_buff, counter, i, write_size, read_size, match;
	size_t			copysize;
	off_t			copied;
	int			db_fd, exec_fd;
	enum db_acc_method	access;
	blk_hdr_ptr_t		bp, bptr;
	inc_header		*outbuf;
	mval			val;
	unsigned short		port;
	void			(*common_write)(BFILE *, char *, int);
	muinc_blk_hdr_ptr_t	sblkh_p;
	trans_num		blk_tn;
	int4			blk_bsiz;
	block_id		blk_num_base, blk_num;
	boolean_t		is_bitmap_blk, backup_this_blk;
	enum db_ver		dummy_odbv;
	int			rc;
	int                     fstat_res;
	struct stat		stat_buf;
	int			group_id;
	int			perm;
	struct perm_diag_data	pdd;
	DEBUG_INCBKUP_ONLY(int	blks_this_lmap;)
	DEBUG_INCBKUP_ONLY(gtm_uint64_t backup_write_offset = 0;)

	assert(list->reg == gv_cur_region);
	assert(incremental);
	/* Make sure inc_header  can be same size on all platforms. Some platforms pad 8 byte aligned structures
	   that end on a 4 byte boundary and some do not. It is critical that this structure is the same size on
	   all platforms as it is sent across TCP connections when doing TCP backup.
	*/
	assert(0 == (SIZEOF(inc_header) % 8));

	/* ================= Initialization and some checks ======================== */
	header	=	list->backup_hdr;
	file	=	&(list->backup_file);

	if (list->tn >= header->trans_hist.curr_tn)
	{
		util_out_print("!/TRANSACTION number is greater than or equal to current transaction,", TRUE);
		util_out_print("no blocks backed up from database !AD", TRUE, DB_LEN_STR(gv_cur_region));
		return TRUE;
	}
	if (!mubtomag)
		mubmaxblk = (64 * 1024);
	db_fd = ((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fd;

	/* =================== open backup destination ============================= */
	backup_write_errno = 0;
	switch(list->backup_to)
	{
		case backup_to_file:
			common_write = iob_write;
			backup = iob_open_wt(file->addr, DISK_BLOCK_SIZE, BLOCKING_FACTOR);
			if (NULL == backup)
			{
				PERROR("open error: ");
				util_out_print("Error: Cannot create backup file !AD.", TRUE, file->len, file->addr);
				return FALSE;
			}
			if (is_raw_dev(file->addr))
			{
				ESTABLISH_RET(iob_io_error1, FALSE);
			} else
			{
				FSTAT_FILE(db_fd, &stat_buf, fstat_res);
				if (-1 != fstat_res)
					if (gtm_set_group_and_perm(&stat_buf, &group_id, &perm, PERM_FILE, &pdd) < 0)
					{
						send_msg(VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
							ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
							RTS_ERROR_STRING(((unix_db_info *)
								(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
							PERMGENDIAG_ARGS(pdd));
						gtm_putmsg(VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
							ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
							RTS_ERROR_STRING(((unix_db_info *)
								(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
							PERMGENDIAG_ARGS(pdd));
						CLEANUP_AND_RETURN_FALSE;
					}
				/* setup new group and permissions if indicated by the security rules. */
				if ((-1 == fstat_res) || (-1 == FCHMOD(backup->fd, perm))
					|| ((-1 != group_id) && (-1 == fchown(backup->fd, -1, group_id))))
				{
					PERROR("fchmod/fchown error: ");
					util_out_print("ERROR: Cannot access incremental backup file !AD.",
						       TRUE, file->len, file->addr);
					util_out_print("WARNING: Backup file !AD is not valid.", TRUE, file->len, file->addr);
					CLEANUP_AND_RETURN_FALSE;
				}
				memcpy(incbackupfile, file->addr, file->len);
				incbackupfile[file->len] = 0;
				ESTABLISH_RET(iob_io_error2, FALSE);
			}
			break;
		case backup_to_exec:
			pipe_child = 0;
			common_write = exec_write;
			backup = (BFILE *)malloc(SIZEOF(BFILE));
			backup->blksiz = DISK_BLOCK_SIZE;
			backup->remaining = 0;		/* number of zeros to be added in the end, just use this field */
			if (0 > (backup->fd = gtm_pipe(file->addr, output_to_comm)))
			{
				util_out_print("ERROR: Cannot create backup pipe.", TRUE);
				util_out_print("WARNING: backup !AD is not valid.", TRUE, file->len, file->addr);
				return FALSE;
			}
			break;
		case backup_to_tcp:
			common_write = tcp_write;
			iotcp_fillroutine();
			backup = (BFILE *)malloc(SIZEOF(BFILE));
			backup->blksiz = DISK_BLOCK_SIZE;
			backup->remaining = 0; /* number of zeros to be added in the end, just use this field */
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
			assert(SIZEOF(timeout) == SIZEOF(int));
			if ((0 == cli_get_int("NETTIMEOUT", (int4 *)&timeout)) || (0 > timeout))
				timeout = DEFAULT_BKRS_TIMEOUT;
			if (0 > (backup->fd = tcp_open(addr, port, timeout, FALSE)))
			{
				util_out_print("ERROR: Cannot open tcp connection due to the above error.", TRUE);
				util_out_print("WARNING: Backup !AD is not valid.", TRUE, file->len, file->addr);
				return FALSE;
			}
			break;
		default :
			util_out_print("ERROR: Backup format not supported.", TRUE);
			util_out_print("WARNING: Backup not valid.", TRUE);
			return FALSE;
	}

	/* ============================= write inc_header =========================================== */
	outbuf = (inc_header*)malloc(SIZEOF(inc_header));
	if (header->is_encrypted)
		MEMCPY_LIT(&outbuf->label[0], INC_HEADER_LABEL);
	else
		MEMCPY_LIT(&outbuf->label[0], V5_INC_HEADER_LABEL);
	stringpool.free = stringpool.base;
	op_horolog(&val);
	stringpool.free = stringpool.base;
	op_fnzdate(&val, (mval *)&mu_bin_datefmt, (mval *)&literal_null, (mval *)&literal_null, &val);
	memcpy(&outbuf->date[0], val.str.addr, val.str.len);
	memcpy(&outbuf->reg[0], gv_cur_region->rname, MAX_RN_LEN);
	outbuf->start_tn = list->tn;
	outbuf->end_tn = header->trans_hist.curr_tn;
	outbuf->db_total_blks = header->trans_hist.total_blks;
	outbuf->blk_size = header->blk_size;
	outbuf->blks_to_upgrd = header->blks_to_upgrd;
	/* is_encrypted field of incremental header is defined for all platforms.
	 * Hence set the is_encrypted field unconditionally.
	 */
	outbuf->is_encrypted = header->is_encrypted;
	util_out_print("MUPIP backup of database file !AD to !AD", TRUE, DB_LEN_STR(gv_cur_region), file->len, file->addr);
	COMMON_WRITE(backup, (char *)outbuf, SIZEOF(inc_header));
#	ifdef GTM_CRYPT
		if (header->is_encrypted)
			COMMON_WRITE(backup, (char *)header->encryption_hash, GTMCRYPT_HASH_LEN);
#	endif
	free(outbuf);

	if (mu_ctrly_occurred  ||  mu_ctrlc_occurred)
	{
		util_out_print("WARNING:  DB file !AD backup aborted, file !AD not valid", TRUE,
			DB_LEN_STR(gv_cur_region), file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE;
	}

	/* ============================ read/write appropriate blocks =============================== */
	bsize		= header->blk_size;
	gds_ratio	= bsize / DISK_BLOCK_SIZE;
	blks_per_buff	= BACKUP_READ_SIZE / bsize;	/* Worse case holds one block */
	read_size	= blks_per_buff * bsize;
	outsize		= SIZEOF(muinc_blk_hdr) + bsize;
	outptr		= (char_ptr_t)malloc(MAX(outsize, mubmaxblk));
	sblkh_p		= (muinc_blk_hdr_ptr_t)outptr;
	data_ptr	= (char_ptr_t)(sblkh_p + 1);
	bp		= (blk_hdr_ptr_t)mubbuf;
	bm_blk_buff	= (uchar_ptr_t)malloc(SIZEOF(blk_hdr) + (BLKS_PER_LMAP * BML_BITS_PER_BLK / BITS_PER_UCHAR));
	save_blks	= 0;
	memset(sblkh_p, 0, SIZEOF(*sblkh_p));
	sblkh_p->use.bkup.ondsk_blkver = GDSNOVER;

	if (-1 == lseek(db_fd, (off_t)(header->start_vbn - 1) * DISK_BLOCK_SIZE, SEEK_SET))
	{
		PERROR("fseek error: ");
		util_out_print("Error reading from database file !AD.", TRUE, DB_LEN_STR(gv_cur_region));
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, DB_LEN_STR(gv_cur_region));
		free(outptr);
		free(bm_blk_buff);
		CLEANUP_AND_RETURN_FALSE;
	}
	DEBUG_INCBKUP_ONLY(blks_this_lmap = 0);
	if (cs_addrs->nl->onln_rlbk_pid)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_DBROLLEDBACK);
		free(outptr);
		free(bm_blk_buff);
		CLEANUP_AND_RETURN_FALSE;
	}
	for (blk_num_base = 0;  blk_num_base < header->trans_hist.total_blks;  blk_num_base += blks_per_buff)
	{
		if (online && (0 != cs_addrs->shmpool_buffer->failed))
			break;
		if (header->trans_hist.total_blks - blk_num_base < blks_per_buff)
		{
			blks_per_buff = header->trans_hist.total_blks - blk_num_base;
			read_size = blks_per_buff * bsize;
		}
		DOREADRC(db_fd, bp, read_size, status);
		if (0 != status)
		{
			PERROR("read error: ");
			util_out_print("Error reading from database file !AD.", TRUE, DB_LEN_STR(gv_cur_region));
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, DB_LEN_STR(gv_cur_region));
			free(outptr);
			free(bm_blk_buff);
			CLEANUP_AND_RETURN_FALSE;
		}
		bptr = (blk_hdr *)bp;
		/* The blocks we back up will be whatever version they are. There is no implicit conversion in this
		 * part of the backup/restore. Since we aren't even looking at the blocks (and indeed some of these blocks
		 * could potentially contain unintialized garbage data), we set the block version to GDSNOVER to signal
		 * that the block version is unknown. The above applies to "regular" blocks but not to bitmap blocks which
		 * we know are initialized. Because we have to read the bitmap blocks, they will be converted as necessary.
		 */
		for (i = 0; i < blks_per_buff; i++, bptr = (blk_hdr *)((char *)bptr + bsize))
		{
			blk_num = blk_num_base + i;
			if (mu_ctrly_occurred  ||  mu_ctrlc_occurred)
			{
				free(outptr);
				free(bm_blk_buff);
				util_out_print("WARNING:  DB file !AD backup aborted, file !AD not valid", TRUE,
					   DB_LEN_STR(gv_cur_region), file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE;
			}
			if (cs_addrs->nl->onln_rlbk_pid)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_DBROLLEDBACK);
				free(outptr);
				free(bm_blk_buff);
				CLEANUP_AND_RETURN_FALSE;
			}
			/* Before we check if this block needs backing up, check if this is a new bitmap block or not. If it is,
			 * we can fall through and back it up as normal. But if this is NOT a bitmap block, use the
			 * existing bitmap to determine if this block has ever been allocated or not. If not, we don't want to
			 * even look at this block. It could be uninitialized which will just make things run slower if we
			 * go to read it and back it up.
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
				 * basis of whether or not we have to process a given block or not. We process allocated and
				 * recycled blocks leaving free (never used) blocks alone as they have no data worth saving.
				 * But after saving it, upgrade it to the current format if necessary.
				 */
#ifdef DEBUG_INCBKUP
				if (0 != blk_num)	/* Skip first time thorugh loop */
				{
					PRINTF("Dumped %d blks from lcl bitmap blk 0x%016lx\n", blks_this_lmap,
					       (blk_num - BLKS_PER_LMAP));
					blks_this_lmap = 0;
				}
#endif
				is_bitmap_blk = TRUE;
				memcpy(bm_blk_buff, bptr, BM_SIZE(header->bplmap));
				if (SIZEOF(v15_blk_hdr) <= ((v15_blk_hdr_ptr_t)bm_blk_buff)->bsiz)
				{	/* This is a V4 format block -- needs upgrading */
					status = gds_blk_upgrade(bm_blk_buff, bm_blk_buff, bsize, &dummy_odbv);
					if (SS_NORMAL != status)
					{
						free(outptr);
						free(bm_blk_buff);
						util_out_print("Error: Block 0x!XL is too large for automatic upgrade", TRUE,
							       blk_num);
						CLEANUP_AND_RETURN_FALSE;
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
			 * 1) If blk is larger than size of db at time backup was initiated, we ignore the block.
			 * 2) Always backup blocks 0, 1, and 2 as these are the only blocks that can contain data
			 *    and still have a transaction number of 0.
			 * 3) For bitmap blocks, if blks_to_upgrd != 0 and the TN is 0 and the block number >=
			 *    last_blk_at_last_bkup, then backup the block. This way we get the correct version of
			 *    the bitmap block in the restore (otherwise have no clue what version to create them in
			 *    as bitmaps are created with a TN of 0 when before image journaling is enabled).
			 *    Given the possibility of a db file truncate occurring between backups, it is now possible
			 *    for gdsfilext to create a new bitmap block such that blk_num < list->last_blk_at_last_bkup.
			 *    This means the previous state (at the last backup) of a bitmap block with tn=0 is
			 *    indeterminate: it might have held data and it might have had a different version. So
			 *    backup all bitmap blocks with tn=0.
			 *    One concern is that incremental backup files can end up somewhat larger (due to backing
			 *    up extra bitmap blocks), even if no truncate occurred between backups. Users can zip their
			 *    incremental backup files which should deflate almost all of the tn=0 bitmap blocks.
			 *    Future enhancement: Instead of backing up bitmap blocks with tn=0, make a list of the
			 *    the blk_nums and versions of all such blocks. The bitmap blocks can be created in
			 *    mupip_restore from that information alone.
			 * 4) If the block TN is below our TN threshold, ignore the block.
			 * 5) Else if none of the above conditions, backup the block.
			 */
			if (online && (header->trans_hist.curr_tn <= blk_tn))
				backup_this_blk = FALSE;
			else if ((3 > blk_num) || (is_bitmap_blk && (0 != header->blks_to_upgrd) && ((trans_num)0 == blk_tn)
						 && (blk_num >= list->last_blk_at_last_bkup)))
				backup_this_blk = TRUE;
#			ifdef GTM_TRUNCATE
			else if (is_bitmap_blk && ((trans_num)0 == blk_tn))
				backup_this_blk = TRUE;
#			endif
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
			DEBUG_INCBKUP_ONLY(++blks_this_lmap);
			sblkh_p->blkid = blk_num;
			memcpy(data_ptr, bptr, blk_bsiz);
			sblkh_p->valid_data = TRUE;	/* Validation marker */
			DEBUG_INCBKUP_ONLY(PRINTF("DEBUG - write block: blk_num = 0x%08lx, start = 0x%016lx, size = 0x%x \n",
				blk_num, backup_write_offset, outsize);)
			COMMON_WRITE(backup, outptr, outsize);
			if (online)
			{
				if (0 != cs_addrs->shmpool_buffer->failed)
					break;
				cs_addrs->nl->nbb = blk_num;
			}
			save_blks++;
		}
		DEBUG_INCBKUP_ONLY(PRINTF("Dumped %d blks from lcl bitmap blk 0x%016lx\n", blks_this_lmap,
					  (blk_num & ~(BLKS_PER_LMAP - 1))));
	}
	if (cs_addrs->nl->onln_rlbk_pid)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_DBROLLEDBACK);
		free(outptr);
		free(bm_blk_buff);
		CLEANUP_AND_RETURN_FALSE;
	}
	/* After this point, if an Online Rollback is detected, the BACKUP will NOT e affected as all the before images it needs
	 * is already written by GT.M in the temporary file and the resulting BACKUP will be valid*/
	/* ============================ write saved information for online backup ========================== */
	if (online && (0 == cs_addrs->shmpool_buffer->failed))
	{
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		/* By getting crit here, we ensure that there is no process still in transaction logic that sees
		 * (nbb != BACKUP_NOT_IN_PRORESS). After rel_crit(), any process that enters transaction logic will
		 * see (nbb == BACKUP_NOT_IN_PRORESS) because we just set it to that value. At this point, backup
		 * buffer is complete and there will not be any more new entries in the backup buffer until the next
		 * backup.
		 */
		assert(!cs_addrs->hold_onto_crit); /* this ensures we can safely do unconditional grab_crit and rel_crit */
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
				gtm_putmsg(VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1,
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
		while (0 != cs_addrs->shmpool_buffer->backup_cnt)
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
			{	/* Force shmpool recovery to see if it can find the lost blocks */
				if (!shmpool_lock_hdr(gv_cur_region))
				{
					gtm_putmsg(VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					assert(FALSE);
					CLEANUP_AND_RETURN_FALSE;
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}
		if (-1 == lseek(list->backup_fd, 0, SEEK_SET))
		{
			PERROR("lseek error : ");
			CLEANUP_AND_RETURN_FALSE;
		}
		copysize = BACKUP_READ_SIZE;
		for (copied = 0; copied < cs_addrs->shmpool_buffer->dskaddr; copied += copysize)
		{
			if (cs_addrs->shmpool_buffer->dskaddr < copied + copysize)
				copysize = (size_t)(cs_addrs->shmpool_buffer->dskaddr - copied);
			DOREADRC(list->backup_fd, mubbuf, copysize, status);
			if (0 != status)
			{
				PERROR("read error : ");
				CLEANUP_AND_RETURN_FALSE;
			}
			COMMON_WRITE(backup, (char *)mubbuf, copysize);
		}
	}
	/* Write one last (zero-filled) block into this file that designates the end of the blocks */
	memset(outptr, 0, SIZEOF(muinc_blk_hdr) + bsize);
	COMMON_WRITE(backup, outptr, outsize);

	/* ============================= write end_msg and fileheader =============================== */
	if ((!online) || (0 == cs_addrs->shmpool_buffer->failed))
	{	/* Write a secondary end-of-block list marker used for further validation on restore */
		rsize = SIZEOF(END_MSG) + SIZEOF(int4);
		COMMON_WRITE(backup, (char *)&rsize, SIZEOF(int4));
		COMMON_WRITE(backup, END_MSG, SIZEOF(END_MSG));

		ptr1 = (uchar_ptr_t)header;
		ptr1_top = ptr1 + ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
		for ( ;  ptr1 < ptr1_top;  ptr1 += size1)
		{
			if ((size1 = (int4)(ptr1_top - ptr1)) > mubmaxblk)
				size1 = (mubmaxblk / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE;
			size1 += SIZEOF(int4);
			COMMON_WRITE(backup, (char *)&size1, SIZEOF(int4));
			size1 -= SIZEOF(int4);
			COMMON_WRITE(backup, (char *)ptr1, size1);
		}
		rsize = SIZEOF(HDR_MSG) + SIZEOF(int4);
		COMMON_WRITE(backup, (char *)&rsize, SIZEOF(int4));
		COMMON_WRITE(backup, HDR_MSG, SIZEOF(HDR_MSG));
		ptr1 = MM_ADDR(header);
		ptr1_top = ptr1 + ROUND_UP(MASTER_MAP_SIZE(header), DISK_BLOCK_SIZE);
		for ( ; ptr1 < ptr1_top ; ptr1 += size1)
		{
			if ((size1 = (int4)(ptr1_top - ptr1)) > mubmaxblk)
				size1 = (mubmaxblk / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE;
			size1 += SIZEOF(int4);
			COMMON_WRITE(backup, (char *)&size1, SIZEOF(int4));
			size1 -= SIZEOF(int4);
			COMMON_WRITE(backup, (char *)ptr1, size1);
		}
		rsize = SIZEOF(MAP_MSG) + SIZEOF(int4);
		COMMON_WRITE(backup, (char *)&rsize, SIZEOF(int4));
		COMMON_WRITE(backup, MAP_MSG, SIZEOF(MAP_MSG));
		rsize = 0;
		COMMON_WRITE(backup, (char *)&rsize, SIZEOF(rsize));
	}

	/* ========================== close backup destination ======================================== */
	switch(list->backup_to)
	{
		case backup_to_file:
			REVERT;
			iob_close(backup);
			backup = NULL;
			break;
		case backup_to_exec:
			if (0 != backup->remaining)
			{
				assert(backup->blksiz > backup->remaining);
				memset(outptr, 0, backup->blksiz - backup->remaining);
				COMMON_WRITE(backup, outptr, backup->blksiz - backup->remaining);
			}
			CLOSEFILE_RESET(backup->fd, rc);	/* resets "backup->fd" to FD_INVALID */
			/* needs to wait till the child dies, because of the rundown issues */
			if ((pipe_child > 0) && (FALSE != is_proc_alive(pipe_child, 0)))
			{
				pid_t waitpid_res;

				WAITPID(pipe_child, (int *)&status, 0, waitpid_res);
			}
			break;
		case backup_to_tcp:
			if (0 != backup->remaining)
			{
				assert(backup->blksiz > backup->remaining);
				memset(outptr, 0, backup->blksiz - backup->remaining);
				COMMON_WRITE(backup, outptr, backup->blksiz - backup->remaining);
			}
			CLOSEFILE_RESET(backup->fd, rc);	/* resets "backup->fd" to FD_INVALID */
			break;
	}

	/* ============================ output and return =========================================== */
	free(outptr);
	free(bm_blk_buff);
	if (online && (0 != cs_addrs->shmpool_buffer->failed))
	{
		util_out_print("Process !UL encountered the following error.", TRUE,
			cs_addrs->shmpool_buffer->failed);
		if (0 != cs_addrs->shmpool_buffer->backup_errno)
			gtm_putmsg(VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
		util_out_print("!AD, backup for DB file !AD, is not valid.", TRUE,
			file->len, file->addr, DB_LEN_STR(gv_cur_region));
	} else
	{
		util_out_print("DB file !AD incrementally backed up in file !AD", TRUE,
			DB_LEN_STR(gv_cur_region), file->len, file->addr);
		util_out_print("!UL blocks saved.", TRUE, save_blks);
		util_out_print("Transactions from 0x!16@XQ to 0x!16@XQ are backed up.", TRUE,
			&list->tn, &header->trans_hist.curr_tn);
		cs_addrs->hdr->last_inc_backup = header->trans_hist.curr_tn;
		cs_addrs->hdr->last_inc_bkup_last_blk = (block_id)header->trans_hist.total_blks;
		if (record)
		{
			cs_addrs->hdr->last_rec_backup = header->trans_hist.curr_tn;
			cs_addrs->hdr->last_com_bkup_last_blk = (block_id)header->trans_hist.total_blks;
		}
		file_backed_up = TRUE;
		return TRUE;
	}
	CLEANUP_AND_RETURN_FALSE;
}

void exec_write(BFILE *bf, char *buf, int nbytes)
{
	int	nwritten;
	uint4	status;
	pid_t	waitpid_res;
	int	rc;

	DOWRITERL(bf->fd, buf, nbytes, nwritten);

	bf->remaining += nwritten;
	bf->remaining %= bf->blksiz;

	if ((nwritten < nbytes) && (-1 == nwritten))
	{
		gtm_putmsg(VARLSTCNT(1) errno);
		CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
		if ((pipe_child > 0) && (FALSE != is_proc_alive(pipe_child, 0)))
			WAITPID(pipe_child, (int *)&status, 0, waitpid_res);
		backup_write_errno = errno;
	}
	return;
}

void tcp_write(BFILE *bf, char *buf, int nbytes)
{
	int	nwritten, iostatus;
	int	send_retry;
	int	rc;

	nwritten = 0;
	send_retry = 5;
	do
	{
		if (-1 != (iostatus = tcp_routines.aa_send(bf->fd, buf + nwritten, nbytes - nwritten, 0)))
		{
			nwritten += iostatus;
			if (nwritten == nbytes)
				break;
		} else
			break;
	} while (0 < send_retry--);

	bf->remaining += nwritten;
	bf->remaining %= bf->blksiz;

	if ((nwritten != nbytes) && (-1 == iostatus))
	{
		gtm_putmsg(VARLSTCNT(1) errno);
		CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
		backup_write_errno = errno;
	}
	return;
}

CONDITION_HANDLER(iob_io_error1)
{
	int	dummy1, dummy2;
	char	s[80];
	char	*fgets_res;

	START_CH;
	if (SIGNAL == ERR_IOEOF)
	{
		PRINTF("End of media reached, please mount next volume and press Enter: ");
		FGETS(s, 79, stdin, fgets_res);
		util_out_print(0, 2, 0);  /* clear error message */
		if (mu_ctrly_occurred  ||  mu_ctrlc_occurred)
		{
			util_out_print("WARNING:  DB file backup aborted, backup file is not valid.", TRUE);
			UNWIND(dummy1, dummy2);
		}
		CONTINUE;
	}
	PRN_ERROR;
	UNWIND(dummy1, dummy2);
}

CONDITION_HANDLER(iob_io_error2)
{
	int	dummy1, dummy2;
	char	s[80];

	START_CH;
	PRN_ERROR;
	if (!debug_mupip)
		UNLINK(incbackupfile);
	UNWIND(dummy1, dummy2);
}
