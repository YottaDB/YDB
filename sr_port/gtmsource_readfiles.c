/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include <sys/ipc.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef UNIX
#include <sys/un.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#ifdef UNIX
#include "gtm_stat.h"
#endif
#ifdef VMS
#include <ssdef.h>
#include <rms.h>
#include <devdef.h>
#include <fab.h>
#include <iodef.h>
#include <nam.h>
#include <rmsdef.h>
#include <descrip.h> /* Required for gtmsource.h */
#include <efndef.h>
#include <secdef.h>
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#ifdef UNIX
#include "gtmio.h"
#endif
#include "iosp.h"
#include "gtm_stdio.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "is_file_identical.h"
#include "repl_log.h"
#include "longcpy.h"

#define LOG_WAIT_FOR_JNL_RECS_PERIOD	(10 * 1000) /* ms */
#define LOG_WAIT_FOR_JNLOPEN_PERIOD	(10 * 1000) /* ms */
#define EOF_RECLEN			ROUND_UP(JREC_PREFIX_SIZE + sizeof(struct_jrec_eof) + JREC_SUFFIX_SIZE, JNL_REC_START_BNDRY)

#define LSEEK_ERR_STR		"Error in lseek"
#define LSEEK_ERR_LEN		(sizeof(LSEEK_ERR_STR) - 1)
#define READ_ERR_STR		"Error in lseek"
#define READ_ERR_LEN		(sizeof(READ_ERR_STR) - 1)
#define UNKNOWN_ERR_STR		"Error in lseek"
#define UNKNOWN_ERR_LEN		(sizeof(LSEEK_ERR_STR) - 1)

GBLDEF	unsigned char		*gtmsource_tcombuff_start = NULL;
GBLDEF	unsigned char		*gtmsource_tcombuff_end = NULL;
GBLDEF	unsigned char		*gtmsource_tcombuffp = NULL;

GBLREF	jnlpool_addrs		jnlpool;
GBLREF  repl_ctl_element        *repl_ctl_list;
GBLREF  seq_num                 gtmsource_save_read_jnl_seqno;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF	gd_region		*gv_cur_region;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	FILE			*gtmsource_statslog_fp;

static	int4			num_tcom = -1;
static	boolean_t		trans_read = FALSE;
static	int			tot_tcom_len = 0;
static	int			total_wait_for_jnl_recs = 0;
static	int			total_wait_for_jnlopen = 0;

static 	int			adjust_buff_leaving_hdr(repl_buff_t *rb);
static	int			position_read(repl_ctl_element*, seq_num, tr_search_method_t);
static	int			 read_regions(
					uchar_ptr_t *buff, int *buff_avail,
		         		boolean_t attempt_open_oldnew,
					boolean_t *brkn_trans,
					seq_num read_jnl_seqno);

static	int			first_read(repl_ctl_element*);
static	int 			update_max_seqno_info(repl_ctl_element *ctl);
static	int			adjust_for_eof_or_extension(repl_ctl_element *ctl);
static	int 			scavenge_closed_jnl_files(seq_num ack_seqno);

LITREF	int			jnl_fixed_size[];

static	int repl_read_file(repl_buff_t *rb)
{
	repl_buff_desc 		*b;
	repl_file_control_t	*fc;
	int			nb;
	sgmnt_addrs		*csa;
	uint4			dskaddr;
	int4			read_less, status;
	VMS_ONLY(
		short           iosb[4];
		int4		read_off;
		int4		extra_bytes;
		sm_uc_ptr_t 	read_buff;
		DEBUG_ONLY(unsigned char verify_buff[DISK_BLOCK_SIZE];)
	)

	error_def(ERR_TEXT);

	fc = rb->fc;
	b = &rb->buff[rb->buffindex];
	csa = &FILE_INFO(rb->backctl->reg)->s_addrs;
	read_less = 0;
	assert(b->readaddr >= b->recaddr);
	dskaddr = csa->jnl->jnl_buff->dskaddr;
	if (is_gdid_gdid_identical(&fc->id, JNL_GDID_PTR(csa)))
	{	/* Make sure we do not read beyond end of data in the journal file */
		/* Note : This logic is always needed when journal file is pre-allocated.
		 * With no pre-allocation, this logic is needed only when repl_read_file is called from
		 * update_max_seqno_info -> repl_next. Specifically, this logic is needed till the existing
		 * JRT_EOF record is completely overwritten and the file grows beyond its existing size.
		 */
		if (b->readaddr + b->buffremaining > dskaddr)
		{
			if (b->readaddr == dskaddr)
			{
				REPL_DPRINT3("READ FILE : Jnl file %s yet to grow from offset %u\n",
						rb->backctl->jnl_fn, b->readaddr);
				return (SS_NORMAL);
			}
			read_less = b->readaddr + b->buffremaining - dskaddr;
			REPL_DPRINT5("READ FILE : Racing with jnl file %s avoided. Read size reduced from %u to %u at offset %u\n",
					rb->backctl->jnl_fn, b->buffremaining, b->buffremaining - read_less, b->readaddr);
		}
	}
#ifdef UNIX
	if (lseek(fc->fd, (off_t)b->readaddr, SEEK_SET) == (off_t)-1)
	{
		repl_errno = EREPL_JNLFILESEEK;
		return (ERRNO);
	}
	READ_FILE(fc->fd, b->base + REPL_BLKSIZE(rb) - b->buffremaining, b->buffremaining - read_less, nb);
	status = ERRNO;
#elif defined(VMS)
	nb = b->buffremaining - read_less; /* to be read */
	read_off = ROUND_DOWN2(b->readaddr, DISK_BLOCK_SIZE); /* since read has to start at a disk block boundary */
	extra_bytes = b->readaddr - read_off;
	read_buff = b->base + REPL_BLKSIZE(rb) - b->buffremaining - extra_bytes;
	DEBUG_ONLY(
		if (0 != extra_bytes)
			memcpy(verify_buff, read_buff, extra_bytes);
		else;
	)
	assert(read_buff >= b->base);
	status = sys$qiow(EFN$C_ENF, fc->fd, IO$_READVBLK, &iosb[0], 0, 0, read_buff, nb + extra_bytes,
			  DIVIDE_ROUND_DOWN(read_off, DISK_BLOCK_SIZE) + 1, 0, 0, 0);
	if (SS$_NORMAL == status && (SS$_NORMAL == (status = iosb[0]) || SS$_ENDOFFILE == status))
	{
		GET_LONG(nb, &iosb[1]); /* num bytes actually read */
		nb -= extra_bytes; /* that we are interested in */
		if ((SS$_NORMAL == status && nb < b->buffremaining - read_less) || (0 >= nb))
			GTMASSERT; /* we thought VMS wouldn't return less than what we requested for */
		DEBUG_ONLY((0 != extra_bytes) ? assert(0 == memcmp(verify_buff, read_buff, extra_bytes)) : 0;)
		status = SS$_NORMAL;
	} else
		nb = -1;
#else
#error Unsupported platform
#endif
	if (nb >= 0)
	{
		b->buffremaining -= nb;
		b->readaddr += nb;
		if (fc->eof_addr < b->readaddr)
			fc->eof_addr = b->readaddr;
		return (SS_NORMAL);
	}
	repl_errno = EREPL_JNLFILEREAD;
	return (status);
}

