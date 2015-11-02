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

#include "mdef.h"

#include "gtm_string.h"

#ifdef UNIX
#include "gtm_stat.h"
#elif defined(VMS)
#include <fab.h>
#include <iodef.h>
#include <nam.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <descrip.h> /* Required for gtmsource.h */
#include <efndef.h>
#else
#error Unsupported platform
#endif
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#ifdef UNIX
#include "gtmio.h"
#endif
#include "iosp.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "tp_change_reg.h"
#include "is_file_identical.h"
#include "get_fs_block_size.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLDEF repl_ctl_element		*repl_ctl_list = NULL;

GBLREF jnlpool_addrs		jnlpool;
GBLREF seq_num			seq_num_zero;

GBLREF gd_addr			*gd_header;
GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_addrs		*cs_addrs;

GBLREF int			gtmsource_log_fd;
GBLREF FILE			*gtmsource_log_fp;
GBLREF int			gtmsource_statslog_fd;
GBLREF FILE			*gtmsource_statslog_fp;
#ifdef UNIX
GBLREF gtmsource_state_t	gtmsource_state;
GBLREF uint4			process_id;
#endif

error_def(ERR_JNLFILRDOPN);
error_def(ERR_JNLNOREPL);

repl_buff_t *repl_buff_create(uint4 buffsize, uint4 jnl_fs_block_size);

repl_buff_t *repl_buff_create(uint4 buffsize, uint4 jnl_fs_block_size)
{
	repl_buff_t	*tmp_rb;
	int		index;
	unsigned char	*buff_ptr;

	tmp_rb = (repl_buff_t *)malloc(SIZEOF(repl_buff_t));
	tmp_rb->buffindex = REPL_MAINBUFF;
	for (index = REPL_MAINBUFF; REPL_NUMBUFF > index; index++)
	{
		tmp_rb->buff[index].reclen = 0;
		tmp_rb->buff[index].recaddr = JNL_FILE_FIRST_RECORD;
		tmp_rb->buff[index].readaddr = JNL_FILE_FIRST_RECORD;
		tmp_rb->buff[index].buffremaining = buffsize;
		buff_ptr = (unsigned char *)malloc(buffsize + jnl_fs_block_size);
		tmp_rb->buff[index].base_buff = buff_ptr;
		tmp_rb->buff[index].base = (unsigned char *)ROUND_UP2((uintszofptr_t)buff_ptr, jnl_fs_block_size);
		tmp_rb->buff[index].recbuff = tmp_rb->buff[index].base;
	}
	tmp_rb->fc = (repl_file_control_t *)malloc(SIZEOF(repl_file_control_t));
	return (tmp_rb);
}

/* Given a journal file name, this function opens that file explicitly without going through "jnl_ensure_open" */
int repl_open_jnl_file_by_name(repl_ctl_element *tmp_ctl, int jnl_fn_len, char *jnl_fn, int *fd_ptr, void *stat_buf_ptr)
{
	int		tmp_fd;
	int		status;
#ifdef UNIX
	struct stat	stat_buf;
#elif defined(VMS)
	struct FAB	fab;
	struct NAM	stat_buf;
#else
#error Unsupported platform
#endif

	tmp_ctl->jnl_fn_len = jnl_fn_len;
	memcpy(tmp_ctl->jnl_fn, jnl_fn, jnl_fn_len);
	tmp_ctl->jnl_fn[jnl_fn_len] = '\0';
	status = SS_NORMAL;

	/* Open Journal File */
#	ifdef UNIX
	OPENFILE(tmp_ctl->jnl_fn, O_RDONLY, tmp_fd);
	if (0 > tmp_fd)
	{
		status = errno;
	}
	if (SS_NORMAL == status)
	{
		FSTAT_FILE(tmp_fd, &stat_buf, status);
		if (0 > status)
		{
			status = errno;
			assert(FALSE);
		}
#		ifdef __MVS
		else if (-1 == gtm_zos_tag_to_policy(tmp_fd, TAG_BINARY))
		{
			status = errno;
			assert(FALSE);
		}
#		endif
	}
	*((struct stat *)stat_buf_ptr) = stat_buf;
#	elif defined(VMS)
	fab = cc$rms_fab;
	fab.fab$l_fna = tmp_ctl->jnl_fn;
	fab.fab$b_fns = tmp_ctl->jnl_fn_len;
	fab.fab$l_fop = FAB$M_UFO;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	stat_buf      = cc$rms_nam;
	fab.fab$l_nam = &stat_buf;
	fab.fab$l_dna = JNL_EXT_DEF;
	fab.fab$b_dns = SIZEOF(JNL_EXT_DEF) - 1;
	status = sys$open(&fab);
	if (RMS$_NORMAL == status)
	{
		status = SS_NORMAL;
		tmp_fd = fab.fab$l_stv;
	}
	assert(SS_NORMAL == status);
	*((struct NAM *)stat_buf_ptr) = stat_buf;
#	endif
	REPL_DPRINT2("CTL INIT :  Direct open of file %s\n", tmp_ctl->jnl_fn);
	*fd_ptr = tmp_fd;
	return status;
}

