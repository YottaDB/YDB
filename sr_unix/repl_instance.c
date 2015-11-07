/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "anticipatory_freeze.h"
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
GBLREF	int4			strm_index;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		is_rcvr_server;

ZOS_ONLY(error_def(ERR_BADTAG);)
error_def(ERR_LOGTOOLONG);
error_def(ERR_NOTALLDBOPN);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_REPLINSTACC);
error_def(ERR_REPLINSTCLOSE);
error_def(ERR_REPLINSTCREATE);
error_def(ERR_REPLINSTFMT);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLINSTOPEN);
error_def(ERR_REPLINSTREAD);
error_def(ERR_REPLINSTSEQORD);
error_def(ERR_REPLINSTUNDEF);
error_def(ERR_REPLINSTWRITE);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

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
	char		temp_inst_fn[MAX_FN_LEN + 1];
	mstr		log_nam, trans_name;
	uint4		ustatus;
	int4		status;
	boolean_t	ret;

	log_nam.addr = GTM_REPL_INSTANCE;
	log_nam.len = SIZEOF(GTM_REPL_INSTANCE) - 1;
	trans_name.addr = temp_inst_fn;
	ret = FALSE;
	GET_INSTFILE_NAME(do_sendmsg_on_log2long, issue_gtm_putmsg);
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

	/* Assert that except for MUPIP REPLIC -INSTANCE_CREATE or -EDITINSTANCE or MUPIP FTOK, all callers hold the FTOK semaphore
	 * on the replication instance file OR the journal pool lock. Note that the instance file might be pointed to by one of the
	 * two region pointers "jnlpool.jnlpool_dummy_reg" or "recvpool.recvpool_dummy_reg" depending on whether the journal pool
	 * or the receive pool was attached to first by this particular process. If both of them are non-NULL, both the region
	 * pointers should be identical. This is also asserted below.
	 * Note: Typically, journal pool lock should have sufficed. However, in certain places like jnlpool_init and recvpool_init,
	 * the journal pool is not yet created and hence grab_lock/rel_lock does not make sense. In those cases we need the FTOK
	 * lock on the instance file. The ONLY exception to this is ROLLBACK in which case it does NOT hold the FTOK semaphore and
	 * since it is NOT necessary for ROLLBACK to have a journal pool open, grab_lock will not be done either. Assert
	 * accordingly.
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
		assert(udi->grabbed_ftok_sem || ((NULL != jnlpool.jnlpool_ctl) && udi->s_addrs.now_crit) || jgbl.mur_rollback);
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
				rts_error(VARLSTCNT(8) ERR_REPLINSTFMT, 6, LEN_AND_STR(fn),
					LEN_AND_LIT(ENDIANTHIS), LEN_AND_LIT(ENDIANOTHER));
			}
			/* Check 64bitness match */
			if (GTM_IS_64BIT != replhdr->is_64bit)
			{
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

	/* Assert that except for MUPIP REPLIC -INSTANCE_CREATE or -EDITINSTANCE, all callers hold the FTOK semaphore on the
	 * replication instance file OR the journal pool lock. Note that the instance file might be pointed to by one of the
	 * two region pointers "jnlpool.jnlpool_dummy_reg" or "recvpool.recvpool_dummy_reg" depending on whether the journal pool
	 * or the receive pool was attached to first by this particular process. If both of them are non-NULL, both the region
	 * pointers should be identical. This is also asserted below.
	 * Note: Typically, journal pool lock should have sufficed. However, in certain places like jnlpool_init and recvpool_init,
	 * the journal pool is not yet created and hence grab_lock/rel_lock does not make sense. In those case we need the FTOK
	 * lock on the instance file. The ONLY exception to this is ROLLBACK in which case it does NOT hold the FTOK semaphore and
	 * since it is NOT necessary for ROLLBACK to have a journal pool open, grab_lock will not be done either. Assert
	 * accordingly.
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
			assert(udi->grabbed_ftok_sem || ((NULL != jnlpool.jnlpool_ctl) && udi->s_addrs.now_crit)
				|| jgbl.mur_rollback);
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
	REPL_INST_LSEEKWRITE(fd, offset, buff, buflen, status);
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
		assert((NULL != jnlpool.jnlpool_ctl) && udi->s_addrs.now_crit);
	)
	oflag = O_RDWR;
	OPENFILE3(fn, oflag, 0666, fd);
	if (FD_INVALID == fd)
		rts_error(VARLSTCNT(5) ERR_REPLINSTOPEN, 2, LEN_AND_STR(fn), errno);
	GTM_REPL_INST_FSYNC(fd, status);
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

	assert(!jgbl.mur_rollback); /* Rollback already has standalone access and will not ask for ftok lock */
	assert((NULL != jnlpool.jnlpool_dummy_reg) || (NULL != recvpool.recvpool_dummy_reg));
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| (recvpool.recvpool_dummy_reg == jnlpool.jnlpool_dummy_reg));
	reg = jnlpool.jnlpool_dummy_reg;
	if (NULL == reg)
		reg = recvpool.recvpool_dummy_reg;
	assert(NULL != reg);
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem);
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

	assert(!jgbl.mur_rollback); /* Rollback already has standalone access and will not ask for ftok lock */
	assert((NULL != jnlpool.jnlpool_dummy_reg) || (NULL != recvpool.recvpool_dummy_reg));
	assert((NULL == jnlpool.jnlpool_dummy_reg) || (NULL == recvpool.recvpool_dummy_reg)
		|| (recvpool.recvpool_dummy_reg == jnlpool.jnlpool_dummy_reg));
	reg = jnlpool.jnlpool_dummy_reg;
	if (NULL == reg)
		reg = recvpool.recvpool_dummy_reg;
	assert(NULL != reg);
	udi = FILE_INFO(reg);
	assert(udi->grabbed_ftok_sem);
	if (udi->grabbed_ftok_sem) /* Be safe in PRO and avoid releasing if we do not hold the ftok semaphore */
	{
		assert(0 == have_crit(CRIT_HAVE_ANY_REG));
		if (!ftok_sem_release(reg, FALSE, FALSE))
		{
			assert(FALSE);
			rts_error(VARLSTCNT(4) ERR_REPLFTOKSEM, 2, LEN_AND_STR(udi->fn));
		}
	}
	assert(!udi->grabbed_ftok_sem);
}