static	int repl_next(repl_buff_t *rb)
{
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	int4			reclen;
	uint4			maxreclen;
	int			status, sav_buffremaining;
	char			err_string[BUFSIZ];

	error_def(ERR_REPLFILIOERR);
	error_def(ERR_TEXT);

	fc = rb->fc;
	b = &rb->buff[rb->buffindex];
	b->recbuff += b->reclen; /* The next record */
	b->recaddr += b->reclen;
	if (b->recaddr == b->readaddr && b->buffremaining == 0)
	{
		/* Everything in this buffer processed */
		b->recbuff = b->base;
		b->reclen = 0;
		b->buffremaining = REPL_BLKSIZE(rb);
	}
	if (b->recaddr == b->readaddr || b->reclen == 0)
	{
		sav_buffremaining = b->buffremaining;
		if ((status = repl_read_file(rb)) == SS_NORMAL)
		{
			if (sav_buffremaining == b->buffremaining)
			{
				b->reclen = 0;
				repl_errno = EREPL_JNLRECINCMPL;
				return (repl_errno);
			}
		} else
		{
			if (repl_errno == EREPL_JNLFILESEEK)
				memcpy(err_string, LSEEK_ERR_STR, LSEEK_ERR_LEN);
			else if (repl_errno == EREPL_JNLFILEREAD)
				memcpy(err_string, READ_ERR_STR, READ_ERR_LEN);
			else
				memcpy(err_string, UNKNOWN_ERR_STR, UNKNOWN_ERR_LEN);
			rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, rb->backctl->jnl_fn_len, rb->backctl->jnl_fn,
			  	  ERR_TEXT, 2, LEN_AND_STR(err_string), status);
		}
	}
	maxreclen = ((b->base + REPL_BLKSIZE(rb)) - b->recbuff) - b->buffremaining;
	assert(maxreclen > 0);
	reclen = jnl_record_length((jnl_record *)b->recbuff, maxreclen);
	if (reclen > 0 && reclen <= maxreclen)
	{
		b->reclen = reclen;
		return (SS_NORMAL);
	}
	if (reclen > 0)
		repl_errno = EREPL_JNLRECINCMPL;
	else if (reclen == -1)
		repl_errno = EREPL_JNLRECFMT;
	else if (b->buffremaining == 0)
		repl_errno = EREPL_JNLBADALIGN;
	else
		GTMASSERT;
	b->reclen = 0;
	return (repl_errno);
}

static int open_prev_gener(repl_ctl_element **old_ctl, repl_ctl_element *ctl, seq_num read_seqno)
{

	if (0 == ctl->repl_buff->fc->jfh->prev_jnl_file_name_length ||
		 QWLE(ctl->repl_buff->fc->jfh->start_seqno, read_seqno))
	{
		/* No need to open previous generation, or no previous
		 * generation */
		REPL_DPRINT2("No need to open prev gener of %s or no prev gener\n", ctl->jnl_fn);
		return (0);
	}
	repl_ctl_create(old_ctl, ctl->reg, ctl->repl_buff->fc->jfh->prev_jnl_file_name_length,
			ctl->repl_buff->fc->jfh->prev_jnl_file_name, FALSE);
	REPL_DPRINT2("Prev gener file %s opened\n", ctl->repl_buff->fc->jfh->prev_jnl_file_name);
	(*old_ctl)->prev = ctl->prev;
	(*old_ctl)->next = ctl;
	(*old_ctl)->prev->next = *old_ctl;
	(*old_ctl)->next->prev = *old_ctl;
	first_read(*old_ctl);
	if ((*old_ctl)->file_state == JNL_FILE_OPEN)
	{
		(*old_ctl)->file_state = JNL_FILE_CLOSED;
		REPL_DPRINT2("open_prev_gener : %s jnl file marked closed\n", (*old_ctl)->jnl_fn);
	} else if ((*old_ctl)->file_state == JNL_FILE_UNREAD)
	{
		(*old_ctl)->file_state = JNL_FILE_EMPTY;
		REPL_DPRINT2("open_prev_gener :  %s jnl file marked empty\n", (*old_ctl)->jnl_fn);
	} else
		GTMASSERT;
	return (1);
}

static	int open_newer_gener_jnlfiles(gd_region *reg, repl_ctl_element *reg_ctl_end)
{
	sgmnt_addrs		*csa;
	repl_ctl_element	*new_ctl, *ctl;
	ino_t			save_jnl_inode;
	int			jnl_fn_len;
	char			jnl_fn[JNL_NAME_SIZE];
#ifdef UNIX
	struct stat		stat_buf;
#endif
	int			nopen, n;
	int			status;
	gd_region		*r_save;
	uint4			jnl_status;
	int			stat_res;
	boolean_t		do_jnl_ensure_open;

	error_def(ERR_REPLFILIOERR);
	error_def(ERR_TEXT);

	/* Attempt to open newer generation journal files. Return the number of new files opened. Create new
	 * ctl element(s) for each newer generation and attach at reg_ctl_end. Work backwards from the current journal file.
	 */
	jnl_status = 0;
	nopen = 0;
	csa = &FILE_INFO(reg)->s_addrs;
	if (is_gdid_gdid_identical(&reg_ctl_end->repl_buff->fc->id, JNL_GDID_PTR(csa))) /* Journal file remains same */
		return (nopen);
	jnl_fn_len = 0; jnl_fn[0] = '\0';
	for (do_jnl_ensure_open = TRUE; ; do_jnl_ensure_open = FALSE)
	{
		repl_ctl_create(&new_ctl, reg, jnl_fn_len, jnl_fn, do_jnl_ensure_open);
		nopen++;
		REPL_DPRINT2("Newer generation file %s opened\n", new_ctl->jnl_fn);
		new_ctl->prev = reg_ctl_end;
		new_ctl->next = reg_ctl_end->next;
		if (new_ctl->next)
			new_ctl->next->prev = new_ctl;
		new_ctl->prev->next = new_ctl;
		jnl_fn_len = new_ctl->repl_buff->fc->jfh->prev_jnl_file_name_length;
		memcpy(jnl_fn, new_ctl->repl_buff->fc->jfh->prev_jnl_file_name, jnl_fn_len);
		jnl_fn[jnl_fn_len] = '\0';
#ifdef UNIX
		STAT_FILE(jnl_fn, &stat_buf, stat_res);
		if (stat_res < 0)
			rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, jnl_fn_len, jnl_fn,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in stat in open_newer_gener_jnlfiles"), errno);
		if (is_gdid_stat_identical(&reg_ctl_end->repl_buff->fc->id, &stat_buf))
			break;
#elif defined(VMS)
		if (is_gdid_file_identical(&reg_ctl_end->repl_buff->fc->id, jnl_fn, jnl_fn_len))
			break;
#else
#error Unsupported platform
#endif
	}
	/* Name of the journal file corresponding to reg_ctl_end might have changed. Update the name.
	 * Since inode info doesn't change when a file is renamed, it is not necessary to close and reopen the file.
	 */
	reg_ctl_end->jnl_fn[reg_ctl_end->jnl_fn_len] = '\0'; /* For safety */
	jnl_fn[jnl_fn_len] = '\0';
	if (strcmp(reg_ctl_end->jnl_fn, jnl_fn) != 0) /* Name has changed */
	{
		REPL_DPRINT3("Detected name change of %s to %s\n", reg_ctl_end->jnl_fn, jnl_fn);
		reg_ctl_end->jnl_fn_len = reg_ctl_end->reg->jnl_file_len = jnl_fn_len;
		memcpy(reg_ctl_end->jnl_fn, jnl_fn, jnl_fn_len);
		memcpy(reg_ctl_end->reg->jnl_file_name, jnl_fn, jnl_fn_len);
	}
	/* Except the latest generation, mark the newly opened future generations CLOSED, or EMPTY.
	 * We assume that when a new file is opened, the previous generation has been flushed to disk fully.
	 */
	for (ctl = reg_ctl_end, n = nopen; n; n--, ctl = ctl->next)
	{
		if (ctl->file_state == JNL_FILE_UNREAD)
			first_read(ctl);
		else if (ctl->file_state == JNL_FILE_OPEN)
		{
			if (update_max_seqno_info(ctl) != SS_NORMAL)
			{
				assert(repl_errno == EREPL_JNLEARLYEOF);
				GTMASSERT; /* Program bug */
			}
		} else
			GTMASSERT;
		if (ctl->file_state == JNL_FILE_UNREAD)
		{
			ctl->file_state = JNL_FILE_EMPTY;
			REPL_DPRINT2("Open_newer_gener_files : %s marked empty\n", ctl->jnl_fn);
		} else
		{
			assert(ctl->file_state == JNL_FILE_OPEN);
			ctl->file_state = JNL_FILE_CLOSED;
			REPL_DPRINT2("Open_newer_gener_files : %s marked closed\n", ctl->jnl_fn);
		}
	}
	return (nopen);
}