int repl_ctl_create(repl_ctl_element **ctl, gd_region *reg, int jnl_fn_len, char *jnl_fn, boolean_t init)
{
	gd_region		*r_save;
	sgmnt_addrs 		*csa;
	sgmnt_data_ptr_t	csd;
	repl_ctl_element	*tmp_ctl = NULL;
	jnl_file_header		*tmp_jfh = NULL;
	jnl_file_header		*tmp_jfh_base = NULL;
	jnl_private_control	*jpc;
	int			tmp_fd = NOJNL;
	int			status;
	int 			gtmcrypt_errno;
	uint4			jnl_status;
	boolean_t		did_jnl_ensure_open = FALSE, was_crit;
	int4			lcl_jnl_fn_len;
	char			lcl_jnl_fn[JNL_NAME_SIZE];
#ifdef UNIX
	struct stat	stat_buf;
#elif defined(VMS)
	struct NAM	stat_buf;
	short		iosb[4];	/* needed by the F_READ_BLK_ALIGNED macro */
#else
#error Unsupported platform
#endif
	uint4			jnl_fs_block_size;

	status = SS_NORMAL;
	jnl_status = 0;

	tmp_ctl = (repl_ctl_element *)malloc(SIZEOF(repl_ctl_element));
	tmp_ctl->reg = reg;
	csa = &FILE_INFO(reg)->s_addrs;
	jpc = csa->jnl;
	if (init)
	{
		assert((0 == jnl_fn_len) && ((NULL == jnl_fn) || ('\0' == jnl_fn[0])));
		r_save = gv_cur_region;
		gv_cur_region = reg;
		tp_change_reg();
		assert(csa == cs_addrs);
		jpc->channel = NOJNL; /* Not to close the prev gener file */
		was_crit = csa->now_crit;
		if (!was_crit)
			grab_crit(reg);
#		ifdef UNIX
		if (csa->onln_rlbk_cycle != csa->nl->onln_rlbk_cycle)
		{	/* Concurrent online rollback. Possible only if we are called from gtmsource_update_zqgblmod_seqno_and_tn
			 * in which case we don't hold the gtmsource_srv_latch. Assert that.
			 */
			assert(process_id != jnlpool.gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
			SYNC_ONLN_RLBK_CYCLES;
			gtmsource_onln_rlbk_clnup();
			if (!was_crit)
				rel_crit(reg);
			return -1;
		}
#		endif
		/* Although replication may be WAS_ON, it is possible that source server has not yet sent records
		 * that were generated when replication was ON. We have to open and read this journal file to
		 * cover such a case. But in the WAS_ON case, do not ask for a jnl_ensure_open to be done since
		 * it will return an error (it will try to open the latest generation journal file and that will
		 * fail because of a lot of reasons e.g. "jpc->cycle" vs "jpc->jnl_buff->cycle" mismatch or db/jnl
		 * tn mismatch JNLTRANSLSS error etc.). This will even cause a journal file switch which the source
		 * server should never do (it is only supposed to READ from journal files). Open the journal file
		 * stored in the database file header (which will be non-NULL even though journaling is currently OFF)
		 * thereby can send as many seqnos as possible until the repl=WAS_ON/jnl=OFF state was reached.
		 */
		csd = csa->hdr;
		assert(REPL_ALLOWED(csd));
		if (!REPL_WAS_ENABLED(csd))
		{
			/* replication is allowed and has not gone into the WAS_ON state so journaling is expected to be ON*/
			assert(JNL_ENABLED(csd));
			did_jnl_ensure_open = TRUE;
			jnl_status = jnl_ensure_open();
			if (0 != jnl_status)
			{
				if (!was_crit)
					rel_crit(reg);
				if (SS_NORMAL != jpc->status)
					rts_error(VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), jpc->status);
				else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
			} else
			{
				tmp_ctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(tmp_ctl->jnl_fn, csd->jnl_file_name, tmp_ctl->jnl_fn_len);
				tmp_ctl->jnl_fn[tmp_ctl->jnl_fn_len] = '\0';
			}
			/* stash the shared fileid into private storage before rel_crit as it is used in JNL_GDID_PVT macro below */
			VMS_ONLY (jpc->fileid = csa->nl->jnl_file.jnl_file_id;)
			UNIX_ONLY(jpc->fileid = csa->nl->jnl_file.u;)
			REPL_DPRINT2("CTL INIT :  Open of file %s thru jnl_ensure_open\n", tmp_ctl->jnl_fn);
			tmp_fd = jpc->channel;
		} else
		{	/* Note that we hold crit so it is safe to pass csd->jnl_file_name (no one else will be changing it) */
			status = repl_open_jnl_file_by_name(tmp_ctl, csd->jnl_file_len, (char *)csd->jnl_file_name,
													&tmp_fd, &stat_buf);
		}
		if (!was_crit)
			rel_crit(reg);
		gv_cur_region = r_save;
		tp_change_reg();
		assert(NOJNL != tmp_fd);
	} else
		status = repl_open_jnl_file_by_name(tmp_ctl, jnl_fn_len, jnl_fn, &tmp_fd, &stat_buf);
	if (status == SS_NORMAL)
	{
		jnl_fs_block_size = get_fs_block_size(tmp_fd);
		/* Because the read below will be to the aligned buffer, and it will read an aligned size, we need
		 * to allocate jnl_file_header + 2 * jnl_fs_block_size.  There will be some throw-away before the
		 * buffer to get the alignment in place, and then up to jnl_fs_block_size after the header in order
		 * for the size to be a multiple of jnl_fs_block_size.
		 */
		tmp_jfh_base = (jnl_file_header *)malloc(SIZEOF(jnl_file_header) + (2 * jnl_fs_block_size));
		tmp_jfh = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)tmp_jfh_base, jnl_fs_block_size));
		F_READ_BLK_ALIGNED(tmp_fd, 0, tmp_jfh, ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size), status);
		assert(SS_NORMAL == status);
		if (SS_NORMAL == status)
			CHECK_JNL_FILE_IS_USABLE(tmp_jfh, status, FALSE, 0, NULL); /* FALSE => NO gtm_putmsg even if errors */
		assert(SS_NORMAL == status);
	}
	if ((SS_NORMAL != status) || (!REPL_ALLOWED(tmp_jfh)))
	{
		/* We need tmp_ctl->jnl_fn to issue the error but want to free up tmp_ctl before the error.
		 * So copy jnl_fn into local buffer before the error.
		 */
		lcl_jnl_fn_len = tmp_ctl->jnl_fn_len;
		assert(ARRAYSIZE(lcl_jnl_fn) >= ARRAYSIZE(tmp_ctl->jnl_fn));
		assert(lcl_jnl_fn_len < ARRAYSIZE(lcl_jnl_fn));
		memcpy(lcl_jnl_fn, tmp_ctl->jnl_fn, lcl_jnl_fn_len);
		lcl_jnl_fn[lcl_jnl_fn_len] = '\0';
		assert((NULL == tmp_jfh) || !REPL_ALLOWED(tmp_jfh));
		free(tmp_ctl);
		tmp_ctl = NULL;
		if (NULL != tmp_jfh_base)
			free(tmp_jfh_base);
		tmp_jfh = NULL;
		tmp_jfh_base = NULL;
		if (SS_NORMAL != status)
			rts_error(VARLSTCNT(7) ERR_JNLFILRDOPN, 4, lcl_jnl_fn_len, lcl_jnl_fn, DB_LEN_STR(reg), status);
		else
			rts_error(VARLSTCNT(6) ERR_JNLNOREPL, 4, lcl_jnl_fn_len, lcl_jnl_fn, DB_LEN_STR(reg));
	}
	assert(SS_NORMAL == status);	/* so jnl_fs_block_size is guaranteed to have been initialized */
	tmp_ctl->repl_buff = repl_buff_create(tmp_jfh->alignsize, jnl_fs_block_size);
	tmp_ctl->repl_buff->backctl = tmp_ctl;
	tmp_ctl->repl_buff->fc->eof_addr = JNL_FILE_FIRST_RECORD;
	tmp_ctl->repl_buff->fc->fs_block_size = jnl_fs_block_size;
	tmp_ctl->repl_buff->fc->jfh_base = tmp_jfh_base;
	tmp_ctl->repl_buff->fc->jfh = tmp_jfh;
	tmp_ctl->repl_buff->fc->fd = tmp_fd;
