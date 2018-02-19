/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_ipc.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_un.h"

#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtmio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_repl.h"
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
#include "repl_instance.h"
#include "wbox_test_init.h"
#include "gtmcrypt.h"
#include "gtmdbgflags.h"
#include "anticipatory_freeze.h"

#define LSEEK_ERR_STR		"Error in lseek"
#define READ_ERR_STR		"Error in read"
#define UNKNOWN_ERR_STR		"Error unknown"

/* Get journal end of data, adjusted if file not virtually truncated by recover/rollback */
#define REAL_END_OF_DATA(FC)	(FC->jfh->prev_recov_end_of_data ? FC->jfh->end_of_data : FC->jfh->end_of_data + EOF_RECLEN)

#define	DO_EOF_ADDR_CHECK		FALSE
#define	SKIP_EOF_ADDR_CHECK		TRUE

/* Callers of this macro ensure that the maximum seqno which can be found until offset MAX_SEQNO_EOF_ADDR is MAX_SEQNO. */
#define	CTL_SET_MAX_SEQNO(CTL, MAX_SEQNO, MAX_SEQNO_ADDR, MAX_SEQNO_EOF_ADDR, SKIP_EOF_ADDR_CHECK)		\
MBSTART {													\
	assert(MAX_SEQNO_ADDR <= MAX_SEQNO_EOF_ADDR);								\
	assert(SKIP_EOF_ADDR_CHECK || (MAX_SEQNO_EOF_ADDR <= CTL->repl_buff->fc->eof_addr));			\
	assert(ctl->max_seqno <= MAX_SEQNO);									\
	ctl->max_seqno = MAX_SEQNO;										\
	assert(ctl->max_seqno_dskaddr <= MAX_SEQNO_ADDR);							\
	ctl->max_seqno_dskaddr = MAX_SEQNO_ADDR;								\
	assert(MAX_SEQNO_EOF_ADDR >= MAX_SEQNO_ADDR);								\
	assert(ctl->max_seqno_eof_addr <= MAX_SEQNO_EOF_ADDR);							\
	ctl->max_seqno_eof_addr = MAX_SEQNO_EOF_ADDR;								\
} MBEND

#define BUNCHING_TIME	(8 * MILLISECS_IN_SEC)

#define	GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE(CTL, CSA)									\
MBSTART {														\
	boolean_t		flush_done;										\
	uint4			dskaddr, freeaddr, rsrv_freeaddr, saved_jpc_cycle;					\
	jnl_private_control	*jpc;											\
	int			rc;											\
															\
	jpc = CSA->jnl;													\
	saved_jpc_cycle = jpc->cycle; /* remember current cycle */							\
	DO_JNL_FLUSH_IF_POSSIBLE(CTL->reg, CSA, flush_done, dskaddr, freeaddr, rsrv_freeaddr);				\
	if (flush_done)													\
	{	/* Source server did a flush. Log that event for debugging purposes */					\
		repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_INFO : Source server did flush of journal file %s "	\
			"state %s : seqno %llu [0x%llx]. [dskaddr 0x%x freeaddr 0x%x rsrv_freeaddr 0x%x]\n",		\
			CTL->jnl_fn, jnl_file_state_lit[CTL->file_state], CTL->seqno, CTL->seqno,			\
			dskaddr, freeaddr, rsrv_freeaddr);								\
		/* Since "flush_done" is TRUE, it means the source server has done a "jnl_ensure_open" inside the	\
		 * DO_JNL_FLUSH_IF_POSSIBLE macro and has opened the journal file. It already has a copy of the fd in	\
		 * ctl->repl_buff->fc->fd which it uses to read the journal file. Now that the jnl_flush (rare event)	\
		 * is done, close the jpc->channel and continue to use the ctl's fd.					\
		 */													\
		assert(NOJNL != jpc->channel);										\
		JNL_FD_CLOSE(jpc->channel, rc);	/* sets jpc->channel to NOJNL */					\
		assert(NOJNL == jpc->channel);										\
		jpc->cycle = saved_jpc_cycle; /* do not want cycle to be changed by our flushing so restore it */	\
	}														\
} MBEND

GBLREF	unsigned char		*gtmsource_tcombuff_start;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	repl_ctl_element	*repl_ctl_list;
GBLREF	repl_rctl_elem_t	*repl_rctl_list;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;
GBLREF	repl_msg_ptr_t		gtmsource_msgp;
GBLREF	int			gtmsource_msgbufsiz;
GBLREF	seq_num			seq_num_zero, seq_num_one;
GBLREF	gd_region		*gv_cur_region;
GBLREF	FILE			*gtmsource_log_fp;
GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	uint4			process_id;

LITREF char *jnl_file_state_lit[];

error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLEMPTY);
error_def(ERR_JNLFILRDOPN);
error_def(ERR_JNLRECINCMPL);
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
#if 0 /* Not used for now */
static	int			scavenge_closed_jnl_files(seq_num ack_seqno);
#endif /* 0 */
static	int			update_eof_addr(repl_ctl_element *ctl, int *eof_change);

static	int repl_read_file(repl_buff_t *rb)
{
	repl_buff_desc 		*b;
	repl_file_control_t	*fc;
	ssize_t			nb;
	sgmnt_addrs		*csa;
	uint4			dskaddr;
	uint4			read_less, status;
	int			eof_change;
	uint4			start_addr;
	uint4			end_addr;

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
		dskaddr = REAL_END_OF_DATA(fc);
	}
#	ifdef DEBUG
	b->save_readaddr = b->readaddr;
	b->save_dskaddr = dskaddr;
	b->save_buffremaining = b->buffremaining;
#	endif
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
	start_addr = ROUND_DOWN2(b->readaddr, fc->fs_block_size);
	end_addr = ROUND_UP2(b->readaddr + b->buffremaining - read_less, fc->fs_block_size);
	if ((off_t)-1 == lseek(fc->fd, (off_t)start_addr, SEEK_SET))
	{
		repl_errno = EREPL_JNLFILESEEK;
		return (errno);
	}
	READ_FILE(fc->fd, b->base + REPL_BLKSIZE(rb) - b->buffremaining - (b->readaddr - start_addr),
		  end_addr - start_addr, nb);
	status = errno;
	if (nb < (b->readaddr - start_addr))
	{	/* This case means that we didn't read enough bytes to get from the alignment point in the disk file
		 * to the start of the actual desired read (the offset we did the lseek to above).  This can't happen
		 * and represents an out of design situation and we must return an error.
		 */
		assert(FALSE);
		nb = -1;
	} else
	{
		nb = nb - (b->readaddr - start_addr);
		if (nb > (b->buffremaining - read_less))
			nb = b->buffremaining - read_less;
	}
	if (0 <= nb)
	{
		b->buffremaining -= (uint4)nb;
		b->readaddr += (uint4)nb;
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
	repl_ctl_element	*backctl;
	int			gtmcrypt_errno;
	enum jnl_record_type	rectype;
	jnl_record		*rec;
	jnl_string		*keystr;
	jnl_file_header		*jfh;
	boolean_t		use_new_key;

	b = &rb->buff[rb->buffindex];
	b->recbuff += b->reclen; /* The next record */
	b->recaddr += b->reclen;
	backctl = rb->backctl;
	if (b->recaddr == b->readaddr && b->buffremaining == 0)
	{
		/* Everything in this buffer processed */
		b->recbuff = b->base;
		b->reclen = 0;
		b->buffremaining = REPL_BLKSIZE(rb);
	}
	if (b->recaddr == b->readaddr || b->reclen == 0 ||
		(backctl->eof_addr_final && (b->readaddr != REAL_END_OF_DATA(rb->fc)) && b->buffremaining))
	{
		sav_buffremaining = b->buffremaining;
		if ((status = repl_read_file(rb)) == SS_NORMAL)
		{
			if (sav_buffremaining == b->buffremaining)
			{
				/* Even though we are returning with EREPL_JNLRECINCMPL, this is not a case of an incomplete record
				 * if the journal file is a previous generation journal file.  It actually indicates that the end
				 * of file of the previous generation journal file is now reached.
				 */
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
			rts_error_csa(CSA_ARG(&FILE_INFO(backctl->reg)->s_addrs) VARLSTCNT(9) ERR_REPLFILIOERR, 2,
				backctl->jnl_fn_len, backctl->jnl_fn, ERR_TEXT, 2, LEN_AND_STR(err_string), status);
		}
	}
	maxreclen = (uint4)(((b->base + REPL_BLKSIZE(rb)) - b->recbuff) - b->buffremaining);
	assert(maxreclen > 0);
	jfh = rb->fc->jfh;
	if (maxreclen > JREC_PREFIX_UPTO_LEN_SIZE &&
		(reclen = ((jrec_prefix *)b->recbuff)->forwptr) <= maxreclen &&
		IS_VALID_JNLREC((jnl_record *)b->recbuff, jfh))
	{
		if (USES_ANY_KEY(jfh))
		{
			rec = ((jnl_record *)(b->recbuff));
			rectype = (enum jnl_record_type)rec->prefix.jrec_type;
			if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))
			{
				assert(!IS_ZTP(rectype));
				keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
				/* Assert that ZTWORMHOLE and LGTRIG type record too has same layout as KILL/SET */
				assert((sm_uc_ptr_t)keystr == (sm_uc_ptr_t)&rec->jrec_ztworm.ztworm_str);
				assert((sm_uc_ptr_t)keystr == (sm_uc_ptr_t)&rec->jrec_lgtrig.lgtrig_str);
				use_new_key = USES_NEW_KEY(jfh);
				assert(NEEDS_NEW_KEY(jfh, ((jrec_prefix *)b->recbuff)->tn) == use_new_key);
				MUR_DECRYPT_LOGICAL_RECS(
						keystr,
						(use_new_key ? TRUE : jfh->non_null_iv),
						rec->prefix.forwptr,
						(use_new_key ? backctl->encr_key_handle2 : backctl->encr_key_handle),
						gtmcrypt_errno);
				if (0 != gtmcrypt_errno)
					GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, backctl->jnl_fn_len, backctl->jnl_fn);
			}
		}
		b->reclen = reclen;
		return SS_NORMAL;
	}
	repl_errno = (maxreclen > JREC_PREFIX_SIZE && reclen <= maxreclen) ? EREPL_JNLRECFMT : EREPL_JNLRECINCMPL;
	if (EREPL_JNLRECFMT == repl_errno)
	{	/* not sure how this can happen. take a dump (fork_n_core) to gather more information */
		assert(FALSE);
		gtm_fork_n_core();
	}
	b->reclen = 0;
	return repl_errno;
}

