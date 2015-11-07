/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stddef.h>	/* for offsetof macro */
#include "gtm_socket.h"
#include "gtm_inet.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
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
#include "iosb_disk.h"
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
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "is_file_identical.h"
#include "repl_log.h"
#include "min_max.h"
#include "error.h"
#include "repl_tr_good.h"

#define LOG_WAIT_FOR_JNL_RECS_PERIOD	(10 * 1000) /* ms */
#define LOG_WAIT_FOR_JNLOPEN_PERIOD	(10 * 1000) /* ms */

#define LSEEK_ERR_STR		"Error in lseek"
#define READ_ERR_STR		"Error in read"
#define UNKNOWN_ERR_STR		"Error unknown"

GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF	gd_region		*gv_cur_region;
GBLREF	FILE			*gtmsource_log_fp;

error_def(ERR_JNLBADRECFMT);
error_def(ERR_NOPREVLINK);
error_def(ERR_REPLBRKNTRANS);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLFILIOERR);
error_def(ERR_TEXT);

static	int4			num_tcom = -1;
static	boolean_t		trans_read = FALSE;
static	int			tot_tcom_len = 0;
static	int			total_wait_for_jnl_recs = 0;
static	int			total_wait_for_jnlopen = 0;
static	unsigned char		*tcombuffp = NULL;

static	int			adjust_buff_leaving_hdr(repl_buff_t *rb);
static	tr_search_state_t	position_read(repl_ctl_element*, seq_num);
static	int			read_regions(
					unsigned char **buff, int *buff_avail,
					boolean_t attempt_open_oldnew,
					boolean_t *brkn_trans,
					seq_num read_jnl_seqno);

static	int			first_read(repl_ctl_element*);
static	int			update_max_seqno_info(repl_ctl_element *ctl);
static	int			scavenge_closed_jnl_files(seq_num ack_seqno);
static	int			update_eof_addr(repl_ctl_element *ctl, int *eof_change);