#	ifdef GTM_CRYPT
	if (tmp_jfh->is_encrypted)
	{
		ASSERT_ENCRYPTION_INITIALIZED;	/* should be done in db_init (gtmsource() -> gvcst_init() -> db_init()) */
		GTMCRYPT_GETKEY(csa, tmp_jfh->encryption_hash, tmp_ctl->encr_key_handle, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, tmp_ctl->jnl_fn_len, tmp_ctl->jnl_fn);
	}
#	endif
	if (did_jnl_ensure_open)
	{
		F_COPY_GDID(tmp_ctl->repl_buff->fc->id, JNL_GDID_PVT(csa));
		/* reset jpc->channel (would have been updated by jnl_ensure_open) as that corresponds to an
		 * actively updated journal file and is only for GT.M and never for source server which only
		 * READS from journal files. Source server anyways has a copy of the fd in tmp_ctl->repl_buff->fc->fd.
		 */
		jpc->channel = NOJNL;
	} else
	{
		F_COPY_GDID_FROM_STAT(tmp_ctl->repl_buff->fc->id, stat_buf);  /* For VMS stat_buf is a NAM structure */
	}

	QWASSIGN(tmp_ctl->min_seqno, seq_num_zero);
	QWASSIGN(tmp_ctl->max_seqno, seq_num_zero);
	QWASSIGN(tmp_ctl->seqno, seq_num_zero);
	tmp_ctl->tn = 1;
	tmp_ctl->file_state = JNL_FILE_UNREAD;
	tmp_ctl->lookback = FALSE;
	tmp_ctl->first_read_done = FALSE;
	tmp_ctl->eof_addr_final = FALSE;
	tmp_ctl->max_seqno_final = FALSE;
	tmp_ctl->read_complete = FALSE;
	tmp_ctl->min_seqno_dskaddr = 0;
	tmp_ctl->max_seqno_dskaddr = 0;
	tmp_ctl->next = tmp_ctl->prev = NULL;
	*ctl = tmp_ctl;

	return (SS_NORMAL);
}

