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
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLDEF repl_ctl_element	*repl_ctl_list = NULL;

GBLREF jnlpool_addrs	jnlpool;
GBLREF seq_num		seq_num_zero;

GBLREF gd_addr          *gd_header;
GBLREF gd_region        *gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

GBLREF int		gtmsource_log_fd;
GBLREF FILE		*gtmsource_log_fp;
GBLREF int		gtmsource_statslog_fd;
GBLREF FILE		*gtmsource_statslog_fp;

repl_buff_t *repl_buff_create(uint4 buffsize);

repl_buff_t *repl_buff_create(uint4 buffsize)
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
		buff_ptr = (unsigned char *)malloc(buffsize + DISK_BLOCK_SIZE);
		tmp_rb->buff[index].base_buff = buff_ptr;
		tmp_rb->buff[index].base = (unsigned char *)ROUND_UP2((uintszofptr_t)buff_ptr, DISK_BLOCK_SIZE);
		tmp_rb->buff[index].recbuff = tmp_rb->buff[index].base;
	}
	tmp_rb->fc = (repl_file_control_t *)malloc(SIZEOF(repl_file_control_t));
	return (tmp_rb);
}

int repl_ctl_create(repl_ctl_element **ctl, gd_region *reg,
		    int jnl_fn_len, char *jnl_fn, boolean_t init)
{

	gd_region		*r_save;
	sgmnt_addrs 		*csa;
	repl_ctl_element	*tmp_ctl = NULL;
	jnl_file_header		*tmp_jfh = NULL;
	jnl_file_header		*tmp_jfh_base = NULL;
	int			tmp_fd = NOJNL;
	int			status;
#ifdef UNIX
	struct stat		stat_buf;
#elif defined(VMS)
	struct FAB	fab;
	struct NAM	stat_buf;
	short		iosb[4];
#else
#error Unsupported platform
#endif
	GTMCRYPT_ONLY(
		int 		crypt_status;
	)
	uint4			jnl_status;

	error_def(ERR_JNLFILOPN);

	status = SS_NORMAL;
	jnl_status = 0;

	tmp_ctl = (repl_ctl_element *)malloc(SIZEOF(repl_ctl_element));
	tmp_ctl->reg = reg;
	csa = &FILE_INFO(reg)->s_addrs;

	if (init)
	{
		r_save = gv_cur_region;
		gv_cur_region = reg;
		tp_change_reg();
		cs_addrs->jnl->channel = NOJNL; /* Not to close the prev gener file */
		grab_crit(reg);
		jnl_status = jnl_ensure_open();
		if (0 != jnl_status)
		{
			rel_crit(reg);
			rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csa->hdr),
						DB_LEN_STR(gv_cur_region));
		}
		else
		{
			tmp_ctl->jnl_fn_len = csa->hdr->jnl_file_len;
			memcpy(tmp_ctl->jnl_fn, csa->hdr->jnl_file_name, tmp_ctl->jnl_fn_len);
		}
		/* stash the shared fileid into private storage before rel_crit() as it is used in JNL_GDID_PVT macro below */
		VMS_ONLY (cs_addrs->jnl->fileid = cs_addrs->nl->jnl_file.jnl_file_id;)
		UNIX_ONLY(cs_addrs->jnl->fileid = cs_addrs->nl->jnl_file.u;)
		rel_crit(reg);
		gv_cur_region = r_save;
		tp_change_reg();
		tmp_fd = csa->jnl->channel;
		tmp_ctl->jnl_fn[tmp_ctl->jnl_fn_len] = '\0';
		REPL_DPRINT2("CTL INIT :  Open of file %s thru jnl_ensure_open\n", tmp_ctl->jnl_fn);
	} else
	{
		tmp_ctl->jnl_fn_len = jnl_fn_len;
		memcpy(tmp_ctl->jnl_fn, jnl_fn, jnl_fn_len);
		tmp_ctl->jnl_fn[jnl_fn_len] = '\0';

		/* Open Journal File */
#		ifdef UNIX
		OPENFILE(tmp_ctl->jnl_fn, O_RDONLY, tmp_fd);
		if (0 > tmp_fd)
			status = errno;
		if (SS_NORMAL == status)
		{
			STAT_FILE(tmp_ctl->jnl_fn, &stat_buf, status);
			if (0 > status)
				status = errno;
		}
#		ifdef __MVS
		if (-1 == gtm_zos_tag_to_policy(tmp_fd, TAG_BINARY))
			status = errno;
#		endif
#		elif defined(VMS)
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
#		else
#		error Unsupported platform
#		endif
		REPL_DPRINT2("CTL INIT :  Direct open of file %s\n", tmp_ctl->jnl_fn);
	}
	if (status == SS_NORMAL)
	{
		tmp_jfh_base = (jnl_file_header *)malloc(SIZEOF(jnl_file_header) + DISK_BLOCK_SIZE);
		tmp_jfh = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)tmp_jfh_base, DISK_BLOCK_SIZE));
		F_READ_BLK_ALIGNED(tmp_fd, 0, tmp_jfh, ROUND_UP2(SIZEOF(jnl_file_header), DISK_BLOCK_SIZE), status);
		if (SS_NORMAL == status)
			CHECK_JNL_FILE_IS_USABLE(tmp_jfh, status, FALSE, 0, NULL); /* FALSE => NO gtm_putmsg even if errors */
	}
	if (SS_NORMAL != status)
	{
		free(tmp_ctl);
		tmp_ctl = NULL;
		if (NULL != tmp_jfh_base)
			free(tmp_jfh_base);
		tmp_jfh = NULL;
		tmp_jfh_base = NULL;
		rts_error(VARLSTCNT(7) ERR_JNLFILOPN, 4, jnl_fn_len, jnl_fn, DB_LEN_STR(reg), status);
	}
	tmp_ctl->repl_buff = repl_buff_create(tmp_jfh->alignsize);
	tmp_ctl->repl_buff->backctl = tmp_ctl;

	tmp_ctl->repl_buff->fc->eof_addr = JNL_FILE_FIRST_RECORD;
	tmp_ctl->repl_buff->fc->jfh_base = tmp_jfh_base;
	tmp_ctl->repl_buff->fc->jfh = tmp_jfh;
	tmp_ctl->repl_buff->fc->fd = tmp_fd;
