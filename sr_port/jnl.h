/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#ifndef JNL_H_INCLUDED
#define JNL_H_INCLUDED

#ifdef DEBUG
#include <stddef.h>           /* for offsetof macro (see OFFSETOF usage in assert below) */
#include "wbox_test_init.h"
#endif

#ifndef JNLSP_H_INCLUDED
#include "jnlsp.h"
#endif
#include "gtmcrypt.h"
#include "sleep.h"

error_def(ERR_JNLBADLABEL);
error_def(ERR_JNLENDIANBIG);
error_def(ERR_JNLENDIANLITTLE);

#define TID_STR_SIZE		8
#define JPV_LEN_NODE		16
#define JPV_LEN_USER		12
#define JPV_LEN_PRCNAM		16
#define JPV_LEN_TERMINAL	15

/* Whenever JNL_LABEL_TEXT changes, also change the following
 * 	1) Update JNL_VER_THIS
 * 	2) Add REPL_JNL_Vxx enum to repl_jnl_t typedef AND Vxx_JNL_VER #define in repl_filter.h
 * 	3) Add an entry each to repl_filter_old2cur & repl_filter_cur2old arrays in repl_filter.c.
 *	4) Bump JNL_EXTR_LABEL and JNL_DET_EXTR_LABEL as appropriate in muprec.h.
 *		This is not needed if for example the newly added jnl record types don't get extracted at all.
 *	5) Add a line in the big comment block just before "repl_filter_t" is typedefed in repl_filter.h
 *		to update the "Filter format", "Journal format" and "GT.M version" columns.
 *	6) Add Vxx_JNL_VER to repl_filter.h and examine usages of Vxx_JNL_VER where xx corresponds to older
 *		journal version to see if those need to be replaced with the newer version macro.
 * If the FILTER format is also changing, then do the following as well
 * 	7) Add REPL_FILTER_Vxx enum to repl_filter_t typedef in repl_filter.h
 * 	8) Add/Edit IF_xTOy macros in repl_filter.h to transform from/to the NEW jnl format version only.
 * 		Remove all entries that don't have the new jnl format in either the from or to part of the conversion.
 * 	9) Add/Edit prototype and implement functions jnl_xTOy() and jnl_yTOx() in repl_filter.c
 * 	10) Enhance repl_tr_endian_convert() to endian convert journal records from previous jnl formats to new format.
 * 		This is similar to the jnl_xTOy() filter conversion functions except that lot of byte-swaps are needed.
 * 	11) Periodically determine if the size of the array repl_filter_old2cur is huge and if so trim support of
 * 		rolling upgrade (using replication internal filters) for older GT.M versions/jnl-formats.
 * 		This would mean bumping the macro JNL_VER_EARLIEST_REPL and examining all arrays that are defined
 * 		using this macro and changing the entries in all those arrays accordingly (e.g. repl_filter_old2cur
 * 		array currently assumes earliest supported version is V15 and hence has the function named IF_curTO15
 * 		which needs to change to say IF_curTO17 if the earliest supported version changes to V17 or so).
 *
 */
#define JNL_LABEL_TEXT		"GDSJNL27"	/* see above comment paragraph for todos whenever this is changed */
#define JNL_VER_THIS		27
#define JNL_VER_EARLIEST_REPL	17		/* Replication filter support starts here GDSJNL17 = GT.M V5.1-000.
						 * (even though it should be V5.0-000, since that is pre-multisite,
						 * the replication connection with V55000 will error out at handshake
						 * time so V5.1-000 is the minimum that will even reach internal filter code)
						 */
#define JRT_MAX_V17		JRT_AIMG	/* Maximum jnl record type in GDSJNL17 or GDSJNL18 that can be input to replication
						 * filter. Actually JRT_TRIPLE is a higher record type than JRT_AIMG but it is only
						 * sent through the replication pipe and never seen by filter routines.
						 */
#define JRT_MAX_V19		JRT_UZTWORM	/* Max jnlrec type in GDSJNL19/GDSJNL20 that can be input to replication filter */
#define JRT_MAX_V21		JRT_UZTRIG	/* Max jnlrec type in GDSJNL21 that can be input to replication filter */
#define JRT_MAX_V22		JRT_UZTRIG	/* Max jnlrec type in GDSJNL22/GDSJNL23 that can be input to replication filter.
						 * Actually JRT_HISTREC is a higher record type than JRT_UZTRIG but it is only
						 * sent through the replication pipe and never seen by filter routines.
						 */
#define JRT_MAX_V24		JRT_ULGTRIG	/* Max jnlrec type in GDSJNL24/GDSJNL25 that can be input to replication filter */

#define JNL_ALLOC_DEF		2048
#define JNL_ALLOC_MIN		2048

/* The journal buffer size (specified in pages of size DISK_BLOCK_SIZE) should be large enough for one largest record,
 * which is equivalent to the largest possible value (of size MAX_STRLEN) and largest possible key (of size MAX_KEY_SZ),
 * plus the overhead of storing the journal records.
 */
#define JNL_BUFFER_MIN		((MAX_LOGI_JNL_REC_SIZE + ROUND_UP(2 * MAX_IO_BLOCK_SIZE, DISK_BLOCK_SIZE)) / DISK_BLOCK_SIZE + 1)
#define JNL_BUFFER_MAX		32768	/* # of 512-byte blocks = 16Mb journal buffer size */

/*	JNL_EXTEND_DEF	allocation size / 10
#define JNL_EXTEND_DEF_PERC	0.1
* 	Uncomment this section when code is ready to use extension = 10% of allocation
*/

#define JNL_EXTEND_MIN		0
#define JNL_EXTEND_DEF		2048
#define JNL_EXTEND_MAX		1073741823
#define JNL_MIN_WRITE		32768
#define JNL_MAX_WRITE		65536
/* FE was changed to EB because, the bit pattern there seems to vary more than the one for "FE".
 * Also a research in ELWOOD journal file showed that "EB" was one of the few patterns that had the least occurrences */
#define JNL_REC_SUFFIX_CODE	0xEB

/* With sync_io, we do journal writes to disk at filesystem block size boundaries. */
#define JNL_WRT_START_MODULUS(jb)	jb->fs_block_size
#define JNL_WRT_START_MASK(jb)	~(JNL_WRT_START_MODULUS(jb) - 1)	/* mask defining where the next physical write needs to
									 * happen as follows from the size of JNL_WRT_START_MODULUS
									 */

#define	JNL_WRT_END_MODULUS	8
#define JNL_WRT_END_MASK	((uint4)~(JNL_WRT_END_MODULUS - 1))

#define JNL_MIN_ALIGNSIZE	(1 << 12)	/*    4096 disk blocks effectively    2M alignsize */
#define JNL_DEF_ALIGNSIZE	(1 << 12)	/*    4096 disk blocks effectively    2M alignsize */
#define	JNL_MAX_ALIGNSIZE	(1 << 22)	/* 4194304 disk blocks effectively    2G alignsize */
#define JNL_REC_START_BNDRY	8
/* maximum logical journal record size */
#define MAX_LOGI_JNL_REC_SIZE		(ROUND_UP(MAX_STRLEN, DISK_BLOCK_SIZE) + ROUND_UP(MAX_KEY_SZ, DISK_BLOCK_SIZE))
/* one more disk-block for PBLK record header/footer */
#define MAX_JNL_REC_SIZE		(MAX_LOGI_JNL_REC_SIZE + DISK_BLOCK_SIZE)
/* Very large records require spanning nodes, which only happen in TP. */
#define MAX_NONTP_JNL_REC_SIZE(BSIZE)	((BSIZE) + DISK_BLOCK_SIZE)
#define MAX_MAX_NONTP_JNL_REC_SIZE	MAX_NONTP_JNL_REC_SIZE(MAX_DB_BLK_SIZE)

#ifdef GTM_TRIGGER
/* Define maximum size that $ZTWORMHOLE can be. Since $ZTWORMHOLE should be able to fit in a journal record and the
 * minimum alignsize used to be 128K (until V55000), we did not want it to go more than 128K (that way irrespective
 * of whatever alignsize the user specifies for the journal file, $ZTWORMHOLE will fit in the journal record). Leaving
 * a max of 512 bytes for the journal record prefix/suffix (32-byte overhead) and MIN_ALIGN_RECLEN (see comment in
 * JNL_MAX_RECLEN macro for why this is needed) we had allowed for a max of 128K-512 bytes in $ZTWORMHOLE. The minimum
 * alignsize (JNL_MIN_ALIGNSIZE) has changed to 2Mb (since V60000) and so we can potentially increase the maximum
 * $ZTWORMHOLE length to be 1Mb but we defer doing this until there is a need for this bigger length.
 */
#define	MAX_ZTWORMHOLE_LEN	(128 * 1024)
#define	MAX_ZTWORMHOLE_SIZE	(MAX_ZTWORMHOLE_LEN - 512)
#define	MAX_ZTWORM_JREC_LEN	(MAX_ZTWORMHOLE_LEN - MIN_ALIGN_RECLEN)

/* Define maximum size that a LGTRIG jnl record can get to. Cannot exceed minimum align size for sure */
#define	MAX_LGTRIG_LEN		((1 << 20) + (1 << 15))	/* 1Mb for XECUTE string part of the trigger,
							 * 32K for rest of trigger definition. Together this should
							 * still be less than 2Mb which is the JNL_MIN_ALIGNSIZE
							 */
#define	MAX_LGTRIG_JREC_LEN	(MAX_LGTRIG_LEN + DISK_BLOCK_SIZE)	/* Give 512 more bytes for journal record related
									 * overhead (jrec_prefix etc.) */
#endif

#define MIN_YIELD_LIMIT		0
#define MAX_YIELD_LIMIT		2048
#define DEFAULT_YIELD_LIMIT	8

/* Have a minimum jnl-file-auto-switch-limit of 4 align boundaries (currently each align boundary is 2M) */
#define	JNL_AUTOSWITCHLIMIT_MIN	(4 * JNL_MIN_ALIGNSIZE)
#define	JNL_AUTOSWITCHLIMIT_DEF	8386560	/* Instead of 8388607 it is adjusted for default allocation = extension = 2048 */

/* options (4-bytes unsigned integer) to wcs_flu() (currently flush_hdr, write_epoch, sync_epoch) are bit-wise ored */
#define	WCSFLU_NONE		 0
#define	WCSFLU_FLUSH_HDR	 1
#define	WCSFLU_WRITE_EPOCH	 2
#define	WCSFLU_SYNC_EPOCH	 4
#define	WCSFLU_FSYNC_DB		 8	/* Currently used only in Unix wcs_flu() */
#define	WCSFLU_IN_COMMIT	16	/* Set if caller is t_end or tp_tend. See wcs_flu for explanation of when this is set */
#define	WCSFLU_MSYNC_DB		32	/* Force a full msync if NO_MSYNC is defined. Currently used only in Unix wcs_flu(). */
#define	WCSFLU_SPEEDUP_NOBEFORE	64	/* Do not flush dirty db buffers. Just write an epoch record.
					 * Used to speedup nobefore jnl.
					 */
#define	WCSFLU_CLEAN_DBSYNC    128	/* wcs_flu invoked by wcs_clean_dbsync (as opposed to t_end/tp_tend invocation) */
#define	WCSFLU_FORCE_EPOCH     256	/* skip the optimization that writes epoch only if an epoch is not the most recently
					 * written record. This is necessary for some callers (e.g. update process writing
					 * an epoch for the -noresync startup case).
					 */
/* options for error_on_jnl_file_lost */
#define JNL_FILE_LOST_TURN_OFF	0	/* Turn off journaling. */
#define JNL_FILE_LOST_ERRORS	1	/* Throw an rts_error. */
#define MAX_JNL_FILE_LOST_OPT	JNL_FILE_LOST_ERRORS

#ifdef DEBUG
#define	DEFAULT_EPOCH_INTERVAL_IN_SECONDS	30 /* exercise epoch-syncing code relatively more often in DBG */
#else
#define	DEFAULT_EPOCH_INTERVAL_IN_SECONDS	300
#endif

#define DEFAULT_EPOCH_INTERVAL	SECOND2EPOCH_SECOND(DEFAULT_EPOCH_INTERVAL_IN_SECONDS) /* ***MUST*** include math.h for VMS */

#define	MAX_EPOCH_INTERVAL	32767	/* in seconds. Amounts to nearly 10 hours. Don't want to keep db stale so long */

#define JNL_ENABLED(X)		((X)->jnl_state == jnl_open)		/* If TRUE, journal records are to be written */
#define JNL_ALLOWED(X)		((X)->jnl_state != jnl_notallowed)	/* If TRUE, journaling is allowed for the file */
#define REPL_ENABLED(X)		((X)->repl_state == repl_open)		/* If TRUE, replication records are to be written */
#define REPL_WAS_ENABLED(X)	((X)->repl_state == repl_was_open) /* If TRUE, replication is now closed, but was open earlier */
									/* In this state, replication records are not written */
#define REPL_ALLOWED(X)		((X)->repl_state != repl_closed)	/* If TRUE, replication records are/were written */

/* Logical records should be written if journaling is enabled in the region OR if replication state is WAS_ON (repl_was_open).
 * In the former case, the journal records will be written to the journal pool, journal buffer and journal file.
 * In the latter case, the journal records will be written to the journal pool but not to the journal buffer and journal file.
 * All code that generates logical journal records should use the below macro instead of JNL_ENABLED macro.
 * Note that replication does not care about non-logical records (PBLK/AIMG/INCTN etc.) and hence code that generates them does
 * not need to (and should not) use this macro.
 */