int gtmsource_ctl_init(void)
{
	/* Setup ctl for reading from journal files */

	gd_region		*region_top, *reg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	repl_ctl_element	*tmp_ctl, *prev_ctl;
	int			jnl_file_len, status;

	repl_ctl_list = (repl_ctl_element *)malloc(SIZEOF(repl_ctl_element));
	memset((char_ptr_t)repl_ctl_list, 0, SIZEOF(*repl_ctl_list));
	prev_ctl = repl_ctl_list;

	UNIX_ONLY(assert(GTMSOURCE_HANDLE_ONLN_RLBK != gtmsource_state)); /* can't come here without handling online rollback */
	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{
			status = repl_ctl_create(&tmp_ctl, reg, 0, NULL, TRUE);
			assert((SS_NORMAL == status) UNIX_ONLY(|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)));
			UNIX_ONLY(
				if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
					return -1;
			)
			prev_ctl->next = tmp_ctl;
			tmp_ctl->prev = prev_ctl;
			tmp_ctl->next = NULL;
			prev_ctl = tmp_ctl;
		}
	}
	if (NULL == repl_ctl_list->next) /* No replicated region */
		GTMASSERT;
	return (SS_NORMAL);
}

int repl_ctl_close(repl_ctl_element *ctl)
{
	int	index;
	int	status;

	if (NULL != ctl)
	{
		if (NULL != ctl->repl_buff)
		{
			for (index = REPL_MAINBUFF; REPL_NUMBUFF > index; index++)
				if (NULL != ctl->repl_buff->buff[index].base_buff)
					free(ctl->repl_buff->buff[index].base_buff);
			if (NULL != ctl->repl_buff->fc)
			{
				if (NULL != ctl->repl_buff->fc->jfh_base)
					free(ctl->repl_buff->fc->jfh_base);
				if (NOJNL != ctl->repl_buff->fc->fd)
					F_CLOSE(ctl->repl_buff->fc->fd, status); /* resets "ctl->repl_buff->fc->fd" to FD_INVALID */
				free(ctl->repl_buff->fc);
			}
			free(ctl->repl_buff);
		}
		free(ctl);
	}
	return (SS_NORMAL);
}