#	ifdef GTM_CRYPT
	if (tmp_jfh->is_encrypted)
	{
		INIT_PROC_ENCRYPTION(crypt_status);
		/* If the encryption init failed in db_init, the below MACRO should return an error.
		 * Depending on the error returned, report the error.*/
		GTMCRYPT_GETKEY(tmp_jfh->encryption_hash, tmp_ctl->encr_key_handle, crypt_status);
		if (0 != crypt_status)
			GC_RTS_ERROR(crypt_status, jnl_fn);
	}
#	endif
	if (init)
	{
		F_COPY_GDID(tmp_ctl->repl_buff->fc->id, JNL_GDID_PVT(csa));
	}
	else
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
	tmp_ctl->fh_read_done = FALSE;
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
	unsigned char		jnl_file_name[JNL_NAME_SIZE];


	repl_ctl_list = (repl_ctl_element *)malloc(SIZEOF(repl_ctl_element));
	memset((char_ptr_t)repl_ctl_list, 0, SIZEOF(*repl_ctl_list));
	prev_ctl = repl_ctl_list;

	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{	/* Although replication may be WAS_ON, it is possible that source server has not yet sent records
			 * that were generated when replication was ON. We have to open and read this journal file to
			 * cover such a case. But in the WAS_ON case, do not ask for a jnl_ensure_open to be done since
			 * it will return an error (it will try to open the latest generation journal file and that will
			 * fail because of "jpc->cycle" vs "jpc->jnl_buff->cycle" mismatch). The idea is that we try and
			 * send as many seqnos as possible in that case.
			 */
			if (REPL_WAS_ENABLED(csd))
				status = repl_ctl_create(&tmp_ctl, reg, csd->jnl_file_len, (char *)csd->jnl_file_name, FALSE);
			else
				status = repl_ctl_create(&tmp_ctl, reg, 0, NULL, TRUE);
			assert(SS_NORMAL == status);
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
				{
					F_CLOSE(ctl->repl_buff->fc->fd, status); /* resets "ctl->repl_buff->fc->fd" to FD_INVALID */
				}
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

	if (repl_ctl_list)
	{
		for (ctl = repl_ctl_list->next; NULL != ctl; ctl = repl_ctl_list->next)
		{
			repl_ctl_list->next = ctl->next; /* next element becomes head thereby removing this element,
							    the current head from the list; if there is an error path that returns
							    us to this function before all elements were freed, we won't try to
							    free elements that have been freed already */
			if (NULL == ctl->next || ctl->reg != ctl->next->reg)
			{	/* end of list, OR next region follows, either way, we are at the last generation for this region */
				csa = &FILE_INFO(ctl->reg)->s_addrs;
				if (csa->jnl && NOJNL != csa->jnl->channel)
				{	/* The last generation file source server opened must have been done using
					 * jnl_ensure_open(). In which case, we would have copied jnl->channel to fc->fd.
					 * Validate. */
					assert(csa->jnl->channel == ctl->repl_buff->fc->fd);
					JNL_FD_CLOSE(csa->jnl->channel, status);	/* sets csa->jnl->channel to NOJNL */
					ctl->repl_buff->fc->fd = NOJNL; /* closed jnl->channel which is the same as fc->fd.
									 * no need to close again */
					assert(csa->jnl->channel == ctl->repl_buff->fc->fd);
					csa->jnl->cycle--; /* decrement cycle so jnl_ensure_open() knows to reopen the journal */
				}
			}
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
