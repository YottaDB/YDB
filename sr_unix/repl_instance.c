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

#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_inet.h"
#include "gtm_time.h"

#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

#include "eintr_wrappers.h"
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
#include "gtmrecv.h"
#include "iosp.h"
#include "gtmio.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "gtmmsg.h"
#include "repl_sem.h"
#include "repl_instance.h"
#include "ftok_sems.h"
#include "error.h"
#include "gds_rundown.h"
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	boolean_t		in_repl_inst_edit;	/* Used by an assert in repl_inst_read/repl_inst_write */
GBLREF	boolean_t		in_repl_inst_create;	/* Used by repl_inst_read/repl_inst_write */
GBLREF	boolean_t		in_mupip_ftok;		/* Used by an assert in repl_inst_read */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	bool			in_backup;

/* Description:
 *	Get the environment of replication instance.
 * Parameters:
 *	fn : repl instance file name it gets
 *	fn_len: length of fn.
 *	bufsize: the buffer size caller gives. If exceeded, it trucates file name.
 * Return Value:
 *	TRUE, on success
 *	FALSE, otherwise.
 */
boolean_t repl_inst_get_name(char *fn, unsigned int *fn_len, unsigned int bufsize, instname_act error_action)
{
	char		temp_inst_fn[MAX_FN_LEN+1];
	mstr		log_nam, trans_name;
	uint4		ustatus;
	int4		status;
	boolean_t	ret;

	error_def(ERR_LOGTOOLONG);
	error_def(ERR_REPLINSTACC);
	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_TEXT);

	log_nam.addr = GTM_REPL_INSTANCE;
	log_nam.len = SIZEOF(GTM_REPL_INSTANCE) - 1;
	trans_name.addr = temp_inst_fn;
	ret = FALSE;
	if ((SS_NORMAL == (status = TRANS_LOG_NAME(&log_nam, &trans_name, temp_inst_fn, SIZEOF(temp_inst_fn),
							do_sendmsg_on_log2long)))
		&& (0 != trans_name.len))
	{
		temp_inst_fn[trans_name.len] = '\0';
		if (!get_full_path(trans_name.addr, trans_name.len, fn, fn_len, bufsize, &ustatus))
		{
			gtm_putmsg(VARLSTCNT(9) ERR_REPLINSTACC, 2, trans_name.len, trans_name.addr,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("full path could not be found"), ustatus);
		} else
			ret = TRUE;
	}
	if (FALSE == ret)
	{
		if (issue_rts_error == error_action)
		{
			if (SS_LOG2LONG == status)
				rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log_nam.len, log_nam.addr, SIZEOF(temp_inst_fn) - 1);
			else
				rts_error(VARLSTCNT(1) ERR_REPLINSTUNDEF);
		} else if (issue_gtm_putmsg == error_action)
		{
			if (SS_LOG2LONG == status)
				gtm_putmsg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log_nam.len, log_nam.addr, SIZEOF(temp_inst_fn) - 1);
			else
				gtm_putmsg(VARLSTCNT(1) ERR_REPLINSTUNDEF);
		}
	}
	return ret;
}

/* Description:
 *	Reads "buflen" bytes of data into "buff" from the file "fn" at offset "offset"
 * Parameters:
 *	fn    : Instance file name.
 *	offset: Offset at which to read
 *	buff  : Buffer to read into
 *	buflen: Number of bytes to read
 * Return Value:
 *	None
 */