#define	JNL_WRITE_LOGICAL_RECS(X)	(JNL_ENABLED(X) || REPL_WAS_ENABLED(X))

/* The following macro should be used to invoke the function "jnl_write" for any logical record. This macro
 * checks if journaling is enabled and if so invokes "jnl_write" else it invokes "jnl_pool_write" which
 * writes only to the journal pool.
 */
#define	JNL_WRITE_APPROPRIATE(CSA, JPC, RECTYPE, JREC, JFB)								\
MBSTART {														\
	GBLREF	boolean_t		is_replicator;									\
															\
	assert(JNL_ENABLED(CSA) || REPL_WAS_ENABLED(CSA));								\
	assert((NULL == JFB) || (RECTYPE == ((jnl_record *)(((jnl_format_buffer *)JFB)->buff))->prefix.jrec_type));	\
	if (JNL_ENABLED(CSA))												\
		jnl_write(JPC, RECTYPE, JREC, JFB); 	/* write to jnlbuffer */					\
	if (REPL_ALLOWED(CSA) && is_replicator)										\
	{														\
		assert(REPL_ENABLED(CSA) || REPL_WAS_ENABLED(CSA));							\
		jnl_pool_write(CSA, RECTYPE, JREC, JFB);	/* write to jnlpool */					\
	}														\
} MBEND

#define MUEXTRACT_TYPE(A) 	(((A)[0]-'0')*10 + ((A)[1]-'0')) /* A is a character pointer */

#define PADDED			PADDING

/* User must enter this string to ask standard input or output. */
#define	JNL_STDO_EXTR		"-stdout"

#ifdef BIGENDIAN
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)((uchar_ptr_t)&x + 1))
#else
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)(&x))
#endif
#define EXTTIME(S)					extract_len = exttime(S, murgbl.extr_buff, extract_len)

/* This macro should be used to initialize jgbl.gbl_jrec_time to the system time. The reason is that it does additional checks. */
#define	SET_GBL_JREC_TIME				\
MBSTART {						\
	assert(!jgbl.dont_reset_gbl_jrec_time);		\
	JNL_SHORT_TIME(jgbl.gbl_jrec_time);		\
} MBEND

#define	DO_GBL_JREC_TIME_CHECK_FALSE	FALSE
#define	DO_GBL_JREC_TIME_CHECK_TRUE	TRUE

#define	SET_JNLBUFF_PREV_JREC_TIME(JB, CUR_JREC_TIME, DO_GBL_JREC_TIME_CHECK)					\
{														\
	GBLREF	jnl_gbls_t		jgbl;									\
														\
	/* In case of journal rollback, time is set back to turnaround point etc. so assert accordingly */	\
	assert(!DO_GBL_JREC_TIME_CHECK || (jgbl.gbl_jrec_time >= CUR_JREC_TIME));				\
	assert(jgbl.onlnrlbk || (JB->prev_jrec_time <= CUR_JREC_TIME));						\
	JB->prev_jrec_time = CUR_JREC_TIME;									\
}

/* This macro ensures that journal records are written in non-decreasing time order in each journal file.
 * It is passed the time field to adjust and a pointer to the journal buffer of the region.
 * The journal buffer holds the timestamp of the most recently written journal record.
 */
#define	ADJUST_GBL_JREC_TIME(jgbl, jbp)				\
{								\
	if (jgbl.gbl_jrec_time < jbp->prev_jrec_time)		\
	{							\
		assert(!jgbl.dont_reset_gbl_jrec_time);		\
		jgbl.gbl_jrec_time = jbp->prev_jrec_time;	\
	}							\
}

/* This macro is similar to ADJUST_GBL_JREC_TIME except that this ensures ordering of timestamps across
 * ALL replicated regions in a replicated environment. In VMS, we don't maintain this prev_jnlseqno_time
 * field.
 */
#define	ADJUST_GBL_JREC_TIME_JNLPOOL(jgbl, jpl)			\
{								\
	if (jgbl.gbl_jrec_time < jpl->prev_jnlseqno_time)	\
	{							\
		assert(!jgbl.dont_reset_gbl_jrec_time);		\
		jgbl.gbl_jrec_time = jpl->prev_jnlseqno_time;	\
	}							\
	jpl->prev_jnlseqno_time = jgbl.gbl_jrec_time;		\
}

/* Check if journal file is usable from the fields in the file header.
 * Currently, the fields tested are LABEL and ENDIANNESS.
 */
#define	JNL_HDR_ENDIAN_OFFSET	8

#define	CHECK_JNL_FILE_IS_USABLE(JFH, STATUS, DO_GTMPUTMSG, JNL_FN_LEN, JNL_FN)				\
{													\
	assert(JNL_HDR_ENDIAN_OFFSET == OFFSETOF(jnl_file_header, is_little_endian));			\
	if (0 != MEMCMP_LIT((JFH)->label, JNL_LABEL_TEXT))						\
	{												\
		STATUS = ERR_JNLBADLABEL;								\
		if (DO_GTMPUTMSG)									\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) STATUS, 6, JNL_FN_LEN, JNL_FN,	\
				LEN_AND_LIT(JNL_LABEL_TEXT), SIZEOF((JFH)->label), (JFH)->label);	\
	}												\
	BIGENDIAN_ONLY(											\
	else if ((JFH)->is_little_endian)								\
	{												\
		STATUS = ERR_JNLENDIANLITTLE;								\
		if (DO_GTMPUTMSG)									\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) STATUS, 2, JNL_FN_LEN, JNL_FN);	\
	}												\
	)												\
	LITTLEENDIAN_ONLY(										\
	else if (!(JFH)->is_little_endian)								\
	{												\
		STATUS = ERR_JNLENDIANBIG;								\
		if (DO_GTMPUTMSG)									\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) STATUS, 2, JNL_FN_LEN, JNL_FN);	\
	}												\
	)												\
}

/* Token generation used in non-replicated journaled environment. Note the assumption here
   that SIZEOF(token_split_t) == SIZEOF(token_build) which will be asserted in gvcst_init().
   The TOKEN_SET macro below depends on this assumption.
*/
typedef struct token_split_t_struct
{
#	ifdef BIGENDIAN
	uint4	process_id;
	uint4	local_tn;
#	else
	uint4	local_tn;
	uint4	process_id;
#	endif
} token_split_t;

typedef union
{
	token_split_t	t_piece;
	token_num	token;
} token_build;

/* To assist in setting token value, the following macro is supplied to handle the two token parts */
#define TOKEN_SET(BASE, TN, PID) (((token_build_ptr_t)(BASE))->t_piece.local_tn = (uint4)(TN), \
				     ((token_build_ptr_t)(BASE))->t_piece.process_id = (PID))

enum jpv_types
{
        CURR_JPV = 0,
        ORIG_JPV,
        JPV_COUNT
};
/* Note we have two process verctors now for a pini record */
typedef struct jnl_process_vector_struct	/* name needed since this is used in cmmdef.h for "pvec" member */
{
	uint4		jpv_pid;			/* Process id */
	int4		jpv_image_count;		/* Image activations [VMS only] */
	jnl_proc_time	jpv_time;			/* Timestamp of the process genarating this.
								(This could be different than the journal record timestamp) */
	jnl_proc_time	jpv_login_time;			/* Used for process initialization time */
	char		jpv_node[JPV_LEN_NODE],		/* Node name */
			jpv_user[JPV_LEN_USER],		/* User name */
			jpv_prcnam[JPV_LEN_PRCNAM],	/* Process name [VMS only] */
			jpv_terminal[JPV_LEN_TERMINAL];	/* Login terminal */
	unsigned char	jpv_mode;			/* a la JPI$_MODE [VMS only] */
	int4		filler;
	/* SIZEOF(jnl_process_vector) must be a multiple of SIZEOF(int4) */
} jnl_process_vector;

enum pini_rec_stat
{
	IGNORE_PROC = 0,
	ACTIVE_PROC = 1,
	FINISHED_PROC = 2,
	BROKEN_PROC = 4
};

typedef struct pini_list
{
	uint4			pini_addr;
	uint4			new_pini_addr;	/* used in forward phase of recovery */
	jnl_process_vector	jpv;		/* CURR_JPV. Current process's JPV. For GTCM server we also use this. */
	jnl_process_vector	origjpv;	/* ORIG_JPV. Used for GTCM client only */
	jnl_proc_time		pini_jpv_time;	/* Original PINI record timestamp (before forward phase of
						 *	journal recovery modifies jpv->jpv_time */
	enum pini_rec_stat	state;		/* used for show qualifier */
} pini_list_struct;

enum jnl_record_type
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	rectype,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY

	JRT_RECTYPES		/* Total number of JOURNAL record types */
};

#include "jnl_typedef.h"

enum jnl_state_codes
{
	jnl_notallowed,
	jnl_closed,
	jnl_open
};

enum repl_state_codes
{
	repl_closed,	/* region not replicated, no records are written */
	repl_open,	/* region is replicated, and records are written */
	repl_was_open	/* region is currently not replicated, but it was earlier; jnl_file_lost() changes open to was_open */
};

#define	JNL_PHASE2_COMMIT_ARRAY_SIZE	4096	/* Max # of jnl commits that can still be in phase2.
						 * Note that even if a phase2 commit is complete, the slot it occupies
						 * cannot be reused by anyone else until all phase2 commit slots that
						 * started before this slot are also done.
						 */
/* Individual structure describing an active phase2 jnl commit in jnl_buffer shared memory */
typedef struct
{
	trans_num	curr_tn;
	seq_num		jnl_seqno;
	seq_num		strm_seqno;
	uint4		process_id;
	uint4		start_freeaddr;		/* jb->rsrv_freeaddr at start of this commit */
	uint4		tot_jrec_len;		/* total length of jnl records in jnl buffer corresponding to this tn */
	uint4		pini_addr;		/* jpc->pini_addr when this phase2 commit entry was allocated */
	uint4		jrec_time;		/* time corresponding to this transaction */
	bool		in_phase2;		/* TRUE if the UPDATE_JBP_RSRV_FREEADDR call for this phas2 commit entry happened
						 *	through UPDATE_JRS_RSRV_FREEADDR in t_end/tp_tend.
						 * FALSE if the call happened directly in "jnl_write" (e.g. PFIN/INCTN etc.).
						 * This distinction helps cleanup orphaned slots (owning pid is killed prematurely).
						 */
	bool		replication;		/* TRUE if this transaction bumped the jnl_seqno. Note that even though a
						 * region might have replication turned on, there are some transactions
						 * (e.g. inctn operations from reorg, file extensions, 2nd phase of kill etc.)
						 * which will not bump the jnl_seqno. Those will have this set to FALSE.
						 * This is needed by "jnl_write_salvage" to know if a JRT_INCTN or JRT_NULL needs
						 * to be written.
						 */
	bool		write_complete;		/* TRUE if this pid is done writing jnl records to jnlbuff */
	bool		filler_8byte_align[1];
} jbuf_phase2_in_prog_t;

