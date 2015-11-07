/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#if defined(UNIX)
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_permissions.h"
#if defined(__MVS__)
#include "gtm_zos_io.h"
#endif
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gtm_file_stat.h"
#include "gtm_rename.h"
#include "error.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmio.h"
#include "util.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "iosp.h"
#include "repl_sp.h"
#include "is_file_identical.h"
#include "jnl_get_checksum.h"
#include "gtmimagename.h"
#include "get_fs_block_size.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "anticipatory_freeze.h"

/* Note : Now all system error messages are issued here. So callers do not need to issue them again */
#define STATUS_MSG(info)										\
{													\
	if (SS_NORMAL != info->status2)									\
	{												\
		if (IS_GTM_IMAGE)									\
			send_msg(VARLSTCNT(12) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, info->jnl,	\
				info->fn_len, info->fn, info->status, 0, info->status2);		\
		else											\
			gtm_putmsg(VARLSTCNT1(11) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, info->jnl,\
				info->fn_len, info->fn, info->status, PUT_SYS_ERRNO(info->status2));	\
	} else if (SS_NORMAL != info->status)								\
	{												\
		if (IS_GTM_IMAGE)									\
			send_msg(VARLSTCNT(10) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, 		\
				info->jnl, info->fn_len, info->fn, info->status);			\
		else											\
			gtm_putmsg(VARLSTCNT(10) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, 		\
				info->jnl, info->fn_len, info->fn, info->status);			\
	}												\
}
#define RETURN_ON_ERROR(info)						\
if (SYSCALL_ERROR(info->status) || SYSCALL_ERROR(info->status2))	\
{									\
	int	status;							\
	F_CLOSE(channel, status);/* resets "channel" to FD_INVALID */	\
	if (NULL != jrecbuf_base)					\
	{								\
		free(jrecbuf_base);					\
		jrecbuf_base = NULL;					\
	}								\
	return EXIT_ERR;						\
}

#if defined(VMS)
#define ZERO_SIZE_IN_BLOCKS	127 /* 127 is RMS maximum blocks / write */
#define ZERO_SIZE		(ZERO_SIZE_IN_BLOCKS * DISK_BLOCK_SIZE)
#endif

GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	jnl_process_vector	*prc_vec;

ZOS_ONLY(error_def(ERR_BADTAG);)
error_def(ERR_FILERENAME);
error_def(ERR_JNLCRESTATUS);
error_def(ERR_JNLFNF);
error_def(ERR_PERMGENFAIL);
error_def(ERR_PREMATEOF);
error_def(ERR_RENAMEFAIL);
error_def(ERR_TEXT);

/* Create a journal file from info.
 * If necessary, it renames journal file of same name.
 * Note: jgbl.gbl_jrec_time must be set by callers
 */
uint4	cre_jnl_file(jnl_create_info *info)
{
	mstr 		filestr;
	int 		org_fn_len, rename_fn_len, fstat;
	char		*org_fn, rename_fn[MAX_FN_LEN];
	boolean_t	no_rename;

	assert(0 != jgbl.gbl_jrec_time);
	if (!info->no_rename)	/* ***MAY*** be rename is required */
	{
		no_rename = FALSE;
		if (SS_NORMAL != (info->status = prepare_unique_name((char *)info->jnl, info->jnl_len, "", "",
				rename_fn, &rename_fn_len, jgbl.gbl_jrec_time, &info->status2)))
		{
			no_rename = TRUE;
		} else
		{
			filestr.addr = (char *)info->jnl;
			filestr.len = info->jnl_len;
			if (FILE_PRESENT != (fstat = gtm_file_stat(&filestr, NULL, NULL, FALSE, (uint4 *)&info->status)))
			{
				if (FILE_NOT_FOUND != fstat)
				{
					STATUS_MSG(info);
					return EXIT_ERR;
				}
				if (IS_GTM_IMAGE)
					send_msg(VARLSTCNT(4) ERR_JNLFNF, 2, filestr.len, filestr.addr);
				else
					gtm_putmsg(VARLSTCNT(4) ERR_JNLFNF, 2, filestr.len, filestr.addr);
				no_rename = TRUE;
			}
			/* Note if info->no_prev_link == TRUE, we do not keep previous link, though rename can happen */
			if (JNL_ENABLED(info) && !info->no_prev_link)
			{
				memcpy(info->prev_jnl, rename_fn, rename_fn_len + 1);
				info->prev_jnl_len = rename_fn_len;
			} else
				assert(info->no_prev_link);
		}
		if (no_rename)
		{
			STATUS_MSG(info);
			info->status = info->status2 = SS_NORMAL;
			info->no_rename = TRUE; /* We wanted to rename, but not required anymore */
			info->no_prev_link = TRUE;	/* No rename => no prev_link */
		}
	} /* else we know for sure rename is not required */
	return (cre_jnl_file_common(info, rename_fn, rename_fn_len));
}