void	repl_inst_read(char *fn, off_t offset, sm_uc_ptr_t buff, size_t buflen)
{
	int			status, fd;
	size_t			actual_readlen;
	unix_db_info		*udi;
	gd_region		*reg;
	repl_inst_hdr_ptr_t	replhdr;

	error_def(ERR_REPLINSTOPEN);
	error_def(ERR_REPLINSTREAD);
	error_def(ERR_REPLINSTCLOSE);
	error_def(ERR_REPLINSTFMT);

	/* Assert that except for MUPIP REPLIC -INSTANCE_CREATE or -EDITINSTANCE, all callers hold the FTOK semaphore
	 * on the replication instance file. Note that the instance file might be pointed to by one of the two region
	 * pointers "jnlpool.jnlpool_dummy_reg" or "recvpool.recvpool_dummy_reg" depending on whether the journal pool
	 * or the receive pool was attached to first by this particular process. If both of them are non-NULL, both the
	 * region pointers should be identical. This is also asserted below.
	 */
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| jnlpool.jnlpool_dummy_reg == recvpool.recvpool_dummy_reg);
	reg = jnlpool.jnlpool_dummy_reg;
	if (NULL == reg)
		reg = recvpool.recvpool_dummy_reg;
	assert((NULL == reg) && (in_repl_inst_create || in_repl_inst_edit || in_mupip_ftok)
		|| (NULL != reg) && !in_repl_inst_create && !in_repl_inst_edit && !in_mupip_ftok);
	if (NULL != reg)
	{
		udi = FILE_INFO(reg);
		assert(udi->grabbed_ftok_sem);
	}
	OPENFILE(fn, O_RDONLY, fd);
	if (FD_INVALID == fd)
		rts_error(VARLSTCNT(5) ERR_REPLINSTOPEN, 2, LEN_AND_STR(fn), errno);
	assert(0 < buflen);
	if (0 != offset)
	{
		LSEEKREAD(fd, offset, buff, buflen, status);
	} else
	{	/* Read starts from the replication instance file header. Assert that the entire file header was requested. */
		assert(REPL_INST_HDR_SIZE <= buflen);
		/* Use LSEEKREAD_AVAILABLE macro instead of LSEEKREAD. This is because if we are not able to read the entire
		 * fileheader, we still want to see if the "label" field of the file header got read in which case we can
		 * do the format check first. It is important to do the format check before checking "status" returned from
		 * LSEEKREAD* macros since the inability to read the entire file header might actually be due to the
		 * older format replication instance file being smaller than even the newer format instance file header.
		 */
		LSEEKREAD_AVAILABLE(fd, offset, buff, buflen, actual_readlen, status);
		if (GDS_REPL_INST_LABEL_SZ <= actual_readlen)
		{	/* Have read the entire label in the instance file header. Check if it is the right version */
			if (memcmp(buff, GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ - 1))
			{
				if ((NULL != reg) && (udi->grabbed_ftok_sem))
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error(VARLSTCNT(8) ERR_REPLINSTFMT, 6, LEN_AND_STR(fn),
					GDS_REPL_INST_LABEL_SZ - 1, GDS_REPL_INST_LABEL, GDS_REPL_INST_LABEL_SZ - 1, buff);
			}
		}
		if (0 == status)
		{	/* Check a few other fields in the file-header for compatibility */
			assert(actual_readlen == buflen);
			replhdr = (repl_inst_hdr_ptr_t)buff;
			/* Check endianness match */
			if (GTM_IS_LITTLE_ENDIAN != replhdr->is_little_endian)
			{
				if ((NULL != reg) && (udi->grabbed_ftok_sem))
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error(VARLSTCNT(8) ERR_REPLINSTFMT, 6, LEN_AND_STR(fn),
					LEN_AND_LIT(ENDIANTHIS), LEN_AND_LIT(ENDIANOTHER));
			}
			/* Check 64bitness match */
			if (GTM_IS_64BIT != replhdr->is_64bit)
			{
				if ((NULL != reg) && (udi->grabbed_ftok_sem))
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error(VARLSTCNT(8) ERR_REPLINSTFMT, 6, LEN_AND_STR(fn),
					LEN_AND_LIT(GTM_BITNESS_THIS), LEN_AND_LIT(GTM_BITNESS_OTHER));
			}
			/* At the time of this writing, the only minor version supported is 1.
			 * Whenever this gets updated, we need to add code to do the online upgrade.
			 * Add an assert as a reminder to do this.
			 */
			assert(1 == replhdr->replinst_minorver);
			/* Check if on-the-fly minor-version upgrade is necessary */
			if (GDS_REPL_INST_MINOR_LABEL != replhdr->replinst_minorver)
				assert(FALSE);
		}
	}
	assert((0 == status) || in_repl_inst_edit);
	if (0 != status)
	{
		if (-1 == status)
			rts_error(VARLSTCNT(6) ERR_REPLINSTREAD, 4, buflen, (qw_off_t *)&offset, LEN_AND_STR(fn));
		else
			rts_error(VARLSTCNT(7) ERR_REPLINSTREAD, 4, buflen, (qw_off_t *)&offset, LEN_AND_STR(fn), status);
	}
	CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	assert(0 == status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_REPLINSTCLOSE, 2, LEN_AND_STR(fn), status);
}

/* Description:
 *	Writes "buflen" bytes of data from "buff" into the file "fn" at offset "offset"
 * Parameters:
 *	fn    : Instance file name.
 *	offset: Offset at which to write
 *	buff  : Buffer to write from
 *	buflen: Number of bytes to write
 * Return Value:
 *	None.
 */
void	repl_inst_write(char *fn, off_t offset, sm_uc_ptr_t buff, size_t buflen)
{
	int		status, fd, oflag;
	unix_db_info	*udi;
	gd_region	*reg;
	ZOS_ONLY(int	realfiletag;)

	error_def(ERR_TEXT);
	error_def(ERR_REPLINSTOPEN);
	error_def(ERR_REPLINSTCREATE);
	error_def(ERR_REPLINSTWRITE);
	error_def(ERR_REPLINSTCLOSE);
	error_def(ERR_SYSCALL);
	ZOS_ONLY(error_def(ERR_BADTAG);)

	/* Assert that except for MUPIP REPLIC -INSTANCE_CREATE or -EDITINSTANCE, all callers hold the FTOK semaphore
	 * on the replication instance file. Note that the instance file might be pointed to by one of the two region
	 * pointers "jnlpool.jnlpool_dummy_reg" or "recvpool.recvpool_dummy_reg" depending on whether the journal pool
	 * or the receive pool was attached to first by this particular process. If both of them are non-NULL, both the
	 * region pointers should be identical. This is also asserted below.
	 */
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| jnlpool.jnlpool_dummy_reg == recvpool.recvpool_dummy_reg);
	DEBUG_ONLY(
		reg = jnlpool.jnlpool_dummy_reg;
		if (NULL == reg)
			reg = recvpool.recvpool_dummy_reg;
	)
	assert((NULL == reg) && (in_repl_inst_create || in_repl_inst_edit)
		|| (NULL != reg) && !in_repl_inst_create && !in_repl_inst_edit);
	DEBUG_ONLY(
		if (NULL != reg)
		{
			udi = FILE_INFO(reg);
			assert(udi->grabbed_ftok_sem);
		}
	)
	oflag = O_RDWR;
	if (in_repl_inst_create)
		oflag |= (O_CREAT | O_EXCL);
	OPENFILE3(fn, oflag, 0666, fd);
	if (FD_INVALID == fd)
	{
		if (!in_repl_inst_create)
			rts_error(VARLSTCNT(5) ERR_REPLINSTOPEN, 2, LEN_AND_STR(fn), errno);
		else
			rts_error(VARLSTCNT(5) ERR_REPLINSTCREATE, 2, LEN_AND_STR(fn), errno);
	}