static	int update_eof_addr(repl_ctl_element *ctl, int *eof_change)
{
	repl_file_control_t	*fc;
	uint4			prev_eof_addr, new_eof_addr;
	int			status;
	sgmnt_addrs		*csa;
#ifdef VMS
	short           	iosb[4];
#endif

	error_def(ERR_REPLFILIOERR);
	error_def(ERR_TEXT);

	csa = &FILE_INFO(ctl->reg)->s_addrs;
	fc = ctl->repl_buff->fc;
	prev_eof_addr = fc->eof_addr;
	*eof_change = 0;
	new_eof_addr = csa->jnl->jnl_buff->dskaddr;
	if (is_gdid_gdid_identical(&fc->id, JNL_GDID_PTR(csa)))
	{
		REPL_DPRINT3("Update EOF : New EOF addr from SHM for %s is %u\n", ctl->jnl_fn, new_eof_addr);
	}
	else
	{
		REPL_DPRINT2("Update EOF : New EOF addr will be found from jnl file hdr for %s\n", ctl->jnl_fn);
		UNIX_ONLY(
			REPL_DPRINT4("Update EOF : FC ID IS %u %d %u\n", fc->id.inode, fc->id.device, fc->id.st_gen);
			REPL_DPRINT4("Update EOF : csa->hdr->jnl_file.u (unreliable) is %u %d %u\n", csa->hdr->jnl_file.u.inode,
					csa->hdr->jnl_file.u.device,  csa->hdr->jnl_file.u.st_gen);
		)
		if (!ctl->fh_read_done)
		{
			F_READ_BLK_ALIGNED(fc->fd, 0, fc->jfh, ROUND_UP(sizeof(jnl_file_header), 8), status);
			if (SS_NORMAL != status)
				rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, ctl->jnl_fn_len, ctl->jnl_fn,
						ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in reading jfh in update_eof_addr"), status);
			REPL_DPRINT2("Update EOF : Jnl file hdr refreshed from file for %s\n", ctl->jnl_fn);
			ctl->fh_read_done = TRUE;
		}
		new_eof_addr = fc->jfh->end_of_data + EOF_RECLEN;
		REPL_DPRINT3("Update EOF : New EOF addr from jfh for %s is %u\n", ctl->jnl_fn, new_eof_addr);
	}
	UNIX_ONLY(assert(new_eof_addr >= prev_eof_addr);)
	fc->eof_addr = new_eof_addr;
	*eof_change = new_eof_addr > prev_eof_addr ? (int4)(new_eof_addr - prev_eof_addr) : -(int4)(prev_eof_addr - new_eof_addr);
		/* Above computation was done that way because the variables involved are unsigned */
	return (SS_NORMAL);
}

static	int force_file_read(repl_ctl_element *ctl)
{	/* The journal file may have grown since we last read it. A previously read EOF record may have been over-written on disk.
	 * Reposition read pointers so that a file read is forced if the current read pointers are positioned at a EOF record.
	 */
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	char			rectype;

	rb = ctl->repl_buff;
	b = &rb->buff[REPL_MAINBUFF];
	fc = rb->fc;
	if (b->reclen == 0 || b->recaddr == b->readaddr || b->buffremaining == 0 || b->buffremaining == REPL_BLKSIZE(rb))
	{	/* A file read will be forced anyway */
		return (SS_NORMAL);
	}
	/* b->recbuff points to valid record */
	rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
	assert(rectype > JRT_BAD && rectype <= JRT_RECTYPES);
	if (rectype != JRT_EOF) /* Can't be stale */
		return (SS_NORMAL);
	assert(b->reclen == (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_EOF] + JREC_SUFFIX_SIZE));
	assert(b->readaddr - b->recaddr >= (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_EOF] + JREC_SUFFIX_SIZE));
	b->buffremaining += (b->readaddr - b->recaddr);
	REPL_DPRINT3("FORCE FILE READ : Changing EOF_ADDR from %u to %u\n", fc->eof_addr, b->readaddr);
	b->readaddr = fc->eof_addr = b->recaddr;
	b->reclen = 0;
	return (SS_NORMAL);
}

static	int adjust_for_eof_or_extension(repl_ctl_element *ctl)
{	/* Filler zeroes and a JRT_EOF record might have been written
	 * - the JRT_EOF record at the next DISK_BLOCK_SIZE boundary, and filler zeroes upto the next DISK_BLOCK_SIZE boundary.
	 * OR
	 * File has been extended putting filler zeroes till the next disk block.
	 * Attempt adjusting the read pointers to move to the next record.
	 */
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	int			zero_len, sav_buffremaining, status;
	unsigned char		*c;
	uint4			next_recaddr;
	char			err_string[BUFSIZ];
	unsigned char		seq_num_str[32], *seq_num_ptr;

	error_def(ERR_REPLFILIOERR);
	error_def(ERR_TEXT);

	rb = ctl->repl_buff;
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	if (*b->recbuff != '\0')
		return (EREPL_JNLRECFMT);
	next_recaddr = ROUND_UP(b->recaddr, DISK_BLOCK_SIZE);
	assert(next_recaddr <= fc->eof_addr);
	zero_len = next_recaddr - b->recaddr;
	assert(zero_len > 0);
	while (next_recaddr > b->readaddr)
	{
		sav_buffremaining = b->buffremaining;
		if ((status = repl_read_file(rb)) == SS_NORMAL)
		{
			if (sav_buffremaining == b->buffremaining)
			{	/* Log warning message for every certain number of attempts. There might have been
				 * a crash and the file might have been corrupted. The file might never grow.
				 * Such cases have to be detected and attempt to read such files aborted.
			 	 */
				gtmsource_poll_actions(TRUE);
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
				if ((total_wait_for_jnl_recs += GTMSOURCE_WAIT_FOR_JNL_RECS) % LOG_WAIT_FOR_JNL_RECS_PERIOD == 0)
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %dms for file "
						 "extension or EOF to be written to journal file %s while attempting to read "
						 "transaction "INT8_FMT". Check for problems with journaling\n",
						 total_wait_for_jnl_recs, ctl->jnl_fn, INT8_PRINT(ctl->seqno));
				}
			}
		} else
		{
			if (repl_errno == EREPL_JNLFILESEEK)
				memcpy(err_string, LSEEK_ERR_STR, LSEEK_ERR_LEN);
			else if (repl_errno == EREPL_JNLFILEREAD)
				memcpy(err_string, READ_ERR_STR, READ_ERR_LEN);
			else
				memcpy(err_string, UNKNOWN_ERR_STR, UNKNOWN_ERR_LEN);
			rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, rb->backctl->jnl_fn_len, rb->backctl->jnl_fn,
								ERR_TEXT, 2, LEN_AND_STR(err_string), status);
		}
	}
	for (c = b->recbuff; c < b->recbuff + zero_len && *c == '\0'; c++)
		;
	if (c == b->recbuff + zero_len)
	{
		b->reclen = zero_len;
		return (SS_NORMAL);
	}
	b->reclen = 0;
	repl_errno = EREPL_JNLRECFMT;
	return (repl_errno);
}

static	int update_max_seqno_info(repl_ctl_element *ctl)
{	/* The information in ctl is outdated. The journal file has grown. Update max_seqno and its dskaddr */
	int 			eof_change;
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	uint4			dskread;
	boolean_t		max_seqno_found, fh_read_done;
	uint4			max_seqno_addr;
	seq_num			max_seqno, reg_seqno;
	int			status;
	char			rectype;
	gd_region		*reg;
	sgmnt_addrs		*csa;

	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_JNLBADALIGN);
	error_def(ERR_TEXT);

	assert(ctl->file_state == JNL_FILE_OPEN);

	rb = ctl->repl_buff;
	fc = rb->fc;
	reg = ctl->reg;
	csa = &FILE_INFO(reg)->s_addrs;
	fh_read_done = ctl->fh_read_done;
	update_eof_addr(ctl, &eof_change);
