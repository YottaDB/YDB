/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_stat.h"
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
#include "gtmio.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "tp_change_reg.h"
#include "is_file_identical.h"
#include "get_fs_block_size.h"
#include "gtmcrypt.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gtm_rename.h"

GBLDEF repl_ctl_element		*repl_ctl_list = NULL;
GBLDEF repl_rctl_elem_t		*repl_rctl_list = NULL;

GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF seq_num			seq_num_zero;

GBLREF gd_addr			*gd_header;
GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_addrs		*cs_addrs;

GBLREF int			gtmsource_log_fd;
GBLREF FILE			*gtmsource_log_fp;
GBLREF int			gtmsource_statslog_fd;
GBLREF FILE			*gtmsource_statslog_fp;
GBLREF gtmsource_state_t	gtmsource_state;
GBLREF uint4			process_id;

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
	struct stat	stat_buf;

	tmp_ctl->jnl_fn_len = jnl_fn_len;
	memcpy(tmp_ctl->jnl_fn, jnl_fn, jnl_fn_len);
	tmp_ctl->jnl_fn[jnl_fn_len] = '\0';
	status = SS_NORMAL;
	/* Open Journal File */
	OPENFILE_CLOEXEC(tmp_ctl->jnl_fn, O_RDONLY, tmp_fd);
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
		*((struct stat *)stat_buf_ptr) = stat_buf;
	}
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
	boolean_t		did_jnl_ensure_open = FALSE, open_jnl_file_by_name, was_crit;
	int4			lcl_jnl_fn_len;
	char			lcl_jnl_fn[JNL_NAME_SIZE];
	struct stat		stat_buf;
	uint4			jnl_fs_block_size;
	int			fn_len;
	unsigned char		*fn;
	int			ext_new_jnl_fn_len;
	unsigned char		ext_new_jnl_fn[MAX_FN_LEN + STR_LIT_LEN(EXT_NEW) + 1];
	uint4			status1, status2, ustatus;
	int			fd, save_errno;
	unsigned char		hdr_buff[REAL_JNL_HDR_LEN + 8];
	jnl_file_header		*header;

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
		if (csa->onln_rlbk_cycle != csa->nl->onln_rlbk_cycle)
		{	/* Concurrent online rollback. Possible only if we are called from gtmsource_update_zqgblmod_seqno_and_tn
			 * in which case we don't hold the gtmsource_srv_latch. Assert that.
			 */
			assert(process_id != jnlpool->gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid);
			SYNC_ONLN_RLBK_CYCLES;
			gtmsource_onln_rlbk_clnup();
			if (!was_crit)
				rel_crit(reg);
			free(tmp_ctl);
			return -1;
		}
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
		fn = csd->jnl_file_name;
		fn_len = csd->jnl_file_len;
		if (REPL_WAS_ENABLED(csd))
			open_jnl_file_by_name = TRUE;
		else
		{
			/* replication is allowed and has not gone into the WAS_ON state so journaling is expected to be ON*/
			assert(JNL_ENABLED(csd));
			jnl_status = jnl_ensure_open(reg, csa);
			if (0 == jnl_status)
			{
				open_jnl_file_by_name = FALSE;
				did_jnl_ensure_open = TRUE;
				tmp_ctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(tmp_ctl->jnl_fn, csd->jnl_file_name, tmp_ctl->jnl_fn_len);
				tmp_ctl->jnl_fn[tmp_ctl->jnl_fn_len] = '\0';
				/* stash the shared fileid into private storage before rel_crit
				 * as it is used in JNL_GDID_PVT macro below.
				 */
				jpc->fileid = csa->nl->jnl_file.u;
				REPL_DPRINT2("CTL INIT :  Open of file %s thru jnl_ensure_open\n", tmp_ctl->jnl_fn);
				tmp_fd = jpc->channel;
			} else if (ERR_JNLSWITCHRETRY == jnl_status)
			{	/* This is a case where the latest generation journal file was properly closed in shared
				 * memory but a new journal file was not created (in the middle of a journal autoswitch)
				 * due to operational issues (e.g. no permissions to create journal files etc.).
				 * In this case, we are guaranteed the latest generation journal file will not be opened
				 * in shared memory (as "jfh->is_not_latest_jnl" is set in its file header). Therefore
				 * open this journal file regularly just like one would do if this was a WAS_ON situation
				 * while we still hold crit.
				 */
				open_jnl_file_by_name = TRUE;
			} else if (ERR_FILEDELFAIL == jnl_status)
			{	/* This is a case where mumps.mjl_%YGTM and mumps.mjl exists but deleting mumps.mjl_%YGTM
				 * failed (see "cre_jnl_file_intrpt_rename" for more details). In this case, open mumps.mjl
				 * (the name stored in the db file header csd->jnl_fn) just like the ERR_JNLSWITCHRETRY case.
				 */
				open_jnl_file_by_name = TRUE;
			} else if (ERR_RENAMEFAIL == jnl_status)
			{	/* This is a case where rename of mumps.mjl_%YGTM to mumps.mjl failed (see
				 * "cre_jnl_file_intrpt_rename" for more details). In this case, open mumps.mjl_%YGTM
				 * (the name stored in the db file header i.e. csd->jnl_fn) and read the file header
				 * and find out the previous journal file name from there and open that journal file
				 * by name (e.g. mumps.mjl_2018144122944). Note that there is no point opening mumps.mjl_%YGTM
				 * since it is guaranteed to contain no data (by the fact that its name still has the _%YGTM
				 * suffix). Hence opening the immediately previous generation journal file in case it has not
				 * yet been seen by the source server. In case it already opened this file, it anyways has a
				 * check of "is_gdid_gdid_identical"in "open_newer_gener_jnlfiles" to avoid duplicate opens.
				 * Note: Some of below code is similar to "cre_jnl_file_intrpt_rename" and "jnl_file_open_common".
				 */
				ext_new_jnl_fn_len = ARRAYSIZE(ext_new_jnl_fn);
				status1 = prepare_unique_name((char *)fn, fn_len, "", EXT_NEW,
								(char *)ext_new_jnl_fn, &ext_new_jnl_fn_len, 0, &ustatus);
				/* We have allocated enough space in ext_new_jnl_fn array to store EXT_NEW suffix.
				 * So no way the above "prepare_unique_name" call can fail. Hence the below assert.
				 */
				assert(SS_NORMAL == status1);
				assert('\0' == ext_new_jnl_fn[ext_new_jnl_fn_len]);
				OPENFILE((char *)ext_new_jnl_fn, O_RDONLY, fd);
				if (FD_INVALID == fd)
				{
					save_errno = errno;
					free(tmp_ctl);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_JNLFILEOPNERR, 2, ext_new_jnl_fn_len,
						ext_new_jnl_fn, ERR_SYSCALL, 5, LEN_AND_STR("open"), CALLFROM, save_errno);
				}
				header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_buff, 8));
				DO_FILE_READ(fd, 0, header, SIZEOF(jnl_file_header), status1, status2);
				if (SS_NORMAL != status1)
				{
					save_errno = errno;
					free(tmp_ctl);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_JNLREAD, 3,
									ext_new_jnl_fn_len, ext_new_jnl_fn, 0, save_errno);
				}
				CHECK_JNL_FILE_IS_USABLE(header, status1, FALSE, 0, NULL);	/* FALSE => NO gtm_putmsg
												 * even if errors.
												 */
				if (SS_NORMAL != status1)
				{
					free(tmp_ctl);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_JNLOPNERR, 4,
						      ext_new_jnl_fn_len, ext_new_jnl_fn, DB_LEN_STR(reg), status1);
				}
				CLOSEFILE_RESET(fd, status1);
				fn_len = header->prev_jnl_file_name_length;
				fn = header->prev_jnl_file_name;
				open_jnl_file_by_name = TRUE;
			} else
			{	/* Some other error inside "jnl_ensure_open". Have to error out. */
				if (!was_crit)
					rel_crit(reg);
				/* In case jnl_status has a severity of INFO, treat that as an ERROR so force it below */
				free(tmp_ctl);
				if (SS_NORMAL != jpc->status)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) MAKE_MSG_TYPE(jnl_status, ERROR), 4,
						JNL_LEN_STR(csd), DB_LEN_STR(reg), jpc->status);
				else
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_TYPE(jnl_status, ERROR), 4,
						JNL_LEN_STR(csd), DB_LEN_STR(reg));
				assert(FALSE);
			}
		}
		if (open_jnl_file_by_name)
		{	/* Note that we hold crit so it is safe to pass csd->jnl_file_name (no one else will be changing it) */
			assert(csa->now_crit);
			status = repl_open_jnl_file_by_name(tmp_ctl, fn_len, (char *)fn, &tmp_fd, &stat_buf);
		}
		if (!was_crit)
			rel_crit(reg);
		gv_cur_region = r_save;
		tp_change_reg();
		assert((NOJNL != tmp_fd)
		       || ((status != SS_NORMAL) && ydb_white_box_test_case_enabled
			   && (WBTEST_JNL_FILE_LOST_DSKADDR == ydb_white_box_test_case_number)));
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
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_JNLFILRDOPN, 4, lcl_jnl_fn_len,
				lcl_jnl_fn, DB_LEN_STR(reg), status);
		else
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_JNLNOREPL, 4, lcl_jnl_fn_len, lcl_jnl_fn, DB_LEN_STR(reg));
	}
	assert(SS_NORMAL == status);	/* so jnl_fs_block_size is guaranteed to have been initialized */
	tmp_ctl->repl_buff = repl_buff_create(tmp_jfh->alignsize, jnl_fs_block_size);
	tmp_ctl->repl_buff->backctl = tmp_ctl;
	tmp_ctl->repl_buff->fc->eof_addr = JNL_FILE_FIRST_RECORD;
	tmp_ctl->repl_buff->fc->fs_block_size = jnl_fs_block_size;
	tmp_ctl->repl_buff->fc->jfh_base = tmp_jfh_base;
	tmp_ctl->repl_buff->fc->jfh = tmp_jfh;
	tmp_ctl->repl_buff->fc->fd = tmp_fd;
	if (USES_ANY_KEY(tmp_jfh))
	{
		ASSERT_ENCRYPTION_INITIALIZED;	/* should be done in db_init ("gtmsource" -> "gvcst_init" -> "db_init") */
		INIT_DB_OR_JNL_ENCRYPTION(tmp_ctl, tmp_jfh, reg->dyn.addr->fname_len, (char *)reg->dyn.addr->fname, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			free(tmp_ctl);
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, tmp_ctl->jnl_fn_len, tmp_ctl->jnl_fn);
		}
	}
	if (did_jnl_ensure_open)
	{
		F_COPY_GDID(tmp_ctl->repl_buff->fc->id, JNL_GDID_PVT(csa));
		/* reset jpc->channel (would have been updated by jnl_ensure_open) as that corresponds to an
		 * actively updated journal file and is only for GT.M and never for source server which only
		 * READS from journal files for the most part (it can do a jnl_flush rarely (in case GT.M failed
		 * to do one) using GTMSRC_DO_JNL_FLUSH_IF_POSSIBLE macro). Source server anyways has a copy of
		 * the fd in tmp_ctl->repl_buff->fc->fd.
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
	tmp_ctl->min_seqno_dskaddr = 0;
	tmp_ctl->max_seqno_dskaddr = 0;
	tmp_ctl->next = tmp_ctl->prev = NULL;
	*ctl = tmp_ctl;
	return (SS_NORMAL);
}

/* Setup ctl for reading from journal files */
int gtmsource_ctl_init(void)
{
	gd_region		*region_top, *reg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	repl_ctl_element	*tmp_ctl, *prev_ctl;
	repl_rctl_elem_t	*repl_rctl, *last_rctl;
#	ifdef DEBUG
	repl_rctl_elem_t	*tmp_rctl;
#	endif
	int			jnl_file_len, status;

	repl_ctl_list = (repl_ctl_element *)malloc(SIZEOF(repl_ctl_element));
	memset((char_ptr_t)repl_ctl_list, 0, SIZEOF(*repl_ctl_list));
	prev_ctl = repl_ctl_list;
	assert(GTMSOURCE_HANDLE_ONLN_RLBK != gtmsource_state); /* can't come here without handling online rollback */
	region_top = gd_header->regions + gd_header->n_regions;
	last_rctl = NULL;
	for (reg = gd_header->regions; reg < region_top; reg++)
	{
		assert(reg->open);
		csa = &FILE_INFO(reg)->s_addrs;
		csd = csa->hdr;
		if (REPL_ALLOWED(csd))
		{
			status = repl_ctl_create(&tmp_ctl, reg, 0, NULL, TRUE);
			assert((SS_NORMAL == status) || (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state));
			if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
				return -1;
			prev_ctl->next = tmp_ctl;
			tmp_ctl->prev = prev_ctl;
			tmp_ctl->next = NULL;
			prev_ctl = tmp_ctl;
			repl_rctl = (repl_rctl_elem_t *)csa->miscptr;
			if (NULL == repl_rctl)
			{
#				ifdef DEBUG
				tmp_rctl = repl_rctl_list;
				while ((NULL != tmp_rctl) && (NULL != tmp_rctl->next))
					tmp_rctl = tmp_rctl->next;
				assert(last_rctl == tmp_rctl);
#				endif
				repl_rctl = (repl_rctl_elem_t *)malloc(SIZEOF(repl_rctl_elem_t));
				repl_rctl->next = NULL;
				repl_rctl->prev = last_rctl;
				if (NULL == repl_rctl_list)
					repl_rctl_list = repl_rctl;
				else
					last_rctl->next = repl_rctl;
				last_rctl = repl_rctl;
				csa->miscptr = (void *)repl_rctl;
			}
			repl_rctl->ctl_start = tmp_ctl;
			/* repl_rctl->read_complete is later initialized in function "read_and_merge" */
			tmp_ctl->repl_rctl = repl_rctl;
		}
	}
	/* This function should never be invoked unless there is at least one replicated region. */
	assertpro(NULL != repl_ctl_list->next);
	return (SS_NORMAL);
}

int repl_ctl_close(repl_ctl_element *ctl)
{
	int	index;
	int	status;

	if (NULL != ctl)
	{
		REPL_DPRINT2("CTL CLOSE : Close of file %s\n", ctl->jnl_fn);
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
	repl_rctl_elem_t	*repl_rctl;

	gtmsource_stop_jnl_release_timer();
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
	for (repl_rctl = repl_rctl_list; NULL != repl_rctl; repl_rctl = repl_rctl->next)
		repl_rctl->ctl_start = NULL;
	return (SS_NORMAL);
}

int gtmsource_set_lookback(void)
{
	/* Scan all the region ctl's and set lookback to TRUE if ctl has to be repositioned for a transaction read from the past.
	 * In all other cases, reset it to FALSE.
	 */
	repl_ctl_element	*ctl;

	for (ctl = repl_ctl_list->next; NULL != ctl; ctl = ctl->next)
	{
		if (((JNL_FILE_OPEN == ctl->file_state) || (JNL_FILE_CLOSED == ctl->file_state))
				&& QWLE(jnlpool->gtmsource_local->read_jnl_seqno, ctl->seqno))
			ctl->lookback = TRUE;
		else
			ctl->lookback = FALSE;
	}
	return (SS_NORMAL);
}