/* Description:
 *	Get the 'n'th histinfo record from the instance file.
 * Parameters:
 *	index  : The number of the histinfo record to be read. 0 for the first histinfo record, 1 for the second and so on...
 *	histinfo : A pointer to the repl_histinfo structure to be filled in.
 * Return Value:
 *	0, on success
 *	ERR_REPLINSTNOHIST, if "index" is not a valid histinfo index.
 */
int4	repl_inst_histinfo_get(int4 index, repl_histinfo *histinfo)
{
	off_t			offset;
	unix_db_info		*udi;
	repl_inst_hdr_ptr_t	repl_inst_filehdr;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->s_addrs.now_crit || jgbl.mur_rollback);
	if (0 > index)
		return ERR_REPLINSTNOHIST;
	repl_inst_filehdr = jnlpool.repl_inst_filehdr;
	assert(NULL != repl_inst_filehdr);
	assert(index < repl_inst_filehdr->num_histinfo);
		/* assert that no caller should request a get of an unused (but allocated) histinfo */
	if (index >= repl_inst_filehdr->num_alloc_histinfo)
		return ERR_REPLINSTNOHIST;
	offset = REPL_INST_HISTINFO_START + (index * SIZEOF(repl_histinfo));
	repl_inst_read((char *)udi->fn, offset, (sm_uc_ptr_t)histinfo, SIZEOF(repl_histinfo));
	assert(histinfo->histinfo_num == index);
	return 0;
}

/*
 * Parameters:
 *	seqno      : The journal seqno that is to be searched in the instance file history.
 *	strm_idx   : -1, 0, 1, 2, ... 15 indicating the stream # within which to search.
 *	           : -1 (aka INVALID_SUPPL_STRM) implies search across ALL streams.
 *	histinfo   : A pointer to the repl_histinfo to be filled in. Contents might have been modified even on error return.
 * Description:
 *	If strm_idx=-1
 *	-----------------
 *		Given an input "seqno", locate the histinfo record (from ANY stream) in the instance file whose "start_seqno"
 *			corresponds to "seqno-1".
 *	If strm_idx=0
 *	----------------
 *		Given an input "seqno", locate the histinfo record (from 0th stream) in the instance file whose "start_seqno"
 *			corresponds to "seqno-1".
 *	If strm_idx=1,2,...,15
 *	-------------------------
 *		Given an input "seqno", locate the histinfo record (from "strm_index"th stream) in the instance file
 *			whose "strm_seqno" (not start_seqno) corresponds to "seqno-1".
 * Return Value:
 *	0, on success
 *	ERR_REPLINSTNOHIST, if "seqno" is NOT present in the instance file history range. There are two cases to consider here.
 *	If there was an error fetching a history record, "histinfo->histinfo_num" will be set to INVALID_HISTINFO_NUM.
 *	Otherwise, if we ran out of history records, "histinfo" will point to the 0th history record corresponding to "strm_idx".
 */
int4	repl_inst_histinfo_find_seqno(seq_num seqno, int4 strm_idx, repl_histinfo *histinfo)
{
	unix_db_info		*udi;
	int4			histnum, status;
	seq_num			cur_seqno;
#	ifdef DEBUG
	seq_num			prev_seqno;
	int4			prev_histnum;
#	endif
	repl_inst_hdr_ptr_t	inst_hdr;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->s_addrs.now_crit || jgbl.mur_rollback);
	assert(0 != seqno);
	inst_hdr = jnlpool.repl_inst_filehdr;
	assert(NULL != inst_hdr);
	assert((INVALID_SUPPL_STRM == strm_idx) || inst_hdr->is_supplementary && (0 <= strm_idx) && (MAX_SUPPL_STRMS > strm_idx));
	assert(inst_hdr->num_histinfo <= inst_hdr->num_alloc_histinfo);
	if (INVALID_SUPPL_STRM == strm_idx)
		histnum = inst_hdr->num_histinfo - 1;
	else
		histnum = inst_hdr->last_histinfo_num[strm_idx];
	assert(-1 == INVALID_HISTINFO_NUM);	/* so we can safely decrement 0 and reach -1 i.e. an invalid history number */
	DEBUG_ONLY(prev_seqno = 0;)
	do
	{
		assert(histnum < inst_hdr->num_histinfo);
		assert(INVALID_HISTINFO_NUM <= histnum);
		if (INVALID_HISTINFO_NUM == histnum)
			return ERR_REPLINSTNOHIST;
		status = repl_inst_histinfo_get(histnum, histinfo);
		if (0 != status)
		{
			assert(FALSE);
			histinfo->histinfo_num = INVALID_HISTINFO_NUM;	/* signal to caller this is an out-of-design situation */
			return ERR_REPLINSTNOHIST;
		}
		assert((INVALID_SUPPL_STRM == strm_idx) || (strm_idx == histinfo->strm_index));
		cur_seqno = (0 < strm_idx) ? histinfo->strm_seqno : histinfo->start_seqno;
		assert(cur_seqno);
		assert((0 == prev_seqno) || (prev_seqno > cur_seqno)
			|| ((INVALID_SUPPL_STRM == strm_idx) && (prev_seqno == cur_seqno)));
		DEBUG_ONLY(prev_seqno = cur_seqno;)
		if (seqno > cur_seqno)
			break;
		DEBUG_ONLY(prev_histnum = histnum;)
		histnum = (INVALID_SUPPL_STRM == strm_idx) ? (histnum - 1) : histinfo->prev_histinfo_num;
	} while (TRUE);
	return 0;
}

/* This function finds the histinfo in the local replication instance file corresponding to seqno "seqno-1".
 * It is a wrapper on top of the function "repl_inst_histinfo_find_seqno" which additionally does error checking.
 * For the case where "repl_inst_histinfo_find_seqno" returns 0 with a -1 histinfo_num, this function returns ERR_REPLINSTNOHIST.
 */