typedef struct
{
 	trans_num		eov_tn;		/* curr_tn is saved as eov_tn by jnl_write_epoch. Used by recover/rollback */
	volatile trans_num	epoch_tn;	/* Transaction number for current epoch */
	seq_num			end_seqno;		/* reg_seqno saved by jnl_write_epoch. Used by recover/rollback */
	seq_num			strm_end_seqno[MAX_SUPPL_STRMS]; /* used to keep jfh->strm_end_seqno up to date with each epoch.
						 * Unused in VMS but defined so shared memory layout is similar in Unix & VMS.
						 */
	int4			min_write_size,	/* if unwritten data gets to this size, write it */
				max_write_size, /* maximum size of any single write */
				size;		/* buffer size */
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	boolean_t		before_images;	/* If TRUE, before-image processing is enabled */
						/* end not volatile QUAD */
	uintszofptr_t		buff_off;	/* relative offset to filesystem-block-size aligned buffer start */
	volatile int4		free,		/* relative index of first byte to write in buffer */
				rsrv_free;	/* relative index corresponding to jb->rsrv_freeaddr */
	volatile uint4		freeaddr,	/* virtual on-disk address which will correspond to free, when it is written */
				rsrv_freeaddr,	/* freeaddr when all in-progress phase2 jnl commits are complete.
						 * rsrv_freeaddr >= freeaddr at all times.
						 */
				next_align_addr,/* rsrv_freeaddr BEFORE which next JRT_ALIGN record needs to be written */
				end_of_data,	/* Synched offset updated by jnl_write_epoch. Used by recover/rollback */
				filesize;	/* highest virtual address available in the file (units in disk-blocks)
						 * file size in bytes limited to 4GB by autoswitchlimit, so 'filesize' <= 8MB
						 * so filesize cannot overflow the four bytes of a uint4
						 */
	uint4			phase2_commit_index1;
	uint4			phase2_commit_index2;
	jbuf_phase2_in_prog_t	phase2_commit_array[JNL_PHASE2_COMMIT_ARRAY_SIZE];
	volatile int4		blocked;
	volatile uint4	 	fsync_dskaddr;  /* dskaddr upto which fsync is done */
	volatile int4		dsk;		/* relative index of 1st byte to write to disk;
						 * if free == dsk, buffer is empty */
	volatile int4		wrtsize;	/* size of write in progress */
        volatile uint4		dskaddr,	/* virtual on-disk address corresponding to dsk */
				now_writer,	/* current owner of io_in_prog (VMS-only) */
				image_count;	/* for VMS is_proc_alive */
	volatile struct				/* must be at least word aligned for memory coherency */
	{
		short		cond;
		unsigned short	length;
		int4		dev_specific;
	}			iosb;
	uint4         		alignsize;      	/* current alignsize (max value possible is 2Gb, not 4Gb) */
	jnl_tm_t		eov_timestamp;		/* jgbl.gbl_jrec_time saved by jnl_write_epoch. Used by recover/rollback */
	uint4			cycle;			/* shared copy of the number of the current journal file generation */
	volatile int4		qiocnt,			/* Number of qio's issued */
				bytcnt,			/* Number of bytes written */
				errcnt,			/* Number of errors during writing */
				reccnt[JRT_RECTYPES];	/* Number of records written per opcode */
	int			filler_align[42 - JRT_RECTYPES];	/* So buff below starts on even (QW) keel */
	/* Note the above filler will fail if JRT_RECTYPES grows beyond 42 elements
	 * In that case, change the start num to the next even number.
	 */
	volatile jnl_tm_t	prev_jrec_time;		/* to ensure that time never decreases across successive jnl records */
	volatile uint4		next_epoch_time;	/* Time when next epoch is to be written (in epoch-seconds) */
	volatile boolean_t	need_db_fsync;          /* need an fsync of the db file */
	volatile int4		io_in_prog;		/* VMS only: write in progress indicator (NOTE: must manipulate
										only with interlocked instructions */
	uint4			enospc_errcnt;		/* number of times jb->errcnt was last incremented due to ENOSPC error
							 * when writing to this journal file */
	uint4			max_jrec_len;		/* copy of max_jrec_len from journal file header */
	uint4			fs_block_size;		/* underlying journal file system block size */
	volatile uint4		post_epoch_freeaddr;	/* virtual on-disk address after last epoch */
	boolean_t		last_eof_written;	/* No more records may be written to the file due to autoswitch */
	uint4			end_of_data_at_open;	/* Offset of EOF record when jnl file is first opened in shm */
	uint4			re_read_dskaddr;	/* If non-zero, "jnl_qio_start" will read a filesystem-block of data
							 * corresponding to jbp->dskaddr before initiating any new writes
							 * to the journal file.
							 */
	/* CACHELINE_PAD macros provide spacing between the following latches so that they do
	   not interfere with each other which can happen if they fall in the same data cacheline
	   of a processor.
	*/
	CACHELINE_PAD(SIZEOF(global_latch_t), 0)	/* start next latch at a different cacheline than previous fields */
	global_latch_t		io_in_prog_latch;	/* UNIX only: write in progress indicator */
	CACHELINE_PAD(SIZEOF(global_latch_t), 1)	/* start next latch at a different cacheline than previous fields */
	global_latch_t		fsync_in_prog_latch;	/* fsync in progress indicator */
        CACHELINE_PAD(SIZEOF(global_latch_t), 2)	/* start next latch at a different cacheline than previous fields */
	global_latch_t		phase2_commit_latch;	/* Used by "jnl_phase2_cleanup" to update "phase2_commit_index1" */
	CACHELINE_PAD(SIZEOF(global_latch_t), 3)	/* pad enough space so next non-filler byte falls in different cacheline */
	/**********************************************************************************************/
	/* Important: must keep header structure quadword (8 byte)  aligned for buffers used in QIO's */
	/**********************************************************************************************/
	unsigned char		buff[1];		/* Actually buff[size] */
} jnl_buffer;

/* Sets jbp->freeaddr & jbp->free. They need to be kept in sync at all times */
#define	SET_JBP_FREEADDR(JBP, FREEADDR)										\
{														\
	DEBUG_ONLY(uint4 jbp_freeaddr;)										\
	DEBUG_ONLY(uint4 jbp_rsrv_freeaddr;)									\
														\
	assert(JBP->phase2_commit_latch.u.parts.latch_pid == process_id);					\
	DEBUG_ONLY(jbp_freeaddr = JBP->freeaddr;)								\
	assert(FREEADDR >= jbp_freeaddr);									\
	DEBUG_ONLY(jbp_rsrv_freeaddr = JBP->rsrv_freeaddr;)							\
	assert(FREEADDR <= jbp_rsrv_freeaddr);									\
	JBP->freeaddr = FREEADDR;										\
	/* Write memory barrier here to enforce the fact that freeaddr *must* be seen to be updated before	\
	 * free is updated. It is less important if free is stale so we do not require a 2nd barrier for that	\
	 * and will let the lock release (crit lock required since clustering not currently supported) do the	\
	 * 2nd memory barrier for us. This barrier takes care of this process's responsibility to broadcast	\
	 * cache changes. It is up to readers to also specify a read memory barrier if necessary to receive	\
	 * this broadcast.											\
	 */													\
	SHM_WRITE_MEMORY_BARRIER;										\
	JBP->free = (FREEADDR) % JBP->size;									\
	DBG_CHECK_JNL_BUFF_FREEADDR(JBP);									\
}

#define	DBG_CHECK_JNL_BUFF_FREEADDR(JBP)							\
{												\
	assert((JBP->freeaddr % JBP->size) == JBP->free);					\
	assert((JBP->freeaddr >= JBP->dskaddr)							\
		|| (gtm_white_box_test_case_enabled						\
			&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));	\
}

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef jnl_buffer	*jnl_buffer_ptr_t;
typedef token_build	*token_build_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

typedef struct jnl_private_control_struct
{
	jnl_buffer_ptr_t	jnl_buff;		/* pointer to shared memory */
	gd_region		*region;		/* backpointer to region head */
	fd_type			channel,		/* output channel, aka fd in UNIX */
				old_channel;		/* VMS only - for dealing with deferred deassign */
	gd_id			fileid;			/* currently initialized and used only by source-server */
	uint4			pini_addr,		/* virtual on-disk address for JRT_PINI record, if journaling */
				new_freeaddr;
	uint4			phase2_freeaddr;	/* Used by "jnl_write_reserve" in phase1 to denote freeaddr
							 * that takes into account the cumulative length of journal records
							 * already reserved till now in phase1.
							 * Used by JB_FREEADDR_APPROPRIATE macro in phase2 to indicate the
							 * freeaddr corresponding to the journal record about to be written
							 * by this process.
							 */
	uint4			phase2_free;		/* jb->free     to use in "jnl_write_phase2" */
	boolean_t		in_jnl_phase2_salvage;	/* in "jnl_phase2_salvage" */
	int4			status;			/* A for error reporting */
	volatile boolean_t	qio_active;		/* jnl buffer write in progress in THIS process (recursion indicator) */
	boolean_t		fd_mismatch;		/* TRUE when jpc->channel does not point to the active journal */
	volatile boolean_t	sync_io;		/* TRUE if the process is using O_SYNC/O_DSYNC for this jnl (UNIX) */
							/* TRUE if writers open NOCACHING to bypass XFC cache (VMS) */
	boolean_t		error_reported;		/* TRUE if jnl_file_lost already reported the journaling error */
	uint4			status2;		/* for secondary error status, currently used only in VMS */
	uint4			cycle;			/* private copy of the number of this journal file generation */
	char			*err_str;		/* pointer to an extended error message */
	trans_num		curr_tn;		/* db tn used by callees of "jnl_write_phase2" */
} jnl_private_control;

typedef enum
{
	JNL_KILL,
	JNL_SET,
	JNL_ZKILL,
#	ifdef GTM_TRIGGER
	JNL_ZTWORM,
	JNL_LGTRIG,
	JNL_ZTRIG,
#	endif
	JA_MAX_TYPES
} jnl_action_code;

typedef enum
{
#define MUEXT_TABLE_ENTRY(muext_rectype, code0, code1)	muext_rectype,
#include "muext_rec_table.h"
#undef MUEXT_TABLE_ENTRY

	  MUEXT_MAX_TYPES	/* Total number of EXTRACT JOURNAL record types */
} muextract_type;

typedef struct
{
	jnl_action_code		operation;
	uint4			nodeflags;
} jnl_action;

#define	JNL_FENCE_LIST_END	((sgmnt_addrs *)-1L)

typedef struct
{
	sgmnt_addrs		*fence_list;
	int			level;
	boolean_t		replication;
	token_num		token;
	seq_num			strm_seqno;	/* valid only in case of replication. uninitialized in case of ZTP */
} jnl_fence_control;

typedef struct
{
	uint4			jrec_type : 8;		/* Offset:0 :: Actually, enum jnl_record_type */
	uint4			forwptr : 24;		/* Offset:1 :: Offset to beginning of next record */
	off_jnl_t		pini_addr;		/* Offset:4 :: Offset in the journal file which contains pini record */
	jnl_tm_t		time;			/* Offset:8 :: 4-byte time stamp both for UNIX and VMS */
	uint4			checksum;		/* Offset:12 :: Generated from journal record */
	trans_num		tn;			/* Offset:16 */
} jrec_prefix;	/* 24-byte */

typedef struct
{
	uint4 			backptr : 24;		/* Offset to beginning of current record */
	uint4 			suffix_code : 8;	/* JNL_REC_SUFFIX_CODE */
} jrec_suffix;	/* 4-byte */

typedef union
{
	seq_num			jnl_seqno;
	token_num		token;
} token_seq_t;

typedef struct
{
	char			label[SIZEOF(JNL_LABEL_TEXT) - 1];
	char			is_little_endian; /* this field's offset (JNL_HDR_ENDIAN_OFFSET) should not change
						   * across journal versions and is checked right after reading the header.
						   */
	char			filler_align8[7];
	jnl_process_vector	who_created,	/* Process who created */
				who_opened;	/* Process who last opened */
	jnl_proc_time		bov_timestamp,	/* 8-byte time when journal was created */
				eov_timestamp;	/* 8-byte time when journal was last updated
							Updated by cre_jnl_file/jnl_file_extend/jnl_file_close */
	trans_num		bov_tn, 	/* Beginning journal record's transaction number */
				eov_tn;		/* End transaction number.
							Updated by cre_jnl_file/jnl_file_extend/jnl_file_close */
	seq_num			start_seqno;	/* reg_seqno when this journal file was created */
	seq_num			end_seqno;	/* reg_seqno when this journal file was closed or last extended.
							Updated by cre_jnl_file/jnl_file_extend/jnl_file_close */
	off_jnl_t		end_of_data;	/* Offset of beginning of last record.
							Updated by cre_jnl_file/jnl_file_extend/jnl_file_close */
	off_jnl_t		prev_recov_end_of_data;	/* Recovered/Rolled back journal's turn around point's offset.
						This offset was supposed to have EOF_RECORD before recover switched journal.
						A non-zero value means this journal was recovered and had the turn around point. */
	off_jnl_t		virtual_size;	/* Allocation + n * Extension (in blocks). jnl_file_extend updates it */
	boolean_t		crash;		/* crashed before "jnl_file_close" completed */
	boolean_t		recover_interrupted;	/* true when recover creates the journal file; false after success. */
	off_jnl_t		turn_around_offset;	/* At turn around point journal record's (EPOCH) offset */
	jnl_tm_t		turn_around_time;	/* At turn around point journal record's timestamp */
	boolean_t		before_images;	/* before image enabled in this journal */
	uint4			alignsize;	/* align size of journal (where a valid record start) */
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	int4			repl_state;	/* To state whether replication is turned on for this journal file */
 	uint4			autoswitchlimit;/* Limit in disk blocks (max 4GBytes) when jnl should be auto switched */
	uint4			jnl_alq;	/* initial allocation (in blocks) */
	uint4			jnl_deq;	/* extension (in blocks) */
	boolean_t		filler_update_disabled;	/* obsoleted as part of multi-site replication changes */
	int4			max_jrec_len;	/* Maximum length in bytes of a journal record.
						 * Although computed from the database block size, we need this
						 * stored as well in case database is not available */
	uint4 			data_file_name_length;			/* Length of data_file_name */
	uint4 			prev_jnl_file_name_length;		/* Length of prev_jnl_file_name */
	uint4 			next_jnl_file_name_length;		/* Length of next_jnl_file_name */
	uint4 			checksum;	/* Calculate from journal file id */
	uint4			prev_recov_blks_to_upgrd_adjust;	/* amount to adjust filehdr "blks_to_upgrd" if ever
									 * backward recovery goes back past this journal file */
	unsigned char		data_file_name[JNL_NAME_SIZE];		/* Database file name */
	unsigned char		prev_jnl_file_name[JNL_NAME_SIZE];	/* Previous generation journal file name */
	unsigned char		next_jnl_file_name[JNL_NAME_SIZE];	/* Next generation journal file name */
	/* Encryption-related fields (mirror those in the database file header) */
	int4			is_encrypted;
	char			encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	char			encryption_hash2[GTMCRYPT_RESERVED_HASH_LEN];
	boolean_t		non_null_iv;
	block_id		encryption_hash_cutoff;
	trans_num		encryption_hash2_start_tn;
	char			encr_filler[80];
	seq_num			strm_start_seqno[MAX_SUPPL_STRMS];
	seq_num			strm_end_seqno[MAX_SUPPL_STRMS];
	boolean_t		last_eof_written;	/* No more records may be written to the file due to autoswitch */
	boolean_t		is_not_latest_jnl;	/* Set to TRUE if this jnl becomes a previous generation jnl file
							 * at some point in time (relied upon by mur_tp_resolve_time).
							 */
	/* filler remaining */
	char			filler[432];
} jnl_file_header;