static	int repl_read_file(repl_buff_t *rb)
{
	repl_buff_desc 		*b;
	repl_file_control_t	*fc;
	int			nb;
	sgmnt_addrs		*csa;
	uint4			dskaddr;
	uint4			read_less, status;
	int			eof_change;
	VMS_ONLY(
		io_status_block_disk	iosb;
		uint4		read_off;
		uint4		extra_bytes;
		sm_uc_ptr_t 	read_buff;
		DEBUG_ONLY(unsigned char	verify_buff[DISK_BLOCK_SIZE];)
	)

	fc = rb->fc;
	b = &rb->buff[rb->buffindex];
	csa = &FILE_INFO(rb->backctl->reg)->s_addrs;
	read_less = 0;
	assert(b->readaddr >= b->recaddr);
	assert(0 < b->buffremaining);
	dskaddr = csa->jnl->jnl_buff->dskaddr;
	if (!is_gdid_gdid_identical(&fc->id, JNL_GDID_PTR(csa)))
	{
		if (!rb->backctl->eof_addr_final)
			update_eof_addr(rb->backctl, &eof_change); /* update possible change in end_of_data, re-read file header */
		assert(!fc->jfh->crash);
		dskaddr = fc->jfh->end_of_data;
		if (0 == fc->jfh->prev_recov_end_of_data) /* file not virtually truncated by recover/rollback */
			dskaddr += EOF_RECLEN;
	}
	/* Make sure we do not read beyond end of data in the journal file */
	/* Note : This logic is always needed when journal file is pre-allocated.
	 * With no pre-allocation, this logic is needed only when repl_read_file is called from
	 * update_max_seqno_info -> repl_next. Specifically, this logic is needed till the existing
	 * JRT_EOF record is completely overwritten and the file grows beyond its existing size.
	 */
	assert(b->readaddr <= dskaddr);
	if (b->buffremaining > (dskaddr - b->readaddr))	/* note the ordering of the operands to take care of 4G overflow */
	{
		if (b->readaddr == dskaddr)
		{
			REPL_DPRINT3("READ FILE : Jnl file %s yet to grow from (or ends at) offset %u\n",
					rb->backctl->jnl_fn, b->readaddr);
			return (SS_NORMAL);
		}
		read_less = b->buffremaining - (dskaddr - b->readaddr);	/* explicit ordering to take care of 4G overflow */
		REPL_DPRINT5("READ FILE : Racing with jnl file %s avoided. Read size reduced from %u to %u at offset %u\n",
				rb->backctl->jnl_fn, b->buffremaining, b->buffremaining - read_less, b->readaddr);
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
	status = sys$qiow(EFN$C_ENF, fc->fd, IO$_READVBLK, &iosb, 0, 0, read_buff, nb + extra_bytes,
			  DIVIDE_ROUND_DOWN(read_off, DISK_BLOCK_SIZE) + 1, 0, 0, 0);
	if (SYSCALL_SUCCESS(status) && ((SYSCALL_SUCCESS(status = iosb.cond)) || SS$_ENDOFFILE == status))
	{
		nb = iosb.length;	/* num bytes actually read */
		nb -= extra_bytes;	/* that we are interested in */
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
		return (SS_NORMAL);
	}
	repl_errno = EREPL_JNLFILEREAD;
	return (status);
}

static	int repl_next(repl_buff_t *rb)
{
	repl_buff_desc		*b;
	int4			reclen;
	jrec_suffix		*suffix;
	uint4			maxreclen;
	int			status, sav_buffremaining;
	char			err_string[BUFSIZ];


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
				MEMCPY_LIT(err_string, LSEEK_ERR_STR);
			else if (repl_errno == EREPL_JNLFILEREAD)
				MEMCPY_LIT(err_string, READ_ERR_STR);
			else
				MEMCPY_LIT(err_string, UNKNOWN_ERR_STR);
			rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, rb->backctl->jnl_fn_len, rb->backctl->jnl_fn,
			  	  ERR_TEXT, 2, LEN_AND_STR(err_string), status);
		}
	}
	maxreclen = ((b->base + REPL_BLKSIZE(rb)) - b->recbuff) - b->buffremaining;
	assert(maxreclen > 0);
	if (maxreclen > JREC_PREFIX_UPTO_LEN_SIZE &&
		(reclen = ((jrec_prefix *)b->recbuff)->forwptr) <= maxreclen &&
		IS_VALID_JNLREC((jnl_record *)b->recbuff, rb->fc->jfh))
	{
		b->reclen = reclen;
		return SS_NORMAL;
	}
	repl_errno = (maxreclen > JREC_PREFIX_SIZE && reclen <= maxreclen) ? EREPL_JNLRECFMT : EREPL_JNLRECINCMPL;
	b->reclen = 0;
	return repl_errno;
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
			(char *)ctl->repl_buff->fc->jfh->prev_jnl_file_name, FALSE);
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
	int			nopen, n;
	int			status;
	gd_region		*r_save;
	uint4			jnl_status;
	boolean_t		do_jnl_ensure_open;
	gd_id_ptr_t		reg_ctl_end_id;

	/* Attempt to open newer generation journal files. Return the number of new files opened. Create new
	 * ctl element(s) for each newer generation and attach at reg_ctl_end. Work backwards from the current journal file.
	 */
	jnl_status = 0;
	nopen = 0;
	csa = &FILE_INFO(reg)->s_addrs;
	reg_ctl_end_id = &reg_ctl_end->repl_buff->fc->id;
	/* Note that at this point, journaling might have been turned OFF (e.g. REPL_WAS_ON state) in which case
	 * JNL_GDID_PTR(csa) would have been nullified by jnl_file_lost. Therefore comparing with that is not a good idea
	 * to use the "id" to check if the journal file remains same (this was done previously). Instead use the ID of
	 * the current reg_ctl_end and the NAME of the newly opened journal file. Because we dont have crit, we cannot
	 * safely read the journal file name from the file header therefore we invoke repl_ctl_create unconditionally
	 * (that has safety protections for crit) and use the new_ctl that it returns to access the journal file name
	 * returned and use that for the ID to NAME comparison.
	 */
	jnl_fn_len = 0; jnl_fn[0] = '\0';
	for (do_jnl_ensure_open = TRUE; ; do_jnl_ensure_open = FALSE)
	{
		repl_ctl_create(&new_ctl, reg, jnl_fn_len, jnl_fn, do_jnl_ensure_open);
		if (do_jnl_ensure_open && is_gdid_file_identical(reg_ctl_end_id, new_ctl->jnl_fn, new_ctl->jnl_fn_len))
		{	/* Current journal file in db file header has been ALREADY opened by source server. Return right away */
			assert(0 == nopen);
			repl_ctl_close(new_ctl);
			return (nopen);
		}
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
		if ('\0' == jnl_fn[0])
		{ /* prev link has been cut, can't follow path back from latest generation jnlfile to the latest we had opened */
			rts_error(VARLSTCNT(4) ERR_NOPREVLINK, 2, new_ctl->jnl_fn_len, new_ctl->jnl_fn);
		}
		if (is_gdid_file_identical(&reg_ctl_end->repl_buff->fc->id, jnl_fn, jnl_fn_len))
			break;
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
	repl_buff_t		*rb;

	assert(!ctl->eof_addr_final); /* The caller should invoke us ONLY if ctl->eof_addr_final is FALSE */
	csa = &FILE_INFO(ctl->reg)->s_addrs;
	rb = ctl->repl_buff;
	fc = rb->fc;
	prev_eof_addr = fc->eof_addr;
	*eof_change = 0;
	if (is_gdid_gdid_identical(&fc->id, JNL_GDID_PTR(csa)))
	{
		new_eof_addr = csa->jnl->jnl_buff->dskaddr;
		REPL_DPRINT3("Update EOF : New EOF addr from SHM for %s is %u\n", ctl->jnl_fn, new_eof_addr);
	} else
	{
		REPL_DPRINT2("Update EOF : New EOF addr will be found from jnl file hdr for %s\n", ctl->jnl_fn);
		UNIX_ONLY(
			REPL_DPRINT4("Update EOF : FC ID IS %u %d %u\n", fc->id.inode, fc->id.device, fc->id.st_gen);
			REPL_DPRINT4("Update EOF : csa->nl->jnl_file.u (unreliable) is %u %d %u\n", csa->nl->jnl_file.u.inode,
					csa->nl->jnl_file.u.device,  csa->nl->jnl_file.u.st_gen);
		)
		if (!ctl->eof_addr_final)
		{
			F_READ_BLK_ALIGNED(fc->fd, 0, fc->jfh, REAL_JNL_HDR_LEN, status);
			if (SS_NORMAL != status)
				rts_error(VARLSTCNT(9) ERR_REPLFILIOERR, 2, ctl->jnl_fn_len, ctl->jnl_fn,
						ERR_TEXT, 2, RTS_ERROR_LITERAL("Error in reading jfh in update_eof_addr"), status);
			REPL_DPRINT2("Update EOF : Jnl file hdr refreshed from file for %s\n", ctl->jnl_fn);
			ctl->eof_addr_final = TRUE; /* No more updates to fc->eof_addr for this journal file */
		}
		new_eof_addr = fc->jfh->end_of_data + EOF_RECLEN;
		REPL_DPRINT3("Update EOF : New EOF addr from jfh for %s is %u\n", ctl->jnl_fn, new_eof_addr);
	}
	/* ensure that new_eof_addr is not less than prev_eof_addr.
	 * the only scenario where this need not be TRUE is the following case
	 * 	(i) if new_eof_addr == (journal file header's end_of_data + size of eof record) and
	 *	(ii) if prev_eof_addr == the next REPL_BLKSIZE boundary (since source server reads in chunks of REPL_BLKSIZE)
	 * in the above case, source server's read of REPL_BLKSIZE bytes while trying to read at offset end_of_data would
	 * 	have succeeded since the whole REPL_BLKSIZE block has been allocated in the journal file both in Unix and VMS.
	 * but only the first EOF_RECLEN bytes of that block is valid data.
	 * For the case new_eof_addr < prev_eof_addr, the assert could have been
	 * DIVIDE_ROUND_UP(prev_eof_addr, REPL_BLKSIZE(rb)) == DIVIDE_ROUND_UP(new_eof_addr, REPL_BLKSIZE(rb)), but we use
	 * DIVIDE_ROUND_DOWN and account for prev_eof_addr being an exact multiple of REPL_BLKSIZE(rb) to avoid 4G overflow
	 */
	assert((new_eof_addr >= prev_eof_addr)
		|| (DIVIDE_ROUND_DOWN(prev_eof_addr, REPL_BLKSIZE(rb)) - ((0 == prev_eof_addr % REPL_BLKSIZE(rb)) ? 1 : 0)
		    == DIVIDE_ROUND_DOWN(new_eof_addr, REPL_BLKSIZE(rb))));
	fc->eof_addr = new_eof_addr;
	/* eof_change calculated below is not used anywhere. In case it needs to be used, the below calculation
	 * 	has to be reexamined in light of the above assert involving new_eof_addr and prev_eof_addr
	 * although the code below is dead for now, it can be used for future performance enhancements.
	 */
	*eof_change = new_eof_addr > prev_eof_addr ? (int4)(new_eof_addr - prev_eof_addr) : -(int4)(prev_eof_addr - new_eof_addr);
		/* Above computation done that way because the variables involved are unsigned */
	return (SS_NORMAL);
}

static	int force_file_read(repl_ctl_element *ctl)
{	/* The journal file may have grown since we last read it. A previously read EOF record may have been over-written on disk.
	 * Reposition read pointers so that a file read is forced if the current read pointers are positioned at a EOF record.
	 */
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	enum jnl_record_type	rectype;

	rb = ctl->repl_buff;
	b = &rb->buff[REPL_MAINBUFF];
	fc = rb->fc;
	if (b->reclen == 0 || b->recaddr == b->readaddr || b->buffremaining == 0 || b->buffremaining == REPL_BLKSIZE(rb))
	{	/* A file read will be forced anyway */
		return (SS_NORMAL);
	}
	/* b->recbuff points to valid record */
	rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
	assert(rectype > JRT_BAD && rectype <= JRT_RECTYPES);
	if (rectype != JRT_EOF) /* Can't be stale */
		return (SS_NORMAL);
	assert(b->reclen == EOF_RECLEN);
	assert(b->readaddr - b->recaddr >= EOF_RECLEN);
	b->buffremaining += (b->readaddr - b->recaddr);
	b->readaddr = b->recaddr;
	b->reclen = 0;
	return (SS_NORMAL);
}