/* This creates info->jnl and if (!info->no_rename) then it renames existing info->jnl to be rename_fn */
uint4 cre_jnl_file_common(jnl_create_info *info, char *rename_fn, int rename_fn_len)
{
	jnl_file_header		*header;
	unsigned char		hdr_base[JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	struct_jrec_pfin	*pfin_record;
	struct_jrec_pini	*pini_record;
	struct_jrec_epoch	*epoch_record;
	struct_jrec_eof		*eof_record;
	unsigned char		*create_fn, fn_buff[MAX_FN_LEN];
	int			create_fn_len, cre_jnl_rec_size, status, write_size, jrecbufbase_size;
	fd_type			channel;
	char            	*jrecbuf, *jrecbuf_base;
	gd_id			jnlfile_id;
#	ifdef VMS
	struct FAB      	fab;
	struct NAM      	nam;
	char            	es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	uint4			blk, block, zero_size;
	io_status_block_disk	iosb;
#	else
	struct stat		stat_buf;
	int			fstat_res;
	ZOS_ONLY(int		realfiletag;)
	int			stat_res;
	int			group_id;
	struct stat		sb;
	int			perm;
	struct perm_diag_data	pdd;
#	endif
	int			idx;
	trans_num		db_tn;
	uint4			temp_offset, temp_checksum, pfin_offset, eof_offset;
	uint4			jnl_fs_block_size;

	jrecbuf = NULL;
	if (info->no_rename)
	{	/* The only cases where no-renaming is possible are as follows
    		 * (i) MUPIP SET JOURNAL where the new journal file name is different from the current journal file name
		 * (ii) For MUPIP BACKUP, MUPIP SET JOURNAL, GT.M Runtime and forw_phase_recovery,
		 * 	in case the current journal file does not exist (due to some abnormal condition).
		 *	But in this case we cut the link and hence info->no_prev_link should be TRUE.
		 * The assert below tries to capture this as much as possible without introducing any new global variables.
		 */
		assert((IS_MUPIP_IMAGE && !jgbl.forw_phase_recovery) || (IS_GTM_IMAGE && info->no_prev_link));
		create_fn_len = info->jnl_len;
		create_fn = info->jnl;
		assert(0 == create_fn[create_fn_len]);
	} else
	{
		create_fn = &fn_buff[0];
		if (SS_NORMAL != (info->status = prepare_unique_name((char *)info->jnl, (int)info->jnl_len, "", EXT_NEW,
								     (char *)create_fn, &create_fn_len, 0, &info->status2)))
		{
			STATUS_MSG(info);
			return EXIT_ERR;
		}
	}
#	ifdef UNIX
	OPENFILE3((char *)create_fn, O_CREAT | O_EXCL | O_RDWR, 0600, channel);
	if (-1 == channel)
	{
		info->status = errno;
		STATUS_MSG(info);
		return EXIT_ERR;
	}
#	ifdef __MVS__
 	if (-1 == gtm_zos_set_tag(channel, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
 		TAG_POLICY_SEND_MSG((char *)create_fn, errno, realfiletag, TAG_BINARY);
#	endif
	FSTAT_FILE(channel, &stat_buf, fstat_res);
	if (-1 == fstat_res)
	{
		info->status = errno;
		STATUS_MSG(info);
		F_CLOSE(channel, status);
		return EXIT_ERR;
	}
	/* check database file again. It was checked earlier so should still be ok. */
	STAT_FILE((sm_c_ptr_t)info->fn, &sb, stat_res);
	if (-1 == stat_res)
	{
		info->status = errno;
		STATUS_MSG(info);
		F_CLOSE(channel, status);
		return EXIT_ERR;
	}
	/* setup new group and permissions if indicated by the security rules.
	 */
	if (gtm_set_group_and_perm(&sb, &group_id, &perm, PERM_FILE, &pdd) < 0)
	{
		send_msg(VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
			ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("journal file"), RTS_ERROR_STRING(info->fn),
			PERMGENDIAG_ARGS(pdd));
		if (IS_GTM_IMAGE)
			rts_error(VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("journal file"), RTS_ERROR_STRING(info->fn),
				PERMGENDIAG_ARGS(pdd));
		else
			gtm_putmsg(VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("journal file"), RTS_ERROR_STRING(info->fn),
				PERMGENDIAG_ARGS(pdd));
		F_CLOSE(channel, status);
		return EXIT_ERR;
	}
	/* if group not the same then change group of temporary file */
	if ((-1 != group_id) && (group_id != stat_buf.st_gid) && (-1 == fchown(channel, -1, group_id)))
	{
		info->status = errno;
		STATUS_MSG(info);
		F_CLOSE(channel, status);
		return EXIT_ERR;
	}
	if (-1 == FCHMOD(channel, perm))
	{
		info->status = errno;
		STATUS_MSG(info);
		F_CLOSE(channel, status);	/* resets "channel" to FD_INVALID */
		return EXIT_ERR;
	}
	jnl_fs_block_size = get_fs_block_size(channel);
	/* We need to write the journal file header, followed by pini/epoch/pfin/eof records followed by 0-padding
	 * to ensure the final size of the journal file is filesystem-block-size aligned.
	 * To have a "jnl_fs_block_size" aligned buffer that is also a multiple of jnl_fs_block_size long,
	 * we need to allocate the needed size + 2 * jnl_fs_block_size.  This will leave up to jnl_fs_block_size
	 * before the actual "used" part for alignment plus enough left after the "used" part to make up a buffer
	 * to write that is a multiple of jnl_fs_block_size.
	 */
	jrecbufbase_size = PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN + (2 * jnl_fs_block_size);
	jrecbuf_base = malloc(jrecbufbase_size);
	jrecbuf = (char *)ROUND_UP2((uintszofptr_t)jrecbuf_base, jnl_fs_block_size);
	memset(jrecbuf, 0, jnl_fs_block_size);
	set_gdid_from_stat(&jnlfile_id, &stat_buf);
#	else
	nam = cc$rms_nam;
	nam.nam$l_rsa = name_buffer;
	nam.nam$b_rss = SIZEOF(name_buffer);
	nam.nam$l_esa = es_buffer;
	nam.nam$b_ess = SIZEOF(es_buffer);
	nam.nam$b_nop = NAM$M_NOCONCEAL;
	fab = cc$rms_fab;
	fab.fab$l_nam = &nam;
	fab.fab$b_org = FAB$C_SEQ;
	fab.fab$b_rfm = FAB$C_FIX;
	fab.fab$l_fop = FAB$M_UFO | FAB$M_MXV | FAB$M_CBT;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
	fab.fab$b_shr = FAB$M_SHRGET | FAB$M_SHRPUT | FAB$M_UPI | FAB$M_NIL;
	fab.fab$w_mrs = DISK_BLOCK_SIZE;
	fab.fab$l_alq = info->alloc;
	fab.fab$w_deq = info->extend;
	fab.fab$l_fna = create_fn;
	fab.fab$b_fns = create_fn_len;
	info->status = sys$create(&fab);
	if (0 == (info->status & 1))
	{
		info->status2 = fab.fab$l_stv;	/* store secondary status information */
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	channel = fab.fab$l_stv;
	jrecbufbase_size = ZERO_SIZE;
	jrecbuf_base = jrecbuf = malloc(ZERO_SIZE);
	memset(jrecbuf, 0, ZERO_SIZE);
	block = (JNL_HDR_LEN >> LOG2_DISK_BLOCK_SIZE);
	assert(block * DISK_BLOCK_SIZE == JNL_HDR_LEN);
	for (blk = block;  blk < info->alloc;  blk += ZERO_SIZE_IN_BLOCKS)
	{
		zero_size = (blk + ZERO_SIZE_IN_BLOCKS <= info->alloc) ?
			ZERO_SIZE : (info->alloc - blk) * DISK_BLOCK_SIZE;
		JNL_DO_FILE_WRITE(NULL, NULL, channel, blk * DISK_BLOCK_SIZE, jrecbuf, zero_size, info->status, info->status2);
		STATUS_MSG(info);
		RETURN_ON_ERROR(info);
	}
	memcpy(jnlfile_id.dvi, &nam.nam$t_dvi, SIZEOF(jnlfile_id.dvi));
	memcpy(jnlfile_id.did, &nam.nam$w_did, SIZEOF(jnlfile_id.did));
	memcpy(jnlfile_id.fid, &nam.nam$w_fid, SIZEOF(jnlfile_id.fid));
	jnl_fs_block_size = get_fs_block_size(channel);
#	endif
	info->checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&jnlfile_id, SIZEOF(gd_id));
	/* Journal file header size relies on this assert */
	assert(256 == GTMCRYPT_RESERVED_HASH_LEN);
	header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_base, jnl_fs_block_size));
	/* We have already saved previous journal file name in info */
	jfh_from_jnl_info(info, header);
	header->recover_interrupted = mupip_jnl_recover;
	assert(ROUND_UP2(JNL_HDR_LEN, jnl_fs_block_size) == JNL_HDR_LEN);
	assert((unsigned char *)header + JNL_HDR_LEN <= ARRAYTOP(hdr_base));
	/* Although the real journal file header is REAL_JNL_HDR_LEN, we write the entire journal file header
	 * including padding (64K in Unix) so we write 0-padding instead of some garbage. Not necessary
	 * but makes analysis of journal file using dump utilities (like od) much easier. Also 0-initialization
	 * might help us in the future when we add some fields in the file header. The cost of the extra padding
	 * write is not so much since it is only once per journal file at creation time. All future writes of the
	 * file header write only the real file header and not the 0-padding.
	 */
	JNL_DO_FILE_WRITE(info->csa, create_fn, channel, 0, header, JNL_HDR_LEN, info->status, info->status2);
	STATUS_MSG(info);
	RETURN_ON_ERROR(info);
	assert(DISK_BLOCK_SIZE >= EPOCH_RECLEN + EOF_RECLEN + PFIN_RECLEN + PINI_RECLEN);
	pini_record = (struct_jrec_pini *)&jrecbuf[0];
	pini_record->prefix.jrec_type = JRT_PINI;
	pini_record->prefix.forwptr = pini_record->suffix.backptr = PINI_RECLEN;
	db_tn = info->csd->trans_hist.curr_tn;
	pini_record->prefix.tn = db_tn;
	pini_record->prefix.pini_addr = JNL_HDR_LEN;
	pini_record->prefix.time = jgbl.gbl_jrec_time;	/* callers must set it */
	pini_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	assert(prc_vec);
	memcpy((unsigned char*)&pini_record->process_vector[CURR_JPV], (unsigned char*)prc_vec, SIZEOF(jnl_process_vector));
	/* Already process_vector[ORIG_JPV] is memset 0 */
	pini_record->filler = 0;
	pini_record->prefix.checksum = INIT_CHECKSUM_SEED;
	temp_checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)pini_record, SIZEOF(struct_jrec_pini));
	temp_offset = JNL_HDR_LEN;
	ADJUST_CHECKSUM(temp_checksum, temp_offset, temp_checksum);
	ADJUST_CHECKSUM(temp_checksum, info->checksum, pini_record->prefix.checksum);
	/* EPOCHs are written unconditionally in Unix while they are written only for BEFORE_IMAGE in VMS */
	if (JNL_HAS_EPOCH(info))
	{
		epoch_record = (struct_jrec_epoch *)&jrecbuf[PINI_RECLEN];
		epoch_record->prefix.jrec_type = JRT_EPOCH;
		epoch_record->prefix.forwptr = epoch_record->suffix.backptr = EPOCH_RECLEN;
		epoch_record->prefix.tn = db_tn;
		epoch_record->prefix.pini_addr = JNL_HDR_LEN;
		epoch_record->prefix.time = jgbl.gbl_jrec_time;
		epoch_record->blks_to_upgrd = info->blks_to_upgrd;
		epoch_record->free_blocks   = info->free_blocks;
		epoch_record->total_blks    = info->total_blks;
		epoch_record->fully_upgraded = info->csd->fully_upgraded;
		epoch_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		epoch_record->jnl_seqno = info->reg_seqno;
		UNIX_ONLY(
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				epoch_record->strm_seqno[idx] = info->csd->strm_reg_seqno[idx];
			if (jgbl.forw_phase_recovery)
			{	/* If MUPIP JOURNAL -ROLLBACK, might need some adjustment. See macro definition for comments */
				MUR_ADJUST_STRM_REG_SEQNO_IF_NEEDED(info->csd, epoch_record->strm_seqno);
			}
		)
		VMS_ONLY(
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				assert(0 == epoch_record->strm_seqno[idx]); /* should have been zeroed already by above memset */
		)
		epoch_record->filler = 0;
		epoch_record->prefix.checksum = INIT_CHECKSUM_SEED;
		temp_checksum = compute_checksum(INIT_CHECKSUM_SEED,
							(uint4 *)epoch_record, SIZEOF(struct_jrec_epoch));
		temp_offset = JNL_HDR_LEN + PINI_RECLEN;
		ADJUST_CHECKSUM(temp_checksum, temp_offset, temp_checksum);
		ADJUST_CHECKSUM(temp_checksum, info->checksum, epoch_record->prefix.checksum);
		pfin_record = (struct_jrec_pfin *)&jrecbuf[PINI_RECLEN + EPOCH_RECLEN];
		pfin_offset = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN;
		eof_record = (struct_jrec_eof *)&jrecbuf[PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN];
		eof_offset = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN;
		cre_jnl_rec_size = PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN;
	} else
	{
		pfin_record = (struct_jrec_pfin *)&jrecbuf[PINI_RECLEN];
		pfin_offset = JNL_HDR_LEN + PINI_RECLEN;
		eof_record = (struct_jrec_eof *)&jrecbuf[PINI_RECLEN + PFIN_RECLEN];
		eof_offset = JNL_HDR_LEN + PINI_RECLEN + PFIN_RECLEN;
		cre_jnl_rec_size = PINI_RECLEN + PFIN_RECLEN + EOF_RECLEN;
	}
	pfin_record->prefix.jrec_type = JRT_PFIN;
	pfin_record->prefix.forwptr = pfin_record->suffix.backptr = PFIN_RECLEN;
	pfin_record->prefix.tn = db_tn;
	pfin_record->prefix.pini_addr = JNL_HDR_LEN;
	pfin_record->prefix.time = jgbl.gbl_jrec_time;
	pfin_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	pfin_record->filler = 0;
	pfin_record->prefix.checksum = INIT_CHECKSUM_SEED;
	temp_checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)pfin_record, SIZEOF(struct_jrec_pfin));
	ADJUST_CHECKSUM(temp_checksum, pfin_offset, temp_checksum);
	ADJUST_CHECKSUM(temp_checksum, info->checksum, pfin_record->prefix.checksum);
	eof_record->prefix.jrec_type = JRT_EOF;
	eof_record->prefix.forwptr = eof_record->suffix.backptr = EOF_RECLEN;
	eof_record->prefix.tn = db_tn;
	eof_record->prefix.pini_addr = JNL_HDR_LEN;
	eof_record->prefix.time = jgbl.gbl_jrec_time;
	QWASSIGN(eof_record->jnl_seqno, info->reg_seqno);
	eof_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	eof_record->filler = 0;
	eof_record->prefix.checksum = INIT_CHECKSUM_SEED;
	temp_checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)eof_record, SIZEOF(struct_jrec_eof));
	ADJUST_CHECKSUM(temp_checksum, eof_offset, temp_checksum);
	ADJUST_CHECKSUM(temp_checksum, info->checksum, eof_record->prefix.checksum);
	/* Assert that the journal file header and journal records are all in sync with respect to the db tn. */
	assert(header->bov_tn == db_tn);
	assert(header->eov_tn == db_tn);
	/* Write the journal records, but also 0-fill the tail of the journal file enough to fill
	 * an aligned filesystem-block-size boundary. Take into account that the file header is already written.
	 */
	write_size = ROUND_UP2((JNL_HDR_LEN + cre_jnl_rec_size), jnl_fs_block_size) - JNL_HDR_LEN;
	assert((jrecbuf + write_size) <= (jrecbuf_base + jrecbufbase_size));
	/* Assert that initial virtual-size of the journal file (which is nothing but the journal allocation)
	 * gives us enough space to keep the journal file header + padding + PINI/PFIN/EPOCH/EOF records.
	 * Do the assertion in units of 512-byte blocks as that keeps the values well under 4G. Keeping the
	 * units in bytes causes the highest autoswitchlimit value to round up to 4G effectively becoming 0
	 * on certain platforms and therefore failing the assert.
	 */
	assert(ROUND_UP2(header->virtual_size, jnl_fs_block_size/DISK_BLOCK_SIZE)
			> DIVIDE_ROUND_UP(JNL_HDR_LEN + write_size, DISK_BLOCK_SIZE));
	JNL_DO_FILE_WRITE(info->csa, create_fn, channel, JNL_HDR_LEN, jrecbuf, write_size, info->status, info->status2);
	STATUS_MSG(info);
	RETURN_ON_ERROR(info);
	UNIX_ONLY(GTM_JNL_FSYNC(info->csa, channel, status);)
	F_CLOSE(channel, status);	/* resets "channel" to FD_INVALID */
	free(jrecbuf_base);
	jrecbuf_base = NULL;
	if (info->no_rename)
		return EXIT_NRM;
	/* Say, info->jnl = a.mjl
	 * So system will have a.mjl and a.mjl_new for a crash before the following call
	 * Following does rename of a.mjl to a.mjl_timestamp.
	 * So system will have a.mjl_timestamp and a.mjl_new for a crash after this call
	 */
	WAIT_FOR_REPL_INST_UNFREEZE_SAFE(info->csa);	/* wait for instance freeze before journal file renames */
	if (SS_NORMAL != (info->status = gtm_rename((char *)info->jnl, (int)info->jnl_len,
						    (char *)rename_fn, rename_fn_len, &info->status2)))
	{
		if (IS_GTM_IMAGE)
			send_msg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		else
			gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	/* Following does rename of a.mjl_new to a.mjl.
	 * So system will have a.mjl_timestamp as previous generation and a.mjl as new/current journal file
	 */
	WAIT_FOR_REPL_INST_UNFREEZE_SAFE(info->csa);	/* wait for instance freeze before journal file renames */
	if (SS_NORMAL !=  (info->status = gtm_rename((char *)create_fn, create_fn_len,
						     (char *)info->jnl, (int)info->jnl_len, &info->status2)))
	{
		if (IS_GTM_IMAGE)
			send_msg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		else
			gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	if (IS_GTM_IMAGE)
		send_msg(VARLSTCNT (6) ERR_FILERENAME, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
	else
		gtm_putmsg(VARLSTCNT (6) ERR_FILERENAME, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
	DEBUG_ONLY(
		if (gtm_white_box_test_case_enabled && (WBTEST_JNL_CREATE_INTERRUPT == gtm_white_box_test_case_number))
		{
			UNIX_ONLY(DBGFPF((stderr, "CRE_JNL_FILE: started a wait\n")));	/* this white-box test is for UNIX */
			LONG_SLEEP(600);
			assert(FALSE); /* Should be killed before that */
		}
	)
	return EXIT_NRM;
}