#ifdef GTMSOURCE_SKIP_DSKREAD_WHEN_NO_EOF_CHANGE
	/* This piece of code needs some more testing - Vinaya 03/11/98 */
	if (eof_change == 0 && ctl->first_read_done)
	{
		REPL_DPRINT2("UPDATE MAX SEQNO INFO: No change in EOF addr. Skipping disk read for %s\n", ctl->jnl_fn);
		return (SS_NORMAL);
	}
#endif
	if (fc->eof_addr == JNL_FILE_FIRST_RECORD)
	{
		repl_errno = EREPL_JNLEARLYEOF;
		return (repl_errno);
	}
	QWASSIGN(reg_seqno, csa->hdr->reg_seqno);
	QWDECRBYDW(reg_seqno, 1);
 	if (QWGE(ctl->max_seqno, reg_seqno) || (fh_read_done && ctl->fh_read_done))
 	{	/* have searched already */
 		REPL_DPRINT4("UPDATE MAX SEQNO INFO : not reading file %s; max_seqno = "INT8_FMT", reg_seqno = "INT8_FMT"\n",
 			     ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), INT8_PRINT(reg_seqno));
 		return (SS_NORMAL);
 	}
	rb->buffindex = REPL_SCRATCHBUFF;
	b = &rb->buff[rb->buffindex];
	dskread = ROUND_DOWN(fc->eof_addr, REPL_BLKSIZE(rb));
	if (dskread == fc->eof_addr)
		dskread -= REPL_BLKSIZE(rb);
	QWASSIGN(max_seqno, seq_num_zero);
	max_seqno_addr = 0;
	max_seqno_found = FALSE;
	dskread += REPL_BLKSIZE(rb);
	while (!max_seqno_found && dskread >= REPL_BLKSIZE(rb))
	{
		/* Ignore the existing contents of scratch buffer */
		b->buffremaining = REPL_BLKSIZE(rb);
		b->recbuff = b->base;
		b->reclen = 0;
		dskread -= REPL_BLKSIZE(rb);
		b->readaddr = b->recaddr = JNL_BLK_DSKADDR(dskread, REPL_BLKSIZE(rb));
		if (b->readaddr == JNL_FILE_FIRST_RECORD && adjust_buff_leaving_hdr(rb) != SS_NORMAL)
		{
			assert(repl_errno == EREPL_BUFFNOTFRESH);
			GTMASSERT; /* Program bug */
		}
		while (!max_seqno_found)
		{
			if ((status = repl_next(rb)) == SS_NORMAL)
			{
				rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
				if (IS_REPL_RECTYPE(rectype))
				{
					QWASSIGN(max_seqno, get_jnl_seqno((jnl_record *)b->recbuff));
					max_seqno_addr = b->recaddr;
				} else if (rectype == JRT_EOF)
				{
					if (QWNE(max_seqno, seq_num_zero))
						max_seqno_found = TRUE;
					break;
				}
			} else if (status == EREPL_JNLRECINCMPL)
			{
				if (QWNE(max_seqno, seq_num_zero))
					max_seqno_found = TRUE;
				break;
			} else
			{
				if (status == EREPL_JNLRECFMT && adjust_for_eof_or_extension(ctl) != SS_NORMAL)
					rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
				else if (status == EREPL_JNLBADALIGN)
					rts_error(VARLSTCNT(5) ERR_JNLBADALIGN, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
				else if (status != EREPL_JNLRECFMT)
					GTMASSERT;
			}
		}
	}
	if (max_seqno_found)
	{
		QWASSIGN(ctl->max_seqno, max_seqno);
		ctl->max_seqno_dskaddr = max_seqno_addr;
		return (SS_NORMAL);
	}
	/* dskread < REPL_BLKSIZE(rb), actually dskread == 0 */
	repl_errno = EREPL_JNLEARLYEOF;
	return (repl_errno);
}

static 	int adjust_buff_leaving_hdr(repl_buff_t *rb)
{	/* The first alignsize bytes to be read from jnl file. Adjust fields in rb->buff[rb->buffindex] to skip jnl file header */
	repl_buff_desc	*b;

	if (REPL_BLKSIZE(rb) <= JNL_FILE_FIRST_RECORD)
		return (SS_NORMAL);
	b = &rb->buff[rb->buffindex];
	if (b->buffremaining < REPL_BLKSIZE(rb))
	{
		repl_errno = EREPL_BUFFNOTFRESH;
		return (repl_errno);
	}
	memset(b->base, 0, JNL_FILE_FIRST_RECORD);
	b->recbuff = b->base + JNL_FILE_FIRST_RECORD;
	b->reclen = 0;
	b->recaddr = b->readaddr = JNL_FILE_FIRST_RECORD;
	b->buffremaining -= JNL_FILE_FIRST_RECORD;
	return (SS_NORMAL);
}

static	int first_read(repl_ctl_element *ctl)
{	/* The first read from this generation of the journal file. Find the min_seqno, max_seqno.
	 * If EOF is found while searching for min_seqno, or, nothing could be read from this file,
	 * this is an empty journal file, probably will be written to later.
	 * Position the next read at min_seqno. Update the max_seqno_info.
	 */

	int 			status, eof_change;
	char			rectype;
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	boolean_t		min_seqno_found;
	unsigned char		seq_num_str[32], *seq_num_ptr;

	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_JNLBADALIGN);

	rb = ctl->repl_buff;
	rb->buffindex = REPL_MAINBUFF;
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	/* Ignore the existing contents of the buffer */
	b->buffremaining = REPL_BLKSIZE(rb);
	b->recbuff = b->base;
	b->reclen = 0;
	b->readaddr = JNL_FILE_FIRST_RECORD;
	b->recaddr = JNL_FILE_FIRST_RECORD;
	if (adjust_buff_leaving_hdr(rb) != SS_NORMAL)
	{
		assert(repl_errno == EREPL_BUFFNOTFRESH);
		GTMASSERT; /* Program bug */
	}
	min_seqno_found = FALSE;
	while (!min_seqno_found)
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
			if (IS_REPL_RECTYPE(rectype))
			{
				QWASSIGN(ctl->min_seqno, get_jnl_seqno((jnl_record *)b->recbuff));
				QWASSIGN(ctl->seqno, ctl->min_seqno);
				ctl->tn = get_tn((jnl_record *)b->recbuff);
				QWASSIGN(ctl->max_seqno, ctl->min_seqno);
				ctl->min_seqno_dskaddr = ctl->max_seqno_dskaddr = b->recaddr;
				ctl->file_state = JNL_FILE_OPEN;
				min_seqno_found = TRUE;
			} else if (rectype == JRT_EOF)
			{
				ctl->first_read_done = TRUE;
				REPL_DPRINT2("JRT_EOF read when looking for first replication record. Empty file %s\n",
						ctl->jnl_fn);
				return (SS_NORMAL);
			}
		} else if (status == EREPL_JNLRECINCMPL) /* Nothing read */
		{
			ctl->first_read_done = TRUE;
			REPL_DPRINT2("Nothing read when looking for first replication record. Empty file %s\n", ctl->jnl_fn);
			return (SS_NORMAL);
		} else
		{
			if (status == EREPL_JNLRECFMT && adjust_for_eof_or_extension(ctl) != SS_NORMAL)
				rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
			else if (status == EREPL_JNLBADALIGN)
				rts_error(VARLSTCNT(5) ERR_JNLBADALIGN, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
		}
	}
	REPL_DPRINT4("FIRST READ of %s - Min seqno "INT8_FMT" EOF addr %d\n",
			ctl->jnl_fn, INT8_PRINT(ctl->min_seqno), ctl->repl_buff->fc->eof_addr);
	if (update_max_seqno_info(ctl) != SS_NORMAL)
	{
		assert(repl_errno == EREPL_JNLEARLYEOF);
		GTMASSERT; /* Program bug */
	}
	REPL_DPRINT4("FIRST READ of %s - Max seqno "INT8_FMT" EOF addr %d\n",
			ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), ctl->repl_buff->fc->eof_addr);
	ctl->first_read_done = TRUE;
	return (SS_NORMAL);
}