int gtmsource_ctl_close(void)
{
	repl_ctl_element	*ctl;
	sgmnt_addrs		*csa;
	int			status;

	UNIX_ONLY(gtmsource_stop_jnl_release_timer();)
	if (repl_ctl_list)
	{
		for (ctl = repl_ctl_list->next; NULL != ctl; ctl = repl_ctl_list->next)
		{
			repl_ctl_list->next = ctl->next; /* next element becomes head thereby removing this element,
							    the current head from the list; if there is an error path that returns
							    us to this function before all elements were freed, we won't try to
							    free elements that have been freed already */
#			ifdef DEBUG
			csa = &FILE_INFO(ctl->reg)->s_addrs;
			/* jpc->channel should never be set to a valid value outside of repl_ctl_create. The only exception is if
			 * we were interrupted in the middle of repl_ctl_create by an external signal in which case the process
			 * better be exiting. Assert that.
			 */
			assert(((NULL != csa->jnl) && (NOJNL == csa->jnl->channel)) || process_exiting);
#			endif
			repl_ctl_close(ctl);
		}
		ctl = repl_ctl_list;
		repl_ctl_list = NULL;
		free(ctl);
	}
	return (SS_NORMAL);
}

int gtmsource_set_lookback(void)
{
	/* Scan all the region ctl's and set lookback to TRUE if ctl has to be
	 * repositioned for a transaction read from the past */

	repl_ctl_element	*ctl;

	for (ctl = repl_ctl_list->next; NULL != ctl; ctl = ctl->next)
	{
		if ((JNL_FILE_OPEN == ctl->file_state ||
		     JNL_FILE_CLOSED == ctl->file_state) &&
		    QWLE(jnlpool.gtmsource_local->read_jnl_seqno, ctl->seqno))
			ctl->lookback = TRUE;
		else
			ctl->lookback = FALSE;
	}
	return (SS_NORMAL);
}
