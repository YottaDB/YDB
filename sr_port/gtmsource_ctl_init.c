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
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
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

repl_buff_t *repl_buff_create(int buffsize);

repl_buff_t *repl_buff_create(int buffsize)
{
	repl_buff_t	*tmp_rb;
	int		index;

	tmp_rb = (repl_buff_t *)malloc(sizeof(repl_buff_t));
	tmp_rb->buffindex = REPL_MAINBUFF;
	for (index = REPL_MAINBUFF; REPL_NUMBUFF > index; index++)
	{
		tmp_rb->buff[index].reclen = 0;
		tmp_rb->buff[index].recaddr = JNL_FILE_FIRST_RECORD;
		tmp_rb->buff[index].readaddr = JNL_FILE_FIRST_RECORD;
		tmp_rb->buff[index].buffremaining = buffsize;
		tmp_rb->buff[index].base = (unsigned char *)malloc(buffsize);
		tmp_rb->buff[index].recbuff = tmp_rb->buff[index].base;
	}
	tmp_rb->fc = (repl_file_control_t *)malloc(sizeof(repl_file_control_t));
	return (tmp_rb);
}

int repl_ctl_create(repl_ctl_element **ctl, gd_region *reg,
		    int jnl_fn_len, char *jnl_fn, boolean_t init)
{

	gd_region		*r_save;
	sgmnt_addrs 		*csa;
	repl_ctl_element	*tmp_ctl = NULL;
	jnl_file_header		*tmp_jfh = NULL;
	int			tmp_fd = -1;
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
	uint4			jnl_status;

	error_def(ERR_JNLBADLABEL);
	error_def(ERR_JNLFILOPN);

	status = SS_NORMAL;
	jnl_status = 0;

	tmp_ctl = (repl_ctl_element *)malloc(sizeof(repl_ctl_element));
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
#ifdef UNIX
		OPENFILE(tmp_ctl->jnl_fn, O_RDONLY, tmp_fd);
		if (0 > tmp_fd)
			status = errno;
		if (SS_NORMAL == status)
		{
			STAT_FILE(tmp_ctl->jnl_fn, &stat_buf, status);
			if (0 > status)
				status = errno;
		}
#elif defined(VMS)
		fab = cc$rms_fab;
		fab.fab$l_fna = tmp_ctl->jnl_fn;
		fab.fab$b_fns = tmp_ctl->jnl_fn_len;
		fab.fab$l_fop = FAB$M_UFO;
		fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
		fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
		stat_buf      = cc$rms_nam;
		fab.fab$l_nam = &stat_buf;
		fab.fab$l_dna = JNL_EXT_DEF;
		fab.fab$b_dns = sizeof(JNL_EXT_DEF) - 1;
		status = sys$open(&fab);
		if (RMS$_NORMAL == status)
		{
			status = SS_NORMAL;
			tmp_fd = fab.fab$l_stv;
		}
#else
#error Unsupported platform
#endif
		REPL_DPRINT2("CTL INIT :  Direct open of file %s\n", tmp_ctl->jnl_fn);
	}

	if (status == SS_NORMAL)
	{
		tmp_jfh = (jnl_file_header *)malloc(ROUND_UP(sizeof(jnl_file_header), 8));
		F_READ_BLK_ALIGNED(tmp_fd, 0, tmp_jfh, ROUND_UP(sizeof(jnl_file_header), 8), status);
		if (SS_NORMAL == status && 0 != memcmp(tmp_jfh->label, JNL_LABEL_TEXT, STR_LIT_LEN(JNL_LABEL_TEXT)))
			status = (int4)ERR_JNLBADLABEL;
	}

	if (SS_NORMAL != status)
	{
		free(tmp_ctl);
		tmp_ctl = NULL;
		if (NULL != tmp_jfh)
			free(tmp_jfh);
		tmp_jfh = NULL;
		rts_error(VARLSTCNT(7) ERR_JNLFILOPN, 4, jnl_fn_len, jnl_fn, REG_LEN_STR(reg));
	}

	tmp_ctl->repl_buff = repl_buff_create(tmp_jfh->alignsize);
	tmp_ctl->repl_buff->backctl = tmp_ctl;

	tmp_ctl->repl_buff->fc->eof_addr = JNL_FILE_FIRST_RECORD;
	tmp_ctl->repl_buff->fc->jfh = tmp_jfh;
	tmp_ctl->repl_buff->fc->fd = tmp_fd;

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
	repl_ctl_element	*tmp_ctl, *prev_ctl;
	int			jnl_file_len, status;
	unsigned char		jnl_file_name[JNL_NAME_SIZE];


	repl_ctl_list = (repl_ctl_element *)malloc(sizeof(repl_ctl_element));
	memset((char_ptr_t)repl_ctl_list, 0, sizeof(*repl_ctl_list));
	prev_ctl = repl_ctl_list;

	region_top = gd_header->regions + gd_header->n_regions;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		if (REPL_ENABLED(csa->hdr))
		{
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

	if (ctl)
	{
		if (ctl->repl_buff)
		{
			for (index = REPL_MAINBUFF; REPL_NUMBUFF > index; index++)
				if (ctl->repl_buff->buff[index].base)
					free(ctl->repl_buff->buff[index].base);
			if (ctl->repl_buff->fc)
			{
				if (ctl->repl_buff->fc->jfh)
					free(ctl->repl_buff->fc->jfh);
				if (NOJNL != ctl->repl_buff->fc->fd)
					F_CLOSE(ctl->repl_buff->fc->fd);
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
	gd_region		*reg;

	if (repl_ctl_list)
	{
		for (ctl = repl_ctl_list->next, reg = NULL; NULL != ctl; ctl = repl_ctl_list->next)
		{
			repl_ctl_list->next = ctl->next; /* next element becomes head thereby removing this element,
							    the current head from the list; if there is an error path that returns
							    us to this function before all elements were freed, we won't try to
							    free elements that have been freed already */
			if (reg != ctl->reg)
			{
				csa = &FILE_INFO(ctl->reg)->s_addrs;
				if (csa->jnl && NOJNL != csa->jnl->channel)
				{
					F_CLOSE(csa->jnl->channel);
					csa->jnl->channel = NOJNL;
					ctl->repl_buff->fc->fd = NOJNL;
				}
				reg = ctl->reg;
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