static void increase_buffer(uchar_ptr_t *buff, int *buflen, int buffer_needed)
{
	int 		newbuffsize, alloc_status;
	uchar_ptr_t	old_msgp;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	/* The tr size is not known apriori. Hence, a good guess of 1.5 times the current buffer space is used */
	newbuffsize = gtmsource_msgbufsiz + (gtmsource_msgbufsiz << 1);
	if (buffer_needed > newbuffsize)
		newbuffsize = buffer_needed;
	REPL_DPRINT3("Buff space shortage. Attempting to increase buff space. Curr buff space %d. Attempt increase to atleast %d\n",
		     gtmsource_msgbufsiz, newbuffsize);
	old_msgp = (uchar_ptr_t)gtmsource_msgp;
	if ((alloc_status = gtmsource_alloc_msgbuff(newbuffsize)) != SS_NORMAL)
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  LEN_AND_LIT("Error extending buffer space while reading files. Malloc error"), alloc_status);
	}
	*buff = (uchar_ptr_t)gtmsource_msgp + (*buff - old_msgp);
	*buflen = gtmsource_msgbufsiz - (*buff - (uchar_ptr_t)gtmsource_msgp);
	return;
}

static	int read_transaction(repl_ctl_element *ctl, uchar_ptr_t *buff, int *bufsiz, seq_num read_jnl_seqno)
{	/* Read the transaction ctl->seqno into buff. Position the next read at the next seqno in the journal file.
	 * Update max_seqno if necessary.  If read of the next seqno blocks, leave the read buffers as is.
	 * The next time when this journal file is accessed the read will move forward.
	 */
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	int			readlen;
	seq_num			rec_jnl_seqno;
	char			rectype;
	int			status;
	seq_num			read_seqno;
	unsigned char		*seq_num_ptr, seq_num_str[32];

	error_def(ERR_JNLBADALIGN);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_REPLCOMM);
	error_def(ERR_REPLBRKNTRANS);
	error_def(ERR_TEXT);

	rb = ctl->repl_buff;
	rb->buffindex = REPL_MAINBUFF;
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	ctl->read_complete = FALSE;
	readlen = 0;
	if (b->reclen > *bufsiz)
		increase_buffer(buff, bufsiz, b->reclen);
	assert(b->reclen <= *bufsiz);
	longcpy(*buff, b->recbuff, b->reclen);
	*buff += b->reclen;
	readlen += b->reclen;
	*bufsiz -= b->reclen;
	rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
	assert(IS_REPL_RECTYPE(rectype));
	QWASSIGN(rec_jnl_seqno, get_jnl_seqno((jnl_record *)b->recbuff));
	assert(QWEQ(rec_jnl_seqno, ctl->seqno));
	if (QWGT(rec_jnl_seqno, ctl->max_seqno))
	{
		QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
		ctl->max_seqno_dskaddr = b->recaddr;
	}
	ctl->tn = get_tn((jnl_record *)b->recbuff);
	if (JRT_SET == rectype || JRT_KILL == rectype || JRT_ZKILL == rectype || JRT_NULL == rectype)
	{	/* Entire transaction done */
		ctl->read_complete = TRUE;
		trans_read = TRUE;
	}
	assert(rectype != JRT_TCOM && rectype != JRT_ZTCOM); /* The first record shouldn't be a TCOM */
	/* Suggested optimisation : Instead of waiting for all records pertaining to this transaction to
	 * be written to the journal file, read those available, mark this file BLOCKED, read other journal
	 * files, and come back to this journal file later.
	 */
	while (!ctl->read_complete) /* Read the rest of the transaction */
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
			if (IS_REPL_RECTYPE(rectype))
			{
				if (b->reclen > *bufsiz)
					increase_buffer(buff, bufsiz, b->reclen);
				assert(b->reclen <= *bufsiz);
				if (rectype != JRT_TCOM && rectype != JRT_ZTCOM)
				{
					longcpy(*buff, b->recbuff, b->reclen);
					*buff += b->reclen;
					readlen += b->reclen;
				} else
				{
					longcpy(gtmsource_tcombuffp, b->recbuff, b->reclen);
					gtmsource_tcombuffp += b->reclen;
					tot_tcom_len += b->reclen;
					/* End of transaction in this file */
					ctl->read_complete = TRUE;
					if (num_tcom == -1)
						num_tcom = ((jnl_record *)b->recbuff)->val.jrec_tcom.participants;
					num_tcom--;
					if (num_tcom == 0) /* Read the whole trans */
						trans_read = TRUE;
				}
				*bufsiz -= b->reclen;
				QWASSIGN(rec_jnl_seqno, get_jnl_seqno((jnl_record *)b->recbuff));
				assert(QWEQ(rec_jnl_seqno, ctl->seqno));
				if (QWGT(rec_jnl_seqno, ctl->max_seqno))
				{
					QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
					ctl->max_seqno_dskaddr = b->recaddr;
				}
				ctl->tn = get_tn((jnl_record *)b->recbuff);
			} else if (rectype == JRT_EOF)
			{
				seq_num_ptr = i2ascl(seq_num_str, read_jnl_seqno);
				rts_error(VARLSTCNT(8) ERR_REPLBRKNTRANS, 2, seq_num_ptr - seq_num_str, seq_num_str,
						ERR_TEXT, 2, RTS_ERROR_LITERAL("Early EOF found"));
			}
		} else if (status == EREPL_JNLRECINCMPL)
		{	/* Log warning message for every certain number of attempts. There might have been a crash
			 * and the file might have been corrupted. The file possibly might never grow.
			 * Such cases have to be detected and attempt to read such files aborted.
			 */
			gtmsource_poll_actions(TRUE);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
			if ((total_wait_for_jnl_recs += GTMSOURCE_WAIT_FOR_JNL_RECS) % LOG_WAIT_FOR_JNL_RECS_PERIOD == 0)
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %dms for journal record(s)"
					 " to be written to journal file %s while attempting to read transaction "INT8_FMT". "
					 "Check for problems with journaling\n", total_wait_for_jnl_recs, ctl->jnl_fn,
					 INT8_PRINT(ctl->seqno));
			}
		} else
		{
			if (status == EREPL_JNLRECFMT && adjust_for_eof_or_extension(ctl) != SS_NORMAL)
				rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
			else if (status == EREPL_JNLBADALIGN)
				rts_error(VARLSTCNT(5) ERR_JNLBADALIGN, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
			else if (status != EREPL_JNLRECFMT)
				GTMASSERT;
		}
	}
	/* Try positioning next read to the next seqno. Leave it as is if operation blocks (has to wait for records) */
	QWADD(read_seqno, ctl->seqno, seq_num_one);
	/* Better to do a linear search */
	position_read(ctl, read_seqno, TR_LINEAR_SEARCH);
	return (readlen);
}