static int open_prev_gener(repl_ctl_element **old_ctl, repl_ctl_element *ctl, seq_num read_seqno)
{
	gtmsource_state_t	gtmsource_state_sav;
	repl_ctl_element	*prev_ctl;
	repl_rctl_elem_t	*repl_rctl;

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
	prev_ctl = *old_ctl;
	assert(prev_ctl->reg == ctl->reg);
	prev_ctl->prev = ctl->prev;
	prev_ctl->next = ctl;
	prev_ctl->prev->next = prev_ctl;
	ctl->prev = prev_ctl;
	repl_rctl = ctl->repl_rctl;
	prev_ctl->repl_rctl = repl_rctl;
	repl_rctl->ctl_start = prev_ctl;
	GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
	first_read(prev_ctl);
	if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Abandoning open_prev_gener\n");
		return (0);
	}
	if (JNL_FILE_OPEN == prev_ctl->file_state)
	{
		prev_ctl->file_state = JNL_FILE_CLOSED;
		REPL_DPRINT2("open_prev_gener : %s jnl file marked closed\n", prev_ctl->jnl_fn);
	} else
	{
		MARK_CTL_AS_EMPTY(prev_ctl);
		REPL_DPRINT2("open_prev_gener :  %s jnl file marked empty\n", prev_ctl->jnl_fn);
	}
	return (1);
}

static	int open_newer_gener_jnlfiles(gd_region *reg, repl_ctl_element *reg_ctl_end)
{
	sgmnt_addrs		*csa;
	repl_ctl_element	*new_ctl, *ctl;
	int			jnl_fn_len;
	char			jnl_fn[JNL_NAME_SIZE];
	int			nopen, n;
	boolean_t		do_jnl_ensure_open;
	gd_id_ptr_t		reg_ctl_end_id;
	gtmsource_state_t	gtmsource_state_sav;
	repl_rctl_elem_t	*repl_rctl;
	jnl_private_control	*jpc;

	/* Attempt to open newer generation journal files. Return the number of new files opened. Create new
	 * ctl element(s) for each newer generation and attach at reg_ctl_end. Work backwards from the current journal file.
	 */
	nopen = 0;
	csa = &FILE_INFO(reg)->s_addrs;
	/* If no journal file switch happened since the last time we did a "jnl_ensure_open" on this region, do a "cycle"
	 * check (lightweight) and avoid the heavyweight "jnl_ensure_open" call.
	 */
	jpc = csa->jnl;
	if (!JNL_FILE_SWITCHED(jpc))
		return nopen;
	reg_ctl_end_id = &reg_ctl_end->repl_buff->fc->id;
	/* Note that at this point, journaling might have been turned OFF (e.g. REPL_WAS_ON state) in which case
	 * JNL_GDID_PTR(csa) would have been nullified by jnl_file_lost. Therefore comparing with that is not a good idea
	 * to use the "id" to check if the journal file remains same (this was done previously). Instead use the ID of
	 * the current reg_ctl_end and the NAME of the newly opened journal file. Because we don't have crit, we cannot
	 * safely read the journal file name from the file header therefore we invoke repl_ctl_create unconditionally
	 * (that has safety protections for crit) and use the new_ctl that it returns to access the journal file name
	 * returned and use that for the ID to NAME comparison.
	 */
	jnl_fn_len = 0; jnl_fn[0] = '\0';
	for (do_jnl_ensure_open = TRUE; ; do_jnl_ensure_open = FALSE)
	{
		repl_ctl_create(&new_ctl, reg, jnl_fn_len, jnl_fn, do_jnl_ensure_open);
		if (do_jnl_ensure_open && is_gdid_gdid_identical(reg_ctl_end_id, &new_ctl->repl_buff->fc->id))
		{	/* Current journal file in db file header has been opened ALREADY by source server. Return right away */
			assert(0 == nopen);
			repl_ctl_close(new_ctl);
			return nopen;
		}
		nopen++;
		REPL_DPRINT2("Newer generation file %s opened\n", new_ctl->jnl_fn);
		new_ctl->prev = reg_ctl_end;
		new_ctl->next = reg_ctl_end->next;
		if (NULL != new_ctl->next)
			new_ctl->next->prev = new_ctl;
		reg_ctl_end->next = new_ctl;
		repl_rctl = reg_ctl_end->repl_rctl;
		new_ctl->repl_rctl = repl_rctl;
		jnl_fn_len = new_ctl->repl_buff->fc->jfh->prev_jnl_file_name_length;
		memcpy(jnl_fn, new_ctl->repl_buff->fc->jfh->prev_jnl_file_name, jnl_fn_len);
		jnl_fn[jnl_fn_len] = '\0';
		if ('\0' == jnl_fn[0])
		{ /* prev link has been cut, can't follow path back from latest generation jnlfile to the latest we had opened */
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_NOPREVLINK, 2, new_ctl->jnl_fn_len, new_ctl->jnl_fn);
		}
		if (is_gdid_file_identical(reg_ctl_end_id, jnl_fn, jnl_fn_len))
			break;
	}
	/* Name of the journal file corresponding to reg_ctl_end might have changed. Update the name.
	 * Since inode info doesn't change when a file is renamed, it is not necessary to close and reopen the file.
	 */
	reg_ctl_end->jnl_fn[reg_ctl_end->jnl_fn_len] = '\0'; /* For safety */
	jnl_fn[jnl_fn_len] = '\0';
	if (STRCMP(reg_ctl_end->jnl_fn, jnl_fn) != 0) /* Name has changed */
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
		if (JNL_FILE_UNREAD == ctl->file_state)
		{
			GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
			first_read(ctl);
			if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Abandoning open_newer_gener_jnlfiles (first_read)\n");
				return 0;
			}
		} else
		{
			assert(JNL_FILE_OPEN == ctl->file_state);
			GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
			if (update_max_seqno_info(ctl) != SS_NORMAL)
			{
				assert(repl_errno == EREPL_JNLEARLYEOF);
				assertpro(FALSE); /* Program bug */
			}
			if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "Abandoning open_newer_gener_jnlfiles (update_max)\n");
				return 0;
			}
		}
		if (JNL_FILE_UNREAD == ctl->file_state)
		{
			MARK_CTL_AS_EMPTY(ctl);
			REPL_DPRINT2("Open_newer_gener_files : %s marked empty\n", ctl->jnl_fn);
		} else
		{
			assert(JNL_FILE_OPEN == ctl->file_state);
			ctl->file_state = JNL_FILE_CLOSED;
			REPL_DPRINT2("Open_newer_gener_files : %s marked closed\n", ctl->jnl_fn);
		}
	}
	return nopen;
}

