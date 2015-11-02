/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#if defined(UNIX)
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gtm_stat.h"
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

/* Note : Now all system error messages are issued here. So callers do not need to issue them again */
#define STATUS_MSG(info)										\
{													\
	if (SS_NORMAL != info->status2)									\
	{												\
		if (run_time)										\
			send_msg(VARLSTCNT(12) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, info->jnl,\
				info->fn_len, info->fn, info->status, 0, info->status2);		\
		else											\
			gtm_putmsg(VARLSTCNT1(11) ERR_JNLCRESTATUS, 7, CALLFROM, info->jnl_len, info->jnl,\
				info->fn_len, info->fn, info->status, PUT_SYS_ERRNO(info->status2));	\
	} else if (SS_NORMAL != info->status)								\
	{												\
		if (run_time)										\
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
	F_CLOSE(channel, status);					\
	if (NULL != jrecbuf)						\
		free(jrecbuf);						\
	return EXIT_ERR;						\
}

#if defined(VMS)
#define ZERO_SIZE_IN_BLOCKS	127 /* 127 is RMS maximum blocks / write */
#define ZERO_SIZE		(ZERO_SIZE_IN_BLOCKS * DISK_BLOCK_SIZE)
#endif

#ifdef DEBUG
#include "gtmimagename.h"
GBLREF	enum gtmImageTypes	image_type;
#endif
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	bool run_time;
GBLREF	jnl_process_vector	*prc_vec;

/* Create a journal file from info.
 * If necessary, it renames journal file of same name.
 * Note: jgbl.gbl_jrec_time must be set by callers
 */
uint4	cre_jnl_file(jnl_create_info *info)
{
	mstr 		filestr;
	int 		org_fn_len, rename_fn_len, fstat;
	uint4		ustatus;
	char		*org_fn, rename_fn[MAX_FN_LEN];
	error_def	(ERR_JNLCRESTATUS);
	error_def	(ERR_JNLFNF);

	if (!info->no_rename)	/* ***MAY*** be rename is required */
	{
		if (SS_NORMAL != (info->status = prepare_unique_name((char *)info->jnl, info->jnl_len, "", "",
				rename_fn, &rename_fn_len, &info->status2)))
		{	/* prepare_unique_name calls append_time_stamp which needs to open the info->jnl file.
			 * We are here because append_time_stamp failed to open info->jnl or something else.
			 * So check if info->jnl is present in the system */
			filestr.addr = (char *)info->jnl;
			filestr.len = info->jnl_len;
			if (FILE_NOT_FOUND != (fstat = gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus)))
			{
				if (FILE_STAT_ERROR == fstat)
				{
					STATUS_MSG(info);	/* for prepare_unique_name call */
					info->status = ustatus;
					info->status2 = SS_NORMAL;
				}
				STATUS_MSG(info);
				return EXIT_ERR;
			}
			if (run_time)
				send_msg(VARLSTCNT(4) ERR_JNLFNF, 2, filestr.len, filestr.addr);
			else
				gtm_putmsg(VARLSTCNT(4) ERR_JNLFNF, 2, filestr.len, filestr.addr);
			STATUS_MSG(info);
			info->status = info->status2 = SS_NORMAL;
			info->no_rename = TRUE;		/* We wanted to rename, but not required */
			info->no_prev_link = TRUE;	/* No rename => no prev_link */
		} else
		{
			/* Note if info->no_prev_link == TRUE, we do not keep previous link, though rename can happen */
			if (JNL_ENABLED(info) && !info->no_prev_link)
			{
				memcpy(info->prev_jnl, rename_fn, rename_fn_len + 1);
				info->prev_jnl_len = rename_fn_len;
			} else
				assert(info->no_prev_link);
		}
	} /* else we know for sure rename is not required */
	return (cre_jnl_file_common(info, rename_fn, rename_fn_len));
}


/* This creates info->jnl and
 * if (!info->no_rename) then it renames existing info->jnl to be rename_fn */