static	tr_search_state_t do_linear_search(repl_ctl_element *ctl, uint4 lo_addr, seq_num read_seqno)
{
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	seq_num			rec_jnl_seqno;
	char			rectype;
	tr_search_state_t	found;
	int			status;

	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_JNLBADALIGN);

	rb = ctl->repl_buff;
	rb->buffindex = REPL_MAINBUFF;
	b = &rb->buff[rb->buffindex];
	assert(lo_addr >= JNL_FILE_FIRST_RECORD);
	if (lo_addr != b->readaddr)
	{	/* Initiate a fresh read */
		b->recaddr = b->readaddr = lo_addr;
		b->recbuff = b->base;
		b->reclen = 0;
		b->buffremaining = REPL_BLKSIZE(rb);
	}	/* else use what has been read already */
	if (b->readaddr == JNL_FILE_FIRST_RECORD && adjust_buff_leaving_hdr(rb) != SS_NORMAL)
	{
		assert(repl_errno == EREPL_BUFFNOTFRESH);
		GTMASSERT;	/* Program bug */
	}
	found = TR_NOT_FOUND;
	while (found == TR_NOT_FOUND)
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = REF_CHAR(&((jnl_record *)b->recbuff)->jrec_type);
			if (IS_REPL_RECTYPE(rectype))
			{
				rec_jnl_seqno = get_jnl_seqno((jnl_record *)b->recbuff);
				if (QWLT(ctl->max_seqno, rec_jnl_seqno))
				{
					QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
					ctl->max_seqno_dskaddr = b->recaddr;
				}
				QWASSIGN(ctl->seqno, rec_jnl_seqno);
				ctl->tn = get_tn((jnl_record *)b->recbuff);
				if (QWEQ(rec_jnl_seqno, read_seqno))
					found = TR_FOUND;
				else if (QWGT(rec_jnl_seqno, read_seqno))
					found = TR_WILL_NOT_BE_FOUND;
			} else if (rectype == JRT_EOF)
				found = TR_WILL_NOT_BE_FOUND;
		} else if (status == EREPL_JNLRECINCMPL)
			found = TR_FIND_WOULD_BLOCK;
		else if (status == EREPL_JNLRECFMT && adjust_for_eof_or_extension(ctl) != SS_NORMAL)
			rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
		else if (status == EREPL_JNLBADALIGN)
			rts_error(VARLSTCNT(5) ERR_JNLBADALIGN, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
	}
	return (found);
}

static	tr_search_state_t do_binary_search(repl_ctl_element *ctl, uint4 lo_addr, uint4 hi_addr, seq_num read_seqno)
{
	GTMASSERT; /* Don't call it till 'tis coded */
	return 0;
}

static	int position_read(repl_ctl_element *ctl, seq_num read_seqno, tr_search_method_t srch_meth)
{
	int		status;
	repl_buff_t	*rb;
	uint4		lo_addr, hi_addr;

	/* Position read pointers so that the next read should get the first journal record with JNL_SEQNO atleast read_seqno.
	 * Do a search between min_seqno and seqno if read_seqno < ctl->seqno; else search from ctl->seqno onwards.
	 * If ctl->seqno > ctl->max_seqno update max_seqno as you move, else search between ctl->seqno and ctl->max_seqno.
	 * We want to do a binary search here. For now do a linear search.
	 */
	rb = ctl->repl_buff;
	if (QWLE(read_seqno, ctl->seqno))
	{
		lo_addr = JNL_BLK_DSKADDR(ctl->min_seqno_dskaddr, REPL_BLKSIZE(rb));
		assert(lo_addr != rb->buff[REPL_MAINBUFF].readaddr);
		hi_addr = JNL_BLK_DSKADDR(rb->buff[REPL_MAINBUFF].recaddr, REPL_BLKSIZE(rb));
		srch_meth = TR_BINARY_SEARCH;	/* Preferred over linear */
	} else		/* QWGT(read_seqno, ctl->seqno) */
	{
		lo_addr = rb->buff[REPL_MAINBUFF].readaddr;
		if (srch_meth == TR_BINARY_SEARCH && QWLE(read_seqno, ctl->max_seqno))
			hi_addr = JNL_BLK_DSKADDR(ctl->max_seqno_dskaddr, REPL_BLKSIZE(rb));
		else
			srch_meth = TR_LINEAR_SEARCH;
	}
	srch_meth = TR_LINEAR_SEARCH; /* force linear for now, binary_search is not yet ready */
	status = ((srch_meth == TR_BINARY_SEARCH) ?
		  	do_binary_search(ctl, lo_addr, hi_addr, read_seqno) : do_linear_search(ctl, lo_addr, read_seqno));
	return (SS_NORMAL);
}

static	int read_and_merge(uchar_ptr_t buff, int maxbufflen, seq_num read_jnl_seqno)
{
	int 			buff_avail, total_read, read_len, pass;
	boolean_t		brkn_trans;
	unsigned char		*seq_num_ptr, seq_num_str[32];
	repl_ctl_element	*ctl;

	error_def(ERR_REPLBRKNTRANS);

	trans_read = FALSE;
	num_tcom = -1;
	tot_tcom_len = 0;
	total_read = 0;
	total_wait_for_jnl_recs = 0;
	total_wait_for_jnlopen = 0;
	gtmsource_tcombuffp = gtmsource_tcombuff_start;
	buff_avail = maxbufflen;
	for (ctl = repl_ctl_list->next; ctl != NULL; ctl = ctl->next)
		ctl->read_complete = FALSE;
	for (pass = 1; !trans_read; pass++)
	{
		if (pass > 1)
		{
			gtmsource_poll_actions(TRUE);
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNLOPEN);
			if ((total_wait_for_jnlopen += GTMSOURCE_WAIT_FOR_JNLOPEN) % LOG_WAIT_FOR_JNLOPEN_PERIOD == 0)
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %dms for journal file(s) "
					 "to be opened, or updated while attempting to read transaction "INT8_FMT". Check for "
					 "problems with journaling\n", total_wait_for_jnlopen, INT8_PRINT(read_jnl_seqno));
		}
		read_len = read_regions(&buff, &buff_avail, pass > 1, &brkn_trans, read_jnl_seqno);
		if (brkn_trans)
		{
			seq_num_ptr = i2ascl(seq_num_str, read_jnl_seqno);
			rts_error(VARLSTCNT(4) ERR_REPLBRKNTRANS, 2, seq_num_ptr - seq_num_str, seq_num_str);
		}
		total_read += read_len;
	}
	if (tot_tcom_len > 0)
	{	/* Copy all the TCOM records to the end of the buffer */
		assert(tot_tcom_len <= ((uchar_ptr_t)gtmsource_msgp + gtmsource_msgbufsiz - buff));
		longcpy(buff, gtmsource_tcombuff_start, tot_tcom_len);
		total_read += tot_tcom_len;
	}
	return (total_read);
}