#ifdef __MVS__
	if (-1 == (in_repl_inst_create ? gtm_zos_set_tag(fd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag) :
					 gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag)))
		TAG_POLICY_GTM_PUTMSG(fn, errno, realfiletag, TAG_BINARY);
#endif
	assert(0 < buflen);
	LSEEKWRITE(fd, offset, buff, buflen, status);
	assert(0 == status);
	if (0 != status)
		rts_error(VARLSTCNT(7) ERR_REPLINSTWRITE, 4, buflen, (qw_off_t *)&offset, LEN_AND_STR(fn), status);
	CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	assert(0 == status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_REPLINSTCLOSE, 2, LEN_AND_STR(fn), status);
}

/* Description:
 *	Hardens all pending writes for the instance file to disk
 * Parameters:
 *	fn    : Instance file name.
 * Return Value:
 *	None.
 */
void	repl_inst_sync(char *fn)
{
	int		status, fd, oflag;
	unix_db_info	*udi;
	gd_region	*reg;

	error_def(ERR_TEXT);
	error_def(ERR_REPLINSTOPEN);
	error_def(ERR_REPLINSTCREATE);
	error_def(ERR_REPLINSTWRITE);
	error_def(ERR_REPLINSTCLOSE);
	error_def(ERR_SYSCALL);

	/* Assert that except for MUPIP REPLIC -INSTANCE_CREATE or -EDITINSTANCE, all callers hold the FTOK semaphore
	 * on the replication instance file. Note that the instance file might be pointed to by one of the two region
	 * pointers "jnlpool.jnlpool_dummy_reg" or "recvpool.recvpool_dummy_reg" depending on whether the journal pool
	 * or the receive pool was attached to first by this particular process. If both of them are non-NULL, both the
	 * region pointers should be identical. This is also asserted below.
	 */
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| jnlpool.jnlpool_dummy_reg == recvpool.recvpool_dummy_reg);
	DEBUG_ONLY(
		reg = jnlpool.jnlpool_dummy_reg;
		if (NULL == reg)
			reg = recvpool.recvpool_dummy_reg;
	)
	DEBUG_ONLY(
		assert(NULL != reg);
		udi = FILE_INFO(reg);
		assert(udi->grabbed_ftok_sem);
	)
	oflag = O_RDWR;
	OPENFILE3(fn, oflag, 0666, fd);
	if (FD_INVALID == fd)
		rts_error(VARLSTCNT(5) ERR_REPLINSTOPEN, 2, LEN_AND_STR(fn), errno);
	GTM_FSYNC(fd, status);
	assert(0 == status);
	if (0 != status)
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fsync()"), CALLFROM, errno);
	CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	assert(0 == status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_REPLINSTCLOSE, 2, LEN_AND_STR(fn), status);
}