int4	repl_inst_wrapper_histinfo_find_seqno(seq_num seqno, int4 strm_idx, repl_histinfo *local_histinfo)
{
	unix_db_info	*udi;
	char		histdetail[256];
	int4		status;
	repl_histinfo	*next_histinfo;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->s_addrs.now_crit || jgbl.mur_rollback);
	assert(NULL != jnlpool.repl_inst_filehdr);	/* journal pool should be set up */
	assert((is_src_server && ((INVALID_SUPPL_STRM == strm_index) || (0 == strm_index)))
		|| (!is_src_server && ((INVALID_SUPPL_STRM == strm_index)
						|| ((0 <= strm_index) && (MAX_SUPPL_STRMS > strm_index)))));
	status = repl_inst_histinfo_find_seqno(seqno, strm_idx, local_histinfo);
	assert((0 == status) || (ERR_REPLINSTNOHIST == status)); /* the only error returned by "repl_inst_histinfo_find_seqno" */
	if (0 != status)
	{
		status = ERR_REPLINSTNOHIST;
		SPRINTF(histdetail, "seqno "INT8_FMT" "INT8_FMTX, seqno - 1, seqno - 1);
		gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTNOHIST, 4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
	} else
		assert(0 <= local_histinfo->histinfo_num);
	return status;
}

/* Description:
 *	Add a new histinfo record to the replication instance file.
 * Parameters:
 *	histinfo : A pointer to the histinfo structure to be added to the instance file.
 * Return Value:
 *	None
 * Errors:
 *	Issues ERR_REPLINSTSEQORD error if new histinfo will cause seqno to be out of order.
 */