static	int read_regions(uchar_ptr_t *buff, int *buff_avail,
		         boolean_t attempt_open_oldnew, boolean_t *brkn_trans, seq_num read_jnl_seqno)
{
	repl_ctl_element	*ctl, *prev_ctl, *old_ctl;
	gd_region		*region;
	tr_search_state_t	found;
	int			read_len, cumul_read;
	int			nopen;
	unsigned char		seq_num_str[32], *seq_num_ptr;

	cumul_read = 0;
	*brkn_trans = TRUE;
	assert(repl_ctl_list->next != NULL);
	/* For each region */
	for (ctl = repl_ctl_list->next, prev_ctl = repl_ctl_list; ctl != NULL && !trans_read; prev_ctl = ctl, ctl = ctl->next)
	{
		found = TR_NOT_FOUND;
		region = ctl->reg;
		while (found == TR_NOT_FOUND)
		{	/* Find the generation of the journal file which has read_jnl_seqno */
			for ( ; ctl != NULL && ctl->reg == region &&
				((ctl->file_state == JNL_FILE_CLOSED && QWGT(read_jnl_seqno, ctl->max_seqno))
					|| (ctl->file_state == JNL_FILE_EMPTY
						&& QWGE(read_jnl_seqno, ctl->repl_buff->fc->jfh->start_seqno)));
					prev_ctl = ctl, ctl = ctl->next)
				;
			if (ctl == NULL || ctl->reg != region)
			{	/* Hit the end of generation list for journal file */
				if (!attempt_open_oldnew)
				{	/* Reposition to skip prev_ctl */
					REPL_DPRINT2("First pass...not opening newer gener file...skipping %s\n", prev_ctl->jnl_fn);
					ctl = prev_ctl;
					prev_ctl = ctl->prev;
					found = TR_FIND_WOULD_BLOCK;
					continue;
				}
				nopen = open_newer_gener_jnlfiles(region, prev_ctl);
				if (nopen > 0) /* Newer gener files opened */
				{
					if (prev_ctl->file_state == JNL_FILE_CLOSED)
					{	/* Recently updated journal file, search this */
						ctl = prev_ctl;
						prev_ctl = ctl->prev;
						REPL_DPRINT3("Attempt search in %s. Next gener is %s\n",
								ctl->jnl_fn, ctl->next->jnl_fn);
					} else
					{	/* Search next gener onwards */
						ctl = prev_ctl->next;
						prev_ctl = ctl->prev;
						REPL_DPRINT3("Skipping empty gener %s. Moving to gener %s\n",
								prev_ctl->jnl_fn, ctl->jnl_fn);
					}
				} else if (nopen == 0)
				{	/* None opened, the journal file hasn't been written into.
					 * Reposition to skip the prev_ctl.
					 */
					ctl = prev_ctl;
					prev_ctl = ctl->prev;
					found = TR_FIND_WOULD_BLOCK;
				}
			} else if (ctl->file_state == JNL_FILE_UNREAD)
			{
				if (!ctl->first_read_done || attempt_open_oldnew)
					first_read(ctl);
				if (ctl->file_state == JNL_FILE_UNREAD)
				{
					REPL_DPRINT2("First read of %s. Nothing yet written to this file\n", ctl->jnl_fn);
					if (ctl->prev->reg == ctl->reg &&
					    QWGT(ctl->repl_buff->fc->jfh->start_seqno, read_jnl_seqno))
					{	/* Prev gener opened already. Looking for read_jnl_seqno in this gener,
						 * but the first possible seqno in this gener is > read_jnl_seqno.
						 */
						found = TR_WILL_NOT_BE_FOUND;
						continue;
					}
					if (ctl->prev->reg == ctl->reg)
					{	/* Nothing yet written to the file. Attempt opening next gener. */
						assert(QWLE(ctl->prev->repl_buff->fc->jfh->start_seqno,
								ctl->repl_buff->fc->jfh->start_seqno));
						prev_ctl = ctl;
						ctl = ctl->next;
					} else if (attempt_open_oldnew)
					{
						if (open_prev_gener(&old_ctl, ctl, read_jnl_seqno) == 0)
						{
							if (QWGT(ctl->repl_buff->fc->jfh->start_seqno, read_jnl_seqno))
							{
								found = TR_WILL_NOT_BE_FOUND;
								continue;
							}
							/* Nothing yet written to this file. Attempt opening next gener */
							prev_ctl = ctl;
							ctl = ctl->next;
						} else
						{
							assert(old_ctl->file_state == JNL_FILE_CLOSED ||
									old_ctl->file_state == JNL_FILE_EMPTY);
							if (old_ctl->file_state == JNL_FILE_EMPTY ||
									QWGT(old_ctl->min_seqno, read_jnl_seqno))
							{	/* Give the other regions a chance to
								 * open their previous generations.
								 */
								REPL_DPRINT2("Skipping to other regions. Not reading from "
										"previous generation file %s\n", old_ctl->jnl_fn);
								found = TR_FIND_WOULD_BLOCK;
								continue;
							}
							/* Search the prev gener and backwards */
							ctl = old_ctl;
							prev_ctl = old_ctl->prev;
							REPL_DPRINT2("Attempt searching in %s and backwards\n", ctl->jnl_fn);
						}
					} else
					{
						REPL_DPRINT2("First pass...skipping UNREAD file %s\n", ctl->jnl_fn);
						found = TR_FIND_WOULD_BLOCK;
						continue;
					}
				}
			} else if (ctl->file_state == JNL_FILE_EMPTY ||
				   QWGT(ctl->min_seqno, read_jnl_seqno))
			{	/* May be in prev gener */
				if (ctl->prev->reg == ctl->reg && ctl->file_state != JNL_FILE_EMPTY)
				{	/* If prev gener is already open, and we come here, we are looking for
					 * a seqno between prev gener's max_seqno and this gener's min_seqno.
					 * This region is not part of the transaction. Skip to the next region.
					 */
					REPL_DPRINT3("Gap between %s (max seqno "INT8_FMT,
							ctl->prev->jnl_fn, INT8_PRINT(ctl->max_seqno));
					REPL_DPRINT3(") and %s (min seqno "INT8_FMT, ctl->jnl_fn, INT8_PRINT(ctl->min_seqno));
					REPL_DPRINT2(") found while looking for "INT8_FMT"\n", INT8_PRINT(read_jnl_seqno));
					assert(ctl->prev->file_state == JNL_FILE_CLOSED &&
						QWLT(ctl->prev->max_seqno, read_jnl_seqno) ||
						ctl->prev->file_state == JNL_FILE_EMPTY);
					found = TR_WILL_NOT_BE_FOUND;
					continue;
				}
				if (ctl->prev->reg == ctl->reg)
				{	/* Skip the empty gener */
					REPL_DPRINT2("Skipping empty journal file %s\n", ctl->jnl_fn);
					prev_ctl = ctl;
					ctl = ctl->next;
					continue;
				}
				if (!attempt_open_oldnew)
				{
					REPL_DPRINT2("First pass...not opening prev gener file...skipping %s\n", ctl->jnl_fn);
					found = TR_FIND_WOULD_BLOCK;
					continue;
				}
				/* Need to open prev gener */
				if (open_prev_gener(&old_ctl, ctl, read_jnl_seqno) == 0)
				{
					if (ctl->file_state != JNL_FILE_EMPTY)
						found = TR_WILL_NOT_BE_FOUND;
					else
					{	/* Skip the empty generation */
						REPL_DPRINT2("Skipping empty journal file %s\n", ctl->jnl_fn);
						prev_ctl = ctl;
						ctl = ctl->next;
					}
					continue;
				}
				assert(old_ctl->file_state == JNL_FILE_CLOSED || old_ctl->file_state == JNL_FILE_EMPTY);
				if (old_ctl->file_state == JNL_FILE_EMPTY || QWGT(old_ctl->min_seqno, read_jnl_seqno))
				{	/* Give the other regions a chance to open their previous generations */
					found = TR_FIND_WOULD_BLOCK;
					REPL_DPRINT2("Skipping to other regions. Not reading from previous generation file %s\n",
							old_ctl->jnl_fn);
					continue;
				}
				/* Search the prev gener and backwards */
				ctl = old_ctl;
				prev_ctl = old_ctl->prev;
				REPL_DPRINT2("Attempt searching in %s and backwards\n", ctl->jnl_fn);
			} else		/* ctl->file_state == JNL_FILE_OPEN || * QWLE(read_jnl_seqno, ctl->max_seqno) */
			{
				if (ctl->lookback)
				{
					assert(QWLE(read_jnl_seqno, ctl->seqno));
					assert(ctl->file_state == JNL_FILE_OPEN || ctl->file_state == JNL_FILE_CLOSED);
					REPL_DPRINT4("Looking back and attempting to position read for %s at "
							INT8_FMT". File state is %s\n", ctl->jnl_fn, INT8_PRINT(read_jnl_seqno),
							(ctl->file_state == JNL_FILE_OPEN) ? "OPEN" : "CLOSED");
					position_read(ctl, read_jnl_seqno, TR_BINARY_SEARCH);
					ctl->lookback = FALSE;
				}
				if (QWEQ(read_jnl_seqno, ctl->seqno))
				{	/* Found it */
					if (!ctl->read_complete)
					{
						if ((read_len = read_transaction(ctl, buff, buff_avail, read_jnl_seqno)) < 0)
							assert(repl_errno == EREPL_JNLEARLYEOF);
						cumul_read += read_len;
					}
					found = TR_FOUND;
				} else if (QWLT(read_jnl_seqno, ctl->seqno))
				{	/* This region is not involved in transaction read_jnl_seqno */
					found = TR_WILL_NOT_BE_FOUND;
				} else	/* QWGT(read_jl_seqno, ctl->seqno) */
				{
					if (ctl->file_state == JNL_FILE_OPEN)
					{	/* State change from READ_FILE->READ_POOL-> READ_FILE might cause this.
						 * The journal files have grown since the transition from READ_FILE to READ_POOL
						 * was made. Update ctl info.
						 */
						if (update_max_seqno_info(ctl) != SS_NORMAL)
						{
							assert(repl_errno == EREPL_JNLEARLYEOF);
							GTMASSERT; /* Program bug */
						}
						if (QWLE(read_jnl_seqno, ctl->max_seqno))
						{	/* May be found in this journal file,
							 * attempt to position next read to read_jnl_seqno
							 */
							force_file_read(ctl); /* Buffer might be stale with an EOF record */
							position_read(ctl, read_jnl_seqno, TR_BINARY_SEARCH);
						} else
						{	/* Will possibly be found in next gener */
							prev_ctl = ctl;
							ctl = ctl->next;
						}
					} else if (ctl->file_state == JNL_FILE_CLOSED)
					{	/* May be found in this jnl file, attempt to position next read to read_jnl_seqno */
						position_read(ctl, read_jnl_seqno, TR_BINARY_SEARCH);
					} else
					{
						GTMASSERT; /* Program bug - ctl_seqno should never be greater than ctl->max_seqno */
					}
				}
			}
		}
		/* Move to the next region, now that the tr has been found or will not be found */
		*brkn_trans = (*brkn_trans && found == TR_WILL_NOT_BE_FOUND);
		for ( ; ctl->next != NULL && ctl->next->reg == region; prev_ctl = ctl, ctl = ctl->next)
			;
	}
	return (cumul_read);
}

