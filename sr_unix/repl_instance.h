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

#ifndef REPL_INSTANCE_INCLUDED
#define REPL_INSTANCE_INCLUDED

/* MAJOR format of the instance file; Any change to this means recreating instance file.
 * This needs to be changed whenever the layout of the instance file (file header and/or contents) changes in a major way.
 */
#define GDS_REPL_INST_LABEL 		"GDSRPLUNX05"
#define GDS_REPL_INST_LABEL_SZ		12

/* The current MINOR version. Changes to this would mean an on-the-fly upgrade of the instance file.
 * This needs to be changed whenever the changes to the file header layout of the instance file are small enough
 * that it is possible to do an online upgrade of the file header.
 */
#define GDS_REPL_INST_MINOR_LABEL 	1	/* can go upto 255 at which point the major version has to change */

/* Replication Instance file format
 *
 *	[size =  512 bytes] File-Header structure "repl_inst_hdr" (size 512 bytes)
 *	[size = 1024 bytes] Array of 16 "gtmsrc_lcl" structures (each size 64 bytes)
 *	[size = 64*n bytes] Variable-length Array of structure "repl_histinfo" (each size 64 bytes)
 *		Each "repl_histinfo" defines the beginning of a range of seqnos (identified by the "start_seqno" field)
 *		that was received from a particular invocation of a root primary instance. The range ends with the
 *		"start_seqno" field of the next histinfo record.
 * Note: "repl_histinfo" structure is defined in mdef.h (not in this file).
 *
 * An uptodate copy of the fixed length section of the instance file (file header and array of gtmsrc_lcl structures)
 * is maintained in the journal pool. The file on disk is not as uptodate but periodically flushed.
 */

#define DEF_INST_FN "mumps.repl"

typedef struct repl_inst_hdr_struct
{
	unsigned char	label[GDS_REPL_INST_LABEL_SZ];	/* format of instance file. initialized to GDS_REPL_INST_LABEL */
	unsigned char	replinst_minorver;		/* minor version of the GT.M release that last touched this file */
	unsigned char	is_little_endian;		/* TRUE if little-endian, FALSE if big-endian */
	unsigned char	is_64bit;			/* TRUE if created by 64-bit GT.M, FALSE if created by 32-bit GT.M */
	unsigned char	filler_16[1];
	int4		jnlpool_semid;			/* id of IPC_PRIVATE semaphore created for jnlpool */
	int4		jnlpool_shmid;			/* id of IPC_PRIVATE shared memory created for jnlpool */
	int4		recvpool_semid;			/* id of IPC_PRIVATE semaphore created for recvpool */
	int4		recvpool_shmid;			/* id of IPC_PRIVATE semaphore created for recvpool */
	/* All time fields have a NON_GTM64_ONLY filler so all fields in the file header start
	 * at the same offset irrespective of whether time_t is 4-bytes or 8-bytes.
	 */
	time_t		jnlpool_semid_ctime;		/* creation time of IPC_PRIVATE semaphore for jnlpool */
	NON_GTM64_ONLY(int4	filler8bytealign_1;)
	time_t		jnlpool_shmid_ctime;		/* creation time of IPC_PRIVATE shared memory for jnlpool */
	NON_GTM64_ONLY(int4	filler8bytealign_2;)
	time_t		recvpool_semid_ctime;		/* creation time of IPC_PRIVATE semaphore for recvpool */
	NON_GTM64_ONLY(int4	filler8bytealign_3;)
	time_t		recvpool_shmid_ctime;		/* creation time of IPC_PRIVATE shared memory for recvpool */
	NON_GTM64_ONLY(int4	filler8bytealign_4;)
	repl_inst_uuid	inst_info;		/* Initialized at instance file creation time */
	repl_inst_uuid	lms_group_info;		/* Set to NULL at instance file creation time;
						 * Set to a non-NULL value ONLY IF current value is NULL and
						 *	a) the instance is brought up as a rootprimary.
						 *		In this case, the value is inherited from the "inst_info" field
						 *		of the current instance file
						 *	OR
						 *	b) the instance is brought up as a propagating primary.
						 *		In this case, the value is inherited from the "lms_group_info"
						 *		field of the instance file from the root primary instance
						 *		that this instance connects to directly or indirectly (through
						 *		a sequence of propagating primary instances in between).
						 */
	seq_num		jnl_seqno;		/* Holds the current seqno of the instance when the journal pool was last shutdown.
						 * Compared with db reg_seqno at open time to check if both are in sync.
						 */
	uint4		root_primary_cycle;	/* Incremented every time this instance is brought up as root primary */
	int4		num_histinfo;		/* Number of histinfo records currently USED in this file. Usually incremented as
						 * new histinfo records get added to the file. Usually maintained in tandem with
						 * "num_alloc_histinfo". When MUPIP ROLLBACK virtually truncates the histinfo
						 * history it only updates this field to reflect the decrement.
						 */
	int4		num_alloc_histinfo;	/* Actual number of histinfo record slots ALLOCATED in this file. Incremented
						 * whenever a new histinfo record is allocated at the end of the instance file.
						 * Not incremented if a new histinfo record reuses virtually truncated space
						 * (possible by MUPIP ROLLBACK). This field is never decremented.
						 */
	boolean_t	crash;			/* This is set to TRUE if the journal pool exists and set to FALSE whenever
						 * it is cleanly shutdown. Used to identify abnormal shutdowns and require rollback.
						 */
	boolean_t	was_rootprimary;	/* Set to TRUE when this instance starts up as a root primary. Set to FALSE when
						 * this instance is brought up as a propagating primary and the receiver server
						 * connects to the new primary source server successfully.
						 */
	boolean_t	is_supplementary;	/* Whether this instance is a supplementary instance or not */
	int4		last_histinfo_num[MAX_SUPPL_STRMS];	/* Maintained whether "is_supplementary" is TRUE or FALSE.
								 * the slot number corresponding to the most recent history record
							 	 * written for each stream */
	seq_num		strm_seqno[MAX_SUPPL_STRMS];		/* Maintained only if "is_supplementary" is TRUE.
								 * The current jnl seqno of each stream when the jnl pool was last
								 * shutdown. Used to initialize similarly-named field in the
								 * jnlpool_ctl structure at jnlpool_init time. Not as frequently
								 * updated/maintained as the jnlpool_ctl->strm_seqno[] field */
	repl_inst_uuid	strm_group_info[MAX_SUPPL_STRMS - 1];	/* Maintained only if "is_supplementary" is TRUE.
								 * The "lms_group_info" field of the non-supplementary source
								 * that connects to this supplementary root primary instance.
								 * Initialized by a -UPDATERESYNC= receiver server startup on a
								 * supplementary root primary instance. Used by the receiver server
								 * on the supplementary instance when it connects to a source
								 * server from the non-supplementary instance. Note: We dont need
								 * this for the 0th stream as its group info is the "lms_group_info"
								 * member of the instance file header. Hence the MAX_SUPPL_STRMS-1.
								 */
	boolean_t	file_corrupt;		/* Set to TRUE by online rollback at start up. Set to FALSE when online rollback
						 * completes successfully.
						 */
	unsigned char   filler_1024[52];
} repl_inst_hdr;