static	int update_max_seqno_info(repl_ctl_element *ctl)
{	/* The information in ctl is outdated. The journal file has grown. Update max_seqno and its dskaddr */
	int 			eof_change;
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	uint4			dskread, stop_at;
	boolean_t		max_seqno_found;
	uint4			max_seqno_addr;
	seq_num			max_seqno, reg_seqno;
	int			status;
	enum jnl_record_type	rectype;
	gd_region		*reg;
	sgmnt_addrs		*csa;

	assert(ctl->file_state == JNL_FILE_OPEN);

	rb = ctl->repl_buff;
	fc = rb->fc;
	reg = ctl->reg;
	csa = &FILE_INFO(reg)->s_addrs;
	if (!ctl->eof_addr_final)
		update_eof_addr(ctl, &eof_change);
#	ifdef GTMSOURCE_SKIP_DSKREAD_WHEN_NO_EOF_CHANGE
	/* This piece of code needs some more testing - Vinaya 03/11/98 */
	if ((0 == eof_change) && ctl->first_read_done)
	{
		REPL_DPRINT2("UPDATE MAX SEQNO INFO: No change in EOF addr. Skipping disk read for %s\n", ctl->jnl_fn);
		return (SS_NORMAL);
	}
#	endif
	if (fc->eof_addr == JNL_FILE_FIRST_RECORD)
	{
		repl_errno = EREPL_JNLEARLYEOF;
		return (repl_errno);
	}
	QWASSIGN(reg_seqno, csa->hdr->reg_seqno);
	QWDECRBYDW(reg_seqno, 1);
	assert(!ctl->max_seqno_final || ctl->eof_addr_final);
 	if (QWGE(ctl->max_seqno, reg_seqno) || (ctl->max_seqno_final && ctl->first_read_done))
 	{	/* have searched already */
 		REPL_DPRINT4("UPDATE MAX SEQNO INFO : not reading file %s; max_seqno = "INT8_FMT", reg_seqno = "INT8_FMT"\n",
 			     ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), INT8_PRINT(reg_seqno));
 		return (SS_NORMAL);
 	}
	rb->buffindex = REPL_SCRATCHBUFF;	/* temporarily set to scratch buffer */
	b = &rb->buff[rb->buffindex];
	dskread = ROUND_DOWN(fc->eof_addr, REPL_BLKSIZE(rb));
	if (dskread == fc->eof_addr)
		dskread -= REPL_BLKSIZE(rb);
	QWASSIGN(max_seqno, seq_num_zero);
	max_seqno_addr = 0;
	max_seqno_found = FALSE;
	do
	{
		/* Ignore the existing contents of scratch buffer */
		b->buffremaining = REPL_BLKSIZE(rb);
		b->recbuff = b->base;
		b->reclen = 0;
		b->readaddr = b->recaddr = JNL_BLK_DSKADDR(dskread, REPL_BLKSIZE(rb));
		if (b->readaddr == JNL_FILE_FIRST_RECORD && adjust_buff_leaving_hdr(rb) != SS_NORMAL)
		{
			assert(repl_errno == EREPL_BUFFNOTFRESH);
			GTMASSERT; /* Program bug */
		}
		stop_at = dskread + MIN(REPL_BLKSIZE(rb), fc->eof_addr - dskread); /* Limit search to this block */
		/* If we don't limit the search, we may end up re-reading blocks that follow this block. The consequence of
		 * limiting the search is that we may not find the maximum close to the current state, but some time in the past
		 * for a file that is growing. We can live with that as we will redo the max search if necessary */
		assert(stop_at > dskread);
		while (b->reclen < stop_at - b->recaddr)
		{
			if ((status = repl_next(rb)) == SS_NORMAL)
			{
				rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
				if (IS_REPLICATED(rectype))
				{
					QWASSIGN(max_seqno, GET_JNL_SEQNO(b->recbuff));
					max_seqno_addr = b->recaddr;
				} else if (rectype == JRT_EOF)
					break;
			} else if (status == EREPL_JNLRECINCMPL)
			{ /* it is possible to get this return value if jb->dskaddr is in the middle of a journal record */
				break;
			} else
			{
				if (status == EREPL_JNLRECFMT)
					rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
				else
					GTMASSERT;
			}
		}
		if ((max_seqno_found = (0 != max_seqno)) || (0 == dskread))
			break;
		dskread -= REPL_BLKSIZE(rb);
	} while (TRUE);
	rb->buffindex = REPL_MAINBUFF;	/* reset back to the main buffer */
	if (max_seqno_found)
	{
		QWASSIGN(ctl->max_seqno, max_seqno);
		ctl->max_seqno_dskaddr = max_seqno_addr;
		if (ctl->eof_addr_final)
			ctl->max_seqno_final = TRUE; /* No more max_seqno updates as this journal file has switched */
		return (SS_NORMAL);
	}
	/* dskread == 0 */
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
	enum jnl_record_type	rectype;
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	boolean_t		min_seqno_found;
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */

	rb = ctl->repl_buff;
	assert(rb->buffindex == REPL_MAINBUFF);
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
			rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
			if (IS_REPLICATED(rectype))
			{
				QWASSIGN(ctl->min_seqno, GET_JNL_SEQNO(b->recbuff));
				QWASSIGN(ctl->seqno, ctl->min_seqno);
				ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
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
			if (status == EREPL_JNLRECFMT)
				rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
		}
	}
	REPL_DPRINT5("FIRST READ of %s - Min seqno "INT8_FMT" min_seqno_dskaddr %u EOF addr %u\n",
			ctl->jnl_fn, INT8_PRINT(ctl->min_seqno), ctl->min_seqno_dskaddr, ctl->repl_buff->fc->eof_addr);
	if (update_max_seqno_info(ctl) != SS_NORMAL)
	{
		assert(repl_errno == EREPL_JNLEARLYEOF);
		GTMASSERT; /* Program bug */
	}
	REPL_DPRINT5("FIRST READ of %s - Max seqno "INT8_FMT" max_seqno_dskaddr %u EOF addr %d\n",
			ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), ctl->max_seqno_dskaddr, ctl->repl_buff->fc->eof_addr);
	ctl->first_read_done = TRUE;
	return (SS_NORMAL);
}

static void increase_buffer(unsigned char **buff, int *buflen, int buffer_needed)
{
	int 		newbuffsize, alloc_status;
	unsigned char	*old_msgp;

	/* The tr size is not known apriori. Hence, a good guess of 1.5 times the current buffer space is used */
	newbuffsize = gtmsource_msgbufsiz + (gtmsource_msgbufsiz >> 1);
	if (buffer_needed > newbuffsize)
		newbuffsize = buffer_needed;
	REPL_DPRINT3("Buff space shortage. Attempting to increase buff space. Curr buff space %d. Attempt increase to atleast %d\n",
		     gtmsource_msgbufsiz, newbuffsize);
	old_msgp = (unsigned char *)gtmsource_msgp;
	if ((alloc_status = gtmsource_alloc_msgbuff(newbuffsize)) != SS_NORMAL)
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  LEN_AND_LIT("Error extending buffer space while reading files. Malloc error"), alloc_status);
	}
	*buff = (unsigned char *)gtmsource_msgp + (*buff - old_msgp);
	*buflen = gtmsource_msgbufsiz - (*buff - (unsigned char *)gtmsource_msgp);
	return;
}