void	repl_inst_histinfo_add(repl_histinfo *histinfo)
{
	boolean_t	is_supplementary, start_seqno_equal;
	int4		histinfo_num, strm_histinfo_num, prev_histinfo_num, status;
	int		strm_idx, idx;
	off_t		offset;
	repl_histinfo	*last_histinfo, last_histrec, *last_strm_histinfo, last_strm_histrec;
	repl_histinfo	last2_histinfo, *prev_strm_histinfo, prev_strm_histrec;
	seq_num		histinfo_strm_seqno, prev_strm_seqno;
	unix_db_info	*udi;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->s_addrs.now_crit);
	assert(jnlpool.repl_inst_filehdr->num_histinfo <= jnlpool.repl_inst_filehdr->num_alloc_histinfo);
	histinfo_num = jnlpool.repl_inst_filehdr->num_histinfo;
	assert(0 <= histinfo_num);
	strm_idx = histinfo->strm_index;
	/* Assert that the very first history record in any instance file (irrespective of whether the
	 * instance is a root primary or propagating primary) should correspond to stream-0.
	 */
	assert((0 < histinfo_num) || (0 == strm_idx));
	is_supplementary = jnlpool.repl_inst_filehdr->is_supplementary;
	assert(!is_supplementary && (0 == strm_idx) || (is_supplementary && (0 <= strm_idx) && (MAX_SUPPL_STRMS > strm_idx)));
	/* If -updateresync is specified and instance is not supplementary, then there better be NO history records */
	assert((HISTINFO_TYPE_UPDRESYNC != histinfo->history_type) || is_supplementary || (0 == histinfo_num));
	if (strm_idx && !jnlpool.jnlpool_ctl->upd_disabled)
	{	/* A non-supplementary stream history record is being written into a supplementary root primary instance.
		 * Convert the history record as appropriate. See below macro definition for more comments on the conversion.
		 */
		CONVERT_NONSUPPL2SUPPL_HISTINFO(histinfo, jnlpool.jnlpool_ctl)
	}
	if (0 < histinfo_num)
	{
		last_histinfo = &last_histrec;
		status = repl_inst_histinfo_get(histinfo_num - 1, last_histinfo);
		assert(0 == status);	/* Since histinfo_num-1 we are passing is >=0 and < num_histinfo */
		assert(jnlpool.jnlpool_ctl->last_histinfo_seqno == last_histinfo->start_seqno);
		if (histinfo->start_seqno < last_histinfo->start_seqno)
		{	/* cannot create histinfo with out-of-order start_seqno */
			rts_error(VARLSTCNT(8) ERR_REPLINSTSEQORD, 6, LEN_AND_LIT("New history record"),
				&histinfo->start_seqno, &last_histinfo->start_seqno, LEN_AND_STR(udi->fn));
		}
	}
	strm_histinfo_num = jnlpool.repl_inst_filehdr->last_histinfo_num[strm_idx];
	prev_histinfo_num = strm_histinfo_num;
	if (0 <= strm_histinfo_num)
	{
		assert(strm_histinfo_num < histinfo_num);
		if (strm_histinfo_num != (histinfo_num - 1))
		{
			last_strm_histinfo = &last_strm_histrec;
			status = repl_inst_histinfo_get(strm_histinfo_num, last_strm_histinfo);
			assert(0 == status);	/* Since the strm_histinfo_num we are passing is >=0 and < num_histinfo */
		} else
		{	/* Had read this history record just now from the instance file. Use it and avoid another read */
			last_strm_histinfo = last_histinfo;
		}
		assert(strm_idx == last_strm_histinfo->strm_index);
		/* Check if the history record to be added has the same histinfo content as the last history record
		 * already present in the instance file (in the stream of interest). This is possible in case of a secondary
		 * where the receiver was receiving journal records (from the primary) for a while, was shut down and then
		 * restarted. Same instance is sending information so no new histinfo information needed. Return right away.
		 * The only exception is if this is a supplementary instance and the new history record is an UPDATERESYNC
		 * type of record in which case it is possible the two histories have the histinfo content identical but
		 * have different start_seqnos. In this case, some updates went in between the two histories so we want
		 * to record the input history as a separate record instead of returning (since this signals the beginning
		 * of a new stream of updates).
		 */
		if ((!is_supplementary || (HISTINFO_TYPE_UPDRESYNC != histinfo->history_type))
			&& !STRCMP(last_strm_histinfo->root_primary_instname, histinfo->root_primary_instname)
			&& (last_strm_histinfo->root_primary_cycle == histinfo->root_primary_cycle)
			&& (last_strm_histinfo->creator_pid == histinfo->creator_pid)
			&& (last_strm_histinfo->created_time == histinfo->created_time))
		{
			return;
		}
		assert((histinfo->start_seqno != last_strm_histinfo->start_seqno)
				|| (histinfo->strm_seqno == last_strm_histinfo->strm_seqno)
				|| (HISTINFO_TYPE_NORESYNC == histinfo->history_type)
				|| (HISTINFO_TYPE_UPDRESYNC == histinfo->history_type));
		/* If stream seqnos match between input history and last stream specific history in the instance file,
		 * make sure the to-be-written history record skips past the last stream specific history record (as we
		 * expect a decreasing sequence of strm_seqnos in the "prev_histinfo_num" linked list of history records).
		 * The only exception is if we are a supplementary instance and this is stream # 0. In that case, only if
		 * the start_seqno is also equal, will we skip. This is because if start_seqno is not equal, the stream # 0
		 * history records identify a range of updates that happened (even if the updates happened in non-zero
		 * stream #s) and that is used by history record matching between two supplementary instances at replication
		 * connection time.
		 * The same skipping logic applies to "start_seqno" in case the instance is non-supplementary (in which case
		 * the "strm_seqno" field is 0).
		 */
		histinfo_strm_seqno = histinfo->strm_seqno;
		prev_strm_seqno = last_strm_histinfo->strm_seqno;
		if (histinfo_strm_seqno == prev_strm_seqno)
		{
			start_seqno_equal = (histinfo->start_seqno == last_strm_histinfo->start_seqno);
			if (histinfo_strm_seqno && strm_idx || start_seqno_equal)
			{
				assert(prev_histinfo_num > last_strm_histinfo->prev_histinfo_num);
				prev_histinfo_num = last_strm_histinfo->prev_histinfo_num;
			}
			if (start_seqno_equal && (strm_histinfo_num == (histinfo_num - 1)))
			{	/* Starting seqno of the last histinfo in the instance file matches the input histinfo.
				 * This means there are no journal records corresponding to the input stream in the journal
				 * files after the last histinfo (which happens to be same as the input stream) was written
				 * in the instance file. Overwrite the last histinfo with the new histinfo information before
				 * writing new journal records.
				 */
				histinfo_num--;
			}
		} else if (HISTINFO_TYPE_NORESYNC == histinfo->history_type)
		{	/* Determine the correct value of "prev_histinfo_num" */
			prev_strm_histinfo = &prev_strm_histrec;
			prev_strm_histrec = *last_strm_histinfo;
			assert(prev_strm_seqno == prev_strm_histinfo->strm_seqno);
			while (histinfo_strm_seqno <= prev_strm_seqno)
			{
				prev_histinfo_num = prev_strm_histinfo->prev_histinfo_num;
				assert(INVALID_HISTINFO_NUM != prev_histinfo_num);
				if (INVALID_HISTINFO_NUM == prev_histinfo_num)
					break;
				status = repl_inst_histinfo_get(prev_histinfo_num, prev_strm_histinfo);
				assert(0 == status); /* Since prev_histinfo_num we are passing is >=0 and < num_histinfo */
				assert(prev_strm_seqno > prev_strm_histinfo->strm_seqno);
				prev_strm_seqno = prev_strm_histinfo->strm_seqno;
			}
		}
	}
	/* Assert that the history record we are going to add is in sync with the current seqno state of the instance */
	assert(jnlpool.jnlpool_ctl->jnl_seqno == histinfo->start_seqno);
	assert(jnlpool.jnlpool_ctl->strm_seqno[histinfo->strm_index] == histinfo->strm_seqno);
	offset = REPL_INST_HISTINFO_START + (SIZEOF(repl_histinfo) * (off_t)histinfo_num);
	/* Initialize the following members of the repl_histinfo structure. Everything else should be initialized by caller.
	 *	histinfo_num
	 *	prev_histinfo_num
	 *	last_histinfo_num[]
	 */
	histinfo->histinfo_num = histinfo_num;
	histinfo->prev_histinfo_num = (HISTINFO_TYPE_UPDRESYNC == histinfo->history_type)
					? INVALID_HISTINFO_NUM : prev_histinfo_num;
	assert(histinfo->prev_histinfo_num < histinfo->histinfo_num);
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
	{
		assert((jnlpool.repl_inst_filehdr->last_histinfo_num[idx] < histinfo_num)
			|| (idx == strm_idx) && (jnlpool.repl_inst_filehdr->last_histinfo_num[idx] == histinfo_num));
		histinfo->last_histinfo_num[idx] = jnlpool.repl_inst_filehdr->last_histinfo_num[idx];
	}
	if (strm_histinfo_num == histinfo_num)
	{	/* The last history record in the instance file is going to be overwritten with another history record of
		 * the same stream. In this case, jnlpool.repl_inst_filehdr->last_histinfo_num[strm_idx] would not reflect a
		 * state of the instance file BEFORE this history record was added. So find the correct value. Thankfully
		 * the last history record (that we are about to overwrite) already has this value so copy it over.
		 */
		histinfo->last_histinfo_num[strm_idx] = last_histinfo->last_histinfo_num[strm_idx];
	}
	assert(strm_histinfo_num == jnlpool.repl_inst_filehdr->last_histinfo_num[strm_idx]);
	assert(strm_histinfo_num <= histinfo_num);
	assert(strm_histinfo_num >= prev_histinfo_num);
	assert(histinfo_num > prev_histinfo_num);
	assert((INVALID_HISTINFO_NUM == histinfo->prev_histinfo_num) || (0 <= histinfo->prev_histinfo_num));
	assert(is_supplementary || (prev_histinfo_num == (histinfo_num - 1)));
