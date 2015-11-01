/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
 * -------- requires		cs_addrs and gv_cur_region be current.
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
GBLREF	uchar_ptr_t		mubbuf;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	uint4			pipe_child;
GBLREF	boolean_t		debug_mupip;
GBLREF	int4			backup_write_errno;
LITREF	mval			literal_null;

#define	COMMON_WRITE(A,B,C)	{					\
					(*common_write)(A,B,C);		\
					if (0 != backup_write_errno)	\
						return FALSE;		\
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

bool	mubinccpy (backup_reg_list *list)
{
	static readonly char	end_msg[] = "END OF SAVED BLOCKS";
	static readonly char	hdr_msg[] = "END OF FILE HEADER";

	mstr			*file;
	uchar_ptr_t		bm, ptr1, ptr1_top;
	char *			outptr;
	unsigned char		buff[512];
	char 			*c, addr[SA_MAXLEN + 1];
	sgmnt_data_ptr_t	header;
	uint4			status, total_blks, bplmap, gds_ratio, save_blks;
	int4			size1, bsize, bm_num, blk_num, hint, lmsize, rsize, copied, timeout,
				blks_per_buff, counter, i, write_size, copysize, read_size, match;
	int			db_fd, exec_fd;
	enum db_acc_method	access;
	blk_hdr_ptr_t		bp, bptr;
	inc_header		*outbuf;
	mval			val;
	unsigned short		port;
	bool			needacopy, been_flushed;
	void			(*common_write)(BFILE *, char *, int);

	error_def(ERR_BCKUPBUFLUSH);

	assert(list->reg == gv_cur_region);
	assert(incremental);

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
		mubmaxblk = (31 * 1024);
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
				util_out_print("Error: Cannot create backup file !AD.", TRUE, file->len, file->addr);
				PERROR("open error: ");
				return FALSE;
			}
			if (is_raw_dev(file->addr))
			{
				ESTABLISH_RET(iob_io_error1, FALSE);
			} else
			{
				if (-1 == (status = CHMOD(file->addr, 0600)))
				{
					util_out_print("ERROR: Cannot access incremental backup file !AD.",
							TRUE, file->len, file->addr);
					util_out_print("WARNING: Backup file !AD is not valid.", TRUE, file->len, file->addr);
					PERROR("chmod error: ");
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
			backup = (BFILE *)malloc(sizeof(BFILE));
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
			backup = (BFILE *)malloc(sizeof(BFILE));
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
			if ((0 == cli_get_int("NETTIMEOUT", &timeout)) || (0 > timeout))
				timeout = DEFAULT_BKRS_TIMEOUT;
			if (0 > (backup->fd = tcp_open(addr, port, timeout, FALSE)))
			{
				util_out_print("ERROR: Cannot open tcp connection due to the above error.", TRUE);
				util_out_print("WARNING: Backup !AD is not valid.", TRUE, file->len, file->addr);
				return FALSE;
			}
			break;
		default :
			util_out_print("ERROR: Backup format not supported.");
			util_out_print("WARNING: Backup not valid.");
			return FALSE;
	}

	/* ============================= write inc_header =========================================== */
	outbuf = (inc_header*)malloc(sizeof(inc_header));
	memcpy(&outbuf->label[0], INC_HEADER_LABEL, sizeof INC_HEADER_LABEL - 1);
	stringpool.free = stringpool.base;
	op_horolog(&val);
	stringpool.free = stringpool.base;
	op_fnzdate(&val, &mu_bin_datefmt, &literal_null, &literal_null, &val);
	memcpy(&outbuf->date[0], val.str.addr, val.str.len);
	memcpy(&outbuf->reg[0], gv_cur_region->rname, MAX_RN_LEN);
	memcpy(&outbuf->start_tn, &(list->tn), sizeof(trans_num));
	memcpy(&outbuf->end_tn, &(header->trans_hist.curr_tn), sizeof(trans_num));
	outbuf->db_total_blks = header->trans_hist.total_blks;
	outbuf->blk_size = header->blk_size;
	util_out_print("MUPIP backup of database file !AD to !AD", TRUE, DB_LEN_STR(gv_cur_region), file->len, file->addr);
	COMMON_WRITE(backup, (char *)outbuf, sizeof(inc_header));
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
	blks_per_buff	= BACKUP_READ_SIZE / bsize;
	read_size	= blks_per_buff * bsize;
	outptr		= (char *)malloc(sizeof(int4) + sizeof(block_id) + bsize);
	bp		= (blk_hdr_ptr_t)mubbuf;
	save_blks	= 0;

	if (-1 == lseek(db_fd, (off_t)(header->start_vbn - 1) * DISK_BLOCK_SIZE, SEEK_SET))
	{
		util_out_print("Error reading from database file !AD.", TRUE, DB_LEN_STR(gv_cur_region));
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, DB_LEN_STR(gv_cur_region));
		PERROR("fseek error: ");
		free(outptr);
		CLEANUP_AND_RETURN_FALSE;
	}
	for (blk_num = 0;  blk_num < header->trans_hist.total_blks;  blk_num += blks_per_buff)
	{
		if (online && (0 != cs_addrs->backup_buffer->failed))
			break;
		if (header->trans_hist.total_blks - blk_num < blks_per_buff)
		{
			blks_per_buff = header->trans_hist.total_blks - blk_num;
			read_size = blks_per_buff * bsize;
		}
		if (read_size != read(db_fd, bp, read_size))
		{
			util_out_print("Error reading from database file !AD.", TRUE, DB_LEN_STR(gv_cur_region));
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, DB_LEN_STR(gv_cur_region));
			PERROR("read error: ");
			free(outptr);
			CLEANUP_AND_RETURN_FALSE;
		}
		bptr = (blk_hdr *)bp;
		for (i = 0; i < blks_per_buff; i++, bptr = (blk_hdr *)((char *)bptr + bsize))
		{
			if (mu_ctrly_occurred  ||  mu_ctrlc_occurred)
			{
				free(outptr);
				util_out_print("WARNING:  DB file !AD backup aborted, file !AD not valid", TRUE,
					   DB_LEN_STR(gv_cur_region), file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE;
			}
			if ((bptr->tn < list->tn) || (online && (header->trans_hist.curr_tn <= bptr->tn))
							|| (bptr->bsiz > bsize) || (bptr->bsiz < sizeof(blk_hdr)))
				continue; /* not applicable */
			rsize = sizeof(int4) + sizeof(block_id) + bptr->bsiz;
			memcpy(outptr, &rsize, sizeof(int4));
			*((uint4 *) (outptr + sizeof(int4))) = blk_num + i;
			memcpy(outptr + sizeof(int4) + sizeof(block_id), bptr, bptr->bsiz);
			assert (rsize <= sizeof(int4) + sizeof(block_id) + bsize);

			COMMON_WRITE(backup, outptr, rsize);

			if (online)
			{
				if (0 != cs_addrs->backup_buffer->failed)
					break;
				cs_addrs->nl->nbb = blk_num + i;
			}
			save_blks++;
		}
	}

	/* ============================ write saved information for online backup ========================== */
	if (online && (0 == cs_addrs->backup_buffer->failed))
	{
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		grab_crit(gv_cur_region);
		rel_crit(gv_cur_region);
		counter = 1;
		while (cs_addrs->backup_buffer->free != cs_addrs->backup_buffer->disk)
		{
			backup_buffer_flush(gv_cur_region);
			if (counter++ > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg(VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				CLEANUP_AND_RETURN_FALSE;
			}
			wcs_sleep(counter);
		}
		if (-1 == lseek(list->backup_fd, 0, SEEK_SET))
		{
			PERROR("lseek error : ");
			CLEANUP_AND_RETURN_FALSE;
		}
		copysize = BACKUP_READ_SIZE;
		for (copied = 0; copied < cs_addrs->backup_buffer->dskaddr; copied += copysize)
		{
			if (cs_addrs->backup_buffer->dskaddr < copied + copysize)
				copysize = cs_addrs->backup_buffer->dskaddr - copied;
			if (copysize != read(list->backup_fd, mubbuf, copysize))
			{
				PERROR("read error : ");
				CLEANUP_AND_RETURN_FALSE;
			}
			COMMON_WRITE(backup, (char *)mubbuf, copysize);
		}
	}

	/* ============================= write end_msg and fileheader =============================== */
	if ((!online) || (0 == cs_addrs->backup_buffer->failed))
	{
		rsize = sizeof(end_msg) + sizeof(int4);
		COMMON_WRITE(backup, (char *)&rsize, sizeof(int4));
		COMMON_WRITE(backup, end_msg, sizeof(end_msg));

		ptr1 = (uchar_ptr_t)header;
		ptr1_top = ptr1 + ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
		for ( ;  ptr1 < ptr1_top;  ptr1 += size1)
		{
			if((size1 = ptr1_top - ptr1) > mubmaxblk)
			{	/* Want to have block aligned as use qio in mupip_restore */
				size1 = mubmaxblk / DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
			}
			size1 += sizeof(int4);
			COMMON_WRITE(backup, (char *)&size1, sizeof(int4));
			size1 -= sizeof(int4);
			COMMON_WRITE(backup, (char *)ptr1, size1);
		}
		rsize = sizeof(hdr_msg);
		COMMON_WRITE(backup, (char *)&rsize, sizeof(int4));
		COMMON_WRITE(backup, hdr_msg, sizeof(hdr_msg));
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
			close(backup->fd);
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
			close(backup->fd);
			break;
	}

	/* ============================ output and return =========================================== */
	free(outptr);
	if (online && (0 != cs_addrs->backup_buffer->failed))
	{
		util_out_print("Process !UL encountered the following error.", TRUE,
			cs_addrs->backup_buffer->failed);
		if (0 != cs_addrs->backup_buffer->backup_errno)
			gtm_putmsg(VARLSTCNT(1) cs_addrs->backup_buffer->backup_errno);
		util_out_print("!AD, backup for DB file !AD, is not valid.", TRUE,
			file->len, file->addr, DB_LEN_STR(gv_cur_region));
	} else
	{
		util_out_print("DB file !AD incrementally backed up in file !AD", TRUE,
			DB_LEN_STR(gv_cur_region), file->len, file->addr);
		util_out_print("!UL blocks saved.", TRUE, save_blks);
		util_out_print("Transactions from 0x!8XL to 0x!8XL are backed up.", TRUE,
			list->tn, header->trans_hist.curr_tn);
		cs_addrs->hdr->last_inc_backup = header->trans_hist.curr_tn;
		if (record)
			cs_addrs->hdr->last_rec_backup = header->trans_hist.curr_tn;
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

	DOWRITERL(bf->fd, buf, nbytes, nwritten);

	bf->remaining += nwritten;
	bf->remaining %= bf->blksiz;

	if ((nwritten < nbytes) && (-1 == nwritten))
	{
		gtm_putmsg(VARLSTCNT(1) errno);
		close(bf->fd);
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
		close(bf->fd);
		backup_write_errno = errno;
	}
	return;
}

CONDITION_HANDLER(iob_io_error1)
{
	int	dummy1, dummy2;
	char	s[80];
	char	*fgets_res;
	error_def(ERR_IOEOF);

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
	error_def(ERR_IOEOF);

	START_CH;
	PRN_ERROR;
	if (!debug_mupip)
		UNLINK(incbackupfile);
	UNWIND(dummy1, dummy2);
}