/* Description:
 *	Reset journal pool shmid and semid in replication instance file.
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void repl_inst_jnlpool_reset(void)
{
	repl_inst_hdr	repl_instance;
	unix_db_info	*udi;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	if (NULL != jnlpool.repl_inst_filehdr)
	{	/* If journal pool exists, reset sem/shm ids in the file header in the journal pool and flush changes to disk */
		jnlpool.repl_inst_filehdr->jnlpool_semid = INVALID_SEMID;
		jnlpool.repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
		jnlpool.repl_inst_filehdr->jnlpool_semid_ctime = 0;
		jnlpool.repl_inst_filehdr->jnlpool_shmid_ctime = 0;
		repl_inst_flush_filehdr();
	} else
	{	/* If journal pool does not exist, reset sem/shm ids directly in the replication instance file header on disk */
		repl_inst_read((char *)udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		repl_instance.jnlpool_semid = INVALID_SEMID;
		repl_instance.jnlpool_shmid = INVALID_SHMID;
		repl_instance.jnlpool_semid_ctime = 0;
		repl_instance.jnlpool_shmid_ctime = 0;
		repl_inst_write((char *)udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	}
}

/* Description:
 *	Reset receiver pool shmid and semid in replication instance file.
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void repl_inst_recvpool_reset(void)
{
	repl_inst_hdr	repl_instance;
	unix_db_info	*udi;

	udi = FILE_INFO(recvpool.recvpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	if (NULL != jnlpool.repl_inst_filehdr)
	{	/* If journal pool exists, reset sem/shm ids in the file header in the journal pool and flush changes to disk */
		jnlpool.repl_inst_filehdr->recvpool_semid = INVALID_SEMID;
		jnlpool.repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;
		jnlpool.repl_inst_filehdr->recvpool_semid_ctime = 0;
		jnlpool.repl_inst_filehdr->recvpool_shmid_ctime = 0;
		repl_inst_flush_filehdr();
	} else
	{	/* If journal pool does not exist, reset sem/shm ids directly in the replication instance file header on disk */
		repl_inst_read((char *)udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		repl_instance.recvpool_semid = INVALID_SEMID;
		repl_instance.recvpool_shmid = INVALID_SHMID;
		repl_instance.recvpool_semid_ctime = 0;
		repl_instance.recvpool_shmid_ctime = 0;
		repl_inst_write((char *)udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	}
}

/* Wrapper routine to GRAB the ftok semaphore lock of the replication instance file and to test for errors */
void	repl_inst_ftok_sem_lock(void)
{
	gd_region	*reg;
	unix_db_info	*udi;

	error_def(ERR_REPLFTOKSEM);

	/* If caller is online rollback, we would already be holding all the locks
	 * (db crit, ftok lock on instance file etc.) so account for that in the below code.
	 */
	assert((NULL != jnlpool.jnlpool_dummy_reg) || (NULL != recvpool.recvpool_dummy_reg));
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| (recvpool.recvpool_dummy_reg == jnlpool.jnlpool_dummy_reg));
	reg = jnlpool.jnlpool_dummy_reg;
	if (NULL == reg)
		reg = recvpool.recvpool_dummy_reg;
	assert(NULL != reg);
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem || udi->s_addrs.hold_onto_crit);
	if (!udi->grabbed_ftok_sem)
	{
		assert(0 == have_crit(CRIT_HAVE_ANY_REG));
		if (!ftok_sem_lock(reg, FALSE, FALSE))
		{
			assert(FALSE);
			rts_error(VARLSTCNT(4) ERR_REPLFTOKSEM, 2, LEN_AND_STR(udi->fn));
		}
	}
	assert(udi->grabbed_ftok_sem);
}

/* Wrapper routine to RELEASE the ftok semaphore lock of the replication instance file and to test for errors */
void	repl_inst_ftok_sem_release(void)
{
	gd_region	*reg;
	unix_db_info	*udi;

	error_def(ERR_REPLFTOKSEM);

	/* If caller is online rollback, we would already be holding all the locks
	 * (db crit, ftok lock on instance file etc.) so account for that in the below code.
	 */
	assert((NULL != jnlpool.jnlpool_dummy_reg) || (NULL != recvpool.recvpool_dummy_reg));
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| (recvpool.recvpool_dummy_reg == jnlpool.jnlpool_dummy_reg));
	reg = jnlpool.jnlpool_dummy_reg;
	if (NULL == reg)
		reg = recvpool.recvpool_dummy_reg;
	assert(NULL != reg);
	udi = FILE_INFO(reg);
	assert(udi->grabbed_ftok_sem); /* Be safe in PRO and avoid releasing if we do not hold the ftok semaphore */
	if (udi->grabbed_ftok_sem && !udi->s_addrs.hold_onto_crit)
	{
		assert(0 == have_crit(CRIT_HAVE_ANY_REG));
		if (!ftok_sem_release(reg, FALSE, FALSE))
		{
			assert(FALSE);
			rts_error(VARLSTCNT(4) ERR_REPLFTOKSEM, 2, LEN_AND_STR(udi->fn));
		}
	}
	assert(!udi->grabbed_ftok_sem || udi->s_addrs.hold_onto_crit);
}

/* Description:
 *	Get the 'n'th triple from the instance file.
 * Parameters:
 *	index  : The number of the triple to be read. 0 for the first triple, 1 for the second and so on...
 *	triple : A pointer to the triple to be filled in.
 * Return Value:
 *	0, on success
 *	ERR_REPLINSTNOHIST, if "index" is not a valid triple index.
 */
int4	repl_inst_triple_get(int4 index, repl_triple *triple)
{
	off_t			offset;
	unix_db_info		*udi;
	repl_inst_hdr		repl_instance;
	repl_inst_hdr_ptr_t	repl_inst_filehdr;

	error_def(ERR_REPLINSTNOHIST);

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	if (0 > index)
		return ERR_REPLINSTNOHIST;
	repl_inst_filehdr = jnlpool.repl_inst_filehdr;
	assert(NULL != repl_inst_filehdr);
	assert(index < repl_inst_filehdr->num_triples);
		/* assert that no caller should request a get of an unused (but allocated) triple */
	if (index >= repl_inst_filehdr->num_alloc_triples)
		return ERR_REPLINSTNOHIST;
	offset = REPL_INST_TRIPLE_OFFSET + (index * SIZEOF(repl_triple));
	repl_inst_read((char *)udi->fn, offset, (sm_uc_ptr_t)triple, SIZEOF(repl_triple));
	return 0;
}

