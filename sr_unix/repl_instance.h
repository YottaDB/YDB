/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#define GDS_REPL_INST_LABEL 		"GDSRPLUNX04"
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
 *	[size = 64*n bytes] Variable-length Array of structure "repl_triple" (each size 64 bytes)
 *		Each "repl_triple" defines the beginning of a range of seqnos (identified by the "start_seqno" field)
 *		that was received from a particular invocation of a root primary instance. The range ends with the
 *		"start_seqno" field of the next triple instance.
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
	time_t		created_time;			/* Time when this instance file was created */
	NON_GTM64_ONLY(int4	filler8bytealign_5;)
	unsigned char	created_nodename[MAX_NODENAME_LEN];	/* Nodename on which instance file was created */
	unsigned char	this_instname[MAX_INSTNAME_LEN];/* Instance name that this file corresponds to */
	seq_num		jnl_seqno;		/* Holds the current seqno of the instance when the journal pool was last shutdown.
						 * Compared with db reg_seqno at open time to check if both are in sync.
						 */
	uint4		root_primary_cycle;	/* Incremented every time this instance is brought up as root primary */
	int4		num_triples;		/* Number of triples currently USED in this file. Usually incremented as new
						 * triples get added to the file. Usually maintained in tandem with
						 * "num_alloc_triples". When MUPIP ROLLBACK virtually truncates the triple history
						 * it only updates this field to reflect the decrement.
						 */
	int4		num_alloc_triples;	/* Actual number of triples ALLOCATED in this file. Incremented whenever a new
						 * triple is allocated at the end of the instance file. Not incremented if
						 * a new triple reuses virtually truncated space (possible by MUPIP ROLLBACK).
						 * This field is never decremented.
						 */
	boolean_t	crash;			/* This is set to TRUE if the journal pool exists and set to FALSE whenever
						 * it is cleanly shutdown. Used to identify abnormal shutdowns and require rollback.
						 */
	boolean_t	was_rootprimary;	/* Set to TRUE when this instance starts up as a root primary. Set to FALSE when
						 * this instance is brought up as a propagating primary and the receiver server
						 * connects to the new primary source server successfully.
						 */
	unsigned char   filler_512[380];
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
int4		repl_inst_triple_get(int4 index, repl_triple *triple);
int4		repl_inst_triple_find_seqno(seq_num seqno, repl_triple *triple, int4 *index);
int4		repl_inst_wrapper_triple_find_seqno(seq_num seqno, repl_triple *local_triple, int4 *local_triple_num);
void		repl_inst_triple_add(repl_triple *triple);
void		repl_inst_triple_truncate(seq_num rollback_seqno);
void		repl_inst_flush_filehdr(void);
void		repl_inst_flush_gtmsrc_lcl(void);
void		repl_inst_flush_jnlpool(boolean_t reset_recvpool_fields);
boolean_t	repl_inst_was_rootprimary(void);
void		repl_inst_reset_zqgblmod_seqno_and_tn(void);

boolean_t	gtmsource_get_cmp_info(int4 *repl_zlib_cmp_level_ptr);
void            repl_cmp_solve_src_timeout(void);
void            repl_cmp_solve_rcv_timeout(void);
boolean_t	gtmsource_get_instance_info(boolean_t *secondary_was_rootprimary);
boolean_t	gtmsource_get_triple_info(seq_num seqno, repl_triple *triple, int4 *triple_num);
void		gtmsource_triple_get(int4 index, repl_triple *triple);
boolean_t	gtmsource_is_triple_identical(repl_triple *remote_triple, repl_triple *local_triple, seq_num jnl_seqno);
seq_num		gtmsource_find_resync_seqno(
			repl_triple	*local_triple,
			int4 		local_triple_num,
			repl_triple	*remote_triple,
			int4		remote_triple_num);
void		gtmsource_set_next_triple_seqno(boolean_t detect_new_triple);

#endif /* REPL_INSTANCE_INCLUDED */