#	ifdef DEBUG
	/* Assert that the prev_histinfo_num list of history records have decreasing "start_seqno" and "strm_seqno" values.
	 * The only exception is stream # 0 for a supplementary instance as described in a previous comment in this function.
	 */
	if (INVALID_HISTINFO_NUM != histinfo->prev_histinfo_num)
	{
		assert(histinfo->prev_histinfo_num == prev_histinfo_num);
		status = repl_inst_histinfo_get(prev_histinfo_num, &last2_histinfo);
		assert(0 == status);	/* Since the strm_histinfo_num we are passing is >=0 and < num_histinfo */
		assert(strm_idx == last2_histinfo.strm_index);	/* they both better have the same stream # */
		assert(histinfo->start_seqno > last2_histinfo.start_seqno);
		assert(!histinfo->strm_seqno || (histinfo->strm_seqno > last2_histinfo.strm_seqno) || (0 == strm_idx));
	}
	/* Assert that the last_histinfo_num fields reflect a state of the instance file that does not include the about-to-be
	 * added history record. This ensures the instance file header will get restored to a valid state in case of a rollback
	 * that truncates exactly at this history record boundary.
	 */
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		assert(histinfo->last_histinfo_num[idx] < histinfo_num);
#	endif
	/* Assert that if this is not the first history record being written into the instance file
	 * it should have a valid 0th stream history record number. This is relied upon by "gtmsource_send_new_histrec"
	 */
	assert((0 == histinfo_num) || (INVALID_HISTINFO_NUM != histinfo->last_histinfo_num[0]));
	repl_inst_write(udi->fn, offset, (sm_uc_ptr_t)histinfo, SIZEOF(repl_histinfo));
	/* Update stream specific history number fields in the file header to reflect the latest history addition to this stream */
	jnlpool.repl_inst_filehdr->last_histinfo_num[strm_idx] = histinfo_num;
	/* If -updateresync history record for a non-zero stream #, then initialize strm_group_info in file header */
	if ((0 < strm_idx) && (HISTINFO_TYPE_UPDRESYNC == histinfo->history_type))
		jnlpool.repl_inst_filehdr->strm_group_info[strm_idx - 1] = histinfo->lms_group;
	histinfo_num++;
	if (jnlpool.repl_inst_filehdr->num_alloc_histinfo < histinfo_num)
		jnlpool.repl_inst_filehdr->num_alloc_histinfo = histinfo_num;
	jnlpool.repl_inst_filehdr->num_histinfo = histinfo_num;
	repl_inst_flush_filehdr();
	jnlpool.jnlpool_ctl->last_histinfo_seqno = histinfo->start_seqno;
	repl_inst_sync(udi->fn);	/* Harden the new histinfo to disk before any logical records for this arrive. */
	return;
}

/* Description:
 *	Given an input "rollback_seqno", virtually truncate all histinfo records that correspond to seqnos >= "rollback_seqno"
 *	This function also updates other fields (unrelated to histinfo truncation) in the file header
 *	to reflect a clean shutdown by MUPIP JOURNAL ROLLBACK. This function is also invoked by MUPIP BACKUP in order
 *	to ensure the backed up instance file is initialized to reflect a clean shutdown.
 * Parameters:
 *	rollback_seqno : The seqno after which all histinfo records have to be truncated.
 *			 Note: In case of a supplementary instance file, this function expects the caller to have
 *			 set "inst_hdr->strm_seqno[]" to reflect the "rollback_seqno".
 * Return Value:
 *	Sequence number (start_seqno) of the last history record in the instance file
 * Errors:
 *	Issues ERR_REPLINSTNOHIST message if the call to "repl_inst_histinfo_find_seqno" returned an error.
 */