/*
 * Parameters:
 *	seqno  : The journal seqno that is to be searched in the instance file triple history.
 *	triple : A pointer to the triple to be filled in. Contents might have been modified even on error return.
 *	index  : A pointer to the index of the triple. Also to be filled in.
 * Description:
 *	Given an input "seqno", locate the triple in the instance file that corresponds to "seqno-1".
 * Return Value:
 *	0, on success
 *	ERR_REPLINSTNOHIST, if "seqno" is NOT present in the instance file triple history range
 *	If input "seqno" is EQUAL to the "start_seqno" of the first triple in the instance file, it returns
 *		0 with "index" == -1 and "triple" filled in with the very first triple information.
 */
int4	repl_inst_triple_find_seqno(seq_num seqno, repl_triple *triple, int4 *index)
{
	unix_db_info	*udi;
	repl_inst_hdr	repl_instance;
	int4		tripnum, status;
	seq_num		prev_seqno;

	error_def(ERR_REPLINSTNOHIST);

	assert((1 < seqno) || jgbl.mur_rollback || in_backup);
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(0 != seqno);
	assert(NULL != jnlpool.repl_inst_filehdr);
	assert(jnlpool.repl_inst_filehdr->num_triples <= jnlpool.repl_inst_filehdr->num_alloc_triples);
	tripnum = jnlpool.repl_inst_filehdr->num_triples - 1;
	prev_seqno = 0;
	status = 0;
	for ( ; tripnum >= 0; tripnum--)
	{
		status = repl_inst_triple_get(tripnum, triple);
		assert(0 == status);
		if (0 != status)
			break;
		assert((0 == prev_seqno) || (prev_seqno > triple->start_seqno));
		prev_seqno = triple->start_seqno;
		assert(0 != prev_seqno);
		if (seqno > prev_seqno)
			break;
	}
	assert(-1 <= tripnum);
	*index = tripnum;
	if (0 != status)
	{
		assert(ERR_REPLINSTNOHIST == status);	/* this is currently the only error returned by repl_inst_triple_get() */
		return status;
	}
	if ((0 <= tripnum) || (prev_seqno && (triple->start_seqno == seqno)))
		return 0;
	else
		return ERR_REPLINSTNOHIST;
}

/* This is a wrapper on top of the function "repl_inst_triple_find_seqno" which additionally does error checking.
 * Also, for the case where "repl_inst_triple_find_seqno" returns 0 with a -1 triple_num, this function returns ERR_REPLINSTNOHIST.
 * This function finds the triple in the local replication instance file corresponding to seqno "seqno-1".
 */
int4	repl_inst_wrapper_triple_find_seqno(seq_num seqno, repl_triple *local_triple, int4 *local_triple_num)
{
	unix_db_info	*udi;
	char		histdetail[256];
	int4		status;
	repl_triple	*next_triple;

	error_def(ERR_REPLINSTNOHIST);

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	assert(1 < seqno);
	status = repl_inst_triple_find_seqno(seqno, local_triple, local_triple_num);
	assert((0 == status) || (ERR_REPLINSTNOHIST == status)); /* the only error returned by "repl_inst_triple_find_seqno" */
	if ((0 != status) || (-1 == *local_triple_num))
	{
		status = ERR_REPLINSTNOHIST;
		NON_GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%llx]", seqno - 1));
		GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%lx]", seqno - 1));
		gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTNOHIST, 4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
	} else
		assert(0 <= *local_triple_num);
	return status;
}

/* Description:
 *	Add a new triple to the replication instance file.
 * Parameters:
 *	triple : A pointer to the triple to be added to the instance file.
 * Return Value:
 *	None
 * Errors:
 *	Issues ERR_REPLINSTSEQORD error if new triple will cause seqno to be out of order.
 */
void	repl_inst_triple_add(repl_triple *triple)
{
	unix_db_info	*udi;
	int4		triple_num, status;
	off_t		offset;
	repl_triple	temptriple, *last_triple = &temptriple;

	error_def(ERR_REPLINSTSEQORD);

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(jnlpool.repl_inst_filehdr->num_triples <= jnlpool.repl_inst_filehdr->num_alloc_triples);
	triple_num = jnlpool.repl_inst_filehdr->num_triples;
	assert(0 <= triple_num);
	if (0 < triple_num)
	{
		status = repl_inst_triple_get(triple_num - 1, last_triple);
		assert(0 == status);	/* Since the triple_num we are passing is >=0 and <= num_triples */
		assert(jnlpool.jnlpool_ctl->last_triple_seqno == last_triple->start_seqno);
		if (triple->start_seqno < last_triple->start_seqno)
		{	/* cannot create triple with out-of-order start_seqno */
			rts_error(VARLSTCNT(8) ERR_REPLINSTSEQORD, 6, LEN_AND_LIT("New history record"),
				&triple->start_seqno, &last_triple->start_seqno, LEN_AND_STR(udi->fn));
		}
		if (!STRCMP(last_triple->root_primary_instname, triple->root_primary_instname)
				&& (last_triple->root_primary_cycle == triple->root_primary_cycle))
		{	/* Possible in case of a secondary where the receiver was receiving journal records
			 * (from the primary) for a while, was shut down and then restarted. Same instance
			 * is sending information so no new triple information needed. Return right away.
			 */
			return;
		}
		if (triple->start_seqno == last_triple->start_seqno)
		{	/* Starting seqno of the last triple matches the input triple. This means there are
			 * no journal records corresponding to the last triple in the journal files. Overwrite
			 * the last triple with the new triple information before writing new journal records.
			 */
			triple_num--;
		}
	}
	assert(!udi->s_addrs.hold_onto_crit);	/* this ensures we can safely do unconditional grab_lock and rel_lock */
	grab_lock(jnlpool.jnlpool_dummy_reg);
	offset = REPL_INST_TRIPLE_OFFSET + (SIZEOF(repl_triple) * (off_t)triple_num);
	time(&triple->created_time);
	repl_inst_write(udi->fn, offset, (sm_uc_ptr_t)triple, SIZEOF(repl_triple));
	triple_num++;
	if (jnlpool.repl_inst_filehdr->num_alloc_triples < triple_num)
		jnlpool.repl_inst_filehdr->num_alloc_triples = triple_num;
	jnlpool.repl_inst_filehdr->num_triples = triple_num;
	repl_inst_flush_filehdr();
	jnlpool.jnlpool_ctl->last_triple_seqno = triple->start_seqno;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	repl_inst_sync(udi->fn);	/* Harden the new triple to disk before any logical records for this arrive. */
	return;
}