uint4 cre_jnl_file_common(jnl_create_info *info, char *rename_fn, int rename_fn_len)
{
	jnl_file_header		header;
	struct_jrec_pfin	*pfin_record;
	struct_jrec_pini	*pini_record;
	struct_jrec_epoch	*epoch_record;
	struct_jrec_eof		*eof_record;
	unsigned char		*create_fn, fn_buff[MAX_FN_LEN];
	int			create_fn_len, cre_jnl_rec_size, status;
	fd_type			channel;
	char            	*jrecbuf;
	gd_id			jnlfile_id;
#if defined(VMS)
	struct FAB      	fab;
	struct NAM      	nam;
	char            	es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	uint4			blk, block, zero_size;
	io_status_block_disk	iosb;
#elif defined(UNIX)
	struct stat		stat_buf;
	int			fstat_res;
#endif
	uint4			temp_offset, temp_checksum;

	error_def(ERR_FILERENAME);
	error_def(ERR_RENAMEFAIL);
	error_def(ERR_JNLCRESTATUS);
	error_def(ERR_PREMATEOF);

	jrecbuf = NULL;
	if (info->no_rename)
	{	/* The only cases where no-renaming is possible are as follows
    		 * (i) MUPIP SET JOURNAL where the new journal file name is different from the current journal file name
		 * (ii) For MUPIP BACKUP, MUPIP SET JOURNAL, GT.M Runtime and forw_phase_recovery,
		 * 	in case the current journal file does not exist (due to some abnormal condition).
		 *	But in this case we cut the link and hence info->no_prev_link should be TRUE.
		 * The assert below tries to capture this as much as possible without introducing any new global variables.
		 */
		assert((MUPIP_IMAGE == image_type && !jgbl.forw_phase_recovery) || (GTM_IMAGE == image_type && info->no_prev_link));
		create_fn_len = info->jnl_len;
		create_fn = info->jnl;
		assert(0 == create_fn[create_fn_len]);
	} else
	{
		create_fn = &fn_buff[0];
		if (SS_NORMAL != (info->status = prepare_unique_name((char *)info->jnl, (int)info->jnl_len, "", EXT_NEW,
				(char *)create_fn, &create_fn_len, &info->status2)))
		{
			STATUS_MSG(info);
			return EXIT_ERR;
		}
	}
#if defined(UNIX)
	OPENFILE3((char *)create_fn, O_CREAT | O_EXCL | O_RDWR, 0600, channel);
	if (-1 == channel)
	{
		info->status = errno;
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	if (-1 == fchmod(channel, 0666))
	{
		info->status = errno;
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	jrecbuf = malloc(DISK_BLOCK_SIZE);
	memset(jrecbuf, 0, DISK_BLOCK_SIZE);
	FSTAT_FILE(channel, &stat_buf, fstat_res);
	if (-1 == fstat_res)
	{
		info->status = errno;
		STATUS_MSG(info);
		F_CLOSE(channel, status);
		return EXIT_ERR;
	}
	set_gdid_from_stat(&jnlfile_id, &stat_buf);
#elif defined(VMS)
	nam = cc$rms_nam;
	nam.nam$l_rsa = name_buffer;
	nam.nam$b_rss = sizeof(name_buffer);
	nam.nam$l_esa = es_buffer;
	nam.nam$b_ess = sizeof(es_buffer);
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
	jrecbuf = malloc(ZERO_SIZE);
	memset(jrecbuf, 0, ZERO_SIZE);
	block = (JNL_HDR_LEN >> LOG2_DISK_BLOCK_SIZE);
	assert(block * DISK_BLOCK_SIZE == JNL_HDR_LEN);
	for (blk = block;  blk < info->alloc;  blk += ZERO_SIZE_IN_BLOCKS)
	{
		zero_size = (blk + ZERO_SIZE_IN_BLOCKS <= info->alloc) ?
			ZERO_SIZE : (info->alloc - blk) * DISK_BLOCK_SIZE;
		DO_FILE_WRITE(channel, blk * DISK_BLOCK_SIZE, jrecbuf, zero_size, info->status, info->status2);
		STATUS_MSG(info);
		RETURN_ON_ERROR(info);
	}
	memcpy(jnlfile_id.dvi, &nam.nam$t_dvi, sizeof(jnlfile_id.dvi));
	memcpy(jnlfile_id.did, &nam.nam$w_did, sizeof(jnlfile_id.did));
	memcpy(jnlfile_id.fid, &nam.nam$w_fid, sizeof(jnlfile_id.fid));
#endif
	info->checksum = jnl_get_checksum_entire((uint4 *)&jnlfile_id, sizeof(gd_id));
	/* We have already saved previous journal file name in info */
	jfh_from_jnl_info(info, &header);
	header.recover_interrupted = mupip_jnl_recover;
	DO_FILE_WRITE(channel, 0, &header, JNL_HDR_LEN, info->status, info->status2);
	STATUS_MSG(info);
	RETURN_ON_ERROR(info);
	assert(DISK_BLOCK_SIZE >= EPOCH_RECLEN + EOF_RECLEN + PFIN_RECLEN + PINI_RECLEN);
	pini_record = (struct_jrec_pini *)&jrecbuf[0];
	pini_record->prefix.jrec_type = JRT_PINI;
	pini_record->prefix.forwptr = pini_record->suffix.backptr = PINI_RECLEN;
	pini_record->prefix.tn = info->tn;
	pini_record->prefix.pini_addr = JNL_HDR_LEN;
	pini_record->prefix.time = jgbl.gbl_jrec_time;	/* callers must set it */
	temp_offset = JNL_HDR_LEN;
	temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
	pini_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
	pini_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	assert(prc_vec);
	memcpy((unsigned char*)&pini_record->process_vector[CURR_JPV], (unsigned char*)prc_vec, sizeof(jnl_process_vector));
	/* Already process_vector[ORIG_JPV] is memset 0 */
	pini_record->prefix.pini_addr = JNL_HDR_LEN;
	/* EPOCHs are written unconditionally in Unix while they are written only for BEFORE_IMAGE in VMS */
	if (JNL_HAS_EPOCH(info))
	{
		epoch_record = (struct_jrec_epoch *)&jrecbuf[PINI_RECLEN];
		epoch_record->prefix.jrec_type = JRT_EPOCH;
		epoch_record->prefix.forwptr = epoch_record->suffix.backptr = EPOCH_RECLEN;
		epoch_record->prefix.tn = info->tn;
		epoch_record->prefix.pini_addr = JNL_HDR_LEN;
		epoch_record->prefix.time = jgbl.gbl_jrec_time;
		epoch_record->blks_to_upgrd = info->blks_to_upgrd;
		temp_offset = JNL_HDR_LEN + PINI_RECLEN;
		temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
		epoch_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
		epoch_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		QWASSIGN(epoch_record->jnl_seqno, info->reg_seqno);
		pfin_record = (struct_jrec_pfin *)&jrecbuf[PINI_RECLEN + EPOCH_RECLEN];
		temp_offset = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN;
		temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
		pfin_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
		eof_record = (struct_jrec_eof *)&jrecbuf[PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN];
		temp_offset = JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN;
		temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
		eof_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
		cre_jnl_rec_size = PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN;
	} else
	{
		pfin_record = (struct_jrec_pfin *)&jrecbuf[PINI_RECLEN];
		temp_offset = JNL_HDR_LEN + PINI_RECLEN;
		temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
		pfin_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
		eof_record = (struct_jrec_eof *)&jrecbuf[PINI_RECLEN + PFIN_RECLEN];
		temp_offset = JNL_HDR_LEN + PINI_RECLEN + PFIN_RECLEN;
		temp_checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, temp_offset);
		eof_record->prefix.checksum = ADJUST_CHECKSUM(temp_checksum, info->checksum);
		cre_jnl_rec_size = PINI_RECLEN + PFIN_RECLEN + EOF_RECLEN;
	}
	pfin_record->prefix.jrec_type = JRT_PFIN;
	pfin_record->prefix.forwptr = pfin_record->suffix.backptr = PFIN_RECLEN;
	pfin_record->prefix.tn = info->tn;
	pfin_record->prefix.pini_addr = JNL_HDR_LEN;
	pfin_record->prefix.time = jgbl.gbl_jrec_time;
	pfin_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	eof_record->prefix.jrec_type = JRT_EOF;
	eof_record->prefix.forwptr = eof_record->suffix.backptr = EOF_RECLEN;
	eof_record->prefix.tn = info->tn;
	eof_record->prefix.pini_addr = JNL_HDR_LEN;
	eof_record->prefix.time = jgbl.gbl_jrec_time;
	QWASSIGN(eof_record->jnl_seqno, info->reg_seqno);
	eof_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	DO_FILE_WRITE(channel, JNL_HDR_LEN, jrecbuf, cre_jnl_rec_size, info->status, info->status2);
	STATUS_MSG(info);
	RETURN_ON_ERROR(info);
	F_CLOSE(channel, status);
	free(jrecbuf);
	jrecbuf = NULL;
	if (info->no_rename)
		return EXIT_NRM;
	/* Say, info->jnl = a.mjl
	 * So system will have a.mjl and a.mjl_new for a crash before the following call
	 * Following does rename of a.mjl to a.mjl_timestamp.
	 * So system will have a.mjl_timestamp and a.mjl_new for a crash after this call
	 */
	if (SS_NORMAL != (info->status = gtm_rename((char *)info->jnl, (int)info->jnl_len,
			(char *)rename_fn, rename_fn_len, &info->status2)))
	{
		if (run_time)
			send_msg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		else
			gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	/* Following does rename of a.mjl_new to a.mjl.
	 * So system will have a.mjl_timestamp as previous generation and a.mjl as new/current journal file
	 */
	if (SS_NORMAL !=  (info->status = gtm_rename((char *)create_fn, create_fn_len,
			(char *)info->jnl, (int)info->jnl_len, &info->status2)))
	{
		if (run_time)
			send_msg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		else
			gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
		STATUS_MSG(info);
		return EXIT_ERR;
	}
	if (run_time)
		send_msg(VARLSTCNT (6) ERR_FILERENAME, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
	else
		gtm_putmsg(VARLSTCNT (6) ERR_FILERENAME, 4, info->jnl_len, info->jnl, rename_fn_len, rename_fn);
	return EXIT_NRM;
}