static	int update_eof_addr(repl_ctl_element *ctl, int *eof_change)
{
	repl_file_control_t	*fc;
	uint4			prev_eof_addr, new_eof_addr;
	int			status;
	sgmnt_addrs		*csa;
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
		REPL_DPRINT4("Update EOF : FC ID IS %u %d %u\n", fc->id.inode, fc->id.device, fc->id.st_gen);
		REPL_DPRINT4("Update EOF : csa->nl->jnl_file.u (unreliable) is %u %d %u\n", csa->nl->jnl_file.u.inode,
			     csa->nl->jnl_file.u.device,  csa->nl->jnl_file.u.st_gen);
		if (!ctl->eof_addr_final)
		{
			F_READ_BLK_ALIGNED(fc->fd, 0, fc->jfh, ROUND_UP2(REAL_JNL_HDR_LEN, fc->fs_block_size), status);
			if (SS_NORMAL != status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_REPLFILIOERR, 2, ctl->jnl_fn_len, ctl->jnl_fn, ERR_TEXT,
						2, LEN_AND_LIT("Error reading journal fileheader to update EOF address"), status);
			REPL_DPRINT2("Update EOF : Jnl file hdr refreshed from file for %s\n", ctl->jnl_fn);
			ctl->eof_addr_final = TRUE; /* No more updates to fc->eof_addr for this journal file */
		}
		new_eof_addr = REAL_END_OF_DATA(fc);
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
	rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
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
	uint4			dskread, max_seqno_eof_addr, stop_at;
	boolean_t		eof_addr_final, max_seqno_found;
	uint4			max_seqno_addr;
	seq_num			max_seqno, reg_seqno;
	int			status;
	enum jnl_record_type	rectype;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	int			wait_for_jnl = 0;
	gtmsource_state_t	gtmsource_state_sav;

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
	/* Store/cache fc->eof_addr in local variable. It is possible the "repl_next" calls done below invoke "repl_read_file"
	 * which in turn invokes "update_eof_addr" and modify fc->eof_addr. But we limit our max_seqno search to
	 * the stored value of fc->eof_addr. Any changes to fc->eof_addr inside this function cause corresponding
	 * changes to ctl->max_seqno_eof_addr in subsequent calls to "update_max_seqno_info". Store copy of ctl->eof_eof_addr_final
	 * at the same time since ctl->eof_addr_final and ctl->eof_addr are maintained in sync.
	 */
	max_seqno_eof_addr = fc->eof_addr;
	eof_addr_final = ctl->eof_addr_final;
	if (JNL_FILE_FIRST_RECORD == max_seqno_eof_addr)
	{	/* Journal file only has the journal header, no journal records.  That case should have been caught earlier
		* when the journal file was first open.
		*/
		assert(FALSE);
		repl_errno = EREPL_JNLEARLYEOF;
		return (repl_errno);
	}
	QWASSIGN(reg_seqno, csa->hdr->reg_seqno);
	QWDECRBYDW(reg_seqno, 1);
	assert(!ctl->max_seqno_final || eof_addr_final);
 	if ((ctl->max_seqno >= reg_seqno) || (ctl->max_seqno_eof_addr == max_seqno_eof_addr)
		|| (ctl->max_seqno_final && ctl->first_read_done))
 	{	/* have searched already */
 		REPL_DPRINT4("UPDATE MAX SEQNO INFO : not reading file %s; max_seqno = "INT8_FMT", reg_seqno = "INT8_FMT"\n",
 			     ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), INT8_PRINT(reg_seqno));
 		return (SS_NORMAL);
 	}
	rb->buffindex = REPL_SCRATCHBUFF;	/* temporarily set to scratch buffer */
	b = &rb->buff[rb->buffindex];
	dskread = ROUND_DOWN(max_seqno_eof_addr, REPL_BLKSIZE(rb));
	if (dskread == max_seqno_eof_addr)
		dskread -= REPL_BLKSIZE(rb);
	max_seqno = 0;
	max_seqno_addr = 0;
	max_seqno_found = FALSE;
	do
	{
		/* Ignore the existing contents of scratch buffer */
		b->buffremaining = REPL_BLKSIZE(rb);
		b->recbuff = b->base;
		b->reclen = 0;
		b->readaddr = b->recaddr = JNL_BLK_DSKADDR(dskread, REPL_BLKSIZE(rb));
		if (JNL_FILE_FIRST_RECORD == b->readaddr && SS_NORMAL != adjust_buff_leaving_hdr(rb))
		{
			assert(repl_errno == EREPL_BUFFNOTFRESH);
			assertpro(FALSE); /* Program bug */
		}
		stop_at = dskread + MIN(REPL_BLKSIZE(rb), max_seqno_eof_addr - dskread); /* Limit search to this block */
		/* If we don't limit the search, we may end up re-reading blocks that follow this block. The consequence of
		 * limiting the search is that we may not find the maximum close to the current state, but some time in the past
		 * for a file that is growing. We can live with that as we will redo the max search if necessary */
		assert(stop_at > dskread);
		while (b->reclen < stop_at - b->recaddr)
		{
			if (SS_NORMAL == (status = repl_next(rb)))
			{
				rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
				if (IS_REPLICATED(rectype))
				{
					max_seqno = GET_JNL_SEQNO(b->recbuff);
					max_seqno_addr = b->recaddr;
				} else if (JRT_EOF == rectype)
					break;
			} else if (EREPL_JNLRECINCMPL == status)
			{	/* It is possible to get this return value if jb->dskaddr is in the middle of a journal record. Log
				 * warning message for every certain number of attempts. There might have been a crash and the file
				 * might have been corrupted. The file possibly might never grow. Such cases have to be detected
				 * and attempt to read such files aborted. If the journal file is a previous generation, waste no
				 * time waiting for journal records to be written, but error out right away. That should never
				 * happen, hence the assert and gtm_fork_n_core.
				 */
				if (ctl->eof_addr_final)
				{ 	/* EREPL_JNLRECINCMPL is also possible if there is no more records left in the journal
					 * file.  If this happens with a previous generation journal file, there is no point in
					 * retrying the read from this block as the file will never grow.  Add asserts to ensure
					 * to ensure this (that we have reached the end of previous generation journal file) and
					 * create a core otherwise (for diagnostic purposes).
					 */
					if ((b->readaddr != b->recaddr) || (b->recaddr != REAL_END_OF_DATA(fc)))
					{
						assert(FALSE);
						gtm_fork_n_core();
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_JNLRECINCMPL, 4,
							b->recaddr, ctl->jnl_fn_len, ctl->jnl_fn, &ctl->seqno);
					}
					break;
				}
				assert(GTMSOURCE_WAITING_FOR_XON != gtmsource_state);
				if (gtmsource_recv_ctl_nowait())
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "State change detected in update_max_seqno_info\n");
					rb->buffindex = REPL_MAINBUFF;	/* reset back to the main buffer */
					gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
					return (SS_NORMAL);
				}
				GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
				gtmsource_poll_actions(TRUE);
				if(GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
							"State change detected in update_max_seqno_info (poll)\n");
					rb->buffindex = REPL_MAINBUFF;	/* reset back to the main buffer */
					gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
					return (SS_NORMAL);
				}
				SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
				wait_for_jnl += GTMSOURCE_WAIT_FOR_JNL_RECS;
				if (0 == (wait_for_jnl % LOG_WAIT_FOR_JNL_FLUSH_PERIOD))
					GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE(ctl, csa); /* See if a "jnl_flush" might help nudge */
				if (0 == (wait_for_jnl % LOG_WAIT_FOR_JNL_RECS_PERIOD))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %u seconds for"
						" journal record(s) to be written to journal file %s while attempting to read "
						"seqno %llu [0x%llx]. [dskaddr 0x%x freeaddr 0x%x rsrv_freeaddr 0x%x]."
						" Check for problems with journaling\n", wait_for_jnl / 1000, ctl->jnl_fn,
						ctl->seqno, ctl->seqno, csa->jnl->jnl_buff->dskaddr,
						csa->jnl->jnl_buff->freeaddr, csa->jnl->jnl_buff->rsrv_freeaddr);
				}
			} else
			{
				assertpro(EREPL_JNLRECFMT == status);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLBADRECFMT, 3,
					ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
			}
		}
		if ((max_seqno_found = (0 != max_seqno)) || (0 == dskread))
			break;
		assert(REPL_BLKSIZE(rb) <= dskread);
		dskread -= REPL_BLKSIZE(rb);
	} while (TRUE);
	rb->buffindex = REPL_MAINBUFF;	/* reset back to the main buffer */
	if (max_seqno_found)
	{	/* Assert that there is some progress in this call to "update_max_seqno_info" compared to the previous call */
		assert((max_seqno_eof_addr > ctl->max_seqno_eof_addr) || (max_seqno_addr > ctl->max_seqno_dskaddr)
			|| (max_seqno > ctl->max_seqno));
		CTL_SET_MAX_SEQNO(ctl, max_seqno, max_seqno_addr, max_seqno_eof_addr, DO_EOF_ADDR_CHECK);
		if (eof_addr_final)
		{	/* Do not use ctl->eof_addr_final in the check above since it could have changed inside this function.
			 * We want to set all max_seqno* fields based on fc->eof_addr after update_eof_addr
			 * done at beginning of this function. This keeps all of them in sync with each other.
			 */
			ctl->max_seqno_final = TRUE; /* No more max_seqno updates as this journal file has switched */
		}
		return (SS_NORMAL);
	}
	/* Two possibilities to get here :
	 * 1- No replicatable journal record found.  Shouldn't happen since it should have already looked for min seq. number.
	 * 2- Max seq. number found is 0.  This is possible only if the journal file has been created with replication turned OFF.
	 *    That should have been detected earlier when opening the journal file. */
	assert(FALSE);
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
	gtmsource_state_t	gtmsource_state_sav;

	assert(JNL_FILE_UNREAD == ctl->file_state);
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
		assertpro(FALSE); /* Program bug */
	}
	min_seqno_found = FALSE;
	/* Since this is first time we are reading from this journal file, initialize fields cared for in CTL_SET_MAX_SEQNO macro */
	DEBUG_ONLY(ctl->max_seqno = ctl->max_seqno_dskaddr = ctl->max_seqno_eof_addr = 0;)
	while (!min_seqno_found)
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
			if (IS_REPLICATED(rectype))
			{
				ctl->min_seqno = GET_JNL_SEQNO(b->recbuff);
				ctl->min_seqno_dskaddr = b->recaddr;
				ctl->seqno = ctl->min_seqno;
				ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
				/* Since update_eof_addr has not yet been called (will be done as part of "update_max_seqno_info"
				 * at the end of this function), skip the fc->eof_addr check inside the below macro.
				 */
				CTL_SET_MAX_SEQNO(ctl, ctl->min_seqno, b->recaddr, b->recaddr, SKIP_EOF_ADDR_CHECK);
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
				rts_error_csa(CSA_ARG(&FILE_INFO(ctl->reg)->s_addrs) VARLSTCNT(5)
					ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
		}
	}
	REPL_DPRINT5("FIRST READ of %s - Min seqno "INT8_FMT" min_seqno_dskaddr %u EOF addr %u\n",
			ctl->jnl_fn, INT8_PRINT(ctl->min_seqno), ctl->min_seqno_dskaddr, ctl->repl_buff->fc->eof_addr);
	GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
	if (update_max_seqno_info(ctl) != SS_NORMAL)
	{
		assert(repl_errno == EREPL_JNLEARLYEOF);
		assertpro(FALSE); /* Program bug */
	}
	if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Abandoning first_read\n");
		/* file_state was set to JNL_FILE_OPEN above; reset it here. */
		ctl->file_state = JNL_FILE_UNREAD;
		return (SS_NORMAL);
	}
	REPL_DPRINT5("FIRST READ of %s - Max seqno "INT8_FMT" max_seqno_dskaddr %u EOF addr %d\n",
			ctl->jnl_fn, INT8_PRINT(ctl->max_seqno), ctl->max_seqno_dskaddr, ctl->repl_buff->fc->eof_addr);
	ctl->first_read_done = TRUE;
	return (SS_NORMAL);
}