/* Any changes to the following structure might have to be reflected in "gtmsource_local_struct" structure in gtmsource.h as well.
 * All fields here have a corresponding parallel field in the latter structure.
 */
typedef struct gtmsrc_lcl_struct
{
	unsigned char	secondary_instname[MAX_INSTNAME_LEN];	/* Secondary instance corresponding to this structure */
	seq_num		resync_seqno;				/* Next seqno to be sent from this primary instance.
								 * Maintained in parallel with "read_jnl_seqno" in the corresponding
								 * gtmsource_local structure in the journal pool.
								 */
	seq_num		connect_jnl_seqno;			/* jnl_seqno of the instance when last connection with secondary
								 * was made. Used to determine least recently used slot for reuse */
	unsigned char	filler_64[32];				/* For future expansion */
} gtmsrc_lcl;

#define	COPY_GTMSOURCELOCAL_TO_GTMSRCLCL(sourcelocal, srclcl)							\
{														\
	memcpy(&(srclcl)->secondary_instname[0], &(sourcelocal)->secondary_instname[0], MAX_INSTNAME_LEN - 1);	\
	(srclcl)->resync_seqno = (sourcelocal)->read_jnl_seqno;							\
	(srclcl)->connect_jnl_seqno = (sourcelocal)->connect_jnl_seqno;						\
}

#define	COPY_GTMSRCLCL_TO_GTMSOURCELOCAL(srclcl, sourcelocal)							\
{														\
	memcpy(&(sourcelocal)->secondary_instname[0], &(srclcl)->secondary_instname[0], MAX_INSTNAME_LEN - 1);	\
	(sourcelocal)->read_jnl_seqno = (srclcl)->resync_seqno;							\
	(sourcelocal)->connect_jnl_seqno = (srclcl)->connect_jnl_seqno;						\
}

#define	COPY_JCTL_STRMSEQNO_TO_INSTHDR_IF_NEEDED								\
{														\
	int				idx;									\
	GBLREF	jnlpool_addrs		jnlpool;	/* used by the below macro */				\
														\
	/* Keep the file header copy of "strm_seqno" uptodate with jnlpool_ctl */				\
	if (jnlpool.repl_inst_filehdr->is_supplementary)							\
	{													\
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)							\
			jnlpool.repl_inst_filehdr->strm_seqno[idx] = jnlpool.jnlpool_ctl->strm_seqno[idx];	\
	}													\
}