typedef struct
{
	int4			status,
				alloc,
				extend,
				buffer;
	sgmnt_data_ptr_t	csd;
	seq_num			reg_seqno;
	unsigned char		jnl[JNL_NAME_SIZE],
		                *fn;
	uint4			max_jrec_len;
	short			fn_len,
				jnl_len,
				jnl_def_len;
	bool			before_images;
	bool			filler_bool[1];
	uint4			alignsize;
 	int4			autoswitchlimit;	/* limit in disk blocks (8388607 blocks)
							 * when jnl should be auto switched */
	int4			epoch_interval;		/* Time between successive epochs in epoch-seconds */
	char			*prev_jnl;
	int4			prev_jnl_len;
	int4                    jnl_state;              /* current csd->jnl_state */
	int4			repl_state;
	uint4			status2;		/* for secondary error status information in VMS */
	boolean_t		no_rename;
	boolean_t		no_prev_link;
	int4			blks_to_upgrd;		/* Blocks not at current block version level */
	uint4 			checksum;
	uint4			free_blocks;		/* free  blocks counter at time of epoch */
	uint4			total_blks;		/* total blocks counter at time of epoch */
	int4			is_encrypted;
	char			encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	char			encryption_hash2[GTMCRYPT_RESERVED_HASH_LEN];
	boolean_t		non_null_iv;
	block_id		encryption_hash_cutoff;
	trans_num		encryption_hash2_start_tn;
	sgmnt_addrs		*csa;
} jnl_create_info;

/* Journal record definitions */

#define jnl_str_len_t		uint4	/* 4 byte length (which is in turn split into bit fields below) */

/* Bit masks for the "nodeflags" field. Note that there is no flag to indicate whether this update actually invoked any triggers.
 * That is because we have to format the journal record BEFORE invoking any triggers (that way the triggering update comes ahead
 * of its corresponding triggered updates in the journal file as this ordering is relied upon by the update process) and as part
 * of formatting, we also compute the checksum that includes the "nodeflags" field and so fixing this field AFTER trigger
 * invocation to reflect if any triggers were invoked would mean recomputing the checksum all over again. Currently there is no
 * need for the "triggers actually invoked" bit. If it is later desired, care should be taken to recompute the checksum.
 */
#define	JS_NOT_REPLICATED_MASK	(1 << 0) /* 1 if this update should NOT be replicated.
					  * All updates done inside of a trigger and a SET redo (because of changes to $ztval)
					  * fall in this category.
					  */
#define	JS_HAS_TRIGGER_MASK	(1 << 1) /* 1 if the global being updated had at least one trigger defined (not necessarily
					  *	invoked for this particular update)
					  */
#define	JS_NULL_ZTWORM_MASK	(1 << 2) /* 1 if $ZTWORMHOLE for this update should be "" string, 0 otherwise */
#define	JS_SKIP_TRIGGERS_MASK	(1 << 3) /* 1 if MUPIP LOAD update so triggers are not invoked on replay by update process */
#define	JS_IS_DUPLICATE		(1 << 4) /* 1 if this SET or KILL is a duplicate. In case of a SET, this is a duplicate set.
					  * In case of a KILL, it is a kill of a non-existing node aka duplicate kill.
					  * Note that the dupkill occurs only in case of the update process. In case of GT.M,
					  * the KILL is entirely skipped. In both duplicate sets or kills, only a journal
					  * record is written, the database is untouched.
					  */
#define	JS_MAX_MASK		(1 << 8) /* max of 8 bits we have for mask */

/* Note that even though mumps_node, ztworm_str, lgtrig_str, align_str are members defined as type "jnl_string" below,
 * the "nodeflags" field is initialized to non-zero values ONLY in the case of the mumps_node member.
 * For ztworm_str, lgtrig_str and align_str, nodeflags is guaranteed to be zero so the 24-bit "length" member
 * can even be used as a 32-bit length (if necessary) without issues. This is why nodeflags is
 * defined in a different order (BEFORE or AFTER the "length" member) based on big-endian or little-endian.
 */
typedef struct
{
#	ifdef BIGENDIAN
	unsigned int	nodeflags     :  8;
	unsigned int	length        : 24;
#	else
	unsigned int	length        : 24;
	unsigned int	nodeflags     :  8;
#	endif
	char		text[1];		/* Actually text[length] */
} jnl_string;

typedef struct jnl_format_buff_struct
{
	que_ent				free_que;
	struct  jnl_format_buff_struct	*next;
#	ifdef GTM_TRIGGER
	struct  jnl_format_buff_struct	*prev;
#	endif
	enum jnl_record_type		rectype;
	int4				record_size;
	int4				hi_water_bsize;
	char 				*buff;
	uint4				checksum;
	jnl_action			ja;
	char				*alt_buff; /* for storing the unencrypted jnl *SET and *KILL records to be pushed
						    * into the jnl pool. */
} jnl_format_buffer;

/* All fixed size records are 8-byte-multiple size.
 * All variable size records are made 8-byte multiple size by run-time process */

/* struct_jrec_upd for non-TP, TP or ZTP. For replication we use 8-byte jnl_seqno. Otherwise we use 8-byte token.
 * Currently we don't support ZTP + replication.
 */
typedef struct	/* variable length */
{
	jrec_prefix		prefix;
	token_seq_t		token_seq;	/* must start at 8-byte boundary */
	seq_num			strm_seqno;	/* non-zero only if this is a supplementary instance in which case this #
						 * reflects the 60-bit sequence number corresponding to this update on the
						 * originating primary + higher order 4-bits reflecting the stream #.
						 */
	uint4			update_num;	/* 'n' where this is the nth journaled update (across all regions) in this TP
						 * transaction. n=1 for the first update inside TP, 2 for the second update
						 * inside TP and so on. Needed so journal recovery and update process can play
						 * all the updates inside of one TP transaction in the exact same order as GT.M.
						 */
	unsigned short		filler_short;
	unsigned short		num_participants;	/* # of regions that wrote a TCOM record in their jnl files.
							 * Currently written only for TSET/TKILL/TZTWORM/TLGTRIG/TZTRIG records.
							 * Uninitialized for USET/UKILL/UZTWORM/ULGTRIG/UZTRIG records.
							 */
	jnl_string		mumps_node;	/* For set/kill/zkill 	: {jnl_str_len_t key_len, char key[key_len]} */
	 					/* For set additionally : {mstr_len_t data_len, char data[data_len]} */
} struct_jrec_upd;

/* $ztwormhole record */
typedef struct	/* variable length */
{
	jrec_prefix		prefix;
	token_seq_t		token_seq;	/* must start at 8-byte boundary */
	seq_num			strm_seqno;	/* see "struct_jrec_upd" for comment on the purpose of this field */
	uint4			update_num;	/* 'n' where this is the nth journaled update (across all regions) in this TP
						 * transaction. n=1 for the first update inside TP, 2 for the second update
						 * inside TP and so on. Needed so journal recovery and update process can play
						 * all the updates inside of one TP transaction in the exact same order as GT.M.
						 */
	unsigned short		filler_short;
	unsigned short		num_participants;	/* # of regions that wrote a TCOM record in their jnl files.
							 * Currently written only for TSET/TKILL/TZTWORM/TLGTRIG/TZTRIG records.
							 * Uninitialized for USET/UKILL/UZTWORM/ULGTRIG/UZTRIG records.
							 */
	jnl_string		ztworm_str;	/* jnl_str_len_t ztworm_str_len, char ztworm_str[ztworm_str_len]} */
} struct_jrec_ztworm;

/* logical trigger jnl record (TLGTRIG or ULGTRIG record) */
typedef struct	/* variable length */
{
	jrec_prefix		prefix;
	token_seq_t		token_seq;	/* must start at 8-byte boundary */
	seq_num			strm_seqno;	/* see "struct_jrec_upd" for comment on the purpose of this field */
	uint4			update_num;	/* 'n' where this is the nth journaled update (across all regions) in this TP
						 * transaction. n=1 for the first update inside TP, 2 for the second update
						 * inside TP and so on. Needed so journal recovery and update process can play
						 * all the updates inside of one TP transaction in the exact same order as GT.M.
						 */
	unsigned short		filler_short;
	unsigned short		num_participants;	/* # of regions that wrote a TCOM record in their jnl files.
							 * Currently written only for TSET/TKILL/TZTWORM/TLGTRIG/TZTRIG records.
							 * Uninitialized for USET/UKILL/UZTWORM/ULGTRIG/UZTRIG records.
							 */
	jnl_string		lgtrig_str;	/* jnl_str_len_t lgtrig_str_len, char lgtrig_str[lgtrig_str_len]} */
} struct_jrec_lgtrig;

#define	INVALID_UPDATE_NUM	(uint4)-1

typedef struct	/* variable length */
{
	jrec_prefix		prefix;
	block_id		blknum;
	uint4			bsiz;
	enum db_ver		ondsk_blkver;		/* Previous version of block from cache_rec */
	int4			filler;
	char			blk_contents[1];	/* Actually blk_contents[bsiz] */
} struct_jrec_blk;

/* Note: A JRT_ALIGN is the ONLY journal record which does not have the full "jrec_prefix" prefix. This is because
 * we need it to be as small as 16-bytes in size (See "jnl_phase2_salvage" for a discussion on WHY). Hence the below
 * special definition which takes in a minimal set of fields from jrec_prefix to keep the smallest size (including
 * the 4-byte suffix) at 16 bytes. The "time" member is kept at the same offset as other jnl records. This lets us
 * use jnlrec->prefix.time to access the time even for JRT_ALIGN records. But the "checksum" member is at a
 * different offset since it is at offset 12 bytes in other records which is not possible for JRT_ALIGN given it
 * needs to be 16-bytes in size including a 4-byte suffix. A design choice was to move the checksum to offset 4 bytes
 * in all the other records but that meant changes to various stuff (like replication filters to transform from the new
 * format to old format etc.) and was not considered worth the effort. Instead we will need to ensure all code that
 * needs to access the checksum field uses "prefix.checksum" only if it is sure the input record type is not JRT_ALIGN.
 * If the caller code is not sure, it needs to use the GET_JREC_CHECKSUM macro.
 */
typedef struct	/* variable length */
{
	uint4			jrec_type : 8;		/* Offset:0 :: Actually, enum jnl_record_type */
	uint4			forwptr : 24;		/* Offset:1 :: Offset to beginning of next record */
	uint4			checksum;		/* Offset:4 :: Generated from journal record */
	jnl_tm_t		time;			/* Offset:8 :: 4-byte time stamp */
	/* Note: Actual string (potentially 0-length too) follows "forwptr" and then "jrec_suffix" */
} struct_jrec_align;

/* Please change the "GBLDEF struct_jrec_tcom" initialization, if below is changed */
typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	token_seq_t		token_seq;	/* must start at 8-byte boundary */
	seq_num			strm_seqno;	/* see "struct_jrec_upd" for comment on the purpose of this field */
	unsigned short		filler_short;
	unsigned short		num_participants;	/* # of regions that wrote a TCOM record in their jnl files */
	char			jnl_tid[TID_STR_SIZE];
	jrec_suffix		suffix;
} struct_jrec_tcom;

/* Please change the "static struct_jrec_ztcom" initialization in op_ztcommit.c, if below is changed */
typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	token_num		token;		/* must start at 8-byte boundary */
	seq_num			filler_8bytes;	/* To mirror tcom layout. It is ok to waste space because ztcom is
						 * obsoleted record. This keeps logic (e.g. MUR_TCOM_TOKEN_PROCESSING) faster
						 * by avoiding if checks (of whether the rectype is TCOM or ZTCOM and accordingly
						 * taking the appropriate offset).
						 */
	unsigned short		filler_short;
	unsigned short		participants;	/* # of regions that wrote ZTCOM record in their jnl files for this fenced tn */
	jrec_suffix		suffix;
} struct_jrec_ztcom;

/* Below are different inctn_detail_*_t type definitions based on the inctn record opcode.
 * Each of them need to ensure the following.
 * 	a) SIZEOF(inctn_detail_*_t) is identical.
 * 	b) "opcode" member is at the same offset.
 * 	c) "suffix" is the last member.
 * Any new inctn_detail_*_t type definitions should have corresponding code changes in jnl_write_inctn_rec.c
 */
typedef struct
{
	block_id		blknum;		/* block that got upgraded or downgraded (opcode = inctn_blk*grd) */
	uint4			filler_uint4;
	unsigned short		filler_short;
	unsigned short		opcode;
	jrec_suffix		suffix;
} inctn_detail_blknum_t;

typedef struct
{
	int4			blks_to_upgrd_delta;	/* Delta to adjust csd->blks_to_upgrade (opcode = inctn_gdsfilext_*) */
	uint4			filler_uint4;
	unsigned short		filler_short;
	unsigned short		opcode;
	jrec_suffix		suffix;
} inctn_detail_blks2upgrd_t;

typedef union
{
	inctn_detail_blknum_t		blknum_struct;
	inctn_detail_blks2upgrd_t	blks2upgrd_struct;
} inctn_detail_t;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	inctn_detail_t		detail;
	/* jrec_suffix is already part of inctn_detail_t */
} struct_jrec_inctn;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	jnl_process_vector	process_vector[JPV_COUNT];
	int4			filler;
	jrec_suffix		suffix;
} struct_jrec_pini;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	uint4			filler;
	jrec_suffix		suffix;
} struct_jrec_pfin;