/* Description:
 *	Given an input "rollback_seqno", virtually truncate all triples that correspond to seqnos >= "rollback_seqno"
 *	This function also updates other fields (unrelated to triple truncation) in the file header
 *	to reflect a clean shutdown by MUPIP JOURNAL ROLLBACK. This function is also invoked by MUPIP BACKUP in order
 *	to ensure the backed up instance file is initialized to reflect a clean shutdown.
 * Parameters:
 *	rollback_seqno : The seqno after which all triples have to be truncated.
 * Return Value:
 *	None
 * Errors:
 *	Issues ERR_REPLINSTNOHIST message if the call to "repl_inst_triple_find_seqno" returned an error.
 */
void	repl_inst_triple_truncate(seq_num rollback_seqno)
{
	unix_db_info	*udi;
	int4		status, index;
        size_t		nullsize;
	off_t		offset;
	repl_triple	temptriple, *triple = &temptriple;
	char		histdetail[256];

	error_def(ERR_REPLINSTNOHIST);

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr); /* Should have been set when mupip rollback invoked "mu_replpool_grab_sem" */
	if (0 != jnlpool.repl_inst_filehdr->num_triples)
	{
		status = repl_inst_triple_find_seqno(rollback_seqno, triple, &index);
		assert(0 == status);
		if (0 != status)
		{
			assert(ERR_REPLINSTNOHIST == status);	/* the only error returned by "repl_inst_triple_find_seqno" */
			NON_GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%llx]", rollback_seqno - 1));
			GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%lx]", rollback_seqno - 1));
			gtm_putmsg(VARLSTCNT(6) MAKE_MSG_WARNING(ERR_REPLINSTNOHIST),
				4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
			index = -1;
		}
		assert(triple->start_seqno <= rollback_seqno);
		index++;
		if (index != jnlpool.repl_inst_filehdr->num_triples)	/* no truncation necessary */
		{	/* null initialize all those to-be-virtually-truncated triples in the instance file */
			nullsize = (jnlpool.repl_inst_filehdr->num_triples - index) * SIZEOF(repl_triple);
			triple = malloc(nullsize);
			memset(triple, 0, nullsize);
			offset = REPL_INST_TRIPLE_OFFSET + (SIZEOF(repl_triple) * (off_t)index);
			repl_inst_write(udi->fn, offset, (sm_uc_ptr_t)triple, nullsize);
			free(triple);
			assert(jnlpool.repl_inst_filehdr->num_triples >= 0);
			assert(jnlpool.repl_inst_filehdr->num_alloc_triples > index);
		}
		jnlpool.repl_inst_filehdr->num_triples = index;
	}
	/* Reset "jnl_seqno" to the rollback seqno so future REPLINSTDBMATCH errors are avoided in "gtmsource_seqno_init" */
	jnlpool.repl_inst_filehdr->jnl_seqno = rollback_seqno;
	/* Reset "crash" to FALSE so future REPLREQROLLBACK errors are avoided at "jnlpool_init" time */
	jnlpool.repl_inst_filehdr->crash = FALSE;
	/* Reset sem/shm ids to reflect a clean shutdown so future REPLREQRUNDOWN errors are avoided at "jnlpool_init" time */
	jnlpool.repl_inst_filehdr->jnlpool_semid = INVALID_SEMID;
	jnlpool.repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
	jnlpool.repl_inst_filehdr->jnlpool_semid_ctime = 0;
	jnlpool.repl_inst_filehdr->jnlpool_shmid_ctime = 0;
	jnlpool.repl_inst_filehdr->recvpool_semid = INVALID_SEMID;	/* Just in case it is not already reset */
	jnlpool.repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;	/* Just in case it is not already reset */
	jnlpool.repl_inst_filehdr->recvpool_semid_ctime = 0;
	jnlpool.repl_inst_filehdr->recvpool_shmid_ctime = 0;
	/* Flush all file header changes in jnlpool.repl_inst_filehdr to disk */
	assert(!jnlpool.jnlpool_dummy_reg->open); /* Ensure the call below will not do a "grab_lock" as there is no jnlpool */
	repl_inst_flush_filehdr();
}