seq_num	repl_inst_histinfo_truncate(seq_num rollback_seqno)
{
	char			histdetail[256];
	int4			status, index, num_histinfo, last_histnum;
	int			idx;
	repl_histinfo		temphistinfo, nexthistinfo, strmhistinfo;
	repl_inst_hdr_ptr_t	inst_hdr;
	unix_db_info		*udi;
	seq_num			last_histinfo_seqno = 0;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(in_backup || jgbl.mur_rollback); /* Only ROLLBACK or BACKUP calls this function */
	assert(udi->s_addrs.now_crit || jgbl.mur_rollback);
	inst_hdr = jnlpool.repl_inst_filehdr;
	assert(NULL != inst_hdr); /* Should have been set when mupip rollback invoked "mu_replpool_grab_sem" */
	num_histinfo = inst_hdr->num_histinfo;
	if (0 != num_histinfo)
	{
		status = repl_inst_histinfo_find_seqno(rollback_seqno, INVALID_SUPPL_STRM, &temphistinfo);
		if (0 != status)
		{
			assert(ERR_REPLINSTNOHIST == status);	/* the only error returned by "repl_inst_histinfo_find_seqno" */
			if ((INVALID_HISTINFO_NUM == temphistinfo.histinfo_num) || (temphistinfo.start_seqno != rollback_seqno))
			{	/* The truncation seqno is not the starting seqno of the instance file. In that case, issue
				 * a RELINSTNOHIST warning message even though rollback is going to proceed anycase.
				 */
				assert(FALSE);
				NON_GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%llx]", rollback_seqno - 1));
				GTM64_ONLY(SPRINTF(histdetail, "seqno [0x%lx]", rollback_seqno - 1));
				gtm_putmsg(VARLSTCNT(6) MAKE_MSG_WARNING(ERR_REPLINSTNOHIST), 4,
							LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
			}
			index = -1;
			/* Since we are rolling back all history records in the instance file,
			 * clear all of "strm_group_info[]" and "last_histinfo_num[]" arrays.
			 * The following logic is similar to that in "repl_inst_create" to initialize the above 2 fields.
			 * Note that we keep "jnl_seqno" and "strm_seqno" set to whatever value it came in with (as opposed
			 * to setting it to 0). This is different from what is done in "repl_inst_create" because we want
			 * to keep these set to a non-zero value if possible (see detailed comment below where "jnl_seqno"
			 * gets set). Keeping "jnl_seqno" at a non-zero value necessitates keeping "strm_seqno" at a non-zero
			 * value as well in order to avoid REPLINSTDBSTRM errors at source server startup.
			 */
			assert(MAX_SUPPL_STRMS == ARRAYSIZE(inst_hdr->last_histinfo_num));
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				inst_hdr->last_histinfo_num[idx] = INVALID_HISTINFO_NUM;
			if (inst_hdr->is_supplementary)
			{
				assert(MAX_SUPPL_STRMS == ARRAYSIZE(inst_hdr->strm_seqno));
				assert(SIZEOF(seq_num) == SIZEOF(inst_hdr->strm_seqno[0]));
				assert((MAX_SUPPL_STRMS - 1) == ARRAYSIZE(inst_hdr->strm_group_info));
				assert(SIZEOF(repl_inst_uuid) == SIZEOF(inst_hdr->strm_group_info[0]));
				/* Keep the strm_seqno 0 for those streams which this instance has never used/communicated. For all
				 * other stream#, set the strm_seqno to 1 if the current value of strm_seqno is 0. If the current
				 * value of strm_seqno is non-zero, let it stay as it is (see comment above about strm_seqno).
				 * This way, if this instance reconnects after the ROLLBACK to the same instance it was
				 * communicating before, we avoid issuing REPLINSTNOHIST thereby making it user-friendly.
				 * Note: The LMS group info for stream# "i" is found in strm_group_info[i - 1] (used below)
				 */
				for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				{
					if ((idx == 0) || (IS_REPL_INST_UUID_NON_NULL(inst_hdr->strm_group_info[idx - 1])))
					{
						if (0 == inst_hdr->strm_seqno[idx])
							inst_hdr->strm_seqno[idx] = 1;
					}
#					ifdef DEBUG
					else
						assert(0 == inst_hdr->strm_seqno[idx]);
#					endif
				}
				/* Leave the LMS group information as-is in the instance file header. By doing so, we avoid cases
				 * where receiver server continuing after the rollback issues an INSUNKNOWN error. While this is
				 * a valid error, we try to make it as user-friendly as possible.
				 */
			}
		} else
		{
			index = temphistinfo.histinfo_num;
			assert(temphistinfo.start_seqno < rollback_seqno);
			assert(0 <= index);
			assert(index <= (num_histinfo - 1));
			last_histinfo_seqno = temphistinfo.start_seqno;
			if (index < (num_histinfo - 1))
			{
				status = repl_inst_histinfo_get(index + 1, &nexthistinfo);
				assert(0 == status);	/* Since the histinfo_num we are passing is >=0 and <= num_histinfo */
				assert(nexthistinfo.start_seqno >= rollback_seqno);
				assert(nexthistinfo.histinfo_num == (index + 1));
				/* Copy over information from this history record back to the instance file header */
				assert(SIZEOF(inst_hdr->last_histinfo_num) == SIZEOF(nexthistinfo.last_histinfo_num));
				memcpy(inst_hdr->last_histinfo_num, nexthistinfo.last_histinfo_num,
					SIZEOF(nexthistinfo.last_histinfo_num));
				if (inst_hdr->is_supplementary)
				{
					/* inst_hdr->strm_seqno[] is already set by caller */
					assert((MAX_SUPPL_STRMS - 1) == ARRAYSIZE(inst_hdr->strm_group_info));
					for (idx = 0; idx < (MAX_SUPPL_STRMS - 1); idx++)
					{
						last_histnum = nexthistinfo.last_histinfo_num[idx + 1];
						assert(INVALID_HISTINFO_NUM <= last_histnum);
						assert(last_histnum < nexthistinfo.histinfo_num);
						if (INVALID_HISTINFO_NUM != last_histnum)
						{
							status = repl_inst_histinfo_get(last_histnum, &strmhistinfo);
							assert(0 == status);
							assert(strmhistinfo.histinfo_num == last_histnum);
							assert(strmhistinfo.start_seqno < rollback_seqno);
							assert(strmhistinfo.strm_index);
							assert(MAX_SUPPL_STRMS > strmhistinfo.strm_index);
							assert(IS_REPL_INST_UUID_NON_NULL(strmhistinfo.lms_group));
							inst_hdr->strm_group_info[idx] = strmhistinfo.lms_group;
						} else if (IS_REPL_INST_UUID_NON_NULL(inst_hdr->strm_group_info[idx]))
						{	/* stream# (idx + 1) has a non-zero UUID information in the file header
							 * but all the history records corresponding to this stream are now
							 * truncated. This also implies that strm_seqno of this stream is reset
							 * to zero by ROLLBACK. To avoid REPLINSTNOHIST next time a communication
							 * happens with the instance corresponding to stream# idx + 1, set the
							 * strm_seqno to 1.
							 * Note: The LMS group info for stream-i is found in strm_group_info[i - 1]
							 */
							inst_hdr->strm_seqno[idx + 1] = 1;
							/* Also, leave the LMS group information for stream# idx + 1 as-is in the
							 * instance file header By doing so, we avoid cases where receiver server
							 * continuing after the rollback issues an INSUNKNOWN error. While this is
							 * a valid error, we try to mae it as user-friendly as possible.
							 */
						}
					}
				}
			}
			/* else index == "num_histinfo - 1" so no changes needed to "last_histinfo_num[]"
			 *	or "strm_seqno[]" or "strm_group_info[]" arrays.
			 */
		}
		index++;
		assert((index == inst_hdr->num_histinfo)
			|| ((inst_hdr->num_histinfo >= 0) && (inst_hdr->num_alloc_histinfo > index)));
		inst_hdr->num_histinfo = index;
	}
	/* Reset "jnl_seqno" to the rollback seqno so future REPLINSTDBMATCH errors are avoided in "gtmsource_seqno_init".
	 * Note that it is possible inst_hdr->num_histinfo is 0 at this point (i.e. no history records). In that case,
	 * repl_inst_create sets the "jnl_seqno" to 0 whereas we might set it here to a potentially non-zero value.
	 * That is because repl_inst_create does not go through the database and get the max of the reg_seqnos to figure
	 * out the instance jnl_seqno. Hence it sets it to a value of 0 indicating the source server that starts up the
	 * instance to fill it in with a non-zero value. On the other hand, rollback or backup (both of which can call
	 * this function "repl_inst_histinfo_truncate") know exactly what the instance seqno is and so can safely set the
	 * "jnl_seqno" to a non-zero value even though there are no history records. Setting it to a non-zero value whenever
	 * possible is useful for example when we ship a backup of a freshly created live non-supplementary instance (with
	 * jnl_seqno of 1) to be used as input to the -updateresync qualifier of a receiver startup on a supplementary
	 * instance. In this case, if the backup had a jnl_seqno of 0, the startup would fail. But since it has a non-zero
	 * "jnl_seqno" (even though there are no history records), the initial handshake between the non-supplementary and
	 * supplementary instances is possible (they avoid history record exchanges due to jnl_seqno == 1). A zero jnl_seqno
	 * would have resulted in a UPDSYNCINSTFILE error in the initial handshake.
	 */
	inst_hdr->jnl_seqno = rollback_seqno;
	/* Reset sem/shm ids to reflect a clean shutdown so future REPLREQRUNDOWN errors are avoided at "jnlpool_init" time */
	if (!jgbl.mur_rollback)
	{	/* Reset semid/sem_ctime fields in the instance file header. */
		/* Reset "crash" to FALSE so future REPLREQROLLBACK errors are avoided at "jnlpool_init" time */
		inst_hdr->crash = FALSE;
		inst_hdr->jnlpool_semid = INVALID_SEMID;
		inst_hdr->jnlpool_shmid = INVALID_SHMID;
		inst_hdr->jnlpool_semid_ctime = 0;
		inst_hdr->jnlpool_shmid_ctime = 0;
		inst_hdr->recvpool_semid = INVALID_SEMID;	/* Just in case it is not already reset */
		inst_hdr->recvpool_shmid = INVALID_SHMID;	/* Just in case it is not already reset */
		inst_hdr->recvpool_semid_ctime = 0;
		inst_hdr->recvpool_shmid_ctime = 0;
	} /* else for rollback, we reset the IPC fields in mu_replpool_release_sem() and crash in mur_close_files */
	/* Flush all file header changes in jnlpool.repl_inst_filehdr to disk */
	repl_inst_flush_filehdr();
	assert((0 == inst_hdr->num_histinfo) || (0 < last_histinfo_seqno));
	return last_histinfo_seqno;
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
	/* We could come here from several paths. If journal pool exists, we would have done a grab_lock. This covers most of the
	 * cases. If the journal pool doesn't exist, then we could come here from one of the following places
	 *
	 * ROLLBACK (online/noonline):
	 *   We already hold standalone access on the journal pool and if the journal pool exists, we also hold the journal pool
	 *   lock
	 *
	 * MUPIP RUNDOWN -> mu_rndwn_repl_instance:
	 *   We hold the ftok on the instance file and have already made sure that no one else is attached to the journal pool. Even
	 *   though we don't hold the access control on the journal pool, no one else can startup at this point because they need
	 *   the ftok for which they will have to wait.
	 *
	 * gtmsource_shutdown -> repl_inst_jnlpool_reset:
	 *   We hold the ftok on the instance file and have already made sure that no one else is attached to the journal pool. Even
	 *   though we don't hold the access control on the journal pool, no one else can startup at this point because they need
	 *   the ftok for which they will have to wait.
	 *
	 * gtmrecv_shutdown -> repl_inst_recvpool_reset:
	 *   Same as above.
	 * So, in all cases, we are guaranteed that the following code is mutually exclusive (which is what we want).
	 */
	assert(udi->s_addrs.now_crit || udi->grabbed_ftok_sem || (jgbl.mur_rollback && holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]));
	if (jnlpool.jnlpool_dummy_reg->open)
		COPY_JCTL_STRMSEQNO_TO_INSTHDR_IF_NEEDED; /* Keep the file header copy of "strm_seqno" uptodate with jnlpool_ctl */
	assert((NULL == jnlpool.jnlpool_ctl) || udi->s_addrs.now_crit);
	assert(NULL != jnlpool.repl_inst_filehdr);
	/* flush the instance file header */
	repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)jnlpool.repl_inst_filehdr, REPL_INST_HDR_SIZE);
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
	assert(!jgbl.mur_rollback); /* Rollback should never reach here */
	assert(udi->s_addrs.now_crit);
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
void	repl_inst_flush_jnlpool(boolean_t reset_replpool_fields, boolean_t reset_crash)
{
	unix_db_info		*udi;
	int4			index;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;

	assert(NULL != jnlpool.jnlpool_dummy_reg);
	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	/* This function should be invoked only if the caller determines this is last process attached to the journal pool.
	 * Since the ftok lock on the instance file is already held, no other process will be allowed to attach to the
	 * journal pool and hence this is the only process having access to the journal pool during this function. The only
	 * exception is if it is invoked from mur_open_files for Online Rollback. But, in that case Online Rollback will be
	 * holding the access control. Any process calling this function, needs the access control semaphore and hence will
	 * wait for Online Rollback to complete.
	 */
	assert(udi->grabbed_ftok_sem || (jgbl.onlnrlbk && udi->s_addrs.now_crit));
	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	assert(NULL != jnlpool.gtmsource_local_array);
	assert(NULL != jnlpool.gtmsrc_lcl_array);
	assert(NULL != jnlpool.repl_inst_filehdr);
	assert(NULL != jnlpool.jnlpool_ctl);
	assert((sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array == (sm_uc_ptr_t)jnlpool.repl_inst_filehdr + REPL_INST_HDR_SIZE);
	/* Reset the instance file header fields (if needed) before flushing and removing the journal pool shared memory */
	if (reset_crash)
		jnlpool.repl_inst_filehdr->crash = FALSE;
	if (!jgbl.onlnrlbk)
	{
		if (reset_replpool_fields)
		{
			jnlpool.repl_inst_filehdr->jnlpool_semid = INVALID_SEMID;
			jnlpool.repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
			jnlpool.repl_inst_filehdr->recvpool_semid = INVALID_SEMID;	/* Just in case it is not already reset */
			jnlpool.repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;	/* Just in case it is not already reset */
		}
	}
	/* If the source server that created the journal pool died before it was completely initialized in "gtmsource_seqno_init"
	 * do not copy seqnos from the journal pool into the instance file header. Instead keep the instance file header unchanged.
	 */
	if (jnlpool.jnlpool_ctl->pool_initialized)
	{
		assert(jnlpool.jnlpool_ctl->start_jnl_seqno);
		assert(jnlpool.jnlpool_ctl->jnl_seqno);
		jnlpool.repl_inst_filehdr->jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
		COPY_JCTL_STRMSEQNO_TO_INSTHDR_IF_NEEDED; /* Keep the file header copy of "strm_seqno" uptodate with jnlpool_ctl */
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
 * last histinfo record in the instance file and comparing the "root_primary_instname" field there with this instance name.
 * If they are the same, it means the last histinfo was generated by this instance and hence was a root primary then. This
 * function will only be invoked by a propagating primary instance (RECEIVER SERVER or ROLLBACK -FETCHRESYNC).
 *
 * It returns TRUE only if the instance file header field "was_rootprimary" is TRUE and if the last histinfo record was generated
 * by this instance. It returns FALSE otherwise.
 */
boolean_t	repl_inst_was_rootprimary(void)
{
	int4		histinfo_num, status;
	repl_histinfo	temphistinfo, *last_histinfo = &temphistinfo;
	boolean_t	was_rootprimary, was_crit = FALSE;
	sgmnt_addrs	*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL != jnlpool.jnlpool_ctl)
	{	/* If the journal pool is available (indicated by NULL != jnlpool_ctl), we expect jnlpool_dummy_reg to be open.
		 * The only exception is online rollback which doesn't do a jnlpool_init thereby leaving jnlpool_dummy_reg->open
		 * to be FALSE. Assert accordingly.
		 */
		assert(((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open)
				|| jgbl.onlnrlbk || (jgbl.mur_rollback && ANTICIPATORY_FREEZE_AVAILABLE));
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		ASSERT_VALID_JNLPOOL(csa);
		assert(csa->now_crit);
	} else
		assert(jgbl.mur_rollback); /* ROLLBACK (holding access control lock) can come here without journal pool */
	/* If this is a supplementary instance, look at the last history record corresponding to the 0th stream index.
	 * If not, look at the last history record. This is okay since there is no multiple streams in this case.
	 */
	histinfo_num = (!jnlpool.repl_inst_filehdr->is_supplementary) ? (jnlpool.repl_inst_filehdr->num_histinfo - 1)
									: jnlpool.repl_inst_filehdr->last_histinfo_num[0];
	was_rootprimary = jnlpool.repl_inst_filehdr->was_rootprimary;
	assert(histinfo_num < jnlpool.repl_inst_filehdr->num_alloc_histinfo);
	if (was_rootprimary && (0 <= histinfo_num))
	{
		status = repl_inst_histinfo_get(histinfo_num, last_histinfo);
		assert(0 == status);	/* Since the histinfo_num we are passing is >=0 and < num_histinfo */
		was_rootprimary = !STRCMP(last_histinfo->root_primary_instname, jnlpool.repl_inst_filehdr->inst_info.this_instname);
	} else
		was_rootprimary = FALSE;
	return was_rootprimary;
}

/* This function resets "zqgblmod_seqno" and "zqgblmod_tn" in all replicated database file headers to 0.
 * This shares a lot of its code with the function "gtmsource_update_zqgblmod_seqno_and_tn".
 * Any changes there might need to be reflected here.
 */
int4	repl_inst_reset_zqgblmod_seqno_and_tn(void)
{
	gd_region		*reg, *reg_top;
	int			ret;
	boolean_t		all_files_open;
	sgmnt_addrs		*repl_csa;

	ret = SS_NORMAL; /* assume success */
	/* source server calls this from gtmsource_losttncomplete which always holds the journal pool access control semaphore
	 * Assert this.
	 */
	assert(is_rcvr_server || holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	if (0 == jnlpool.jnlpool_ctl->max_zqgblmod_seqno)
	{	/* Already reset to 0 by a previous call to this function. No need to do it again. */
		return ret;
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
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
	{
		assert(reg->open);
		TP_CHANGE_REG(reg);
		if (!REPL_ALLOWED(cs_data))
			continue;
		/* csa->hdr->zqgblmod_seqno is modified by the source server and an online rollback (both of these hold the
		 * database crit while doing so). It is also read by fileheader_sync() which does so while holding crit.
		 * To avoid the latter from reading an inconsistent value (i.e neither the pre-update nor the post-update
		 * value, which is possible if the 8-byte operation is not atomic but a sequence of two 4-byte operations
		 * AND if the pre-update and post-update value differ in their most significant 4-bytes) we grab_crit. We
		 * could have used QWCHANGE_IS_READER_CONSISTENT macro (which checks for most significant 4-byte difference)
		 * instead to determine if it is really necessary to grab crit. But, since the update to zqgblmod_seqno is a
		 * rare operation, we decided to play it safe.
		 */
		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(reg);
		if (cs_addrs->onln_rlbk_cycle != cs_addrs->nl->onln_rlbk_cycle)
		{	/* concurrent online rollback */
			assert(is_rcvr_server);
			SYNC_ONLN_RLBK_CYCLES;
			rel_crit(reg);
			ret = -1; /* failure */
			break;
		}
		cs_addrs->hdr->zqgblmod_seqno = (seq_num)0;
		cs_addrs->hdr->zqgblmod_tn = (trans_num)0;
		rel_crit(reg);
	}
	assert((SS_NORMAL == ret) || (reg < reg_top));
	if (reg >= reg_top)
	{
		assert(!repl_csa->hold_onto_crit); /* so we can do unconditional grab_lock and rel_lock */
		/* Since the source server holds the access control at this point, a concurrent online rollback is NOT possible.
		 * But, if we are here from receiver code, then we cannot guarantee this. So, get the journal pool lock and if
		 * an online rollback is detected, return without resetting max_zqgblmod_seqno. The caller knows to take appropriate
		 * action (on seeing -1 as the return code).
		 */
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
		if (repl_csa->onln_rlbk_cycle != jnlpool.jnlpool_ctl->onln_rlbk_cycle)
		{
			assert(is_rcvr_server);
			SYNC_ONLN_RLBK_CYCLES;
			rel_lock(jnlpool.jnlpool_dummy_reg);
			ret = -1; /* failure */
		} else
		{
			jnlpool.jnlpool_ctl->max_zqgblmod_seqno = 0;
			rel_lock(jnlpool.jnlpool_dummy_reg);
		}
	}
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
	{	/* Rundown all databases that we opened as we dont need them anymore. This is not done in the previous
		 * loop as it has to wait until the ftok semaphore of the instance file has been released as otherwise
		 * an assert in gds_rundown will fail as it tries to get the ftok semaphore of the database while holding
		 * another ftok semaphore already.
		 */
		assert(reg->open);
		TP_CHANGE_REG(reg);
		assert(!cs_addrs->now_crit);
		UNIX_ONLY(ret |=) gds_rundown();
	}
	assert(!repl_csa->now_crit);
	return ret;
}