/* Following 3 are same structures. In case we change it in future, let's define them separately */
typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	seq_num			jnl_seqno;		/* must start at 8-byte boundary */
	seq_num			strm_seqno;		/* see "struct_jrec_upd" for comment on the purpose of this field */
	uint4			filler;
	jrec_suffix		suffix;
} struct_jrec_null;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	seq_num			jnl_seqno;		/* must start at 8-byte boundary */
	uint4			blks_to_upgrd;		/* blocks-to-upgrade counter at time of epoch */
	uint4			free_blocks;		/* free  blocks counter at time of epoch */
	uint4			total_blks;		/* total blocks counter at time of epoch */
	boolean_t		fully_upgraded;		/* cs_data->fully_upgraded at the time of epoch */
	seq_num			strm_seqno[MAX_SUPPL_STRMS];	/* seqno of each possible supplementary stream at epoch time.
								 * used by rollback to restore seqnos on the database.
								 */
	uint4			filler;			/* so as to make the EPOCH record aligned to 8 byte boundary */
	jrec_suffix		suffix;
} struct_jrec_epoch;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;
	seq_num			jnl_seqno;		/* must start at 8-byte boundary */
	uint4			filler;
	jrec_suffix		suffix;
} struct_jrec_eof;

typedef struct	/* fixed length */
{
	jrec_prefix		prefix;			/* 24 bytes */
	uint4			orig_total_blks;
	uint4			orig_free_blocks;
	uint4			total_blks_after_trunc;
	jrec_suffix		suffix;			/* 4 bytes */
} struct_jrec_trunc;

typedef union
{
	jrec_prefix			prefix;
	struct_jrec_upd			jrec_set_kill;	/* JRT_SET or JRT_KILL or JRT_ZTRIG record will use this format */
	struct_jrec_ztworm		jrec_ztworm;
	struct_jrec_lgtrig		jrec_lgtrig;
	struct_jrec_blk			jrec_pblk,
					jrec_aimg;
	struct_jrec_align		jrec_align;
	/** All below are fixed size and above are variable size records */
	struct_jrec_tcom		jrec_tcom;
	struct_jrec_ztcom		jrec_ztcom;
	struct_jrec_inctn               jrec_inctn;
	struct_jrec_pini		jrec_pini;
	struct_jrec_pfin		jrec_pfin;
	struct_jrec_null		jrec_null;
	struct_jrec_epoch		jrec_epoch;
	struct_jrec_eof			jrec_eof;
	struct_jrec_trunc		jrec_trunc;
} jnl_record;


/* Macro to access fixed size record's size */
#define	TCOM_RECLEN		SIZEOF(struct_jrec_tcom)
#define	ZTCOM_RECLEN		SIZEOF(struct_jrec_ztcom)
#define	INCTN_RECLEN		SIZEOF(struct_jrec_inctn)
#define	PINI_RECLEN		SIZEOF(struct_jrec_pini)
#define	PFIN_RECLEN		SIZEOF(struct_jrec_pfin)
#define	NULL_RECLEN		SIZEOF(struct_jrec_null)
#define	EPOCH_RECLEN		SIZEOF(struct_jrec_epoch)
#define	EOF_RECLEN 		SIZEOF(struct_jrec_eof)
#define TRUNC_RECLEN		SIZEOF(struct_jrec_trunc)
/* Macro to access variable size record's fixed part's size */
#define FIXED_ZTWORM_RECLEN	OFFSETOF(struct_jrec_ztworm, ztworm_str)
#define FIXED_LGTRIG_RECLEN	OFFSETOF(struct_jrec_lgtrig, lgtrig_str)
#define FIXED_UPD_RECLEN	OFFSETOF(struct_jrec_upd, mumps_node)
#define FIXED_ALIGN_RECLEN	SIZEOF(struct_jrec_align)
#define MIN_ALIGN_RECLEN	(FIXED_ALIGN_RECLEN + JREC_SUFFIX_SIZE)
#define FIXED_BLK_RECLEN 	OFFSETOF(struct_jrec_blk, blk_contents[0])
#define FIXED_PBLK_RECLEN 	OFFSETOF(struct_jrec_blk, blk_contents[0])
#define FIXED_AIMG_RECLEN 	OFFSETOF(struct_jrec_blk, blk_contents[0])
#define MIN_PBLK_RECLEN		(OFFSETOF(struct_jrec_blk, blk_contents[0]) + JREC_SUFFIX_SIZE)
#define MIN_AIMG_RECLEN		(OFFSETOF(struct_jrec_blk, blk_contents[0]) + JREC_SUFFIX_SIZE)

#define JREC_PREFIX_SIZE	SIZEOF(jrec_prefix)
#define JREC_SUFFIX_SIZE	SIZEOF(jrec_suffix)
#define MIN_JNLREC_SIZE		(FIXED_ALIGN_RECLEN + JREC_SUFFIX_SIZE)	/* An ALIGN record is the smallest possible record */
#define JREC_PREFIX_UPTO_LEN_SIZE	(OFFSETOF(jrec_prefix, pini_addr))

/* JNL_FILE_TAIL_PRESERVE macro indicates maximum number of bytes to ensure allocated at the end of the journal file
 * 	 to store the journal records that will be written whenever the journal file gets closed.
 * (i)	 Any process closing the journal file needs to write at most one PINI, one EPOCH, one PFIN and one EOF record
 *	 In case of wcs_recover extra INCTN will be written
 * (ii)	 We may need to give room for twice the above space to accommodate the EOF writing by a process that closes the journal
 *	 and the EOF writing by the first process that reopens it and finds no space left and switches to a new journal.
 * (iii) We may need to write one ALIGN record at the most since the total calculated from (i) and (ii) above is
 * 	   less than the minimum alignsize that we support (asserted before using JNL_FILE_TAIL_PRESERVE in macros below)
 * 	   The variable portion of this ALIGN record can get at the most equal to the maximum of the sizes of the
 * 	   PINI/EPOCH/PFIN/EOF record. We know PINI_RECLEN is maximum of EPOCH_RECLEN, PFIN_RECLEN, EOF_RECLEN (this
 *	   is in fact asserted in gvcst_init.c).
 */
#define	JNL_FILE_TAIL_PRESERVE	(MIN_ALIGN_RECLEN + (PINI_RECLEN + EPOCH_RECLEN + INCTN_RECLEN + 		\
								PFIN_RECLEN + EOF_RECLEN) * 2 + PINI_RECLEN)

typedef struct set_jnl_options_struct
{
	int			cli_journal, cli_enable, cli_on, cli_replic_on;
	boolean_t		alignsize_specified,
				allocation_specified,
				autoswitchlimit_specified,
				image_type_specified,	/* beofre/nobefore option specified */
				buffer_size_specified,
				epoch_interval_specified,
				extension_specified,
				filename_specified,
				sync_io_specified,
				yield_limit_specified;
	/* since jnl_create_info does not have following fields, we need them here */
	boolean_t		sync_io;
	int4			yield_limit;
} set_jnl_options;

/* rlist_state needed to be moved here to use with mu_set_reglist */
enum rlist_state {
	NONALLOCATED,
	ALLOCATED,
	DEALLOCATED
};

/* mu_set_reglist needed to be moved here for the journal specific fields */
/* ATTN: the first four items in this structure need to be identical to those
 *	 in structure tp_region in tp.h.
 */
typedef struct mu_set_reglist
{
	struct mu_set_reglist	*fPtr;		/* all fields after this are used for mupip_set_journal.c */
	gd_region		*reg;
	char			unique_id[UNIQUE_ID_SIZE];
	enum rlist_state	state;
	sgmnt_data_ptr_t 	sd;
	bool			exclusive;	/* standalone access is required for this region */
	int			fd;
	enum jnl_state_codes	jnl_new_state;
	enum repl_state_codes	repl_new_state;
	boolean_t		before_images;
} mu_set_rlist;

typedef	void	(*pini_addr_reset_fnptr)(sgmnt_addrs *csa);

typedef struct
{
	token_num			mur_jrec_seqno;		/* This is jnl_seqno of the current record that backward
								 * recovery/rollback is playing in its forward phase.
								 */
	token_num			mur_jrec_strm_seqno;	/* This is the strm_seqno of the current record that backward
								 * recovery/rollback is playing in its forward phase.
								 */
	unsigned short			filler_short;
	unsigned short			mur_jrec_participants;
	jnl_tm_t			gbl_jrec_time;
	jnl_tm_t			mur_tp_resolve_time;	/* tp resolve time as determined by journal recovery.
								 * Time of the point upto which a region will be processed for
								 * TP token resolution for backward or forward recover.
								 * Note : This is what prevents user to change system time.
								 */
	boolean_t			forw_phase_recovery;
	boolean_t			mur_rollback;	/* a copy of mur_options.rollback to be accessible to runtime code */
	boolean_t			mupip_journal;	/* the current command is a MUPIP JOURNAL command */
	boolean_t			dont_reset_gbl_jrec_time;	/* Do not reset gbl_jrec_time */
	pini_addr_reset_fnptr		mur_pini_addr_reset_fnptr;	/* function pointer to invoke "mur_pini_addr_reset" */
	uint4				cumul_jnl_rec_len;		/* cumulative length of the replicated journal records
									 * for the current TP or non-TP transaction */
	boolean_t			wait_for_jnl_hard;
	uint4				tp_ztp_jnl_upd_num;	/* Incremented whenever a journaled update happens inside of
								 * TP or ZTP. Copied over to the corresponding journal record
								 * to record the sequence of all updates inside TP/ZTP transaction.
								 */
	uint4				mur_jrec_nodeflags;	/* copy of "nodeflags" from jnl record currently being played */
#	ifdef GTM_TRIGGER
	unsigned char			*prev_ztworm_ptr;	/* Non-NULL if at least one ztwormhole record was successfully
								 * formatted in this transaction. Note that ZTWORMHOLE records are
								 * formatted ONLY in case of journaled & replicated databases.
								 * 1. If replicated database is unencrypted, this points to
								 *	jfb->buff + FIXED_UPD_RECLEN
								 * 2. If replicated database is encrypted, this points to
								 *	jfb->alt_buff + FIXED_UPD_RECLEN
								 * If no ztwormhole record is yet formatted, then points to NULL
								 */
	unsigned char			*save_ztworm_ptr;	/* copy of prev_ztworm_ptr saved until we know for sure whether
								 * a ZTWORMHOLE journal record will be written or not.
								 */
#	endif
#	ifdef DEBUG
	boolean_t			mur_fences_none;	/* a copy of mur_options.fences to be accessible to runtime code */
	uint4				cumul_index;
	uint4				cu_jnl_index;
	uint4				max_tp_ztp_jnl_upd_num;	/* Max of all <jgbl.tp_ztp_jnl_upd_num> values processed in this
								 * potentially multi-region transaction. Used only by jnl recovery.
								 */
	boolean_t			mur_options_forward;	/* a copy of mur_options.forward to be accessible to GT.M runtime */
	boolean_t			in_mupjnl;		/* TRUE if caller is a MUPIP JOURNAL command */
#	endif
	boolean_t			onlnrlbk;		/* TRUE if ONLINE ROLLBACK */
	boolean_t			mur_extract;		/* a copy of mur_options.extr[0] to be accessible to GTM runtime*/
	boolean_t			save_dont_reset_gbl_jrec_time;	/* save a copy of dont_reset_gbl_jrec_time */
	boolean_t			mur_update;		/* a copy of mur_options.update to be accessible to GTM runtime */
} jnl_gbls_t;

#define	IN_PHASE2_JNL_COMMIT(CSA)	(CSA->t_commit_crit)

/* If a transaction's total journal record size exceeds the journal buffer size (minus one filesystem-block-size
 * just to be safe) we might have to flush a part of the transaction's journal records before writing the remaining
 * part. For this reason, we write the entire transaction's journal records while holding crit as otherwise it gets
 * tricky to handle process kills after it has written a part of the journal records but not all.
 */
#define	IS_PHASE2_JNL_COMMIT_NEEDED_IN_CRIT(JBP, JREC_LEN)	((JREC_LEN + (JBP)->fs_block_size) > (JBP)->size)

#define	JB_FREEADDR_APPROPRIATE(IN_PHASE2, JPC, JB)	(IN_PHASE2 ? JPC->phase2_freeaddr : JB->rsrv_freeaddr)
#define	JB_FREE_APPROPRIATE(IN_PHASE2, JPC, JB)		(IN_PHASE2 ? JPC->phase2_free : JB->rsrv_free)
#define	JB_CURR_TN_APPROPRIATE(IN_PHASE2, JPC, CSA)	(IN_PHASE2 ? JPC->curr_tn : CSA->ti->curr_tn)

#define	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(IDX, MAX_INDEX)	assert((0 <= IDX) && (MAX_INDEX > IDX))

#define	INCR_PHASE2_COMMIT_INDEX(IDX, PHASE2_COMMIT_ARRAY_SIZE)	\
MBSTART {							\
	if (PHASE2_COMMIT_ARRAY_SIZE == ++IDX)			\
		IDX = 0;					\
} MBEND								\

#define	DECR_PHASE2_COMMIT_INDEX(IDX, PHASE2_COMMIT_ARRAY_SIZE)	\
MBSTART {							\
	if (0 == IDX--)						\
		IDX = PHASE2_COMMIT_ARRAY_SIZE - 1;		\
} MBEND								\