static	int read_transaction(repl_ctl_element *ctl, unsigned char **buff, int *bufsiz, seq_num read_jnl_seqno)
{	/* Read the transaction ctl->seqno into buff. Position the next read at the next seqno in the journal file.
	 * Update max_seqno if necessary.  If read of the next seqno blocks, leave the read buffers as is.
	 * The next time when this journal file is accessed the read will move forward.
	 */
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	int			readlen;
	seq_num			rec_jnl_seqno;
	enum jnl_record_type	rectype;
	int			status;
	seq_num			read_seqno;
	unsigned char		*seq_num_ptr, seq_num_str[32]; /* INT8_PRINT */

	rb = ctl->repl_buff;
	assert(rb->buffindex == REPL_MAINBUFF);
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	ctl->read_complete = FALSE;
	readlen = 0;
	assert(0 != b->reclen);
	if (b->reclen > *bufsiz)
		increase_buffer(buff, bufsiz, b->reclen);
	assert(b->reclen <= *bufsiz);
	memcpy(*buff, b->recbuff, b->reclen);
	*buff += b->reclen;
	readlen += b->reclen;
	*bufsiz -= b->reclen;
	rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
	assert(IS_REPLICATED(rectype));
	rec_jnl_seqno = GET_REPL_JNL_SEQNO(b->recbuff);
	assert(QWEQ(rec_jnl_seqno, ctl->seqno));
	if (QWGT(rec_jnl_seqno, ctl->max_seqno))
	{
		QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
		ctl->max_seqno_dskaddr = b->recaddr;
	}
	ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
	if (!IS_FENCED(rectype) || JRT_NULL == rectype)
	{	/* Entire transaction done */
		ctl->read_complete = TRUE;
		trans_read = TRUE;
	} else
	{
		assert(IS_TUPD(rectype) || IS_FUPD(rectype)); /* The first record should be the beginning of a transaction */
	}
	/* Suggested optimisation : Instead of waiting for all records pertaining to this transaction to
	 * be written to the journal file, read those available, mark this file BLOCKED, read other journal
	 * files, and come back to this journal file later.
	 */
	while (!ctl->read_complete) /* Read the rest of the transaction */
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
			if (IS_REPLICATED(rectype))
			{
				if (b->reclen > *bufsiz)
					increase_buffer(buff, bufsiz, b->reclen);
				assert(b->reclen <= *bufsiz);
				if (rectype != JRT_TCOM && rectype != JRT_ZTCOM)
				{
					memcpy(*buff, b->recbuff, b->reclen);
					*buff += b->reclen;
					readlen += b->reclen;
				} else
				{
					memcpy(tcombuffp, b->recbuff, b->reclen);
					tcombuffp += b->reclen;
					tot_tcom_len += b->reclen;
					/* End of transaction in this file */
					ctl->read_complete = TRUE;
					if (num_tcom == -1)
						num_tcom = ((jnl_record *)b->recbuff)->jrec_tcom.num_participants;
					num_tcom--;
					if (num_tcom == 0) /* Read the whole trans */
						trans_read = TRUE;
				}
				*bufsiz -= b->reclen;
				QWASSIGN(rec_jnl_seqno, GET_JNL_SEQNO(b->recbuff));
				assert(QWEQ(rec_jnl_seqno, ctl->seqno));
				if (QWGT(rec_jnl_seqno, ctl->max_seqno))
				{
					QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
					ctl->max_seqno_dskaddr = b->recaddr;
				}
				ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
			} else if (rectype == JRT_EOF)
				{
					assert(FALSE);
					rts_error(VARLSTCNT(7) ERR_REPLBRKNTRANS, 1, &read_jnl_seqno,
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
			if (status == EREPL_JNLRECFMT)
				rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
			else
				GTMASSERT;
		}
	}
	/* Try positioning next read to the next seqno. Leave it as is if operation blocks (has to wait for records) */
	QWADD(read_seqno, ctl->seqno, seq_num_one);
	position_read(ctl, read_seqno);
	return (readlen);
}

static	tr_search_state_t do_linear_search(repl_ctl_element *ctl, uint4 lo_addr, uint4 max_readaddr, seq_num read_seqno,
						tr_search_status_t *srch_status)
{
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	seq_num			rec_jnl_seqno;
	enum jnl_record_type	rectype;
	tr_search_state_t	found;
	int			status;

	REPL_DPRINT5("do_linear_search: file : %s lo_addr : %u max_readaddr : %u read_seqno : %llu\n",
			ctl->jnl_fn, lo_addr, max_readaddr, read_seqno);
	assert(lo_addr < max_readaddr);
	rb = ctl->repl_buff;
	assert(rb->buffindex == REPL_MAINBUFF);
	b = &rb->buff[rb->buffindex];
	if (lo_addr != b->recaddr)
	{ /* Initiate a fresh read */
		lo_addr = ROUND_DOWN(lo_addr, REPL_BLKSIZE(rb));
		b->recaddr = b->readaddr = JNL_BLK_DSKADDR(lo_addr, REPL_BLKSIZE(rb));
		b->recbuff = b->base;
		b->reclen = 0;
		b->buffremaining = REPL_BLKSIZE(rb);
		if (b->readaddr == JNL_FILE_FIRST_RECORD && adjust_buff_leaving_hdr(rb) != SS_NORMAL)
		{
			assert(repl_errno == EREPL_BUFFNOTFRESH);
			GTMASSERT;	/* Program bug */
		}
		REPL_DPRINT1("do_linear_search: initiating fresh read\n");
	} else
	{ /* use what has been read already */
		assert(read_seqno != ctl->seqno); /* if not, we'll skip to the next transaction and declare read_seqno not found */
	}
	found = TR_NOT_FOUND;
	srch_status->prev_seqno = srch_status->seqno = 0;
	while (found == TR_NOT_FOUND && b->reclen < max_readaddr - b->recaddr)
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
			if (IS_REPLICATED(rectype))
			{
				rec_jnl_seqno = GET_JNL_SEQNO(b->recbuff);
				if (srch_status->seqno == 0 || srch_status->seqno != rec_jnl_seqno)
				{ /* change srch_status only when records of different transactions are encountered */
					srch_status->prev_seqno = srch_status->seqno;
					srch_status->seqno = rec_jnl_seqno;
				}
				if (QWLT(ctl->max_seqno, rec_jnl_seqno))
				{
					QWASSIGN(ctl->max_seqno, rec_jnl_seqno);
					ctl->max_seqno_dskaddr = b->recaddr;
				}
				QWASSIGN(ctl->seqno, rec_jnl_seqno);
				ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
				if (QWEQ(rec_jnl_seqno, read_seqno))
					found = TR_FOUND;
				else if (QWGT(rec_jnl_seqno, read_seqno))
					found = TR_WILL_NOT_BE_FOUND;
			} else if (rectype == JRT_EOF)
				found = TR_WILL_NOT_BE_FOUND;
		} else if (status == EREPL_JNLRECINCMPL)
			found = TR_FIND_WOULD_BLOCK;
		else if (status == EREPL_JNLRECFMT)
			rts_error(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
	}
	REPL_DPRINT2("do_linear_search: returning %s\n", (found == TR_NOT_FOUND) ? "TR_NOT_FOUND" :
							 (found == TR_FOUND) ? "TR_FOUND" :
							 (found == TR_WILL_NOT_BE_FOUND) ? "TR_WILL_NOT_BE_FOUND" :
							 (found == TR_FIND_WOULD_BLOCK) ? "TR_FIND_WOULD_BLOCK" :
							 "*UNKNOWN RETURN CODE*");
	return (found);
}