/* Description:
 *	Flushes the instance file header pointed to by "jnlpool.repl_inst_filehdr" to disk.
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void	repl_inst_flush_filehdr()
{
	unix_db_info	*udi;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool is not up in case of MUPIP ROLLBACK (the only caller) */
	/* If the journal pool exists, this function should hold the journal pool lock throughout its operation
	 * as it needs to read a consistent copy of the journal pool.  It should use "grab_lock" and "rel_lock"
	 * to achieve this.  Note that it is possible that the journal pool does not exist (if the caller is MUPIP
	 * ROLLBACK) in which case it does not need to hold any lock on the journal pool. In case of online rollback,
	 * we should already be holding the lock so dont need to grab or release it.
	 */
	if (jnlpool.jnlpool_dummy_reg->open && !udi->s_addrs.hold_onto_crit)   /* journal pool exists and this process has done
										* "jnlpool_init" and is not online rollback */
		grab_lock(jnlpool.jnlpool_dummy_reg);
	/* flush the instance file header */
	repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)jnlpool.repl_inst_filehdr, REPL_INST_HDR_SIZE);
	if (jnlpool.jnlpool_dummy_reg->open && !udi->s_addrs.hold_onto_crit)   /* journal pool exists and this process has done
										* "jnlpool_init" and is not online rollback */
		rel_lock(jnlpool.jnlpool_dummy_reg);
}

/* Description:
 *	Flushes the "gtmsrc_lcl" structure corresponding to the jnlpool.gtmsource_local structure for the
 *	calling source server. Updates "gtmsource_local->last_flush_resync_seqno" to equal "gtmsource_local->read_jnl_seqno"
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void	repl_inst_flush_gtmsrc_lcl()
{
	unix_db_info		*udi;
	int4			index;
	off_t			offset;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.gtmsource_local);
	index = jnlpool.gtmsource_local->gtmsrc_lcl_array_index;
	assert(0 <= index);
	assert(jnlpool.gtmsource_local == &jnlpool.gtmsource_local_array[index]);
	gtmsrclcl_ptr = &jnlpool.gtmsrc_lcl_array[index];
	assert(jnlpool.jnlpool_dummy_reg->open);	/* journal pool exists and this process has done "jnlpool_init" */
	/* Copy each field from "gtmsource_local" to "gtmsrc_lcl" before flushing it to disk.
	 * Do not need the journal pool lock, as we are the only ones reading/updating the below fields
	 * in "gtmsource_local" or "gtmsrc_lcl".
	 */
	COPY_GTMSOURCELOCAL_TO_GTMSRCLCL(jnlpool.gtmsource_local, gtmsrclcl_ptr);
	offset = REPL_INST_HDR_SIZE + (SIZEOF(gtmsrc_lcl) * (off_t)index);
	repl_inst_write(udi->fn, offset, (sm_uc_ptr_t)gtmsrclcl_ptr, SIZEOF(gtmsrc_lcl));
	jnlpool.gtmsource_local->last_flush_resync_seqno = jnlpool.gtmsource_local->read_jnl_seqno;
}

/* Description:
 *	Flushes the "repl_inst_hdr" and "gtmsrc_lcl" sections in the journal pool to the on disk copy of the instance file.
 * Parameters:
 *	None
 * Return Value:
 *	None
 */
void	repl_inst_flush_jnlpool(boolean_t reset_recvpool_fields)
{
	unix_db_info		*udi;
	int4			index;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;

	assert(NULL != jnlpool.jnlpool_dummy_reg);
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	/* This function should be invoked only if the caller determines this is last process attached to the journal pool.
	 * Since the ftok lock on the instance file is already held, no other process will be allowed to attach to the
	 * journal pool and hence this is the only process having access to the journal pool during this function.
	 * Therefore there is no need to hold the journal pool access control semaphore (JNL_POOL_ACCESS_SEM).
	 */
	assert(NULL != jnlpool.gtmsource_local_array);
	assert(NULL != jnlpool.gtmsrc_lcl_array);
	assert(NULL != jnlpool.repl_inst_filehdr);
	assert(NULL != jnlpool.jnlpool_ctl);
	assert((sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array == (sm_uc_ptr_t)jnlpool.repl_inst_filehdr + REPL_INST_HDR_SIZE);
	/* Reset the instance file header field before flushing and removing the journal pool shared memory */
	jnlpool.repl_inst_filehdr->crash = FALSE;
	jnlpool.repl_inst_filehdr->jnlpool_semid = INVALID_SEMID;
	jnlpool.repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
	if (reset_recvpool_fields)
	{
		jnlpool.repl_inst_filehdr->recvpool_semid = INVALID_SEMID;	/* Just in case it is not already reset */
		jnlpool.repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;	/* Just in case it is not already reset */
	}
	/* If the source server that created the journal pool died before it was completely initialized in "gtmsource_seqno_init"
	 * do not copy seqnos from the journal pool into the instance file header. Instead keep the instance file header unchanged.
	 */
	if (jnlpool.jnlpool_ctl->pool_initialized)
	{
		assert(jnlpool.jnlpool_ctl->start_jnl_seqno);
		assert(jnlpool.jnlpool_ctl->jnl_seqno);
		jnlpool.repl_inst_filehdr->jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
		/* Copy all "gtmsource_local" to corresponding "gtmsrc_lcl" structures before flushing to instance file */
		gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
		gtmsrclcl_ptr = &jnlpool.gtmsrc_lcl_array[0];
		for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++, gtmsrclcl_ptr++)
			COPY_GTMSOURCELOCAL_TO_GTMSRCLCL(gtmsourcelocal_ptr, gtmsrclcl_ptr);
		repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)jnlpool.repl_inst_filehdr, REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE);
	} else
		repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)jnlpool.repl_inst_filehdr, REPL_INST_HDR_SIZE);
}