/* Sets jbp->rsrv_freeaddr & jbp->rsrv_free. They need to be kept in sync at all times */
#define	SET_JBP_RSRV_FREEADDR(JBP, RSRV_FREEADDR)	\
{							\
	assert(JBP->freeaddr <= RSRV_FREEADDR);		\
	JBP->rsrv_freeaddr = RSRV_FREEADDR;		\
	JBP->rsrv_free = (RSRV_FREEADDR) % JBP->size;	\
}

#define SET_JPC_PHASE2_FREEADDR(JPC, JBP, FREEADDR)	\
{							\
	JPC->phase2_freeaddr = FREEADDR;		\
	JPC->phase2_free = (FREEADDR) % JBP->size;	\
}

#define	UPDATE_JBP_RSRV_FREEADDR(CSA, JPC, JBP, JPL, RLEN, INDEX, IN_PHASE2, JNL_SEQNO, STRM_SEQNO, REPLICATION)	\
MBSTART {														\
	uint4			rsrv_freeaddr;										\
	int			nextIndex, endIndex;									\
	jbuf_phase2_in_prog_t	*phs2cmt;										\
															\
	GBLREF	uint4		process_id;										\
	GBLREF	uint4		dollar_tlevel;										\
															\
	assert(CSA->now_crit);												\
	/* Allocate a slot. But before that, check if the slot array is full.						\
	 * endIndex + 1 == first_index implies full.	Note: INCR_PHASE2_COMMIT_INDEX macro does the + 1		\
	 * endIndex     == first_index implies empty.									\
	 */														\
	endIndex = JBP->phase2_commit_index2;										\
	nextIndex = endIndex;												\
	INCR_PHASE2_COMMIT_INDEX(nextIndex, JNL_PHASE2_COMMIT_ARRAY_SIZE);						\
	while (nextIndex == JBP->phase2_commit_index1)									\
	{	/* Slot array is full. Wait for phase2 to finish. */							\
		jnl_phase2_cleanup(CSA, JBP);										\
		if (nextIndex != JBP->phase2_commit_index1)								\
			break;												\
		BG_TRACE_PRO_ANY(CSA, jnlbuff_phs2cmt_array_full);							\
		SLEEP_USEC(1, FALSE);											\
	}														\
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(endIndex, JNL_PHASE2_COMMIT_ARRAY_SIZE);				\
	phs2cmt = &JBP->phase2_commit_array[endIndex];									\
	phs2cmt->process_id = process_id;										\
	assert(JPC->curr_tn == CSA->ti->curr_tn);									\
	phs2cmt->curr_tn = JPC->curr_tn;										\
	assert(!REPLICATION || JNL_SEQNO);										\
	phs2cmt->jnl_seqno = JNL_SEQNO;											\
	assert(!REPLICATION												\
		|| !((repl_inst_hdr_ptr_t)((sm_uc_ptr_t)JPL + ((jnlpool_ctl_ptr_t)JPL)->filehdr_off))->is_supplementary	\
		|| STRM_SEQNO);												\
	phs2cmt->strm_seqno = STRM_SEQNO;										\
	rsrv_freeaddr = JBP->rsrv_freeaddr;										\
	phs2cmt->start_freeaddr = rsrv_freeaddr;									\
	phs2cmt->tot_jrec_len = RLEN;											\
	/* The below assert ensures "jnl_phase2_salvage" can definitely replace this transaction's journal		\
	 * records with a combination of JRT_NULL/JRT_INCTN/JRT_ALIGN records. See comment there for why		\
	 * this is necessary and how this assert achieves that.								\
	 * In case of the forward phase of MUPIP JOURNAL RECOVER or ROLLBACK, replication would be turned off		\
	 * but it is possible it plays forward a pre-existing NULL record. Account for that below.			\
	 */														\
	assert(!IN_PHASE2												\
		|| (!REPLICATION && ((RLEN == INCTN_RECLEN) || (RLEN >= (INCTN_RECLEN + MIN_ALIGN_RECLEN))))		\
		|| ((REPLICATION || jgbl.forw_phase_recovery)								\
				&& ((RLEN == NULL_RECLEN) || (RLEN >= (NULL_RECLEN + MIN_ALIGN_RECLEN)))));		\
	phs2cmt->pini_addr = JPC->pini_addr;										\
	phs2cmt->jrec_time = jgbl.gbl_jrec_time;									\
	phs2cmt->in_phase2 = IN_PHASE2;											\
	phs2cmt->replication = REPLICATION;										\
	phs2cmt->write_complete = FALSE;										\
	rsrv_freeaddr += RLEN;												\
	assert(rsrv_freeaddr <= JBP->next_align_addr);									\
	assert(rsrv_freeaddr > JBP->freeaddr);										\
	INDEX = endIndex;												\
	SET_JBP_RSRV_FREEADDR(JBP, rsrv_freeaddr);									\
	SHM_WRITE_MEMORY_BARRIER; /* see corresponding SHM_READ_MEMORY_BARRIER in "jnl_phase2_cleanup" */		\
	JBP->phase2_commit_index2 = nextIndex;										\
} MBEND

#define	JNL_PHASE2_WRITE_COMPLETE(CSA, JBP, INDEX, NEW_FREEADDR)				\
MBSTART {											\
	jbuf_phase2_in_prog_t	*phs2cmt;							\
												\
	GBLREF	uint4		process_id;							\
												\
	phs2cmt = &JBP->phase2_commit_array[INDEX];						\
	assert(phs2cmt->process_id == process_id);						\
	assert(FALSE == phs2cmt->write_complete);						\
	assert(((phs2cmt->start_freeaddr + phs2cmt->tot_jrec_len) == NEW_FREEADDR)		\
		|| (gtm_white_box_test_case_enabled						\
			&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));	\
	phs2cmt->write_complete = TRUE;								\
	/* Invoke "jnl_phase2_cleanup" sparingly as it calls "grab_latch". So we do it twice.	\
	 * Once at half-way mark and once when a wrap occurs.					\
	 */											\
	if (!INDEX || ((JNL_PHASE2_COMMIT_ARRAY_SIZE / 2) == INDEX))				\
		jnl_phase2_cleanup(CSA, JBP);							\
} MBEND

#define	JNL_PHASE2_CLEANUP_IF_POSSIBLE(CSA, JBP)						\
MBSTART {											\
	if (JBP->phase2_commit_index1 != JBP->phase2_commit_index2)				\
		jnl_phase2_cleanup(CSA, JBP);	/* There is something to be cleaned up */	\
} MBEND

#define	NEED_TO_FINISH_JNL_PHASE2(JRS)	((NULL != JRS) && JRS->tot_jrec_len)

#define	FINISH_JNL_PHASE2_IN_JNLBUFF(CSA, JRS)										\
MBSTART {														\
	assert(NEED_TO_FINISH_JNL_PHASE2(JRS));										\
	jnl_write_phase2(CSA, JRS);			/* Mark jnl record writing into jnlbuffer as complete */	\
	assert(!NEED_TO_FINISH_JNL_PHASE2(JRS));									\
} MBEND

#define	FINISH_JNL_PHASE2_IN_JNLPOOL_IF_NEEDED(REPLICATION, JNLPOOL)							\
MBSTART {														\
	if (REPLICATION && JNLPOOL->jrs.tot_jrec_len)									\
	{														\
		JPL_PHASE2_WRITE_COMPLETE(JNLPOOL);	/* Mark jnl record writing into jnlpool   as complete */	\
		assert(0 == JNLPOOL->jrs.tot_jrec_len);									\
	}														\
} MBEND

#define	NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(CSA, JRS, REPLICATION, JNLPOOL)					\
MBSTART {														\
	FINISH_JNL_PHASE2_IN_JNLBUFF(CSA, JRS);				/* Step CMT16 (if BG), Step CMT06a (if MM) */	\
	assert(!REPLICATION || (!csa->jnlpool || (csa->jnlpool == JNLPOOL)));						\
	FINISH_JNL_PHASE2_IN_JNLPOOL_IF_NEEDED(REPLICATION, JNLPOOL);	/* Step CMT17 (if BG), Step CMT06b (if MM) */	\
} MBEND

#define	TP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(JNL_FENCE_CTL, REPLICATION, JNLPOOL)				\
MBSTART {														\
	sgmnt_addrs	*csa;												\
	sgm_info	*si;												\
															\
	for (csa = JNL_FENCE_CTL.fence_list;  JNL_FENCE_LIST_END != csa;  csa = csa->next_fenced)			\
	{														\
		si = csa->sgm_info_ptr;											\
		assert(si->update_trans);										\
		jrs = si->jbuf_rsrv_ptr;										\
		assert(JNL_ALLOWED(csa));										\
		assert(!REPLICATION || (!csa->jnlpool || (csa->jnlpool == JNLPOOL)));					\
		if (NEED_TO_FINISH_JNL_PHASE2(jrs))									\
			FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);		/* Step CMT16 (if BG) */			\
	}														\
	FINISH_JNL_PHASE2_IN_JNLPOOL_IF_NEEDED(replication, JNLPOOL);	/* Step CMT17 (if BG) */			\
} MBEND

/* BEGIN : Structures used by "jnl_write_reserve" */
typedef struct
{
	enum jnl_record_type	rectype;	/* equivalent to jrec_prefix.jrec_type */
	uint4			reclen;		/* equivalent to jrec_prefix.forwptr */
	void			*param1;
} jrec_rsrv_elem_t;

typedef struct
{
	uint4			alloclen;	/* # of "jrec_rsrv_elem_t" structures allocated in "jrs_array" array */
	uint4			usedlen;	/* # of "jrec_rsrv_elem_t" structures currently used in "jrs_array" array */
	uint4			tot_jrec_len;	/* Total length (in bytes) of jnl records that will be used up by this array */
	uint4			phase2_commit_index;	/* index into corresponding jb->phase2_commit_array entry */
	jrec_rsrv_elem_t	*jrs_array;	/* Pointer to array of "jrec_rsrv_elem_t" structures currently allocated */
} jbuf_rsrv_struct_t;

#define	ALLOC_JBUF_RSRV_STRUCT(JRS, CSA)								\
MBSTART {												\
	JRS = (jbuf_rsrv_struct_t *)malloc(SIZEOF(jbuf_rsrv_struct_t));					\
	(JRS)->alloclen = 0;			/* initialize "alloclen" */				\
	(JRS)->jrs_array = NULL;									\
	REINIT_JBUF_RSRV_STRUCT(JRS, CSA, CSA->jnl, NULL);/* initialize "usedlen" and "tot_jrec_len" */	\
} MBEND

#define	REINIT_JBUF_RSRV_STRUCT(JRS, CSA, JPC, JBP)				\
MBSTART {									\
	uint4	freeaddr;							\
										\
	(JRS)->usedlen = 0;							\
	(JRS)->tot_jrec_len = 0;						\
	(JPC)->curr_tn = CSA->ti->curr_tn;					\
	if (NULL != JBP)							\
	{									\
		freeaddr = ((jnl_buffer_ptr_t)JBP)->rsrv_freeaddr;		\
		JPC->phase2_freeaddr = freeaddr;				\
	}									\
} MBEND

#define	UPDATE_JRS_RSRV_FREEADDR(CSA, JPC, JBP, JRS, JPL, JNL_FENCE_CTL, REPLICATION)			\
MBSTART {												\
	SET_JNLBUFF_PREV_JREC_TIME(JBP, jgbl.gbl_jrec_time, DO_GBL_JREC_TIME_CHECK_TRUE);		\
		/* Keep jb->prev_jrec_time up to date */						\
	assert(JNL_ENABLED(CSA));									\
	assert(JBP->rsrv_freeaddr >= JBP->freeaddr);							\
	assert(0 < JRS->tot_jrec_len);									\
	UPDATE_JBP_RSRV_FREEADDR(CSA, JPC, JBP, JPL, JRS->tot_jrec_len, JRS->phase2_commit_index, TRUE,	\
					JNL_FENCE_CTL.token, JNL_FENCE_CTL.strm_seqno, REPLICATION);	\
} MBEND

#define	FREE_JBUF_RSRV_STRUCT(JRS)		\
MBSTART {					\
	assert(NULL != JRS);			\
	if (NULL != JRS)			\
	{					\
		if (NULL != JRS->jrs_array)	\
			free(JRS->jrs_array);	\
		free(JRS);			\
		JRS = NULL;			\
	}					\
} MBEND

#define	INIT_NUM_JREC_RSRV_ELEMS	4

/* END  : Structures used by "jnl_write_reserve" */

#define JNL_SHARE_SIZE(X)	(JNL_ALLOWED(X) ? 							\
				(ROUND_UP(JNL_NAME_EXP_SIZE + SIZEOF(jnl_buffer), OS_PAGE_SIZE)		\
                                + ROUND_UP(((sgmnt_data_ptr_t)X)->jnl_buffer_size * DISK_BLOCK_SIZE, 	\
					OS_PAGE_SIZE) + OS_PAGE_SIZE) : 0)

/* pass address of jnl_buffer to get address of expanded jnl file name */
#define JNL_GDID_PVT(CSA)	((CSA)->jnl->fileid)
#define JNL_GDID_PTR(CSA)	((gd_id_ptr_t)(&((CSA)->nl->jnl_file.u)))