int gtmsource_readfiles(uchar_ptr_t buff, int *data_len, int maxbufflen)
{

	int		read_size;
	unsigned char	seq_num_str[32], *seq_num_ptr;
	unsigned char	seq_num_str1[32], *seq_num_ptr1;

#ifdef REPL_DEBUG
	seq_num         repl_dbg_save_read_jnl_seqno;
#endif
	REPL_DPRINT2("Reading "INT8_FMT" from Journal Files\n", INT8_PRINT(jnlpool.gtmsource_local->read_jnl_seqno));
	assert(buff == &gtmsource_msgp->msg[0]); /* else increasing buffer space will not work */
	read_size = read_and_merge(buff, maxbufflen, jnlpool.gtmsource_local->read_jnl_seqno);
	if (QWLE(gtmsource_save_read_jnl_seqno, jnlpool.gtmsource_local->read_jnl_seqno))
	{
		QWINCRBYDW(jnlpool.gtmsource_local->read_addr, (read_size + sizeof(jnldata_hdr_struct)));
#ifdef REPL_DEBUG
		QWASSIGN(repl_dbg_save_read_jnl_seqno, jnlpool.gtmsource_local->read_jnl_seqno);
		QWINCRBY(repl_dbg_save_read_jnl_seqno, seq_num_one);
		REPL_DPRINT2("Readfiles : after sync with pool read_seqno : "INT8_FMT, INT8_PRINT(repl_dbg_save_read_jnl_seqno));
		REPL_DPRINT3(" read_addr : "INT8_FMT" read : %d\n", INT8_PRINT(jnlpool.gtmsource_local->read_addr),
				jnlpool.gtmsource_local->read);
#endif
		if (jnlpool_hasnt_overflowed(jnlpool.gtmsource_local->read_addr)) /* No more overflow, switch to READ_POOL */
		{
			jnlpool.gtmsource_local->read = QWMODDW(jnlpool.gtmsource_local->read_addr,
					jnlpool.jnlpool_ctl->jnlpool_size);
			jnlpool.gtmsource_local->read_state = READ_POOL;
		}
	}
	QWINCRBY(jnlpool.gtmsource_local->read_jnl_seqno, seq_num_one);
#ifdef GTMSOURCE_ALWAYS_READ_FILES
	jnlpool.gtmsource_local->read_state = READ_FILE;
#endif
	if (jnlpool.gtmsource_local->read_state == READ_POOL)
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal pool. Tr num = "INT8_FMT"\n",
				INT8_PRINT(jnlpool.gtmsource_local->read_jnl_seqno));
	*data_len = read_size;
	return (0);
}

static	int scavenge_closed_jnl_files(seq_num ack_seqno)	/* currently  not used */
{	/* Run thru the repl_ctl_list and scavenge for those journal files which are no longer required for
	 * replication (the receiver side has acknowledged that it has successfully processed journal recs upto
	 * and including those with JNL_SEQNO ack_seqno). Close these journal files and report to the operator
	 * that these files are no longer needed for replication so that the operator can take these files off-line.
	 */
	boolean_t		scavenge;
	repl_ctl_element	*ctl, *prev_ctl;

	for (prev_ctl = repl_ctl_list, ctl = repl_ctl_list->next; ctl != NULL; prev_ctl = ctl, ctl = ctl->next)
	{
		if (!ctl->next || ctl->next->reg != ctl->reg)
			open_newer_gener_jnlfiles(ctl->reg, ctl);
		/* following two switche blocks cannot be merged as file_state could change in the first switch block */
		switch(ctl->file_state)
		{
			case JNL_FILE_CLOSED :
			case JNL_FILE_EMPTY :
				break;
			case JNL_FILE_OPEN :
				if (update_max_seqno_info(ctl) != SS_NORMAL)
				{
					assert(repl_errno == EREPL_JNLEARLYEOF);
					GTMASSERT; /* Program bug */
				}
				break;
			case JNL_FILE_UNREAD :
				first_read(ctl);
				break;
		}
		switch(ctl->file_state)
		{
			case JNL_FILE_CLOSED :
				scavenge = (QWGE(ack_seqno, ctl->max_seqno) && ctl->next && ctl->next->reg == ctl->reg);
					/* There should exist a next generation */
				break;
			case JNL_FILE_EMPTY :
				/* Previous generation should have been scavenged and the
				 * ack_seqno should be in one of the next generations.
				 */
				scavenge = (ctl->prev->reg != ctl->reg && ctl->next && ctl->next->reg == ctl->reg
						&& (ctl->next->file_state == JNL_FILE_OPEN
							|| ctl->next->file_state == JNL_FILE_CLOSED)
						&& QWGE(ctl->next->min_seqno, ack_seqno));
				break;
			default :
				scavenge = FALSE;
				break;
		}
		if (scavenge)
		{
			ctl->prev->next = ctl->next;
			ctl->next->prev = ctl->prev;
			REPL_DPRINT2("Journal file %s no longer needed for replication\n", ctl->jnl_fn);
			repl_ctl_close(ctl);
		}
	}
	return 0;
}

int gtmsource_update_resync_tn(seq_num resync_seqno)
{
	repl_ctl_element	*ctl, *prev_ctl;
	gd_region		*region;
	sgmnt_addrs		*csa;
	int			read_size;
	unsigned char		seq_num_str[32], *seq_num_ptr;

	REPL_DPRINT2("UPDATING RESYNC TN with seqno "INT8_FMT"\n", INT8_PRINT(resync_seqno));
	gtmsource_ctl_close();
	gtmsource_ctl_init();
	read_size = read_and_merge((uchar_ptr_t)&gtmsource_msgp->msg[0], gtmsource_msgbufsiz - REPL_MSG_HDRLEN, resync_seqno);
	for (ctl = repl_ctl_list->next, prev_ctl = repl_ctl_list; ctl != NULL; )
	{
		for (region = ctl->reg;
		     ctl != NULL && ctl->reg == region && (ctl->file_state == JNL_FILE_OPEN || ctl->file_state == JNL_FILE_CLOSED);
			prev_ctl = ctl, ctl = ctl->next)
		     ;
		if (ctl == NULL || ctl->reg != region || prev_ctl->reg == region)
		{
			csa = &FILE_INFO(region)->s_addrs;
			csa->hdr->resync_tn = prev_ctl->tn;
			REPL_DPRINT3("RESYNC TN for %s is %d\n", prev_ctl->jnl_fn, prev_ctl->tn);
		} else
		{	/* The only ctl entry for this region is empty or unread */
			assert(FILE_INFO(region)->s_addrs.hdr->resync_tn == 1);
		}
		/* Move to the next region */
		for (; ctl != NULL && ctl->reg == region;
		       prev_ctl = ctl, ctl = ctl->next);
	}
	return (SS_NORMAL);
}