/* This function determines if this replication instance was formerly a root primary. It finds this out by looking at the
 * last triple in the instance file and comparing the "root_primary_instname" field there with this instance name. If they
 * are the same, it means the last triple was generated by this instance and hence was a root primary then. This function
 * will only be invoked by a propagating primary instance (RECEIVER SERVER or ROLLBACK -FETCHRESYNC).
 *
 * It returns TRUE only if the instance file header field "was_rootprimary" is TRUE and if the last triple was generated
 * by this instance. It returns FALSE otherwise.
 */
boolean_t	repl_inst_was_rootprimary(void)
{
	unix_db_info	*udi;
	int4		triple_num, status;
	repl_triple	temptriple, *last_triple = &temptriple;
	boolean_t	was_rootprimary;

	repl_inst_ftok_sem_lock();
	triple_num = jnlpool.repl_inst_filehdr->num_triples;
	was_rootprimary = jnlpool.repl_inst_filehdr->was_rootprimary;
	assert(triple_num <= jnlpool.repl_inst_filehdr->num_alloc_triples);
	assert(0 <= triple_num);
	if (was_rootprimary && (0 < triple_num))
	{
		status = repl_inst_triple_get(triple_num - 1, last_triple);
		assert(0 == status);	/* Since the triple_num we are passing is >=0 and <= num_triples */
		was_rootprimary = !STRCMP(last_triple->root_primary_instname, jnlpool.repl_inst_filehdr->this_instname);
	}
	repl_inst_ftok_sem_release();
	return was_rootprimary;
}

/* This function resets "zqgblmod_seqno" and "zqgblmod_tn" in all replicated database file headers to 0.
 * This shares a lot of its code with the function "gtmsource_update_zqgblmod_seqno_and_tn".
 * Any changes there might need to be reflected here.
 */
void	repl_inst_reset_zqgblmod_seqno_and_tn(void)
{
	gd_region		*reg, *reg_top;
	boolean_t		all_files_open;

	error_def(ERR_NOTALLDBOPN);

	if (0 == jnlpool.jnlpool_ctl->max_zqgblmod_seqno)
	{	/* Already reset to 0 by a previous call to this function. No need to do it again. */
		return;
	}
	/* This function is currently ONLY called by receiver server AND mupip replic -source -losttncomplete
	 * both of which should have NO GBLDIR or REGION OPEN at this time. Assert that.
	 */
	assert(NULL == gd_header);
	if (NULL == gd_header)
		gvinit();
	/* We use the same code dse uses to open all regions but we must make sure they are all open before proceeding. */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
		rts_error(VARLSTCNT(1) ERR_NOTALLDBOPN);
	repl_inst_ftok_sem_lock();
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
	{
		assert(reg->open);
		TP_CHANGE_REG(reg);
		if (!REPL_ALLOWED(cs_data))
			continue;
		/* Although csa->hdr->zqgblmod_seqno is only modified by the source server (while holding the ftok semaphore
		 * on the replication instance file), it is read by fileheader_sync() which does it while holding region crit.
		 * To avoid the latter from reading an inconsistent value (i.e. neither the pre-update nor the post-update value,
		 * which is possible if the 8-byte operation is not atomic but a sequence of two 4-byte operations AND if the
		 * pre-update and post-update value differ in their most significant 4-bytes) we grab crit. We could have used
		 * the QWCHANGE_IS_READER_CONSISTENT macro (which checks for most significant 4-byte differences) instead to
		 * determine if it is really necessary to grab crit. But since the update to zqgblmod_seqno is a rare operation,
		 * we decide to play it safe.
		 */
		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(reg);
		cs_addrs->hdr->zqgblmod_seqno = (seq_num)0;
		cs_addrs->hdr->zqgblmod_tn = (trans_num)0;
		rel_crit(reg);
	}
	jnlpool.jnlpool_ctl->max_zqgblmod_seqno = 0;
	repl_inst_ftok_sem_release();
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
	{	/* Rundown all databases that we opened as we dont need them anymore. This is not done in the previous
		 * loop as it has to wait until the ftok semaphore of the instance file has been released as otherwise
		 * an assert in gds_rundown will fail as it tries to get the ftok semaphore of the database while holding
		 * another ftok semaphore already.
		 */
		assert(reg->open);
		TP_CHANGE_REG(reg);
		gds_rundown();
	}
}