static void increase_buffer(unsigned char **buff, int *buflen, int buffer_needed)
{
	int 		alloc_status, expandsize, newbuffsize;
	unsigned char	*old_msgp;

	/* The tr size is not known apriori. Hence, a good guess of 1.5 times the current buffer space is used */
	expandsize = (gtmsource_msgbufsiz >> 1);
	if (expandsize < buffer_needed)
		expandsize = buffer_needed;
	newbuffsize = gtmsource_msgbufsiz + expandsize;
	REPL_DPRINT3("Buff space shortage. Attempting to increase buff space. Curr buff space %d. Attempt increase to atleast %d\n",
		     gtmsource_msgbufsiz, newbuffsize);
	old_msgp = (unsigned char *)gtmsource_msgp;
	if ((alloc_status = gtmsource_alloc_msgbuff(newbuffsize, FALSE)) != SS_NORMAL)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			  LEN_AND_LIT("Error extending buffer space while reading files. Malloc error"), alloc_status);
	}
	REPL_DPRINT3("Old gtmsource_msgp = 0x%llx; New gtmsource_msgp = 0x%llx\n", old_msgp, gtmsource_msgp);
	REPL_DPRINT2("Old *buff = 0x%llx\n", *buff);
	*buff = (unsigned char *)gtmsource_msgp + (*buff - old_msgp);
	REPL_DPRINT2("New *buff = 0x%llx\n", *buff);
	*buflen =(int)(gtmsource_msgbufsiz - (*buff - (unsigned char *)gtmsource_msgp));
	REPL_DPRINT2("New remaining len = %ld\n", *buflen);
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
	sgmnt_addrs		*csa;
	gtmsource_state_t	gtmsource_state_sav;
	repl_rctl_elem_t	*repl_rctl;

	rb = ctl->repl_buff;
	assert(rb->buffindex == REPL_MAINBUFF);
	b = &rb->buff[rb->buffindex];
	fc = rb->fc;
	repl_rctl = ctl->repl_rctl;
	assert(FALSE == repl_rctl->read_complete);
	csa = &FILE_INFO(ctl->reg)->s_addrs;
	readlen = 0;
	assert(0 != b->reclen);
	if (b->reclen > *bufsiz)
		increase_buffer(buff, bufsiz, b->reclen);
	assert(b->reclen <= *bufsiz);
	memcpy(*buff, b->recbuff, b->reclen);
	*buff += b->reclen;
	readlen += b->reclen;
	assert(readlen % JNL_WRT_END_MODULUS == 0);
	*bufsiz -= b->reclen;
	rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
	assert(IS_REPLICATED(rectype));
	rec_jnl_seqno = GET_REPL_JNL_SEQNO(b->recbuff);
	assert(QWEQ(rec_jnl_seqno, ctl->seqno));
	if (rec_jnl_seqno > ctl->max_seqno)
		CTL_SET_MAX_SEQNO(ctl, rec_jnl_seqno, b->recaddr, b->recaddr, DO_EOF_ADDR_CHECK);
	ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
	if (!IS_FENCED(rectype) || JRT_NULL == rectype)
	{	/* Entire transaction done */
		repl_rctl->read_complete = TRUE;
		trans_read = TRUE;
	} else
	{
		assert(IS_TUPD(rectype) || IS_FUPD(rectype)); /* The first record should be the beginning of a transaction */
	}
	/* Suggested optimisation : Instead of waiting for all records pertaining to this transaction to
	 * be written to the journal file, read those available, mark this file BLOCKED, read other journal
	 * files, and come back to this journal file later.
	 */
	while (!repl_rctl->read_complete) /* Read the rest of the transaction */
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
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
					assert(readlen % JNL_WRT_END_MODULUS == 0);
					*bufsiz -= b->reclen;
				} else
				{
					memcpy(tcombuffp, b->recbuff, b->reclen);
					tcombuffp += b->reclen;
					tot_tcom_len += b->reclen;
					/* End of transaction in this file */
					repl_rctl->read_complete = TRUE;
					if (num_tcom == -1)
						num_tcom = ((jnl_record *)b->recbuff)->jrec_tcom.num_participants;
					num_tcom--;
					if (num_tcom == 0) /* Read the whole trans */
						trans_read = TRUE;
				}
				rec_jnl_seqno = GET_JNL_SEQNO(b->recbuff);
				assert(rec_jnl_seqno == ctl->seqno);
				if (rec_jnl_seqno > ctl->max_seqno)
					CTL_SET_MAX_SEQNO(ctl, rec_jnl_seqno, b->recaddr, b->recaddr, DO_EOF_ADDR_CHECK);
				ctl->tn = ((jrec_prefix *)b->recbuff)->tn;
			} else if (rectype == JRT_EOF)
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_REPLBRKNTRANS, 1, &read_jnl_seqno,
						ERR_TEXT, 2, LEN_AND_LIT("Early EOF found"));
			}
		} else if (status == EREPL_JNLRECINCMPL)
		{	/* Log warning message for every certain number of attempts. There might have been a crash and the file
			 * might have been corrupted. The file possibly might never grow. Such cases have to be detected and
			 * attempt to read such files aborted.  If the journal file is a previous generation, waste no time waiting
			 * for journal records to be written, but error out right away. That should never happen, hence the assert
			 * and gtm_fork_n_core.
			 */
			if (ctl->eof_addr_final)
			{
				assert(FALSE);
				gtm_fork_n_core();
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_JNLRECINCMPL, 4,
					b->recaddr, ctl->jnl_fn_len, ctl->jnl_fn, &ctl->seqno);
			}
			if (gtmsource_recv_ctl_nowait())
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "State change detected in read_transaction\n");
				gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
				return 0;
			}
			GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
			gtmsource_poll_actions(TRUE);
			if(GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "State change detected in read_transaction (poll)\n");
				gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
				return 0;
			}
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNL_RECS);
			total_wait_for_jnl_recs += GTMSOURCE_WAIT_FOR_JNL_RECS;
			if (0 == (total_wait_for_jnl_recs % LOG_WAIT_FOR_JNL_FLUSH_PERIOD))
				GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE(ctl, csa); /* See if a "jnl_flush" might help nudge */
			if (0 == (total_wait_for_jnl_recs % LOG_WAIT_FOR_JNL_RECS_PERIOD))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %u seconds for journal "
					"record(s) to be written to journal file %s while attempting to read seqno %llu [0x%llx]. "
					"[dskaddr 0x%x freeaddr 0x%x rsrv_freeaddr 0x%x]. Check for problems with journaling\n",
					total_wait_for_jnl_recs / 1000, ctl->jnl_fn, ctl->seqno, ctl->seqno,
					csa->jnl->jnl_buff->dskaddr, csa->jnl->jnl_buff->freeaddr,
					csa->jnl->jnl_buff->rsrv_freeaddr);
			}
		} else
		{
			assertpro(status == EREPL_JNLRECFMT);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
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
			assertpro(FALSE);	/* Program bug */
		}
		REPL_DPRINT1("do_linear_search: initiating fresh read\n");
	} else
	{	/* use what has been read already */
		assert(read_seqno != ctl->seqno); /* if not, we'll skip to the next transaction and declare read_seqno not found */
	}
	found = TR_NOT_FOUND;
	srch_status->prev_seqno = srch_status->seqno = 0;
	while (found == TR_NOT_FOUND && b->reclen < max_readaddr - b->recaddr)
	{
		if ((status = repl_next(rb)) == SS_NORMAL)
		{
			rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
			if (IS_REPLICATED(rectype))
			{
				rec_jnl_seqno = GET_JNL_SEQNO(b->recbuff);
				if (srch_status->seqno == 0 || srch_status->seqno != rec_jnl_seqno)
				{ /* change srch_status only when records of different transactions are encountered */
					srch_status->prev_seqno = srch_status->seqno;
					srch_status->seqno = rec_jnl_seqno;
				}
				if (ctl->max_seqno < rec_jnl_seqno)
					CTL_SET_MAX_SEQNO(ctl, rec_jnl_seqno, b->recaddr, b->recaddr, SKIP_EOF_ADDR_CHECK);
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
			rts_error_csa(CSA_ARG(&FILE_INFO(ctl->reg)->s_addrs) VARLSTCNT(5)
					ERR_JNLBADRECFMT, 3, ctl->jnl_fn_len, ctl->jnl_fn, b->recaddr);
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
				rectype = (enum jnl_record_type)((jrec_prefix *)b->recbuff)->jrec_type;
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
				assertpro(FALSE);
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
			 * adjust ctl->seqno to point to ctl->max_seqno that way we don't repeat binary search (wastefully) for
			 * all seqnos between read_seqno and the least seqno larger than read_seqno in this file.
			 */
			willnotbefound_addr = hi_addr;
			assert(ctl->max_seqno_dskaddr < rb->fc->eof_addr);
			willnotbefound_stop_at = rb->fc->eof_addr;
			if (lo_addr == hi_addr)
			{	/* Since hi_addr corresponds to ctl->max_seqno which is in turn > read_seqno, the above
				 * "if" check succeeding implies we can never find read_seqno here. So skip calling
				 * "do_binary_search" in that case as that and "do_linear_search" have asserts that expect
				 * callers to never invoke them with the same value of lo_addr and hi_addr.
				 */
				found = TR_NOT_FOUND;
				srch_func = NULL;
			}
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
		{	/* trying to locate min, better to do linear search */
			srch_func = do_linear_search;
			lo_addr = ctl->min_seqno_dskaddr;
			hi_addr = MAXUINT4;
			/* read_seqno == ctl->seqno == ctl->min_seqno is a special case. But, don't know how that can happen without
			 * lookback set and hence the assert below.
			 */
			assert((read_seqno != ctl->seqno) || ctl->lookback);
			if ((read_seqno == ctl->seqno) && (lo_addr == b->recaddr))
			{	/* we are positioned where we want to be, no need for a read */
				assert(MIN_JNLREC_SIZE <= b->reclen);
				DEBUG_ONLY(jrec = (jnl_record *)b->recbuff);
				DEBUG_ONLY(rectype = (enum jnl_record_type)jrec->prefix.jrec_type);
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
#	if defined(GTMSOURCE_READFILES_LINEAR_SEARCH_TEST)
	srch_func = do_linear_search;
	hi_addr = MAXUINT4;
#	elif defined(GTMSOURCE_READFILES_BINARY_SEARCH_TEST)
	srch_func = do_binary_search;
	hi_addr = rb->fc->eof_addr;
#	endif
	REPL_DPRINT6("position_read: Using %s search to locate %llu in %s between %u and %u\n",
			(srch_func == do_linear_search) ? "linear" : "binary", read_seqno, ctl->jnl_fn, lo_addr, hi_addr);
	if (NULL != srch_func)
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
	int			wait_for_jnlopen_log_num = -1;
	sgmnt_addrs		*csa;
	gtmsource_state_t	gtmsource_state_sav;
	repl_rctl_elem_t	*repl_rctl;

	trans_read = FALSE;
	num_tcom = -1;
	tot_tcom_len = 0;
	total_read = 0;
	total_wait_for_jnl_recs = 0;
	total_wait_for_jnlopen = 0;
	tcombuffp = gtmsource_tcombuff_start;
	buff_avail = maxbufflen;
	/* ensure that buff is always within gtmsource_msgp bounds (especially in case the buffer got expanded in the last call) */
	assert((buff >= (uchar_ptr_t)gtmsource_msgp + REPL_MSG_HDRLEN)
			&& (buff <= (uchar_ptr_t)gtmsource_msgp + gtmsource_msgbufsiz));
	for (repl_rctl = repl_rctl_list; NULL != repl_rctl; repl_rctl = repl_rctl->next)
		repl_rctl->read_complete = FALSE;
	for (pass = 1; !trans_read; pass++)
	{
		if (1 < pass)
		{
			if (gtmsource_recv_ctl_nowait())
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "State change detected in read_and_merge\n");
				gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
				return 0;
			}
			GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
			gtmsource_poll_actions(TRUE);
			if(GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
			{
				repl_log(gtmsource_log_fp, TRUE, TRUE, "State change detected in read_and_merge (poll)\n");
				gtmsource_set_lookback();	/* In case we read ahead, enable looking back. */
				return 0;
			}
			SHORT_SLEEP(GTMSOURCE_WAIT_FOR_JNLOPEN); /* sleep for 10 msec between each iteration */
			total_wait_for_jnlopen += GTMSOURCE_WAIT_FOR_JNLOPEN;
			if (0 == (total_wait_for_jnlopen % LOG_WAIT_FOR_JNL_FLUSH_PERIOD))
			{
				for (ctl = repl_ctl_list->next; NULL != ctl; ctl = ctl->next)
				{
					csa = &FILE_INFO(ctl->reg)->s_addrs;
					GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE(ctl, csa); /* See if a "jnl_flush" might help nudge */
				}
			}
			if (0 == (total_wait_for_jnlopen % LOG_WAIT_FOR_JNLOPEN_PERIOD))
			{	/* We have waited for 5000 intervals of 10 msec each, for a total of 50 seconds sleep.
				 * Issue alert every 50 seconds.
				 */
				repl_log(gtmsource_log_fp, TRUE, TRUE, "REPL_WARN : Source server waited %u seconds for journal"
					" file(s) to be opened, or updated while attempting to read seqno %llu [0x%llx]. Check"
					" for problems with journaling\n", total_wait_for_jnlopen / 1000,
					read_jnl_seqno, read_jnl_seqno);
				for (ctl = repl_ctl_list->next; NULL != ctl; ctl = ctl->next)
				{
					csa = &FILE_INFO(ctl->reg)->s_addrs;
					repl_log(gtmsource_log_fp, TRUE, TRUE, "DBG_INFO: Journal File: %s for"
						" Database File: %s; State: %s. Timer PIDs(%d): (%u %u %u)\n",
						ctl->jnl_fn, ctl->reg->dyn.addr->fname,
						jnl_file_state_lit[ctl->file_state], csa->nl->wcs_timers + 1,
						csa->nl->wt_pid_array[0], csa->nl->wt_pid_array[1],
						csa->nl->wt_pid_array[2]);
					repl_log(gtmsource_log_fp, TRUE, TRUE, " "
						"ctl->seqno = %llu [0x%llx]. [dskaddr = 0x%x,freeaddr = 0x%x,rsrv_freeaddr = 0x%x]."
						" ctl->repl_rctl->read_complete = %d\n",
						ctl->seqno, ctl->seqno, csa->jnl->jnl_buff->dskaddr,
						csa->jnl->jnl_buff->freeaddr, csa->jnl->jnl_buff->rsrv_freeaddr,
						ctl->repl_rctl->read_complete);
				}
			}
		}
		GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
		read_len = read_regions(&buff, &buff_avail, pass > 1, &brkn_trans, read_jnl_seqno);
		if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
			return 0;
		if (brkn_trans)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_REPLBRKNTRANS, 1, &read_jnl_seqno);
		total_read += read_len;
		assert(total_read % JNL_WRT_END_MODULUS == 0);
	}
	if (tot_tcom_len > 0)
	{	/* Copy all the TCOM records to the end of the buffer */
		if (tot_tcom_len > buff_avail)
			increase_buffer(&buff, &buff_avail, tot_tcom_len);
		assert(buff + tot_tcom_len <= ((unsigned char *)gtmsource_msgp + gtmsource_msgbufsiz));
		memcpy(buff, gtmsource_tcombuff_start, tot_tcom_len);
		total_read += tot_tcom_len;
		assert(total_read % JNL_WRT_END_MODULUS == 0);
	}
	return (total_read);
}

static	int read_regions(unsigned char **buff, int *buff_avail,
		         boolean_t attempt_open_oldnew, boolean_t *brkn_trans, seq_num read_jnl_seqno)
{
	repl_ctl_element	*ctl, *prev_ctl, *next_ctl, *old_ctl;
	gd_region		*region;
	tr_search_state_t	found;
	int			read_len, cumul_read;
	int			nopen;
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */
	DEBUG_ONLY(static int	loopcnt;)
	sgmnt_addrs		*csa;
	jnlpool_ctl_ptr_t	jctl;
	uint4			freeaddr;
	gtmsource_state_t	gtmsource_state_sav;
	repl_rctl_elem_t	*repl_rctl;
	boolean_t		ctl_close;
	seq_num			next_ctl_min_seqno;

	cumul_read = 0;
	*brkn_trans = TRUE;
	assert(repl_ctl_list->next != NULL);
	assert(NULL != jnlpool);
	jctl = jnlpool->jnlpool_ctl;
	/* For each region */
	assert(repl_ctl_list->next == repl_rctl_list->ctl_start);
	for (repl_rctl = repl_rctl_list; (NULL != repl_rctl) && !trans_read; repl_rctl = repl_rctl->next)
	{
		ctl = repl_rctl->ctl_start;
		prev_ctl = ctl->prev;
		assert(NULL != prev_ctl);
		found = TR_NOT_FOUND;
		region = ctl->reg;
		DEBUG_ONLY(loopcnt = 0;)
		do
		{	/* Find the generation of the journal file which has read_jnl_seqno */
			for ( ; ; )
			{
				if ((NULL == ctl) || (ctl->reg != region))
					break;
				if ((JNL_FILE_OPEN == ctl->file_state) || (JNL_FILE_UNREAD == ctl->file_state))
					break;
				assert((JNL_FILE_CLOSED == ctl->file_state) || (JNL_FILE_EMPTY == ctl->file_state));
				assert(ctl->first_read_done);
				if (read_jnl_seqno <= ctl->max_seqno)
					break;
				next_ctl = ctl->next;
				/* "ctl" is no longer needed for any future seqnos (until a reconnection occurs)
				 * so close it and free up associated "fd" and "memory" with one exception.
				 * If the max-seqno of "ctl" is lesser than the min-seqno of "next_ctl".
				 * In this case, keep "ctl" open until "read_jnl_seqno becomes >= next_ctl->min_seqno".
				 */
				assert(next_ctl->reg == ctl->reg);
				assert(next_ctl->repl_rctl == repl_rctl);
				if (!next_ctl->first_read_done)
				{
					GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
					first_read(next_ctl);
					if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
					{
						repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Abandoning read_regions (first_read CLOSED/EMPTY)\n");
						return 0;
					}
					assert(next_ctl->first_read_done);
				}
				assert(ctl->max_seqno || ((JNL_FILE_EMPTY == ctl->file_state)
								&& (1 == ctl->repl_buff->fc->jfh->start_seqno)));
				next_ctl_min_seqno = next_ctl->min_seqno;
				if (!next_ctl->min_seqno)
				{
					assert((JNL_FILE_UNREAD == next_ctl->file_state)
						|| (JNL_FILE_EMPTY == next_ctl->file_state));
					next_ctl_min_seqno = next_ctl->repl_buff->fc->jfh->start_seqno;
				}
				assert(next_ctl_min_seqno > ctl->max_seqno);
				ctl_close = (read_jnl_seqno >= next_ctl_min_seqno);
				if (ctl_close)
				{
					prev_ctl->next = next_ctl;
					next_ctl->prev = prev_ctl;
					repl_rctl = ctl->repl_rctl;
					if (repl_rctl->ctl_start == ctl)
						repl_rctl->ctl_start = next_ctl;
					repl_ctl_close(ctl);
				}
				ctl = next_ctl;
			}
			if ((NULL == ctl) || (ctl->reg != region))
			{	/* Hit the end of generation list for journal file */
				if (!attempt_open_oldnew)
				{	/* Reposition to skip prev_ctl */
					REPL_DPRINT2("First pass...not opening newer gener file...skipping %s\n", prev_ctl->jnl_fn);
					ctl = prev_ctl;
					prev_ctl = ctl->prev;
					found = TR_FIND_WOULD_BLOCK;
					continue;
				}
				/* We come here ONLY if we have searched from the current generation journal file (as known to the
				 * source server) backwards till the oldest generation journal files of this region and failed to
				 * find read_jnl_seqno. Open newer generation journal files (if any) to see if they contain
				 * read_jnl_seqno
				 */
				GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
				nopen = open_newer_gener_jnlfiles(region, prev_ctl);
				if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
					return 0;
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
						freeaddr = csa->jnl->jnl_buff->rsrv_freeaddr;
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
				{
					GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
					first_read(ctl);
					if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
					{
						repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Abandoning read_regions (first_read UNREAD)\n");
						return 0;
					}
				}
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
						GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
						if (open_prev_gener(&old_ctl, ctl, read_jnl_seqno) == 0)
						{
							if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
							{
								repl_log(gtmsource_log_fp, TRUE, TRUE,
									"Abandoning read_regions (open_prev_gener UNREAD)\n");
								return 0;
							}
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
					assert((JNL_FILE_CLOSED == ctl->prev->file_state)
						&& (ctl->prev->max_seqno < read_jnl_seqno)
							|| (JNL_FILE_EMPTY == ctl->prev->file_state));
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
				GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
				if (open_prev_gener(&old_ctl, ctl, read_jnl_seqno) == 0)
				{
					if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
					{
						repl_log(gtmsource_log_fp, TRUE, TRUE,
								"Abandoning read_regions (open_prev_gener EMPTY)\n");
						return 0;
					}
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
					&& (ctl->min_seqno <= read_jnl_seqno));
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
					if (!ctl->repl_rctl->read_complete)
					{
						GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
						if ((read_len = read_transaction(ctl, buff, buff_avail, read_jnl_seqno)) < 0)
							assert(repl_errno == EREPL_JNLEARLYEOF);
						if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
							return 0;
						cumul_read += read_len;
						assert(cumul_read % JNL_WRT_END_MODULUS == 0);
					}
					found = TR_FOUND;
				} else if (read_jnl_seqno < ctl->seqno)
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
						GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
						if (update_max_seqno_info(ctl) != SS_NORMAL)
						{
							assert(repl_errno == EREPL_JNLEARLYEOF);
							assertpro(FALSE); /* Program bug */
						}
						if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
						{
							repl_log(gtmsource_log_fp, TRUE, TRUE,
									"Abandoning read_regions (update_max file/pool/file)\n");
							return 0;
						}
						if (read_jnl_seqno <= ctl->max_seqno)
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
					{	/* Program bug - ctl->seqno should never be greater than ctl->max_seqno */
						assertpro(FALSE);
					}
				}
			}
		} while (TR_NOT_FOUND == found);
		/* Move to the next region, now that the tr has been found or will not be found */
		*brkn_trans = (*brkn_trans && (TR_WILL_NOT_BE_FOUND == found));
	}
	assert(!*brkn_trans || (gtm_white_box_test_case_enabled &&
				((WBTEST_REPLBRKNTRANS == gtm_white_box_test_case_number)
					|| (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number))));
	return (cumul_read);
}

int gtmsource_readfiles(unsigned char *buff, int *data_len, int maxbufflen, boolean_t read_multiple)
{
	int4			read_size, read_state, first_tr_len, tot_tr_len, loopcnt;
	unsigned char		*orig_msgp, seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;
	seq_num			read_jnl_seqno, max_read_seqno;
	qw_num			read_addr;
	uint4			jnlpool_size;
	boolean_t		file2pool;
	unsigned int		start_heartbeat;
	boolean_t		stop_bunching;
	gtmsource_state_t	gtmsource_state_sav;

	jctl = jnlpool->jnlpool_ctl;
	gtmsource_local = jnlpool->gtmsource_local;
	jnlpool_size = jctl->jnlpool_size;
	max_read_seqno = jctl->jnl_seqno;
	/* Note that we are fetching the value of "jctl->jnl_seqno" without a lock on the journal pool. This means we could
	 * get an inconsistent 8-byte value (i.e. neither the pre-update nor the post-update value) which is possible if a
	 * concurrent GT.M process updates this 8-byte field in a sequence of two 4-byte operations instead of one
	 * atomic operation (possible in architectures where 8-byte operations are not native) AND if the pre-update and
	 * post-update value differ in their most significant 4-bytes. Since that is considered a virtually impossible
	 * rare occurrence and since we want to avoid the overhead of doing a "grab_lock", we don't do that here.
	 */
	assert(REPL_MSG_HDRLEN == SIZEOF(jnldata_hdr_struct));
	DEBUG_ONLY(loopcnt = 0;)
	do
	{
		assert(maxbufflen == gtmsource_msgbufsiz - REPL_MSG_HDRLEN);
		DEBUG_ONLY(loopcnt++);
		file2pool = FALSE;
		if (max_read_seqno > gtmsource_local->next_histinfo_seqno)
			max_read_seqno = gtmsource_local->next_histinfo_seqno; /* Do not read more than next histinfo boundary */
		assert(MAX_SEQNO != max_read_seqno);
		read_jnl_seqno = gtmsource_local->read_jnl_seqno;
		assert(read_jnl_seqno <= max_read_seqno);
		if (read_jnl_seqno == gtmsource_local->next_histinfo_seqno)
		{	/* Request a REPL_HISTREC message be sent first before sending any more seqnos across */
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SEND_NEW_HISTINFO;
			REPL_DPRINT1("REPL_HISTREC message first needs to be sent before any more seqnos can be sent across\n");
			return 0;
		}
		TIMEOUT_INIT(stop_bunching, BUNCHING_TIME);
		read_addr = gtmsource_local->read_addr;
		GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
		first_tr_len = read_size = read_and_merge(buff, maxbufflen, read_jnl_seqno++) + REPL_MSG_HDRLEN;
		if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
		{
			TIMEOUT_DONE(stop_bunching);
			return 0;
		}
		tot_tr_len = 0;
		do
		{
			tot_tr_len += read_size;
			REPL_DPRINT5("File read seqno : %llu Tr len : %d Total tr len : %d Maxbufflen : %d\n", read_jnl_seqno - 1,
					read_size - REPL_MSG_HDRLEN, tot_tr_len, maxbufflen);
			if (gtmsource_save_read_jnl_seqno < read_jnl_seqno)
			{
				read_addr += read_size;
				if (jnlpool_size >= (jctl->rsrv_write_addr - read_addr))
				{	/* No more overflow, switch to READ_POOL.  To avoid the expense of memory barrier
					 * in jnlpool_hasnt_overflowed(), we use a possibly stale value of rsrv_write_addr
					 * to check if we can switch back to pool. The consequence is that we may switch
					 * back and forth between file and pool read if we are in a situation wherein a GTM
					 * process races with source server, writing transactions into the pool right when
					 * the source server concludes that it can read from pool. We think this condition
					 * is rare enough that the expense of re-opening the files (due to the transition)
					 * and re-positioning read pointers is considered liveable when compared with the
					 * cost of a memory barrier. We can reduce the expense by not clearing the file
					 * information for every transition back to pool. We can wait for a certain period
					 * of time (say 15 minutes) before we close all files.
					 */
					file2pool = TRUE;
					break;
				}
				REPL_DPRINT3("Readfiles : after sync with pool read_seqno: %llu read_addr: %llu\n",
					read_jnl_seqno, read_addr);
			}
			/* If reading multiple transactions in one shot, make sure we stop the bunching if at least 8 seconds
			 * (a heartbeat period) has elapsed during the bunching. This way we send whatever we have now rather
			 * than accumulating transactions in our huge internal buffer and avoid risking the user perception
			 * of no-progress. In the worst case we could be unresponsive for 8 seconds (1 heartbeat period)
			 * with this approach.
			 */
			read_multiple = read_multiple && !stop_bunching;
			if (read_multiple)
			{	/* Ok to read multiple transactions. Limit the multiple reads until there is no more to be read
				 * OR total read size reaches a fixed value MAX_TR_BUFFSIZE. This strikes a fine balance between
				 * reducing the # of "send()" system calls done by the source server versus being responsive to
				 * the user (in case of shutdown requests). Time spent reading multiple transactions is where
				 * the source server is not responsive to outside requests and is hence better minimized.
				 */
				/* Note: Recompute buff and maxbufflen below buffer may have expanded during read_and_merge */
				buff = (unsigned char *)gtmsource_msgp + tot_tr_len + REPL_MSG_HDRLEN;
				maxbufflen = gtmsource_msgbufsiz - tot_tr_len - REPL_MSG_HDRLEN;
				if ((tot_tr_len < MAX_TR_BUFFSIZE) && (read_jnl_seqno < max_read_seqno))
				{
					assert(0 < maxbufflen);
					GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
					read_size = read_and_merge(buff, maxbufflen, read_jnl_seqno++) + REPL_MSG_HDRLEN;
					if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
					{	/* Control message triggered state change */
						TIMEOUT_DONE(stop_bunching);
						return 0;
					}
					/* Don't use buff to assign type and len as buffer may have expanded.
					 * Use gtmsource_msgp instead */
					((repl_msg_ptr_t)((unsigned char *)gtmsource_msgp + tot_tr_len))->type = REPL_TR_JNL_RECS;
					((repl_msg_ptr_t)((unsigned char *)gtmsource_msgp + tot_tr_len))->len = read_size;
					continue;
				}
				REPL_DPRINT5("Readfiles : tot_tr_len %d read_jnl_seqno %llu max_read_seqno %llu "
					"gtmsource_msgbufsize : %d; stop multiple reads\n", tot_tr_len,
					read_jnl_seqno, max_read_seqno, gtmsource_msgbufsiz);
			}
			break;
		} while (TRUE);
		TIMEOUT_DONE(stop_bunching);
		assert(read_jnl_seqno <= max_read_seqno);
		if ((gtmsource_local->next_histinfo_num < gtmsource_local->num_histinfo)
			|| (gtmsource_local->num_histinfo == jnlpool->repl_inst_filehdr->num_histinfo))
		{	/* We are either sending seqnos of a histinfo that is not the last one in the instance file OR
			 * we are sending seqnos of the last histinfo (that is open-ended) but there has been no more histinfo
			 * records concurrently added to this instance file compared to what is in our private memory. In either
			 * case, it is safe to send these seqnos without worrying about whether a new histinfo record needs to
			 * be sent first.
			 */
			break;
		} else
		{	/* Set the next histinfo record's start_seqno and redo the read with the new
			 * "gtmsource_local->next_histinfo_seqno" */
			assert(MAX_SEQNO == gtmsource_local->next_histinfo_seqno);
			gtmsource_set_next_histinfo_seqno(TRUE);
			if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
				return 0; /* Connection got reset in "gtmsource_set_next_histinfo_seqno" */
			/* Since the buffer may have expanded, reposition buff to the beginning and set maxbufflen to the maximum
			 * available size (as if this is the first time we came into the while loop)
			 */
			buff = (unsigned char *)gtmsource_msgp + REPL_MSG_HDRLEN;
			maxbufflen = gtmsource_msgbufsiz - REPL_MSG_HDRLEN;
		}
	} while (TRUE);
	if (file2pool && !gtmsource_local->jnlfileonly)
	{
		/* Ahead of the transition to pool, force repl_phase2_cleanup() when write_addr is behind read_addr. This
		 * condition happens frequently with replicating instances and instances with infrequent updates.
		 */
		if (jctl->write_addr < read_addr)
			repl_phase2_cleanup(jnlpool);
		assert(jctl->write_addr >= read_addr);
		gtmsource_local->read = (uint4)(read_addr % jnlpool_size) ;
		gtmsource_local->read_state = read_state = READ_POOL;
	} else
		read_state = gtmsource_local->read_state;
	gtmsource_local->read_addr = read_addr;
	assert(read_jnl_seqno <= gtmsource_local->next_histinfo_seqno);
	gtmsource_local->read_jnl_seqno = read_jnl_seqno;
	GTMDBGFLAGS_NOFREQ_ONLY(GTMSOURCE_FORCE_READ_FILE_MODE, gtmsource_local->read_state = read_state = READ_FILE);
	if (read_state == READ_POOL)
	{
		gtmsource_ctl_close(); /* no need to keep files open now that we are going to read from pool */
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Source server now reading from journal pool at seqno %llu [0x%llx]\n",
				read_jnl_seqno, read_jnl_seqno);
		REPL_DPRINT3("Readfiles : after switch to pool, read_addr : "INT8_FMT" read : %u\n",
				INT8_PRINT(read_addr), gtmsource_local->read);
	}
	*data_len = (first_tr_len - REPL_MSG_HDRLEN);
	return (tot_tr_len);
}