static	tr_search_state_t do_binary_search(repl_ctl_element *ctl, uint4 lo_addr, uint4 hi_addr, seq_num read_seqno,
						tr_search_status_t *srch_status)
{
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	repl_file_control_t	*fc;
	tr_search_state_t	found;
	uint4			low, high, mid, new_mid, mid_further, stop_at;
	uint4			srch_half_lo_addr, srch_half_hi_addr, othr_half_lo_addr, othr_half_hi_addr;
	uint4			hi_addr_mod, hi_addr_diff, mid_mod, mid_diff;
	uint4			willnotbefound_addr = 0, willnotbefound_stop_at = 0;
	int			iter, max_iter;
	enum jnl_record_type	rectype;
	boolean_t		search_complete = FALSE;

	REPL_DPRINT5("do_binary_search: file : %s lo_addr : %u hi_addr : %u read_seqno : %llu\n",
			ctl->jnl_fn, lo_addr, hi_addr, read_seqno);
	if (lo_addr > hi_addr)
	{
		REPL_DPRINT1("do_binary_search: lower limit is larger than upper limit, search not initiated\n");
		return TR_NOT_FOUND;
	}
	rb = ctl->repl_buff;
	assert(rb->buffindex == REPL_MAINBUFF);
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	assert(lo_addr <  fc->eof_addr);
	assert(hi_addr <= fc->eof_addr);
	assert(lo_addr <= hi_addr);
	low = ROUND_DOWN(lo_addr, REPL_BLKSIZE(rb));
	if (0 == (hi_addr_mod = hi_addr % REPL_BLKSIZE(rb)))
		hi_addr_mod = REPL_BLKSIZE(rb); /* avoid including additional block if already aligned */
	hi_addr_diff = fc->eof_addr - hi_addr;
	high = hi_addr + MIN(REPL_BLKSIZE(rb) - hi_addr_mod, hi_addr_diff);
	for (found = TR_NOT_FOUND, mid = ROUND_DOWN((low >> 1) + (high >> 1), REPL_BLKSIZE(rb)); ; )
	{
		mid_mod = mid % REPL_BLKSIZE(rb);
		mid_diff = fc->eof_addr - mid;
		assert(0 != mid_diff);
		stop_at = mid + MIN(REPL_BLKSIZE(rb) - mid_mod, mid_diff);
		/* Note that for high values of journal-alignsize (which is what REPL_BLKSIZE macro expands to), it is
		 * possible that stop_at is GREATER than high. Since it is not necessary to search any further than high,
		 * limit it accordingly before calling the linear search.
		 */
		found = do_linear_search(ctl, mid, MIN(stop_at, high), read_seqno, srch_status);
		assert(srch_status->seqno == 0 || srch_status->seqno == ctl->seqno);
		switch (found)
		{
			case TR_FOUND:
				rectype = ((jrec_prefix *)b->recbuff)->jrec_type;
				if (!IS_FENCED(rectype) || IS_TUPD(rectype) || IS_FUPD(rectype) || JRT_NULL == rectype)
				{
					REPL_DPRINT4("do_binary_search: found %llu at %u in %s\n", read_seqno, b->recaddr,
							ctl->jnl_fn);
					return found;
				}
				assert(b->recaddr >= REPL_BLKSIZE(rb)); /* cannot have trailing records of a tr in first block */
				low = ((b->recaddr > REPL_BLKSIZE(rb)) ?
					ROUND_DOWN(b->recaddr - REPL_BLKSIZE(rb), REPL_BLKSIZE(rb)) : 0);
				high = ROUND_DOWN(b->recaddr, REPL_BLKSIZE(rb));
				REPL_DPRINT5("do_binary_search: found record of type %d with seqno %llu at %u in %s\n",
						rectype, read_seqno, b->recaddr, ctl->jnl_fn);
				REPL_DPRINT3("do_binary_search: will back off and search for beginning of transaction, changing "
						"low to %u high to %u\n", low, high);
				break;
			case TR_NOT_FOUND:
				assert(b->readaddr == stop_at);
				if (srch_status->seqno != 0)
				{ /* at least one replicated record found in block */
					assert(read_seqno > srch_status->seqno);
					low = stop_at; /* search in the upper half */
					REPL_DPRINT5("do_binary_search: could not find %llu in block %u, last seen replicated "
							"record has seqno %llu changing low to %u\n", read_seqno, mid,
							srch_status->seqno, low);
				} else
				{ /* no replicated record found in block */
					assert(srch_status->prev_seqno == 0); /* must hold if no replicated record found */
					REPL_DPRINT3("do_binary_search: could not find %llu, no replicated record in block %u, "
							"searching lower half", read_seqno, mid);
					if (low < mid) /* seach lower half recursively */
						found = do_binary_search(ctl, low, mid, read_seqno, srch_status);
					else
					{
						REPL_DPRINT4("do_binary_search: all blocks searched in lower half search, "
								"seqno %llu not found, low %u high %u\n", read_seqno, low, mid);
					}
					if (found == TR_NOT_FOUND && stop_at < high) /* search upper half recursively */
						found = do_binary_search(ctl, stop_at, high, read_seqno, srch_status);
					else
					{
						REPL_DEBUG_ONLY(
							if (found == TR_NOT_FOUND)
							{
								REPL_DPRINT4("do_binary_search: all blocks searched in upper half"
									" search, seqno %llu not found, low %u high %u\n",
									read_seqno, stop_at, high);
							}
						)
					}
					if (found != TR_NOT_FOUND)
						return found;
					search_complete = TRUE;
				}
				break;

			case TR_WILL_NOT_BE_FOUND:
				if (srch_status->seqno != 0 && read_seqno < srch_status->seqno)
				{
					if (srch_status->prev_seqno == 0)
					{ /* first replicated record in block has larger seqno than read_seqno */
						REPL_DPRINT5("do_binary_search: will not find %llu, first replicated record in "
								"block %u has seqno %llu, changing high to %u\n", read_seqno, mid,
								srch_status->seqno, mid);
						willnotbefound_addr = mid;
						willnotbefound_stop_at = stop_at;
						high = mid; /* search in the lower half */
					} else
					{ /* at least two replicated records found, and the read_seqno is between the seqno
					   * numbers of two records */
						REPL_DPRINT5("do_binary_search: will not find %llu in block %u, last two seqnos are"
								" %llu and %llu, return WILL_NOT_BE_FOUND\n", read_seqno, mid,
								srch_status->prev_seqno, srch_status->seqno);
						assert(srch_status->prev_seqno < read_seqno);
						return found; /* read_seqno is not in this journal file */
					}
				} else
				{ /* found EOF record, read_seqno is not in this journal file */
					REPL_DPRINT3("do_binary_search: will not find %llu in block %u, EOF found\n", read_seqno,
							mid);
					return found;
				}
				break;

			case TR_FIND_WOULD_BLOCK:
				assert(mid == ROUND_DOWN(high, REPL_BLKSIZE(rb))); /* must be the last block for this retval */
				assert(ctl->file_state == JNL_FILE_OPEN || /* file that is yet to grow, or truncated file */
					ctl->file_state == JNL_FILE_CLOSED && 0 != fc->jfh->prev_recov_end_of_data);
				if (srch_status->seqno != 0)
				{ /* journal flush yet to be done, or truncated file's end_of_data reached, can't locate seqno */
					assert(read_seqno > srch_status->seqno);
					REPL_DPRINT4("do_binary_search: find of %llu would block, last seqno %llu found in block "
							"%u\n", read_seqno, srch_status->seqno, mid);
					return (ctl->file_state == JNL_FILE_OPEN ? found : TR_WILL_NOT_BE_FOUND);
				}
				/* no replicated record found in the block, search in previous block(s) */
				high = (high > REPL_BLKSIZE(rb)) ? high - REPL_BLKSIZE(rb) : 0;
				REPL_DPRINT4("do_binary_search: find of %llu would block, no seqno found in block %u, "
						"change high to %u\n", read_seqno, mid, high);
				break;

			default: /* Why didn't we cover all cases? */
				GTMASSERT;
		} /* end switch */
		if (!search_complete && low < high)
		{
			new_mid = ROUND_DOWN((low >> 1) + (high >> 1), REPL_BLKSIZE(rb));
			mid_further = (new_mid != mid) ? 0 : REPL_BLKSIZE(rb); /* if necessary, move further to avoid repeat */
			if (high - new_mid > mid_further)
			{
				mid = new_mid + mid_further;
				continue;
			}
		}
		REPL_DPRINT6("do_binary_search: all blocks searched, seqno %llu not found, low %u high %u mid %u mid_further %u\n",
				read_seqno, low, high, mid, mid_further);
		assert(found != TR_FOUND);
		/* done searching all blocks between lo_addr and hi_addr */
		if (found == TR_NOT_FOUND && 0 != willnotbefound_addr)
		{ /* There is a block that contains a seqno larger than read_seqno; leave ctl positioned at that seqno.
		   * If we don't do this, we may repeat binary search (wastefully) for all seqnos between read_seqno and
		   * the least seqno larger than read_seqno in this file.
		   */
			found = do_linear_search(ctl, willnotbefound_addr, willnotbefound_stop_at, read_seqno, srch_status);
			REPL_DPRINT4("do_binary_search: position at seqno %llu in block [%u, %u)\n", srch_status->seqno,
					willnotbefound_addr, willnotbefound_stop_at);
			assert(found == TR_WILL_NOT_BE_FOUND);
			assert(read_seqno < ctl->seqno);
		}
		return found;
	} /* end for */
}