/* Note that since "cycle" (in jpc and jb below) can rollover the 4G limit back to 0, it should
 * only be used to do "!=" checks and never to do ordered checks like "<", ">", "<=" or ">=".
 * Use JNL_FILE_SWITCHED when only JPC is available. Use JNL_FILE_SWITCHED2 when JPC and JB (i.e. JPC->jnl_buff)
 * are both available. The latter saves a dereference in t_end/tp_tend (every drop helps while inside crit).
 */
#define JNL_FILE_SWITCHED(JPC) 		((JPC)->cycle != (JPC)->jnl_buff->cycle)
#define JNL_FILE_SWITCHED2(JPC, JB) 	((JPC)->cycle != (JB)->cycle)

/* The jrec_len of 0 causes a switch. */
#define SWITCH_JNL_FILE(JPC)	jnl_file_extend(JPC, 0)

#define REG_STR		"region"
#define FILE_STR	"database file"

/* Given a journal record, get_jnl_seqno returns the jnl_seqno field
 * Now all replication type records, EOF and EPOCH have the jnl_seqno at the same offset.
 * Modify the macro GET_JNL_SEQNO if offset of jnl_seqno is changed for any journal records
 */
#define GET_JNL_SEQNO(j)	(((jnl_record *)(j))->jrec_null.jnl_seqno)
#define GET_STRM_SEQNO(j)	(((jnl_record *)(j))->jrec_null.strm_seqno)
#define GET_REPL_JNL_SEQNO(j)	(IS_REPLICATED(((jrec_prefix *)j)->jrec_type) ? GET_JNL_SEQNO(j) : 0)

/* Note: JRT_ALIGN record does not have a "prefix.tn" or "prefix.pini_addr" field (all other jnl records have it)
 * and has "prefix.checksum" at a different offset. Hence the below GET_JREC* and SET_JREC* macros. IF the caller knows
 * for sure that "RECTYPE" is not a JRT_ALIGN, then it can directly use "prefix.tn" etc. ELSE it has to use the below macros.
 */
#define GET_JREC_TN(JNLREC, RECTYPE)	((JRT_ALIGN != RECTYPE)									\
						? (DBG_ASSERT(TN_INVALID != JNLREC->prefix.tn) JNLREC->prefix.tn) : TN_INVALID)
/* A JRT_ALIGN record does not have a "pini_addr" field so treat it as pointing to the first PINI record in the journal file
 * for the purposes of a journal extract etc.
 */
#define GET_JREC_PINI_ADDR(JNLREC, RECTYPE) ((JRT_ALIGN != RECTYPE) ? JNLREC->prefix.pini_addr : JNL_FILE_FIRST_RECORD)
#define GET_JREC_CHECKSUM(JNLREC, RECTYPE) ((JRT_ALIGN != RECTYPE)							\
						? JNLREC->prefix.checksum : ((struct_jrec_align *)JNLREC)->checksum)
#define	SET_JREC_CHECKSUM(JNLREC, RECTYPE, CHECKSUM)			\
MBSTART {								\
	if (JRT_ALIGN != RECTYPE)					\
		JNLREC->prefix.checksum = CHECKSUM;			\
	else								\
		((struct_jrec_align *)JNLREC)->checksum = CHECKSUM;	\
} MBEND

/* For MUPIP JOURNAL -ROLLBACK, getting the strm_reg_seqno from the file header is not as straightforward
 * as accessing csd->strm_reg_seqno[idx]. This is because it increments this field in mur_output_record even
 * before we reach t_end/tp_tend. That is done for convenience of the implementation. But this assumes that the
 * commit has actually completed. Therefore, in case we need to invoke jnl_file_extend() inside t_end/tp_tend even
 * before the commit, we would see an incorrect value of csd->strm_reg_seqno[idx]. In that case, we use the
 * global variable jgbl.mur_jrec_strm_seqno to identify if the strm_reg_seqno[idx] value is 1 more than that and
 * if so return 1 lesser than that as the real strm_reg_seqno[idx]. This is used by routines that write journal
 * records (EPOCH, jfh->strm_end_seqno etc.) to write the correct strm_seqno. Not doing so will cause the strm_seqno
 * to be higher than necessary and confuse everything else (including rollback) as far as replication is concerned.
 * Note: We check for process_exiting to differentiate between calls made from mur_close_files() to before. Once we
 * reach mur_close_files, we should no longer be in an active transaction and so we don't need to make any adjustments.
 * VMS does not support supplementary instances so the below macro does not apply there at all.
 */
#define	MUR_ADJUST_STRM_REG_SEQNO_IF_NEEDED(CSD, DST)							\
{													\
	int			strm_num;								\
	seq_num			strm_seqno;								\
													\
	GBLREF	int		process_exiting;							\
													\
	if (jgbl.mur_jrec_strm_seqno && !process_exiting)						\
	{												\
		assert(jgbl.mur_rollback);								\
		/* This macro should be called only when journal records are about to be written	\
		 * and that is not possible in case of a FORWARD rollback. Assert that.			\
		 */											\
		assert(!jgbl.mur_options_forward);							\
		strm_seqno = jgbl.mur_jrec_strm_seqno;							\
		strm_num = GET_STRM_INDEX(strm_seqno);							\
		strm_seqno = GET_STRM_SEQ60(strm_seqno);						\
		if (CSD->strm_reg_seqno[strm_num] == (strm_seqno + 1))					\
		{											\
			assert(DST[strm_num] == (strm_seqno + 1));					\
			DST[strm_num] = strm_seqno;							\
		}											\
	}												\
}

/* In t_end(), we need to write the after-image if DSE or mupip recover/rollback is playing it.
 * But to write it out, we should have it already built before bg_update().
 * Hence, we pre-build the block here itself before invoking t_end().
 */
#define	BUILD_AIMG_IF_JNL_ENABLED(CSD, TN)									\
{														\
	GBLREF	cw_set_element   	cw_set[];								\
	GBLREF	unsigned char		cw_set_depth;								\
	GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;							\
														\
	cw_set_element			*cse;									\
														\
	if (JNL_ENABLED(CSD))											\
	{													\
		assert(1 == cw_set_depth); /* Only DSE uses this macro and it updates one block at a time */	\
		cse = (cw_set_element *)(&cw_set[0]);								\
		cse->new_buff = (unsigned char *)non_tp_jfb_ptr->buff;						\
		gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, TN);						\
		cse->done = TRUE;										\
	}													\
}

/* In Unix, the journal file header size is currently set to 64K so it is aligned with any possible filesystem block size
 * known at this point. This will help us do aligned writes to the journal file header as well as the journal file contents
 * without needing to mix both of them in the same aligned disk write. In VMS, we continue with 512-byte alignment so no change.
 * Note that the journal_file_header structure is only 2K currently and is captured using the REAL_JNL_HDR_LEN macro while
 * the padded 64K file header is captured using the JNL_HDR_LEN macro. Use either one as appropriate in the code. Both of them
 * are identical in VMS where it is currently 2K.
 */
#define	REAL_JNL_HDR_LEN	SIZEOF(jnl_file_header)
#define	JNL_HDR_LEN		64 * 1024
#define	JNL_FILE_FIRST_RECORD	JNL_HDR_LEN

/* Minimum possible journal file size */
#define MIN_JNL_FILE_SIZE	(JNL_HDR_LEN + PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN)

/* maximum required journal file size (in 512-byte blocks), if the current transaction was the only one in a fresh journal file */
#define	MAX_REQD_JNL_FILE_SIZE(tot_jrec_size) DIVIDE_ROUND_UP((tot_jrec_size + MIN_JNL_FILE_SIZE), DISK_BLOCK_SIZE)

/* this macro aligns the input size to account that journal file sizes can increase only in multiples of the extension size */
#define	ALIGNED_ROUND_UP(tmp_tot_jrec_size, jnl_alq, jnl_deq)					\
	(((tmp_tot_jrec_size) <= (jnl_alq) || !(jnl_deq))					\
	 	? (jnl_alq) 									\
		: ((jnl_alq) + ROUND_UP((tmp_tot_jrec_size) - (jnl_alq), (jnl_deq))))

/* this macro aligns the input size to account that journal file sizes can increase only in multiples of the extension size */
#define	ALIGNED_ROUND_DOWN(tmp_tot_jrec_size, jnl_alq, jnl_deq)					\
	(((tmp_tot_jrec_size) <= (jnl_alq) || !(jnl_deq))					\
	 	? (jnl_alq) 									\
		: ((jnl_alq) + ROUND_DOWN((tmp_tot_jrec_size) - (jnl_alq), (jnl_deq))))

/* the following macro uses 8-byte quantities (gtm_uint64_t) to perform additions that might cause a 4G overflow */
#define	DISK_BLOCKS_SUM(freeaddr, jrec_size)	DIVIDE_ROUND_UP((((gtm_uint64_t)(freeaddr)) + (jrec_size)), DISK_BLOCK_SIZE)

/* For future portability JNLBUFF_ALLOC is defined in jnl.h instead of jnlsp.h */
#define JPC_ALLOC(csa)								\
{										\
	csa->jnl = (jnl_private_control *)malloc(SIZEOF(*csa->jnl));		\
	memset(csa->jnl, 0, SIZEOF(*csa->jnl));					\
}
#define	ASSERT_JNLFILEID_NOT_NULL(csa)						\
{										\
	assert(0 != csa->nl->jnl_file.u.inode);					\
	assert(0 != csa->nl->jnl_file.u.device);				\
}
#define NULLIFY_JNL_FILE_ID(csa) 			\
{							\
		csa->nl->jnl_file.u.inode = 0;		\
		csa->nl->jnl_file.u.device = 0;		\
}
#define JNL_INIT(csa, reg, csd)													\
{																\
	csa->jnl_state = csd->jnl_state;											\
	csa->jnl_before_image = csd->jnl_before_image;										\
	csa->repl_state = csd->repl_state;											\
	if JNL_ALLOWED(csa)													\
	{															\
		JPC_ALLOC(csa);													\
		csa->jnl->region = reg;												\
		csa->jnl->jnl_buff = (jnl_buffer_ptr_t)((sm_uc_ptr_t)(csa->nl) + NODE_LOCAL_SPACE(csd) + JNL_NAME_EXP_SIZE);	\
		csa->jnl->channel = NOJNL;											\
	} else															\
		csa->jnl = NULL;												\
}
#define	JNL_FD_CLOSE(CHANNEL, RC)										\
{														\
	fd_type	lcl_channel;											\
														\
	/* Reset incoming channel BEFORE closing it. This way, if we get interrupted BEFORE the close but	\
	 * after we have reset channel, we could at most end up with a file descriptor leak. Doing it the	\
	 * other way around could cause us to close the channel but yet have a dangling pointer to it that	\
	 * could result in more than one close of the same file descriptor where the second close could		\
	 * be on some other valid open file descriptor.								\
	 */													\
	lcl_channel = CHANNEL;											\
	CHANNEL = NOJNL;											\
	CLOSEFILE_RESET(lcl_channel, RC);	/* resets "lcl_channel" to FD_INVALID */			\
	assert(SS_NORMAL == RC);										\
}

#define MAX_EPOCH_DELAY		30
#define PREFIX_ROLLED_BAK	"rolled_bak_"
#define REC_TOKEN(jnlrec)	((struct_jrec_upd *)jnlrec)->token_seq.token
#define REC_JNL_SEQNO(jnlrec)	((struct_jrec_upd *)jnlrec)->token_seq.jnl_seqno
#define REC_LEN_FROM_SUFFIX(ptr, reclen)	((jrec_suffix *)((unsigned char *)ptr + reclen - JREC_SUFFIX_SIZE))->backptr

/* The below macro now relies on MAX_STRLEN value rather than on CSD->blk_size used previously because
 * with nodes spanning blocks journal records might be comprised of several blocks, with the limit of
 * MAX_STRLEN for the actual database record. But that only takes care of the value part of the record.
 * The key can still be MAX_KEY_SZ long. So take that into account as well.
 */
#define	JNL_MAX_SET_KILL_RECLEN(CSD)	(uint4)ROUND_UP2((FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE)				\
						+ MAX_KEY_SZ + MAX_STRLEN						\
						+ SIZEOF(jnl_str_len_t) + SIZEOF(mstr_len_t), JNL_REC_START_BNDRY)

#define	JNL_MAX_PBLK_RECLEN(CSD)	(uint4)ROUND_UP2(MIN_PBLK_RECLEN + (CSD)->blk_size, JNL_REC_START_BNDRY)

/* Macro to compute the maximum possible journal record length in the journal file.
 * In order to compute the maximum jnl record length, note that an align record is written whenever
 * a <non-align-record + an-align-record> would cause the jnl file offset to move past an aligned boundary.
 * Therefore after computing the maximum possible non-align-jnl-record-length, we need to add MIN_ALIGN_RECLEN
 * as this is the maximum possible align-jnl-record-length and should be the eventual max_jrec_len.
 */
#define JNL_MAX_RECLEN(JINFO, CSD)										\
{														\
	/* This macro used to compare the value returned from JNL_MAX_SET_KILL_RECLEN with that from		\
	 * JNL_MAX_PBLK_RECLEN and, in case of triggers, MAX_ZTWORK_JREC_LEN. However, in the current design	\
	 * max_logi_reclen includes MAX_STR_LEN as one of its summants, thus always exceeding both		\
	 * MAX_ZTWORK_JREC_LEN and JNL_MAX_PBLK_RECLEN.								\
	 *													\
	 * A logical record is a SET/KILL record. The SET could be as big as (CSD)->max_rec_size, but since	\
	 * csd->max_rec_size can be changed independent of journal file creation (through DSE), we consider	\
	 * the max possible record size that can be ever produced.						\
	 */													\
	(JINFO)->max_jrec_len = JNL_MAX_SET_KILL_RECLEN(CSD) + MIN_ALIGN_RECLEN;				\
}