/* The below assert ensures that on a supplementary instance, an update to any stream only occurs if there is a history record
 * corresponding to that stream in the instance file. If this is not the case, those history-less updates will have no way of
 * being replicated (propagated downstream) from this instance as there is no history record to identify the update originator.
 */
#define	ASSERT_INST_FILE_HDR_HAS_HISTREC_FOR_STRM(STRM_IDX)					\
{												\
	GBLREF	jnlpool_addrs		jnlpool;	/* used by the below macro */		\
												\
	assert(INVALID_HISTINFO_NUM != jnlpool.repl_inst_filehdr->last_histinfo_num[STRM_IDX]);	\
}

#define	OK_TO_LOG_FALSE		FALSE
#define	OK_TO_LOG_TRUE		TRUE

#define GET_INSTFILE_NAME(sendmsg, err_act)									\
{														\
	if ((SS_NORMAL == (status = TRANS_LOG_NAME(&log_nam, &trans_name, temp_inst_fn, SIZEOF(temp_inst_fn),	\
							sendmsg)))						\
		&& (0 != trans_name.len))									\
	{													\
		temp_inst_fn[trans_name.len] = '\0';								\
		if (!get_full_path(trans_name.addr, trans_name.len, fn, fn_len, bufsize, &ustatus) && err_act)	\
		{												\
			gtm_putmsg(VARLSTCNT(9) ERR_REPLINSTACC, 2, trans_name.len, trans_name.addr,		\
				ERR_TEXT, 2, RTS_ERROR_LITERAL("full path could not be found"), ustatus);	\
		} else												\
			ret = TRUE;										\
	}													\
}

typedef enum {
	return_on_error,
	issue_rts_error,
	issue_gtm_putmsg
} instname_act;

boolean_t	repl_inst_get_name(char *, unsigned int *, unsigned int, instname_act error_action);
void		repl_inst_create(void);
void		repl_inst_edit(void);
void		repl_inst_read(char *fn, off_t offset, sm_uc_ptr_t buff, size_t buflen);
void		repl_inst_write(char *fn, off_t offset, sm_uc_ptr_t buff, size_t buflen);
void		repl_inst_sync(char *fn);
void		repl_inst_jnlpool_reset(void);
void		repl_inst_recvpool_reset(void);
void		repl_inst_ftok_sem_lock(void);
void		repl_inst_ftok_sem_release(void);
int4		repl_inst_histinfo_get(int4 index, repl_histinfo *histinfo);
int4		repl_inst_histinfo_find_seqno(seq_num seqno, int4 strm_idx, repl_histinfo *histinfo);
int4		repl_inst_wrapper_histinfo_find_seqno(seq_num seqno, int4 strm_idx, repl_histinfo *local_histinfo);
void		repl_inst_histinfo_add(repl_histinfo *histinfo);
seq_num		repl_inst_histinfo_truncate(seq_num rollback_seqno);
void		repl_inst_flush_filehdr(void);
void		repl_inst_flush_gtmsrc_lcl(void);
void		repl_inst_flush_jnlpool(boolean_t reset_recvpool_fields, boolean_t reset_crash);
boolean_t	repl_inst_was_rootprimary(void);
int4		repl_inst_reset_zqgblmod_seqno_and_tn(void);

boolean_t	gtmsource_get_cmp_info(int4 *repl_zlib_cmp_level_ptr);
void            repl_cmp_solve_src_timeout(void);
void            repl_cmp_solve_rcv_timeout(void);
boolean_t	gtmsource_get_instance_info(boolean_t *secondary_was_rootprimary, seq_num *strm_jnl_seqno);
boolean_t	gtmsource_get_remote_histinfo(seq_num seqno, repl_histinfo *histinfo);
boolean_t	gtmsource_check_remote_strm_histinfo(seq_num seqno, boolean_t *rollback_first);
void		gtmsource_histinfo_get(int4 index, repl_histinfo *histinfo);
boolean_t	gtmsource_is_histinfo_identical(repl_histinfo *remote_histinfo, repl_histinfo *local_histinfo,
										seq_num jnl_seqno, boolean_t ok_to_log);
seq_num		gtmsource_find_resync_seqno(repl_histinfo *local_histinfo, repl_histinfo *remote_histinfo);
void		gtmsource_set_next_histinfo_seqno(boolean_t detect_new_histinfo);

#endif /* REPL_INSTANCE_INCLUDED */