static	tr_search_state_t position_read(repl_ctl_element *ctl, seq_num read_seqno)
{
	repl_buff_t		*rb;
	repl_buff_desc		*b;
	uint4			lo_addr, hi_addr;
	tr_search_state_t	found, (*srch_func)();
	tr_search_status_t	srch_status;
	uint4			willnotbefound_addr = 0, willnotbefound_stop_at = 0;
	int			eof_change;
	DEBUG_ONLY(jnl_record	*jrec;)
	DEBUG_ONLY(enum jnl_record_type	rectype;)

	/* Position read pointers so that the next read should get the first journal record with JNL_SEQNO atleast read_seqno.
	 * Do a search between min_seqno and seqno if read_seqno < ctl->seqno; else search from ctl->seqno onwards.
	 * If ctl->seqno > ctl->max_seqno update max_seqno as you move, else search between ctl->seqno and ctl->max_seqno.
	 * We want to do a binary search here.
	 */

	/* If the receiver disconnects while we were previously in the read-file mode and later reconnects requesting a
	 * particular sequence number, it is possible that we can have an out-of-date fc->eof_addr as fc->eof_addr is not
	 * frequently updated. But, now that we will be searching for the sequence number (either with binary or linear
	 * search) update the fc->eof_addr unconditionally. This is considered okay since the update_eof_addr function
	 * is a almost always a light weight function (with the only exception being the case when the jnl file source
	 * server is interested in is not the latest generation file then it could end up reading the journal file header
	 * from disk but that too it does only once per older generation journal file so it is okay)
	 */
	if (!ctl->eof_addr_final)
		update_eof_addr(ctl, &eof_change);
	/* Validate the range; if called from read_transaction(), it is possible that we are looking out of range, but
	 * we clearly can identify such a case */
	assert(ctl->min_seqno <= read_seqno);
	assert(read_seqno <= ctl->max_seqno || read_seqno == ctl->seqno + 1);
	rb = ctl->repl_buff;
	assert(REPL_MAINBUFF == rb->buffindex);
	b = &rb->buff[rb->buffindex];
	/* looking up something we read already? */
	assert(read_seqno != ctl->seqno || read_seqno == ctl->min_seqno || read_seqno == ctl->max_seqno || ctl->lookback);
	srch_func = do_binary_search;
	if (read_seqno > ctl->seqno)
	{
		if (read_seqno == ctl->seqno + 1)
		{ /* trying to position to next seqno or the max, better to do linear search */
			srch_func = do_linear_search;
			lo_addr = b->recaddr;
			hi_addr = MAXUINT4;
		} else if (read_seqno < ctl->max_seqno)
		{
			lo_addr = b->recaddr;
			hi_addr = ctl->max_seqno_dskaddr;
			/* Since we know that read_seqno is LESSER than ctl->max_seqno, we know for sure we should not return
			 * this function with found = TR_NOT_FOUND. If ever the binary/linear search invocation at the end of
			 * this function returns with TR_NOT_FOUND, we have to change that to TR_WILL_NOT_BE_FOUND and also
			 * adjust ctl->seqno to point to ctl->max_seqno that way we dont repeat binary search (wastefully) for
			 * all seqnos between read_seqno and the least seqno larger than read_seqno in this file.
			 */
			willnotbefound_addr = hi_addr;
			assert(ctl->max_seqno_dskaddr < rb->fc->eof_addr);
			willnotbefound_stop_at = rb->fc->eof_addr;
		} else if (read_seqno == ctl->max_seqno)
		{	/* For read_seqno == ctl->max_seqno, do not use linear search. Remember, max_seqno_dskaddr may be the
			 * the address of the TCOM record of a transaction, and this TCOM record may be in a different block
			 * than the block containing the first record of the transcation. To get it right, we may have to
			 * back off to previous blocks. Well, do_binary_search() handles this condition. So, better we use binary
			 * search. */
			lo_addr = b->recaddr;
			assert(ctl->max_seqno_dskaddr < rb->fc->eof_addr);
			hi_addr = rb->fc->eof_addr;
		} else /* read_seqno > ctl->max_seqno */
		{
			lo_addr = ctl->max_seqno_dskaddr;
			hi_addr = rb->fc->eof_addr;
		}
	} else /* (read_seqno <= ctl->seqno) */
	{
		if (read_seqno != ctl->min_seqno)
		{
			lo_addr = ctl->min_seqno_dskaddr;
			hi_addr = b->recaddr + b->reclen;
		} else
		{ /* trying to locate min, better to do linear search */
			srch_func = do_linear_search;
			lo_addr = ctl->min_seqno_dskaddr;
			hi_addr = MAXUINT4;
			if (read_seqno == ctl->seqno) /* we are positioned where we want to be, no need for read */
			{
				assert(lo_addr == b->recaddr);
				assert(MIN_JNLREC_SIZE <= b->reclen);
				DEBUG_ONLY(jrec = (jnl_record *)b->recbuff;)
				DEBUG_ONLY(rectype = jrec->prefix.jrec_type;)
				assert(b->reclen == jrec->prefix.forwptr);
				assert(IS_VALID_JNLREC(jrec, rb->fc->jfh));
				assert(IS_REPLICATED(rectype));
				assert(!IS_FENCED(rectype) || IS_TUPD(rectype) || IS_FUPD(rectype) || JRT_NULL == rectype);
				REPL_DPRINT3("position_read: special case, read %llu is same as min in %s, returning TR_FOUND\n",
						read_seqno, ctl->jnl_fn);
				return TR_FOUND;
			}
		}
	}
#if defined(GTMSOURCE_READFILES_LINEAR_SEARCH_TEST)
	srch_func = do_linear_search;
	hi_addr = MAXUINT4;
#elif defined(GTMSOURCE_READFILES_BINARY_SEARCH_TEST)
	srch_func = do_binary_search;
	hi_addr = rb->fc->eof_addr;
#endif
	REPL_DPRINT6("position_read: Using %s search to locate %llu in %s between %u and %u\n",
			(srch_func == do_linear_search) ? "linear" : "binary", read_seqno, ctl->jnl_fn, lo_addr, hi_addr);
	found = srch_func(ctl, lo_addr, hi_addr, read_seqno, &srch_status);
	if ((TR_NOT_FOUND == found) && (0 != willnotbefound_addr))
	{	/* There is a block that contains a seqno larger than read_seqno; leave ctl positioned at this higher seqno.
		 * If we don't do this, we could end up in an infinite loop if the caller of this function is "read_regions".
		 * That caller redoes the "position_read" function call assuming there would have been some progress in the
		 * previous call to the same function. So not resetting ctl->seqno here will result in an infinite loop.
		 */
		found = do_linear_search(ctl, willnotbefound_addr, willnotbefound_stop_at, read_seqno, &srch_status);
		REPL_DPRINT4("do_binary_search: position at seqno %llu in block [%u, %u)\n", srch_status.seqno,
				willnotbefound_addr, willnotbefound_stop_at);
		assert(found == TR_WILL_NOT_BE_FOUND);
		assert(read_seqno < ctl->seqno);
	}
	assert(found != TR_NOT_FOUND);
	return (found);
}