/* Macro that checks that the region seqno in the filehdr is never more than the seqno in the journal pool */
#define	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(CSA, JNLPOOL)										\
{	/* The seqno in the file header should be at most 1 greater than that in the journal pool.				\
	 * See step (5) of of commit logic flow in secshr_db_clnup.c for why. Assert that.					\
	 * If CSA->jnlpool is NULL it means there have been no updates to the region so no jnlpool is attached.			\
	 */															\
	assert((!CSA->jnlpool || !JNLPOOL || !JNLPOOL->jnlpool_ctl)								\
			|| (CSA->hdr->reg_seqno <= (JNLPOOL->jnlpool_ctl->jnl_seqno + 1)));					\
}
/* Given the record size, construct an IV to be used for a subsequent encryption or decryption operation. Currently, the maximum IV
 * length our encryption plug-in supports is 16 bytes, and we only have three bytes of information suitable for an IV at the
 * encryption time (explained below), so just fit four copies of a three-byte integer into the IV array.
 */
# define PREPARE_LOGICAL_REC_IV(REC_SIZE, IV_ARRAY)							\
{													\
	uint4 *iv_ptr, iv_val;										\
													\
	/* Encryption happens prior to grabbing crit, when the only initialized fields of a journal	\
	 * record prefix are jrec_type and forwptr, collectively occupying the first four bytes of the	\
	 * jrec_prefix structure. However, if a JRT_TZTWORM-type record remains unused in a particular	\
	 * transaction, it is removed, while the preceding record's type is modified from UUPD to TUPD, \
	 * via the REMOVE_ZTWORM_JFB_IF_NEEDED macro. Since we cannot be changing IV after encryption,	\
	 * jrec_type does not qualify. Therefore, we are left with three bytes taken by forwptr.	\
	 */												\
	assert((ARRAYSIZE(IV_ARRAY) == GTM_MAX_IV_LEN) && (GTM_MAX_IV_LEN == 4 * SIZEOF(uint4)));	\
	iv_ptr = (uint4 *)IV_ARRAY;									\
	iv_val = REC_SIZE;										\
	*iv_ptr++ = iv_val;										\
	*iv_ptr++ = iv_val;										\
	*iv_ptr++ = iv_val;										\
	*iv_ptr = iv_val;										\
}

/* Decrypt a logical journal record. */
# define MUR_DECRYPT_LOGICAL_RECS(MUMPS_NODE_PTR, USE_NON_NULL_IV, REC_SIZE, KEY_HANDLE, RC)		\
{													\
	int	span_length;										\
	char	iv[GTM_MAX_IV_LEN];									\
													\
	RC = 0;												\
	assert(FIXED_UPD_RECLEN == FIXED_ZTWORM_RECLEN);						\
	assert(FIXED_UPD_RECLEN == FIXED_LGTRIG_RECLEN);						\
	span_length = REC_SIZE - FIXED_UPD_RECLEN - JREC_SUFFIX_SIZE;					\
	if (USE_NON_NULL_IV)										\
		PREPARE_LOGICAL_REC_IV(REC_SIZE, iv);							\
	GTMCRYPT_DECRYPT(NULL, USE_NON_NULL_IV, KEY_HANDLE, (char *)MUMPS_NODE_PTR, span_length, NULL,	\
			iv, GTM_MAX_IV_LEN, RC);							\
}

/* The following define an appendix message, used along with JNLBUFFREGUPD and JNLBUFFDBUPD messages in
 * various places, as well as its length, allowing for six digits for both lower and upper journal buffer
 * size limits, even though neither is expected to have more than five in the near future. */
#define JNLBUFFUPDAPNDX		"The previous value was outside the allowable range of %d to %d"
#define JNLBUFFUPDAPNDX_SIZE	(SIZEOF(JNLBUFFUPDAPNDX) - 4 + (2 * 6))

/* Yields a portable value for the minimum journal buffer size */
#define JNL_BUFF_PORT_MIN(CSD)		(JNL_BUFFER_MIN)

/* Defines the increment value for journal buffer size's rounding-up */
#define JNL_BUFF_ROUND_UP_STEP(CSD)	(MIN(MAX_IO_BLOCK_SIZE, CSD->blk_size) / DISK_BLOCK_SIZE)

/* Rounds up the passed journal buffer value and assigns it to the specified variable */
#define ROUND_UP_JNL_BUFF_SIZE(DEST, VALUE, CSD)							\
{													\
	DEST = ROUND_UP(VALUE, JNL_BUFF_ROUND_UP_STEP(CSD));						\
}

/* Rounds up the minimum journal buffer value and assigns it to the specified variable */
#define ROUND_UP_MIN_JNL_BUFF_SIZE(DEST, CSD)								\
{													\
	DEST = ROUND_UP(JNL_BUFF_PORT_MIN(CSD), JNL_BUFF_ROUND_UP_STEP(CSD));				\
}

/* Rounds down the maximum journal buffer value and assigns it to the specified variable */
#define ROUND_DOWN_MAX_JNL_BUFF_SIZE(DEST, CSD)								\
{													\
	int jnl_buffer_adj_value, jnl_buffer_decr_step;							\
													\
	jnl_buffer_decr_step = JNL_BUFF_ROUND_UP_STEP(CSD);						\
	jnl_buffer_adj_value = ROUND_UP(JNL_BUFFER_MAX, jnl_buffer_decr_step);				\
	while (JNL_BUFFER_MAX < jnl_buffer_adj_value)							\
		jnl_buffer_adj_value -= jnl_buffer_decr_step;						\
	DEST = jnl_buffer_adj_value;									\
}

#define CURRENT_JNL_IO_WRITER(JB)	JB->io_in_prog_latch.u.parts.latch_pid
#define CURRENT_JNL_FSYNC_WRITER(JB)	JB->fsync_in_prog_latch.u.parts.latch_pid

/* This macro is invoked by callers just before grabbing crit to check if a db fsync is needed and if so do it.
 * Note that we can do the db fsync only if we already have the journal file open. If we do not, we will end
 * up doing this later after grabbing crit. This just minimizes the # of times db fsync happens while inside crit.
 */
#define	DO_DB_FSYNC_OUT_OF_CRIT_IF_NEEDED(REG, CSA, JPC, JBP)			\
MBSTART {									\
	assert(!CSA->now_crit);							\
	if ((NULL != JPC) && JBP->need_db_fsync)				\
		jnl_wait(REG);	/* Try to do db fsync outside crit */		\
} MBEND

/* The below macro is currently used by the source server but is placed here in case others want to avail it later.
 * It does the equivalent of a "jnl_flush" but without holding crit for the most part.
 * The only case it needs crit is if it finds we do not have access to the latest journal file.
 * In that case it needs to do a "jnl_ensure_open" which requires crit.
 * For the caller's benefit, this macro also returns whether a jnl_flush happened (in FLUSH_DONE)
 * and the value of jbp->dskaddr & jbp->freeaddr before the flush (in DSKADDR & FREEADDR variables).
 */
#define	DO_JNL_FLUSH_IF_POSSIBLE(REG, CSA, FLUSH_DONE, DSKADDR, FREEADDR, RSRV_FREEADDR)	\
MBSTART {											\
	jnl_private_control	*jpc;								\
	jnl_buffer_ptr_t	jbp;								\
	boolean_t		was_crit;							\
	uint4			jnl_status;							\
												\
	assert(JNL_ENABLED(CSA));								\
	assert(CSA == &FILE_INFO(REG)->s_addrs);						\
	jpc = CSA->jnl;										\
	assert(NULL != jpc);									\
	jbp = jpc->jnl_buff;									\
	assert(NULL != jbp);									\
	FLUSH_DONE = FALSE;									\
	if (jbp->dskaddr != jbp->rsrv_freeaddr)							\
	{											\
		was_crit = CSA->now_crit;							\
		if (!was_crit)									\
			grab_crit(REG);								\
		DSKADDR = jbp->dskaddr;								\
		FREEADDR = jbp->freeaddr;							\
		RSRV_FREEADDR = jbp->rsrv_freeaddr;						\
		if (JNL_ENABLED(CSA->hdr) && (DSKADDR != RSRV_FREEADDR))			\
		{										\
			jnl_status = jnl_ensure_open(REG, CSA);					\
			assert(0 == jnl_status);						\
			if (0 == jnl_status)							\
			{									\
				FLUSH_DONE = TRUE;						\
				jnl_status = jnl_flush(REG);					\
				assert(SS_NORMAL == jnl_status);				\
			}									\
			/* In case of error, silently return for now. */			\
		}										\
		if (!was_crit)									\
			rel_crit(REG);								\
	}											\
} MBEND

/* jnl_ prototypes */
uint4	jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size);
uint4	jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat);
uint4	jnl_qio_start(jnl_private_control *jpc);
uint4	jnl_write_attempt(jnl_private_control *jpc, uint4 threshold);
void	jnl_prc_vector(jnl_process_vector *pv);
void	jnl_send_oper(jnl_private_control *jpc, uint4 status);
uint4	cre_jnl_file(jnl_create_info *info);
uint4 	cre_jnl_file_common(jnl_create_info *info, char *rename_fn, int rename_fn_len);
void	cre_jnl_file_intrpt_rename(sgmnt_addrs *csa);
void	jfh_from_jnl_info (jnl_create_info *info, jnl_file_header *header);
uint4	jnl_ensure_open(gd_region *reg, sgmnt_addrs *csa);
void	set_jnl_info(gd_region *reg, jnl_create_info *set_jnl_info);
void	jnl_write_epoch_rec(sgmnt_addrs *csa);
void	jnl_write_inctn_rec(sgmnt_addrs *csa);
void	jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb, uint4 com_csum);
void	jnl_write_ztp_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb, uint4 com_csum, seq_num jnl_seqno);
void	jnl_write_eof_rec(sgmnt_addrs *csa, struct_jrec_eof *eof_record);
void	jnl_write_trunc_rec(sgmnt_addrs *csa, uint4 orig_total_blks, uint4 orig_free_blocks, uint4 total_blks_after_trunc);
void	jnl_write_reserve(sgmnt_addrs *csa, jbuf_rsrv_struct_t *nontp_jbuf_rsrv,
					enum jnl_record_type rectype, uint4 reclen, void *param1);
void	jnl_write_phase2(sgmnt_addrs *csa, jbuf_rsrv_struct_t *jbuf_rsrv_ptr);
void	jnl_phase2_cleanup(sgmnt_addrs *csa, jnl_buffer_ptr_t jbp);
void	jnl_phase2_salvage(sgmnt_addrs *csa, jnl_buffer_ptr_t jbp, jbuf_phase2_in_prog_t *deadCmt);
void	jnl_pool_write(sgmnt_addrs *csa, enum jnl_record_type rectype, jnl_record *jnl_rec, jnl_format_buffer *jfb);
void	jnl_write_align_rec(sgmnt_addrs *csa, uint4 align_filler_len, jnl_tm_t time);
void	jnl_write_multi_align_rec(sgmnt_addrs *csa, uint4 align_filler_len, jnl_tm_t time);

jnl_format_buffer	*jnl_format(jnl_action_code opcode, gv_key *key, mval *val, uint4 nodeflags);

void	wcs_defer_wipchk_ast(jnl_private_control *jpc);
uint4	set_jnl_file_close(void);
uint4 	jnl_file_open_common(gd_region *reg, off_jnl_t os_file_size, char *err_str);
uint4	jnl_file_open_switch(gd_region *reg, uint4 sts);
void	jnl_file_close(gd_region *reg, boolean_t clean, boolean_t in_jnl_switch);

/* Consider putting followings in a mupip only header file  : Layek 2/18/2003 */
boolean_t  mupip_set_journal_parse(set_jnl_options *jnl_options, jnl_create_info *jnl_info);
uint4	mupip_set_journal_newstate(set_jnl_options *jnl_options, jnl_create_info *jnl_info, mu_set_rlist *rptr);
void	mupip_set_journal_fname(jnl_create_info *jnl_info);
uint4	mupip_set_jnlfile_aux(jnl_file_header *header, char *jnl_fname);
void	jnl_extr_init(void);
int 	exttime(uint4 time, char *buffer, int extract_len);
unsigned char *ext2jnlcvt(char *ext_buff, int4 ext_len, unsigned char **tr, int *tr_bufsiz,
					seq_num saved_jnl_seqno, seq_num saved_strm_seqno);
char	*ext2jnl(char *ptr, jnl_record *rec, seq_num saved_jnl_seqno, seq_num saved_strm_seqno);
char	*jnl2extcvt(jnl_record *rec, int4 jnl_len, char **ext_buff, int *extract_bufsiz);
char	*jnl2ext(char *jnl_buff, char *ext_buff);
void	jnl_set_cur_prior(gd_region *reg, sgmnt_addrs *csa, sgmnt_data *csd);
void	jnl_set_fd_prior(int jnl_fd, sgmnt_addrs* csa, sgmnt_data* csd, jnl_file_header *jfh);

#endif /* JNL_H_INCLUDED */