/* This function resets "zqgblmod_seqno" and "zqgblmod_tn" in all replicated database file headers to correspond to the
 * "resync_seqno" passed in as input. This shares some of its code with the function "repl_inst_reset_zqgblmod_seqno_and_tn".
 * Any changes there might need to be reflected here.
 */
int gtmsource_update_zqgblmod_seqno_and_tn(seq_num resync_seqno)
{
	repl_ctl_element	*ctl, *next_ctl, *old_ctl;
	gd_region		*region;
	sgmnt_addrs		*csa;
	unsigned char		seq_num_str[32], *seq_num_ptr;  /* INT8_PRINT */
	boolean_t		was_crit;
	seq_num			start_seqno, max_zqgblmod_seqno;
	trans_num		bov_tn;
	gtmsource_state_t	gtmsource_state_sav;

	gtmsource_ctl_close();
	gtmsource_ctl_init();
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
	{	/* Possible if grab_crit done from gtmsource_ctl_init -> repl_ctl_create detected an Online Rollback as part
		 * of grab_crit
		 */
		return -1; /* gtmsource_ctl_close will be done by gtmsource_process */
	}
	max_zqgblmod_seqno = 0;
	for (ctl = repl_ctl_list->next; ctl != NULL; ctl = next_ctl)
	{
		next_ctl = ctl->next;
		assert((NULL == next_ctl) || (ctl->reg != next_ctl->reg));
		repl_log(gtmsource_log_fp, TRUE, FALSE, "Updating ZQGBLMOD SEQNO and TN for Region [%s]\n", ctl->reg->rname);
		GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
		first_read(ctl);
		if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
		{
			repl_log(gtmsource_log_fp, TRUE, TRUE, "Abandoning gtmsource_update_zqgblmod_seqno_and_tn (first_read)\n");
			return (SS_NORMAL);
		}
		do
		{
			assert(ctl->first_read_done);
			start_seqno = ctl->repl_buff->fc->jfh->start_seqno;
			repl_log(gtmsource_log_fp, TRUE, FALSE, "Region [%s] : Journal file [%s] : Start Seqno : [0x%llx]\n",
				ctl->reg->rname, ctl->jnl_fn, start_seqno);
			if (start_seqno <= resync_seqno)
				break;
			GTMSOURCE_SAVE_STATE(gtmsource_state_sav);
			if (0 == open_prev_gener(&old_ctl, ctl, resync_seqno))	/* this automatically does a "first_read" */
			{	/* Previous journal file link was NULL. Issue error. */
				if (GTMSOURCE_NOW_TRANSITIONAL(gtmsource_state_sav))
				{
					repl_log(gtmsource_log_fp, TRUE, TRUE,
							"Abandoning gtmsource_update_zqgblmod_seqno_and_tn (open_prev_gener)\n");
					return (SS_NORMAL);
				}
				rts_error_csa(CSA_ARG(&FILE_INFO(ctl->reg)->s_addrs)
					VARLSTCNT(4) ERR_NOPREVLINK, 2, ctl->jnl_fn_len, ctl->jnl_fn);
			}
			assert(old_ctl->next == ctl);
			assert(ctl->prev == old_ctl);
			ctl = old_ctl;
		} while (TRUE);
		assert(NULL != ctl);
		csa = &FILE_INFO(ctl->reg)->s_addrs;
		assert(REPL_ALLOWED(csa->hdr));
		bov_tn = ctl->repl_buff->fc->jfh->bov_tn;
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Assigning Region [%s] : Resync Seqno [0x%llx] : ZQGBLMOD SEQNO [0x%llx] "
			": ZQGBLMOD TN : [0x%llx]\n", ctl->reg->rname, resync_seqno, start_seqno, bov_tn);
		/* csa->hdr->zqgblmod_seqno is modified ONLY by the source server OR online rollback (both of these hold the
		 * database crit while doing so). It is also read by fileheader_sync() which does so while holding crit.
		 * To avoid the latter from reading an inconsistent value (i.e neither the pre-update nor the post-update
		 * value, which is possible if the 8-byte operation is not atomic but a sequence of two 4-byte operations
		 * AND if the pre-update and post-update value differ in their most significant 4-bytes) we grab_crit. We
		 * could have used QWCHANGE_IS_READER_CONSISTENT macro (which checks for most significant 4-byte difference)
		 * instead to determine if it is really necessary to grab crit. But, since the update to zqgblmod_seqno is a
		 * rare operation, we decided to play it safe.
		 */
		assert(!csa->hold_onto_crit);
		if (FALSE == (was_crit = csa->now_crit))
			grab_crit(ctl->reg);
		if (csa->onln_rlbk_cycle != csa->nl->onln_rlbk_cycle)
		{
			assert(process_id != jnlpool->gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
			SYNC_ONLN_RLBK_CYCLES;
			gtmsource_onln_rlbk_clnup(); /* would have set gtmsource_state accordingly */
			if (!was_crit)
				rel_crit(ctl->reg);
			return -1; /* gtmsource_ctl_close will be done by gtmsource_process */
		}
		csa->hdr->zqgblmod_seqno = start_seqno;
		csa->hdr->zqgblmod_tn = bov_tn;
		if (FALSE == was_crit)
			rel_crit(ctl->reg);
		if (max_zqgblmod_seqno < start_seqno)
			max_zqgblmod_seqno = start_seqno;
	}
	assert(!jnlpool->jnlpool_ctl->max_zqgblmod_seqno || jnlpool->jnlpool_ctl->max_zqgblmod_seqno > resync_seqno);
	assert(0 < max_zqgblmod_seqno);
	assert(resync_seqno >= max_zqgblmod_seqno);
	assert(!(FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs.now_crit));
	grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK);
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
	{
		assert(process_id != jnlpool->gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
		return -1; /* gtmsource_ctl_close will be done by gtmsource_process */
	}
	jnlpool->jnlpool_ctl->max_zqgblmod_seqno = max_zqgblmod_seqno;
	rel_lock(jnlpool->jnlpool_dummy_reg);
	gtmsource_ctl_close(); /* close all structures now that we are done; if we have to read from journal files; we'll open
				* the structures again */
	return (SS_NORMAL);
}