static	int read_and_merge(unsigned char *buff, int maxbufflen, seq_num read_jnl_seqno)
{
	int 			buff_avail, total_read, read_len, pass;
	boolean_t		brkn_trans;
	unsigned char		*seq_num_ptr, seq_num_str[32]; /* INT8_PRINT */
	repl_ctl_element	*ctl;

	trans_read = FALSE;
	num_tcom = -1;
	tot_tcom_len = 0;
	total_read = 0;
	total_wait_for_jnl_recs = 0;
	total_wait_for_jnlopen = 0;
	tcombuffp = gtmsource_tcombuff_start;
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
			rts_error(VARLSTCNT(3) ERR_REPLBRKNTRANS, 1, &read_jnl_seqno);
		total_read += read_len;
	}
	if (tot_tcom_len > 0)
	{	/* Copy all the TCOM records to the end of the buffer */
		assert(tot_tcom_len <= ((unsigned char *)gtmsource_msgp + gtmsource_msgbufsiz - buff));
		memcpy(buff, gtmsource_tcombuff_start, tot_tcom_len);
		total_read += tot_tcom_len;
	}
	return (total_read);
}

static	int read_regions(unsigned char **buff, int *buff_avail,
		         boolean_t attempt_open_oldnew, boolean_t *brkn_trans, seq_num read_jnl_seqno)
{
	repl_ctl_element	*ctl, *prev_ctl, *old_ctl;
	gd_region		*region;
	tr_search_state_t	found;
	int			read_len, cumul_read;
	int			nopen;
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */
	DEBUG_ONLY(static int	loopcnt;)
	sgmnt_addrs		*csa;
	jnlpool_ctl_ptr_t	jctl;
	uint4			freeaddr;

	cumul_read = 0;
	*brkn_trans = TRUE;
	assert(repl_ctl_list->next != NULL);
	jctl = jnlpool.jnlpool_ctl;
	/* For each region */
	for (ctl = repl_ctl_list->next, prev_ctl = repl_ctl_list; ctl != NULL && !trans_read; prev_ctl = ctl, ctl = ctl->next)
	{
		found = TR_NOT_FOUND;
		region = ctl->reg;
		DEBUG_ONLY(loopcnt = 0;)
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
					if (QWLT(read_jnl_seqno, jctl->jnl_seqno))
					{
						csa = &FILE_INFO(ctl->reg)->s_addrs;
						freeaddr = csa->jnl->jnl_buff->freeaddr;
						if ((ctl->repl_buff->fc->eof_addr == freeaddr) || (!JNL_ENABLED(csa->hdr)))
						{	/* No more pending updates in the journal file. Next update to the
							 * journal file will take the seqno jctl->jnl_seqno which will be
							 * greater than read_jnl_seqno
							 */
							found = TR_WILL_NOT_BE_FOUND;
							continue;
						}
					}
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
			} else if (ctl->file_state == JNL_FILE_EMPTY || QWGT(ctl->min_seqno, read_jnl_seqno))
			{	/* May be in prev gener */
				if (ctl->prev->reg == ctl->reg && ctl->file_state != JNL_FILE_EMPTY)
				{	/* If prev gener is already open, and we come here, we are looking for
					 * a seqno between prev gener's max_seqno and this gener's min_seqno.
					 * This region is not part of the transaction. Skip to the next region.
					 */
					REPL_DPRINT3("Gap between %s (max seqno "INT8_FMT,
							ctl->prev->jnl_fn, INT8_PRINT(ctl->prev->max_seqno));
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
			} else
			{
				assert((ctl->file_state == JNL_FILE_OPEN || read_jnl_seqno <= ctl->max_seqno)
				       && ctl->min_seqno <= read_jnl_seqno);
				if (ctl->lookback)
				{
					assert(QWLE(read_jnl_seqno, ctl->seqno));
					assert(ctl->file_state == JNL_FILE_OPEN || ctl->file_state == JNL_FILE_CLOSED);
					REPL_DPRINT4("Looking back and attempting to position read for %s at "
							INT8_FMT". File state is %s\n", ctl->jnl_fn, INT8_PRINT(read_jnl_seqno),
							(ctl->file_state == JNL_FILE_OPEN) ? "OPEN" : "CLOSED");
					position_read(ctl, read_jnl_seqno);
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
				} else	/* QWGT(read_jnl_seqno, ctl->seqno) */
				{
					/* Detect infinite loop of calls to position_read() by limiting # of calls to 1024 in dbg */
					DEBUG_ONLY(loopcnt++;)
					assert(1024 > loopcnt);
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
							position_read(ctl, read_jnl_seqno);
						} else
						{	/* Will possibly be found in next gener */
							prev_ctl = ctl;
							ctl = ctl->next;
						}
					} else if (ctl->file_state == JNL_FILE_CLOSED)
					{	/* May be found in this jnl file, attempt to position next read to read_jnl_seqno */
						position_read(ctl, read_jnl_seqno);
					} else
					{ /* Program bug - ctl->seqno should never be greater than ctl->max_seqno */
						GTMASSERT;
					}
				}
			}
		}
		/* Move to the next region, now that the tr has been found or will not be found */
		*brkn_trans = (*brkn_trans && found == TR_WILL_NOT_BE_FOUND);
		for ( ; ctl->next != NULL && ctl->next->reg == region; prev_ctl = ctl, ctl = ctl->next)
			;
	}
	assert(!*brkn_trans);
	return (cumul_read);
}

int gtmsource_readfiles(unsigned char *buff, int *data_len, int maxbufflen, boolean_t read_multiple)
{

	int4			read_size, read_state, first_tr_len, tot_tr_len;
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;
	seq_num			read_jnl_seqno, jnl_seqno;
	qw_num			read_addr;
	uint4			jnlpool_size;
	static int4		max_tr_size = MAX_TR_BUFFSIZE; /* Will generally be less than initial gtmsource_msgbufsiz;
								* allows for space to accommodate the last transaction */

	jctl = jnlpool.jnlpool_ctl;
	gtmsource_local = jnlpool.gtmsource_local;
	jnlpool_size = jctl->jnlpool_size;
	jnl_seqno = jctl->jnl_seqno;
	read_jnl_seqno = gtmsource_local->read_jnl_seqno;
	read_state = gtmsource_local->read_state;
	assert(buff == (unsigned char *)gtmsource_msgp + REPL_MSG_HDRLEN); /* else increasing buffer space will not work */
	assert(maxbufflen == gtmsource_msgbufsiz - REPL_MSG_HDRLEN);
	tot_tr_len = 0;
	assert(REPL_MSG_HDRLEN == SIZEOF(jnldata_hdr_struct));
	read_addr = gtmsource_local->read_addr;
	first_tr_len = read_size = read_and_merge(buff, maxbufflen, read_jnl_seqno++) + REPL_MSG_HDRLEN;
	max_tr_size = MAX(max_tr_size, read_size);
	do
	{
		tot_tr_len += read_size;
		REPL_DPRINT5("File read seqno : %llu Tr len : %d Total tr len : %d Maxbufflen : %d\n", read_jnl_seqno - 1,
				read_size - REPL_MSG_HDRLEN, tot_tr_len, maxbufflen);
		if (gtmsource_save_read_jnl_seqno < read_jnl_seqno)
		{
			read_addr += read_size;
			if (jnlpool_size >= (jctl->early_write_addr - read_addr)) /* No more overflow, switch to READ_POOL */
			{ /* To avoid the expense of memory barrier in jnlpool_hasnt_overflowed(), we use a possibly stale
			   * value of early_write_addr to check if we can switch back to pool. The consequence
			   * is that we may switch back and forth between file and pool read if we are in a situation wherein a
			   * GTM process races with source server, writing transactions into the pool right when the source server
			   * concludes that it can read from pool. We think this condition is rare enough that the expense
			   * of re-opening the files (due to the transition) and re-positioning read pointers is considered
			   * living with when compared with the cost of a memory barrier. We can reduce the expense by
			   * not clearing the file information for every transition back to pool. We can wait for a certain
			   * period of time (say 15 minutes) before we close all files. */
				gtmsource_local->read = read_addr % jnlpool_size;
				gtmsource_local->read_state = read_state = READ_POOL;
				break;
			}
			REPL_DPRINT3("Readfiles : after sync with pool read_seqno: %llu read_addr: %llu\n", read_jnl_seqno,
					read_addr);
		}
		if (read_multiple)
		{
			if (tot_tr_len < max_tr_size && read_jnl_seqno < jnl_seqno)
			{ /* Limit the read by the buffer length, or until there is no more to be read. If not limited by the
			   * buffer length, the buffer will keep growing due to logic that expands the buffer when needed */
				/* recompute buff and maxbufflen as buffer may have expanded during read_and_merge */
				buff = (unsigned char *)gtmsource_msgp + tot_tr_len + REPL_MSG_HDRLEN;
				maxbufflen = gtmsource_msgbufsiz - tot_tr_len - REPL_MSG_HDRLEN;
				read_size = read_and_merge(buff, maxbufflen, read_jnl_seqno++) + REPL_MSG_HDRLEN;
				max_tr_size = MAX(max_tr_size, read_size);
				/* don't use buff to assign type and len as buffer may have expanded, use gtmsource_msgp instead */
				((repl_msg_ptr_t)((unsigned char *)gtmsource_msgp + tot_tr_len))->type = REPL_TR_JNL_RECS;
				((repl_msg_ptr_t)((unsigned char *)gtmsource_msgp + tot_tr_len))->len  = read_size;
				continue;
			}
			REPL_DPRINT6("Readfiles : tot_tr_len %d max_tr_size %d read_jnl_seqno %llu jnl_seqno %llu "
				     "gtmsource_msgbufsize : %d; stop multiple reads\n", tot_tr_len, max_tr_size, read_jnl_seqno,
				     jnl_seqno, gtmsource_msgbufsiz);
		}
		break;
	} while (TRUE);
	gtmsource_local->read_addr = read_addr;
	gtmsource_local->read_jnl_seqno = read_jnl_seqno;
#ifdef GTMSOURCE_ALWAYS_READ_FILES
	gtmsource_local->read_state = read_state = READ_FILE;
#endif
	if (read_state == READ_POOL)
	{
		gtmsource_ctl_close(); /* no need to keep files open now that we are going to read from pool */
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal pool at transaction %llu\n",
				read_jnl_seqno);
		REPL_DPRINT3("Readfiles : after switch to pool, read_addr : "INT8_FMT" read : %u\n",
				INT8_PRINT(read_addr), gtmsource_local->read);
	}
	*data_len = (first_tr_len - REPL_MSG_HDRLEN);
	return (tot_tr_len);
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
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */

	REPL_DPRINT2("UPDATING RESYNC TN with seqno "INT8_FMT"\n", INT8_PRINT(resync_seqno));
	gtmsource_ctl_close();
	gtmsource_ctl_init();
	read_size = read_and_merge((unsigned char *)&gtmsource_msgp->msg[0], gtmsource_msgbufsiz - REPL_MSG_HDRLEN, resync_seqno);
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
	gtmsource_ctl_close(); /* close all structures now that we are done; if we have to read from journal files; we'll open
				* the structures again */
	return (SS_NORMAL);
}
