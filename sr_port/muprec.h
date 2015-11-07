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

#ifndef MUPREC_H_INCLUDED
#define MUPREC_H_INCLUDED

#include "muprecsp.h" /* non-portable interface prototype */

#include "jnl_typedef.h"	/* for IS_VALID_JRECTYPE macro */

#if defined(UNIX)
#include "dollarh.h"	    /* for dollarh function */
#elif defined(VMS)
#include "op.h"	    /* for dollarh function */
#endif

#define JNL_EXTR_LABEL		"GDSJEX06"	/* format of the simple journal extract */
#define JNL_DET_EXTR_LABEL	"GDSJDX06"	/* format of the detailed journal extract */

error_def(ERR_MUINFOSTR);
error_def(ERR_MUINFOUINT6);
error_def(ERR_MUINFOUINT4);
error_def(ERR_MUINFOUINT8);
error_def(ERR_MUJNLSTAT);

#define EXTQW(I)							\
{									\
	ptr = &murgbl.extr_buff[extract_len];				\
	ptr = (char *)i2ascl((uchar_ptr_t)ptr, I);			\
	extract_len += (int)(ptr - &murgbl.extr_buff[extract_len]);	\
	murgbl.extr_buff[extract_len++] = '\\';				\
}

#define	EXT_STRM_SEQNO(SEQ)					\
{								\
	seq_num	input_seq, lcl_strm_seqno;			\
	uint4	lcl_strm_num;					\
								\
	input_seq = SEQ;					\
	/* display high order 4-bit strm_num */			\
	UNIX_ONLY(lcl_strm_num = GET_STRM_INDEX(input_seq);)	\
	VMS_ONLY(lcl_strm_num = 0;)				\
	EXTINT(lcl_strm_num);					\
	/* display low order 60-bit strm_seqno */		\
	UNIX_ONLY(lcl_strm_seqno = GET_STRM_SEQ60(input_seq);)	\
	VMS_ONLY(lcl_strm_seqno = 0;)				\
	EXTQW(lcl_strm_seqno);					\
}

#define EXTINT(I)							\
{									\
	ptr = &murgbl.extr_buff[extract_len];				\
	ptr = (char *)i2asc((uchar_ptr_t)ptr, I);			\
	extract_len += (int)(ptr - &murgbl.extr_buff[extract_len]);	\
	murgbl.extr_buff[extract_len++] = '\\';				\
}

#define	EXT_DET_COMMON_PREFIX(JCTL)					\
{									\
	extract_len = SPRINTF(murgbl.extr_buff, "0x%08x [0x%04x] :: ",	\
		JCTL->rec_offset, JCTL->reg_ctl->mur_desc->jreclen);	\
	assert(extract_len == STRLEN(murgbl.extr_buff));		\
}

#define EXT_DET_PREFIX(JCTL)									\
{												\
	EXT_DET_COMMON_PREFIX(JCTL);								\
	memcpy(murgbl.extr_buff + extract_len, jrt_label[rec->prefix.jrec_type], LAB_LEN);	\
	extract_len += LAB_LEN;									\
	memcpy(murgbl.extr_buff + extract_len, LAB_TERM, LAB_TERM_SZ);				\
	extract_len += LAB_TERM_SZ;								\
}

#define	EXTTXT(T,L)							\
{									\
	actual = real_len(L, (uchar_ptr_t)T);				\
	memcpy (&murgbl.extr_buff[extract_len], T, actual);		\
	extract_len += actual;						\
	murgbl.extr_buff[extract_len++] = '\\';				\
}

#define EXT2BYTES(T)							\
{									\
	murgbl.extr_buff[extract_len++] = *(caddr_t)(T);		\
	murgbl.extr_buff[extract_len++] = *((caddr_t)(T) + 1);		\
	murgbl.extr_buff[extract_len++] = '\\';				\
}

/* extract jnl record "rec" using extraction routine "extract" into file "file_info" (extract/lost/broken transaction files) */
#define EXTRACT_JNLREC(jctl, rec, extract, file_info, status)		\
{									\
	pini_list_struct	*plst;					\
									\
	status = mur_get_pini(jctl, (rec)->prefix.pini_addr, &plst);	\
	if (SS_NORMAL == status)					\
		(*extract)(jctl, (file_info), (rec), plst);		\
}

#define	EXTPID(plst)					\
{							\
	EXTINT(plst->jpv.jpv_pid);			\
	EXTINT(plst->origjpv.jpv_pid);			\
}

#define JNL_PUT_MSG_PROGRESS(LIT)								\
{												\
	now_t	now;	/* for GET_CUR_TIME macro */						\
	char	*time_ptr, time_str[CTIME_BEFORE_NL + 2];					\
												\
	GET_CUR_TIME;										\
	gtm_putmsg(VARLSTCNT(6) ERR_MUJNLSTAT, 4, LEN_AND_LIT(LIT), CTIME_BEFORE_NL, time_ptr);	\
}

#define JNL_SUCCESS_MSG(mur_options)									\
{													\
	if (mur_options.show)										\
		gtm_putmsg(VARLSTCNT(4) ERR_JNLSUCCESS, 2, LEN_AND_LIT(SHOW_STR));			\
	if (mur_options.extr[GOOD_TN])									\
		gtm_putmsg(VARLSTCNT(4) ERR_JNLSUCCESS, 2, LEN_AND_LIT(EXTRACT_STR));			\
	if (mur_options.verify)										\
		gtm_putmsg(VARLSTCNT(4) ERR_JNLSUCCESS, 2, LEN_AND_LIT(VERIFY_STR));			\
	if (mur_options.rollback)									\
		gtm_putmsg(VARLSTCNT(4) ERR_JNLSUCCESS, 2, LEN_AND_LIT(ROLLBACK_STR));			\
	else if (mur_options.update)									\
		gtm_putmsg(VARLSTCNT(4) ERR_JNLSUCCESS, 2, LEN_AND_LIT(RECOVER_STR));			\
}

#define	MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(JCTL, NEW_RCTL, OLD_RCTL, CHK_PREV_GEN)	\
{											\
	assert(!CHK_PREV_GEN || (NULL == JCTL->prev_gen));				\
	do										\
	{										\
		assert(OLD_RCTL == JCTL->reg_ctl);					\
		JCTL->reg_ctl = NEW_RCTL;						\
		JCTL = JCTL->next_gen;							\
	} while (NULL != JCTL);								\
}

#if defined(UNIX)

#define	TIME_FORMAT_STRING	"YYYY/MM/DD HH:MM:SS"
#define LENGTH_OF_TIME		STR_LIT_LEN(TIME_FORMAT_STRING)
#define	GET_TIME_STR(input_time, time_str)								\
{													\
	time_t		short_time;									\
	struct tm	*tsp;										\
													\
	short_time = (time_t)input_time;								\
	GTM_LOCALTIME(tsp, (const time_t *)&short_time);						\
	SPRINTF(time_str, "%04d/%02d/%02d %02d:%02d:%02d", 						\
		(1900 + tsp->tm_year), (1 + tsp->tm_mon), tsp->tm_mday, tsp->tm_hour, tsp->tm_min, tsp->tm_sec);	\
}
#define	GET_LONG_TIME_STR(long_time, time_str, time_str_len) GET_TIME_STR(long_time, time_str)

#define	REL2ABSTIME(deltatime, basetime, roundup)	\
{							\
	deltatime += basetime;				\
}

#elif defined(VMS)

#define	TIME_FORMAT_STRING	"DD-MON-YYYY HH:MM:SS.CC"
#define LENGTH_OF_TIME		STR_LIT_LEN(TIME_FORMAT_STRING)
#define	GET_TIME_STR(input_time, time_str)								\
{													\
	jnl_proc_time	long_time;									\
													\
	assert(LENGTH_OF_TIME < SIZEOF(time_str));							\
	JNL_WHOLE_FROM_SHORT_TIME(long_time, input_time);						\
	GET_LONG_TIME_STR(long_time, time_str, SIZEOF(time_str));					\
}
#define	GET_LONG_TIME_STR(long_time, time_str, time_str_len)						\
{													\
	struct dsc$descriptor_s t_desc									\
		= { time_str_len, DSC$K_DTYPE_T, DSC$K_CLASS_S, time_str};				\
													\
	sys$asctim(0, &t_desc, &long_time, 0);								\
	/* ascii time string is returned in time_str in the format DD-MMM-YYYY HH:MM:SS.CC */		\
	time_str[20] = '\0';	/* do not need hundredths of seconds field */				\
	assert(20 < time_str_len);									\
}

#define	REL2ABSTIME(deltatime, basetime, roundup)						\
	deltatime = mur_rel2abstime(deltatime, basetime, roundup);

#endif

#define	MUR_GET_IMAGE_COUNT(JCTL, REC, REC_IMAGE_COUNT, STATUS)					\
{												\
	pini_list_struct	*plst;								\
												\
	GBLREF 	jnl_gbls_t	jgbl;								\
												\
	UNIX_ONLY(assert(FALSE);)								\
	STATUS = mur_get_pini(JCTL, REC->prefix.pini_addr, &plst);				\
	/* In backward processing, it is possible we encounter corrupt journal records in	\
	 * case the journal file had a crash and we have not yet reached the first epoch	\
	 * (mur_fread_eof on the journal file at the start might not have caught it). In this	\
	 * case, mur_back_process knows to restart the backward processing. Assert this though.	\
	 */											\
	assert((SS_NORMAL == STATUS) || !jgbl.forw_phase_recovery);				\
	if (SS_NORMAL == STATUS)								\
		REC_IMAGE_COUNT = plst->jpv.jpv_image_count;					\
}

/* Note that JRT_TRIPLE and JRT_HISTREC are NOT considered valid rectypes by this macro.
 * This is because this macro is not used by the update process and receiver server, the only
 * processes which see this journal record type. Anyone else that sees this record type (update
 * process reader, mupip journal etc.) should treat this as an invalid record type.
 */
#define IS_VALID_RECTYPE(JREC)									\
(												\
	IS_VALID_JRECTYPE((JREC)->prefix.jrec_type) && (JRT_TRIPLE != (JREC)->prefix.jrec_type)	\
		&& (JRT_HISTREC != (JREC)->prefix.jrec_type)					\
)

#define IS_VALID_LEN_FROM_PREFIX(JREC, JFH)							\
(	/* length within range */								\
	(ROUND_DOWN2((JREC)->prefix.forwptr, JNL_REC_START_BNDRY) == (JREC)->prefix.forwptr) &&	\
	(JREC)->prefix.forwptr > MIN_JNLREC_SIZE  &&						\
	(JREC)->prefix.forwptr <= (JFH)->max_jrec_len						\
)

#define IS_VALID_LEN_FROM_SUFFIX(SUFFIX, JFH)							\
( /* length within range */									\
	(ROUND_DOWN2((SUFFIX)->backptr, JNL_REC_START_BNDRY) == (SUFFIX)->backptr) &&		\
  	(SUFFIX)->backptr > MIN_JNLREC_SIZE &&							\
	(SUFFIX)->backptr <= (JFH)->max_jrec_len						\
)

#define IS_VALID_LINKS(JREC)													\
(																\
	(JREC)->prefix.forwptr == ((jrec_suffix *)((char *)(JREC) + (JREC)->prefix.forwptr - JREC_SUFFIX_SIZE))->backptr	\
)

#define IS_VALID_SUFFIX(JREC)													\
(  /* our terminator */														\
	JNL_REC_SUFFIX_CODE == ((jrec_suffix *)((char *)(JREC) + (JREC)->prefix.forwptr - JREC_SUFFIX_SIZE))->suffix_code	\
)

#define IS_VALID_PREFIX(JREC, JFH)					\
(									\
  	IS_VALID_RECTYPE(JREC) && IS_VALID_LEN_FROM_PREFIX(JREC, JFH)	\
)

#define IS_VALID_JNLREC(JREC, JFH)												\
(																\
	IS_VALID_RECTYPE(JREC) && IS_VALID_LEN_FROM_PREFIX(JREC, JFH) && IS_VALID_LINKS(JREC) && IS_VALID_SUFFIX(JREC)		\
)

/* The following macro detects abnormal status during forward phase of journal recovery.
 * It considers JNLREADEOF (encountered during end of journal file) as a NORMAL status return.
 */
#define	CHECK_IF_EOF_REACHED(RCTL, STATUS)		\
{							\
	if (ERR_JNLREADEOF == STATUS)			\
	{						\
		assert(FALSE == RCTL->forw_eof_seen);	\
		RCTL->forw_eof_seen = TRUE;		\
		STATUS = SS_NORMAL;			\
	}						\
}

#ifdef UNIX
#define	MUR_SAVE_RESYNC_STRM_SEQNO(RCTL, CSD)									\
{														\
	int		idx;											\
	seq_num		strm_seqno;										\
														\
	GBLREF       mur_gbls_t      murgbl;									\
														\
	assert(CSD == RCTL->csd);										\
	if (murgbl.resync_strm_seqno_nonzero)									\
	{													\
		if (!rctl->recov_interrupted)									\
		{	/* Recovery on this database was not previously interrupted. So note down		\
			 * the resync_strm_seqno specified as input (if any) in file header.			\
			 */											\
			DEBUG_ONLY(										\
				for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)					\
					assert(!CSD->intrpt_recov_resync_strm_seqno[idx]);			\
			)											\
			idx = murgbl.resync_strm_index;								\
			if (INVALID_SUPPL_STRM != idx)								\
			{											\
				assert((0 <= idx) && (MAX_SUPPL_STRMS > idx));					\
				DEBUG_ONLY(strm_seqno = CSD->intrpt_recov_resync_strm_seqno[idx];)		\
				(CSD)->intrpt_recov_resync_strm_seqno[idx] = murgbl.resync_strm_seqno[idx];	\
			}											\
		} else												\
		{	/* Recovery on this database was previously interrupted. Although we know which		\
			 * stream seqno was specified for this round of rollback, we dont know which		\
			 * streams were specified in previous interrupted rollbacks. So set all the		\
			 * stream #s in the file header based on murgbl.resync_strm_seqno[] which has		\
			 * already been computed taking into account ALL region file headers.			\
			 */											\
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)						\
			{											\
				DEBUG_ONLY(strm_seqno = CSD->intrpt_recov_resync_strm_seqno[idx];)		\
				assert(!strm_seqno || (strm_seqno >= murgbl.resync_strm_seqno[idx]));		\
				CSD->intrpt_recov_resync_strm_seqno[idx] = murgbl.resync_strm_seqno[idx];	\
			}											\
		}												\
	}													\
}
#else
#define	MUR_SAVE_RESYNC_STRM_SEQNO(RCTL, CSD)
#endif

#define	SHOW_NONE		0
#define SHOW_HEADER		1
#define SHOW_STATISTICS		2
#define SHOW_BROKEN		4
#define SHOW_ALL_PROCESSES	8
#define SHOW_ACTIVE_PROCESSES	16
#define SHOW_ALL		31	/* All of the above */

#define TRANS_KILLS		1
#define TRANS_SETS		2

#define DEFAULT_EXTR_BUFSIZE			(64 * 1024)
#define JNLEXTR_DELIMSIZE			256
#define MUR_MULTI_LIST_INIT_ALLOC		1024		/* initial allocation for mur_multi_list */
#define MUR_MULTI_HASHTABLE_INIT_ELEMS		(16 * 1024)	/* initial elements in the token table */
#define MUR_PINI_LIST_INIT_ELEMS		 256		/* initial no. of elements in hash table jctl->pini_list */

#define DUMMY_FILE_ID	"123456"		/* needed only in VMS, but included here for lack of a better generic place */
#define SHOW_STR	"Show"
#define RECOVER_STR	"Recover"
#define ROLLBACK_STR	"Rollback"
#define EXTRACT_STR	"Extract"
#define VERIFY_STR	"Verify"
#define DOT		'.'
#define STR_JNLEXTR	"Journal extract"
#define STR_BRKNEXTR	"Broken transactions extract"
#define STR_LOSTEXTR	"Lost transactions extract"

#define	LONG_TIME_FORMAT	0
#define	SHORT_TIME_FORMAT	1

enum mur_error
{
	MUR_DUPTOKEN = 1,
	MUR_PREVJNLNOEOF,
	MUR_JNLBADRECFMT,
	MUR_CHNGTPRSLVTM,
	MUR_BOVTMGTEOVTM
};

enum mur_fence_type
{
	FENCE_NONE,
	FENCE_PROCESS,
	FENCE_ALWAYS
};

enum rec_fence_type
{
	NOFENCE = 0,
	TPFENCE = 1,
	ZTPFENCE = 2
};

enum broken_type
{
	GOOD_TN = 0,
	BROKEN_TN = 1,
	LOST_TN = 2,
	TOT_EXTR_TYPES = 3
};

typedef struct
{
	boolean_t		repl_standalone;	/* If standalone access was acheived for the instance file */
	boolean_t		clean_exit;
	boolean_t		ok_to_update_db;	/* FALSE for LOSTTNONLY type of rollback */
	boolean_t		intrpt_recovery;
	int			reg_total;		/* total number of regions involved in actual mupip journal flow */
	int			reg_full_total;		/* total including those regions that were opened but were discarded */
	int			regcnt_remaining;	/* number of regions yet to be processed in forward phase of recovery */
	int	            	err_cnt;
	int			wrn_count;
	int			broken_cnt;		/* Number of broken entries */
	int			max_extr_record_length;	/* maximum size of zwr-format extracted journal record */
	seq_num			resync_seqno;		/* is 0, if consistent rollback and no interrupted recovery */
	seq_num			consist_jnl_seqno;	/* Simulate replication server's sequence number */
	/* the following 3 variables are stored in a global so "mur_forward_play_cur_jrec" function can see it as well */
	seq_num			losttn_seqno;		/* The smallest seqno of a GOOD transaction that will NOT be played
							 * forward by rollback/recovery due either because it found a broken
							 * seqno before this one or because -resync specified a smaller seqno.
							 * In mur_back_process, this is set to 1 more than the HIGHEST logical
							 * record seqno found JUST BEFORE the tp_resolve_time across ALL regions
							 * that are being rolled back. This value is passed to the function
							 * mur_process_seqno_table which then checks if a journal record with this
							 * seqno was found during backward processing and added to the hashtable
							 * If yes, this would be equal to smallest seqno added to hashtable aka
							 * min_resolve_seqno. This function adjusts the losttn_seqno further
							 * and eventually passes it to mur_forward/mur_forward_play_cur_jrec
							 * which knows not to play any GOOD seqno that is >= murgbl.losttn_seqno
							 */
	seq_num			min_broken_seqno;	/* min broken seqno passed from mupip_recover to mur_forward */
	jnl_tm_t		min_broken_time;	/* min broken time passed from mupip_recover to mur_forward */
	hash_table_int8		token_table;		/* hashtable created during backward & used in forward phase of recovery */
	hash_table_int8		forw_token_table;	/* hashtable created and used only during forward phase of recovery */
	buddy_list      	*multi_list;
	buddy_list      	*forw_multi_list;
	buddy_list     		*pini_buddy_list;	/* Buddy list for pini_list */
	char			*extr_buff;
	jnl_process_vector	*prc_vec;		/* for recover process */
	void			*file_info[TOT_EXTR_TYPES];/* for a pointer to a structure described in filestruct.h */
#	ifdef UNIX
	repl_conn_info_t	remote_side;		/* Details of remote side connection in case of a -FETCHRESYNC rollback */
	boolean_t		was_rootprimary;	/* Whether this instance was previously a root primary. Set by
							 * "gtmrecv_fetchresync" */
	int4			resync_strm_index;	/* the stream # corresponding to -resync or -fetchresync seqno */
	seq_num			resync_strm_seqno[MAX_SUPPL_STRMS];
							/* same as resync_seqno but seqno corresponds to a specific update stream */
	boolean_t		resync_strm_seqno_nonzero; /* TRUE if at least one of resync_strm_seqno[16] entries is non-zero */
#	endif
	boolean_t		extr_file_create[TOT_EXTR_TYPES];
	boolean_t		incr_onln_rlbk_cycle;	/* TRUE if we applied at least one PBLK */
	boolean_t		incr_db_rlbkd_cycle;	/* TRUE if the database is effectively taken back in time */
#	ifdef DEBUG
	seq_num			save_losttn_seqno;	/* A copy of murgbl.losttn_seqno at start of mur_forward. We later check
							 * that this was not tampered with during forward phase of recovery.
							 */
	seq_num			save_resync_seqno;	/* A copy of murgbl.resync_seqno at start of mur_back_process */
#	endif
} mur_gbls_t;

typedef struct multi_element_struct
{
	token_num			token;
	VMS_ONLY(int4			image_count;)	/* Image activations */
	DEBUG_ONLY(boolean_t		this_is_broken;)	/* set in mur_back_process, checked in mur_forward */
	jnl_tm_t			time;
	uint4				regnum;		/* Last partner (region) seen */
	uint4				partner;	/* # of unmatched regions involved in TP/ZTP */
	uint4				tot_partner;	/* Total # of regions originally involved in TP/ZTP */
	enum rec_fence_type 		fence;		/* NOFENCE or TPFENCE or ZTPFENCE */
	struct multi_element_struct	*next;
} multi_struct;

/* The following is the primary structure maintained in a hashtable whose purpose is to record all currently
 * unresolved multi-region TP transactions (no ZTP or non-TP) during the course of forward journal recovery.
 * Elements are added as each region's participating journal records are encountered in the forward phase.
 * Elements are deleted when all participating region's records have been seen and the transaction is then played.
 * A lot of the elements are similar to the "multi_struct" which is maintained during backward phase of recovery.
 * There is one unique forw_multi_struct structure for each of the following
 * 	Unix : <token,time>
 * 	VMS  : <token,time,image_count>
 */
typedef struct forw_multi_element_struct
{
	union {
		ht_ent_int8			*tabent;	/* Pointer to the hashtable entry containing this forw_multi
								 * structure. Valid only if this structure is currently in use.
								 * Having this avoids us from doing a hashtable lookup inside
								 * mur_forward_play_multireg_tp (performance).
								 */
		que_ent				free_que;	/* Should be the first member in the structure in order to be
								 * able to use "free_element" and "get_new_free_element" functions
								 * of the buddy list interface. Valid only if this structure was
								 * previously in use but has since been freed.
								 */
	}					u;
	token_num				token;
	struct reg_ctl_list_struct		*first_tp_rctl; /* linked list of regions which had TCOM written for this TP */
	struct forw_multi_element_struct	*next;		/* if non-NULL, points to next in a linked list of structures
								 * corresponding to same token but with a different time */
	multi_struct				*multi;
	jnl_tm_t				time;
	VMS_ONLY(int4				image_count;)	/* Image activations */
	enum broken_type			recstat;	/* GOOD_TN or BROKEN_TN or LOST_TN */
	uint4					num_reg_total;
	uint4					num_reg_seen_backward;
	uint4					num_reg_seen_forward;
} forw_multi_struct;

typedef struct jnl_ctl_list_struct
{
	trans_num 			turn_around_tn;		/* Turn around point transaction number of EPOCH */
	seq_num 			turn_around_seqno;	/* Turn around point jnl_seqno of EPOCH */
	unsigned char			jnl_fn[JNL_NAME_SIZE];	/* Journal file name */
	unsigned int			jnl_fn_len;		/* Length of journal fine name string */
	jnl_file_header			*jfh;			/* journal file header */
	jnl_tm_t 			lvrec_time;		/* Last Valid Journal Record's Time Stamp */
	off_jnl_t 			lvrec_off;		/* Last Valid Journal Record's Offset */
	off_jnl_t 			rec_offset;		/* Last processed record's offset */
	off_jnl_t			os_filesize;		/* OS file size in bytes  */
	off_jnl_t			eof_addr;		/* Offset of end of last valid record of the journal */
	off_jnl_t 			apply_pblk_stop_offset;	/* Offset where last PBLK was applied. Updated by both
								 * mur_apply_pblk() and mur_back_process()
								 */
	off_jnl_t 			turn_around_offset; 	/* Turn around point journal record's offset for each region */
	jnl_tm_t			turn_around_time; 	/* Turn around time for this region */
	boolean_t 			properly_closed;	/* TRUE if journal was properly closed, having written EOF;
									FALSE otherwise */
        boolean_t                       tail_analysis;		/* true for mur_fread_eof */
        boolean_t                       after_end_of_data;	/* true for record offset more than end_of_data */
        boolean_t                       read_only;		/* TRUE if read_only for extract/show/verify */
	int				jnlrec_cnt[JRT_RECTYPES];/* Count of each type of record found in this journal  */
	int4				status;			/* Last status of the last operation done on this journal */
	uint4				status2;		/* Last secondary status of the last operation done on
									this journal */
	fd_type				channel;
#if defined(VMS)
	struct FAB			*fab;
#endif
	gd_id				fid;
	hash_table_int4			pini_list;		/* hash table of pini_addr to pid list */
	struct reg_ctl_list_struct	*reg_ctl;		/* Back pointer to this region's reg_ctl_list */
	struct jnl_ctl_list_struct 	*next_gen;		/* next generation journal file */
	struct jnl_ctl_list_struct 	*prev_gen;		/* previous generation journal file */
#	ifdef GTM_CRYPT
	gtmcrypt_key_t			encr_key_handle;
	boolean_t			is_same_hash_as_db;	/* to indicate whether the db and the jnl file share
								 * the same encryption key. */
#	endif
	boolean_t			turn_around_fullyupgraded; /* EPOCH record's fully_upgraded field */
} jnl_ctl_list;

/* This structure is used to only pass parameters for the function "mur_back_processing_one_region" */
typedef struct
{
	jnl_ctl_list	*jctl;
	seq_num 	rec_token_seq;
	boolean_t	first_epoch;
	uint4		status;
	jnl_ctl_list	**jjctl;
	boolean_t	apply_pblk;
	seq_num		*pre_resolve_seqno;
} mur_back_opt_t;

typedef struct
{
	unsigned char		*base;		/* Pointer to the buffer base of this mur_buff_desc */
	unsigned char		*top;		/* Pointer to the buffer top of this mur_buff_desc */
	off_jnl_t		blen;		/* Length of the buffer till end of valid data  */
	off_jnl_t		dskaddr;   	/* disk offset from which this buffer was read */
	boolean_t		read_in_progress;/* Asynchronous read requested and in progress */
#if defined(UNIX)
	struct aiocb 		*aiocbp;
	int			rip_channel;	/* channel that has the aio read (for this mur_buff_desc_t) in progress.
						 * valid only if "read_in_progress" field is TRUE.
						 * this is a copy of the active channel "jctl->channel" while issuing the AIO.
						 * in case the active channel "jctl->channel" changes later (due to switching
						 * to a different journal file) and we want to cancel the previously issued aio
						 * we cannot use jctl->channel but should use "rip_channel" for the cancel.
						 */
#elif defined(VMS)
	io_status_block_disk	iosb;
	short			rip_channel;	/* same meaning as the Unix field */
	short			filler;		/* to ensure 4-byte alignment for this structure */
#endif
} mur_buff_desc_t;

typedef struct
{
	int4			blocksize;	/* This amount it reads from disk to memory */
	unsigned char		*alloc_base;	/* Pointer to the buffers allocated. All 5 buffers allocated at once */
	int4			alloc_len;	/* Size of alloc_base buffer */
	mur_buff_desc_t		random_buff;	/* For reading pini_rec which could be at a random offset before current record */
	unsigned char		*aux_buff1;	/* For partial records for mur_next at the end of seq_buff[1] */
	mur_buff_desc_t		seq_buff[2];	/* Two buffers for double buffering  */
	mur_buff_desc_t		aux_buff2;	/* For partial records for mur_prev just previous of seq_buff[0] or for overflow */
	int			buff_index;	/* Which one of the two seq_buff is in use */
	mur_buff_desc_t		*cur_buff;	/* pointer to active mur_buff_desc_t */
	mur_buff_desc_t		*sec_buff; 	/* pointer to second mur_buff_desc_t for the double buffering*/
	/* The following fields were formerly part of a separate mur_rab_t type structure but
	 * are now folded into one region-specific mur_read_desc_t type structure.
	 */
	jnl_record		*jnlrec;		/* points to last jnl record read in this region */
	unsigned int		jreclen;		/* length of the last journal record read in this region */
} mur_read_desc_t;

typedef struct reg_ctl_list_struct
{
	trans_num		db_tn;			/* database curr_tn when region is opened first by recover */
	FILL8DCL(sgmnt_data_ptr_t, csd, 0);		/* cs_data of this region */
	struct gd_region_struct	*gd;			/* region info */
	sgmnt_addrs		*csa;			/* cs_addrs of this region */
	struct sgm_info_struct	*sgm_info_ptr;		/* sgm_info_ptr of this region */
	file_control 		*db_ctl;		/* To do dbfilop() */
	jnl_ctl_list 		*jctl;			/* Current generation journal file control info */
	jnl_ctl_list 		*jctl_head;		/* For forward recovery starting (earliest) generation
							   journal file to be processed. */
	jnl_ctl_list		*jctl_apply_pblk;	/* Journal file where PBLK application last stopped.
							 * Updated by mur_apply_pblk() and mur_back_process()
							 */
	jnl_ctl_list		*jctl_turn_around;	/* final pass turn around point journal file */
	jnl_ctl_list 		*jctl_alt_head;		/* For backward recovery turn around point
							   journal file of interrupted recovery. */
	hash_table_mname	gvntab;     		/* Used for gv_target info for globals in mur_output_record() */
	jnl_tm_t 		lvrec_time;		/* Last Valid Journal Record's Time Stamp across all generations */
	int			jnl_state;
	int			repl_state;
	int4 			lookback_count;
	boolean_t 		before_image;		/* True if the database has before image journaling enabled */
	boolean_t		standalone;		/* If standalone access was acheived for the region */
	boolean_t		recov_interrupted;	/* A copy of csd->recov_interrupted before resetting it to TRUE */
	boolean_t		jfh_recov_interrupted;	/* Whether latest generation journal file was created by recover */
	int4			blks_to_upgrd_adjust;	/* Delta to adjust turn around point's blks_to_upgrd counter with.
							 * This will include all bitmaps created in V4 format by gdsfilext */
	struct pini_list	*mur_plst;		/* pini_addr hash-table entry of currently simulating GT.M process
						 	 * for this region (used only if jgbl.forw_phase_recovery) */
	mur_read_desc_t		*mur_desc;		/* Region specific structure storing last mur_read_file* context.
							 * It is a pointer to a structure (instead of the structure itself)
							 * as otherwise when we swap reg_ctl_list structures in mur_sort_files
							 * while asynchronous reads are active, the iosb buffer pointers could
							 * get mixed up amongst regions and cause hangs in mur_fread_wait.
							 */
	boolean_t		db_updated;	/* whether this region's database has been updated as part of this recovery */
	boolean_t		forw_eof_seen;	/* whether JNLREADEOF was encountered in forward phase */
	/* Below are region-specific variables used in the functions "mur_forward" and "mur_forward_next". They store
	 * region-specific context needed while going back and forth between "mur_forward" and "mur_forward_next".
	 */
	boolean_t		process_losttn;	/* whether this region has started losttn processing */
	trans_num		last_tn;	/* tn of last applied record in this region (to compare with next record's tn) */
	struct reg_ctl_list_struct	*next_rctl;	/* Next region that has records to be processed (used only in mur_forward).
							 * Initially, all journaled regions are in this circular linked list.
							 * As soon as there are no more journal records in a region to be
							 * processed in the forward phase, this region is removed from the list.
							 * Minimizes the time spent in mur_forward hopping around the remaining
							 * regions resolving multi-region tokens.
							 */
	struct reg_ctl_list_struct	*prev_rctl; /* Prev region that has records to be processed (counterpart of next_rctl) */
	struct reg_ctl_list_struct	*next_tp_rctl; /* Next in a linked list of regions participating in this TP transaction */
	struct reg_ctl_list_struct	*prev_tp_rctl; /* Prev in a linked list of regions participating in this TP transaction */
	forw_multi_struct	*forw_multi;	/* If non-NULL, this is a pointer to the structure containing all information
						 * related to the TP transaction that is currently being processed. */
	boolean_t		initialized;		/* Set to TRUE only after journaling and replication state information has
							 * been copied over from csd into rctl. This way mur_close_files knows if
							 * it is safe to use the rctl values to copy them back to csd. Previously,
							 * an interrupt in mur_open_files before the journaling and/or replication
							 * fields in rctl got initialized took us to mur_close_files which
							 * unconditionally used those to restore the corresponding csd fields
							 * resulting in journaling/replication getting incorrectly turned OFF. */
#	ifdef DEBUG
	boolean_t		deleted_from_unprocessed_list;
	jnl_ctl_list 		*last_processed_jctl;
	uint4			last_processed_rec_offset;
	seq_num			last_processed_jnl_seqno;	/* last jnl_seqno processed in this region */
#	endif
	boolean_t		db_present;		/* TRUE if database pointed by curr->gd is present or not */
} reg_ctl_list;

typedef struct redirect_list_struct
{
	struct redirect_list_struct
				*next;
	unsigned int		org_name_len,
				new_name_len;
	char			*org_name,
				*new_name;
} redirect_list;

typedef struct select_list_struct
{
	struct select_list_struct
				*next;
	char			*buff;
	short			len;
	bool			exclude;
	boolean_t		has_wildcard;
} select_list;

typedef struct long_list_struct
{
	struct long_list_struct *next;
	uint4			num;
	bool			exclude;
} long_list;

typedef struct
{
	jnl_proc_time		lookback_time,
				before_time,
				since_time,
				after_time;
	enum mur_fence_type	fences;
	int4			error_limit,
				fetchresync_port;
	int			show,
				lookback_opers;
	boolean_t		forward,
				update,
				rollback,
				rollback_losttnonly,
				verify,
				before_time_specified,
				since_time_specified,
				resync_specified,
				lookback_time_specified,
				lookback_opers_specified,
				interactive,
				selection,
				apply_after_image,
				chain,
				notncheck,
				verbose,
				log,
				detail,
				extract_full,
				show_head_only,
				extr[TOT_EXTR_TYPES];
	char			transaction;
	redirect_list		*redirect;
	select_list		*user,
				*database,		/* UNIX only? */
				*global,
				*process;
	long_list		*id;
	char			*extr_fn[TOT_EXTR_TYPES];
	int			extr_fn_len[TOT_EXTR_TYPES];
} mur_opt_struct;

typedef struct onln_rlbk_reg_list_struct
{
	struct onln_rlbk_reg_list_struct	*fPtr;
	struct gd_region_struct		*reg;
	gd_id				unique_file_id;
	struct reg_ctl_list_struct	*rctl;
} onln_rlbk_reg_list;

/* This macro is invoked whenever all records of a region have been processed. It deletes the current region
 * from the list of unprocessed regions thereby removing this from the list of regions examined whenever a
 * future TP journal record spanning multiple regions needs to be resolved.
 */
#define	DELETE_RCTL_FROM_UNPROCESSED_LIST(rctl)			\
{								\
	GBLREF	reg_ctl_list	*rctl_start;			\
								\
	reg_ctl_list	*p_rctl, *n_rctl;			\
								\
	assert(0 < murgbl.regcnt_remaining);			\
	assert(!rctl->deleted_from_unprocessed_list);		\
	p_rctl = rctl->prev_rctl;				\
	n_rctl = rctl->next_rctl;				\
	assert((NULL != p_rctl) || (NULL == n_rctl));		\
	assert((NULL == p_rctl) || (NULL != n_rctl));		\
	assert(p_rctl != rctl);					\
	assert(n_rctl != rctl);					\
	if (NULL != n_rctl)					\
	{							\
		if (p_rctl != n_rctl)				\
		{						\
			p_rctl->next_rctl = n_rctl;		\
			n_rctl->prev_rctl = p_rctl;		\
		} else						\
		{						\
			p_rctl->next_rctl = NULL;		\
			p_rctl->prev_rctl = NULL;		\
		}						\
	}							\
	DEBUG_ONLY(rctl->deleted_from_unprocessed_list = TRUE;)	\
	murgbl.regcnt_remaining--;				\
	rctl_start = n_rctl;					\
}

#define MUR_CHANGE_REG(rctl)								\
{											\
	GBLREF	gd_region		*gv_cur_region;					\
	GBLREF	sgmnt_data_ptr_t 	cs_data;					\
	GBLREF	sgmnt_addrs		*cs_addrs;					\
	GBLREF	sgm_info		*sgm_info_ptr;					\
	GBLREF	gv_namehead		*gv_target;					\
	GBLREF	uint4			dollar_tlevel;					\
											\
	sgmnt_addrs		*csa;							\
	gd_region		*reg;							\
											\
	reg = rctl->gd;									\
	if (gv_cur_region != reg)							\
	{										\
		gv_cur_region = reg;							\
		cs_addrs = csa = rctl->csa;						\
		cs_data = rctl->csd;							\
		sgm_info_ptr = dollar_tlevel ? rctl->sgm_info_ptr : NULL;		\
		/* Keep gv_target and gv_cur_region in sync always.			\
		 * Now that region has switched, set gv_target to NULL			\
		 * Or else asserts that check the in-syncness (e.g. op_tstart) fail.	\
		 */									\
		gv_target = NULL;							\
	}										\
	assert(gv_cur_region == rctl->gd);						\
	assert(cs_addrs == rctl->csa);							\
	assert(cs_data == rctl->csd);							\
	assert(!dollar_tlevel || (sgm_info_ptr == rctl->sgm_info_ptr));			\
	assert(dollar_tlevel || (NULL == sgm_info_ptr));				\
}

#define	SET_THIS_TN_AS_BROKEN(multi, reg_total)						\
{											\
	multi->partner = reg_total;							\
	multi->tot_partner = reg_total + 1;						\
	/* Set a debug-only flag indicating this "multi" structure never be		\
	 * treated as a GOOD_TN in forward processing. This will be checked there.	\
	 */										\
	DEBUG_ONLY(multi->this_is_broken = TRUE;)					\
}

/* This macro is used in forward processing. A record can be broken only if its time is > minimum broken time determined
 * in backward processing or its seqno is > minimum broken seqno determine in backward processing.
 * The below checks are done in order to avoid hash table lookup (performance), when it is not needed.
 */
#define	IS_REC_POSSIBLY_BROKEN(REC_TIME, REC_TOKEN_SEQ) ((!mur_options.rollback && (REC_TIME >= murgbl.min_broken_time))	\
							|| (mur_options.rollback && (REC_TOKEN_SEQ >= murgbl.min_broken_seqno)))

#define MUR_TOKEN_ADD(multi, rec_token, rec_image_count, rec_tok_time, rec_partner, rec_fence, rec_regno)	\
{														\
	ht_ent_int8	*tabent;										\
	uint4		partner_cnt;										\
														\
	multi = (multi_struct *)get_new_element(murgbl.multi_list, 1);						\
	multi->token = rec_token;										\
	VMS_ONLY(multi->image_count = rec_image_count;)								\
	multi->time = rec_tok_time;										\
	partner_cnt = rec_partner;										\
	assert(0 < (int4)partner_cnt);										\
	multi->tot_partner = partner_cnt;									\
	DEBUG_ONLY(multi->this_is_broken = FALSE;)								\
	partner_cnt--;												\
	multi->partner = partner_cnt;										\
	multi->fence = rec_fence;										\
	multi->regnum = rec_regno;										\
	multi->next = NULL;											\
	if (!add_hashtab_int8(&murgbl.token_table, &multi->token, multi, &tabent))				\
	{													\
		assert(NULL != tabent->value);									\
		multi->next = (multi_struct *)tabent->value;							\
		tabent->value = (char *)multi;									\
	}													\
	if (partner_cnt)											\
		murgbl.broken_cnt = murgbl.broken_cnt + 1;							\
}

/* If any one region that has started losttn processing gets added to the forw_multi region list, the entire
 * transaction (including across all the other regions) should be treated as a LOST transaction even if
 * other regions have not started losttn processing (i.e. have not seen any broken transaction yet). Not doing
 * so will cause only a portion of the transaction to be played which breaks the atomicity ACID property of TP.
 */
#define	MUR_FORW_MULTI_RECSTAT_UPDATE_IF_NEEDED(FORW_MULTI, RCTL)	\
{									\
	assert(RCTL->forw_multi == FORW_MULTI);				\
	if (RCTL->process_losttn && (GOOD_TN == FORW_MULTI->recstat))	\
		FORW_MULTI->recstat = LOST_TN;				\
}

#define MUR_FORW_TOKEN_ADD(FORW_MULTI, REC_TOKEN, REC_TIME, RCTL, REG_TOTAL, RECSTAT, MULTI, REC_IMAGE_COUNT)	\
{														\
	ht_ent_int8	*tabent;										\
														\
	FORW_MULTI = (forw_multi_struct *)get_new_free_element(murgbl.forw_multi_list);				\
	FORW_MULTI->token = REC_TOKEN;										\
	FORW_MULTI->first_tp_rctl = RCTL;									\
	RCTL->next_tp_rctl = RCTL;										\
	RCTL->prev_tp_rctl = RCTL;										\
	FORW_MULTI->multi = MULTI;										\
	UNIX_ONLY(assert(0 == REC_IMAGE_COUNT);)								\
	VMS_ONLY(FORW_MULTI->image_count = REC_IMAGE_COUNT;)							\
	FORW_MULTI->time = REC_TIME;										\
	FORW_MULTI->recstat = RECSTAT;										\
	FORW_MULTI->num_reg_total = REG_TOTAL;									\
	FORW_MULTI->num_reg_seen_forward = 1;									\
	/* If tn is NOT broken, we would have seen all the regions in backward processing.			\
	 * If tn is broken, then we would have a non-null multi use that to find out how many regions		\
	 * were unresolved and accordingly determine how many regions were seen in backward processing.		\
	 */													\
	assert((GOOD_TN == RECSTAT) || (BROKEN_TN == RECSTAT));							\
	if (GOOD_TN == RECSTAT)											\
		FORW_MULTI->num_reg_seen_backward = REG_TOTAL;							\
	else													\
	{													\
		assert(NULL != MULTI);										\
		assert(0 < MULTI->partner);									\
		FORW_MULTI->num_reg_seen_backward = MULTI->tot_partner - MULTI->partner;			\
	}													\
	if (!add_hashtab_int8(&murgbl.forw_token_table, &FORW_MULTI->token, FORW_MULTI, &tabent))		\
	{	/* More than one TP transaction has the same token. This is possible in case of			\
		 * non-replication but we expect the rec_time to be different between the colliding		\
		 * transactions. In replication, we use jnl_seqno which should be unique. Assert that.		\
		 */												\
		assert(!mur_options.rollback);									\
		assert(NULL != tabent->value);									\
		FORW_MULTI->next = (forw_multi_struct *)tabent->value;						\
		tabent->value = (char *)FORW_MULTI;								\
	} else													\
		FORW_MULTI->next = NULL;									\
	assert(NULL != tabent);											\
	FORW_MULTI->u.tabent = tabent;										\
	RCTL->forw_multi = FORW_MULTI;										\
	MUR_FORW_MULTI_RECSTAT_UPDATE_IF_NEEDED(FORW_MULTI, RCTL);						\
}

#define MUR_FORW_TOKEN_ONE_MORE_REG(FORW_MULTI, RCTL)					\
{											\
	reg_ctl_list	*start_rctl, *tmp_rctl;						\
											\
	start_rctl = FORW_MULTI->first_tp_rctl;						\
	assert(NULL != start_rctl);							\
	assert(RCTL != start_rctl);							\
	tmp_rctl = start_rctl->prev_tp_rctl;						\
	assert(NULL != tmp_rctl);							\
	assert(tmp_rctl->next_tp_rctl == start_rctl);					\
	start_rctl->prev_tp_rctl = RCTL;						\
	RCTL->next_tp_rctl = start_rctl;						\
	tmp_rctl->next_tp_rctl = RCTL;							\
	RCTL->prev_tp_rctl = tmp_rctl;							\
	assert(FORW_MULTI->num_reg_seen_forward < FORW_MULTI->num_reg_seen_backward);	\
	FORW_MULTI->num_reg_seen_forward++;						\
	RCTL->forw_multi = FORW_MULTI;							\
	MUR_FORW_MULTI_RECSTAT_UPDATE_IF_NEEDED(FORW_MULTI, RCTL);			\
}

#define MUR_FORW_TOKEN_LOOKUP(FORW_MULTI, REC_TOKEN, REC_TIME, REC_IMAGE_COUNT)							\
{																\
	ht_ent_int8	*tabent;												\
																\
	if (NULL != (tabent = lookup_hashtab_int8(&murgbl.forw_token_table, (gtm_uint64_t *)&REC_TOKEN)))			\
	{															\
		if (mur_options.rollback)											\
		{														\
			assert(0 == REC_IMAGE_COUNT);										\
			assert(NULL != ((forw_multi_struct *)tabent->value));							\
			assert(NULL == ((forw_multi_struct *)tabent->value)->next);						\
			FORW_MULTI = (forw_multi_struct *)tabent->value;							\
			FORW_MULTI->u.tabent = tabent;										\
		} else														\
		{														\
			for (FORW_MULTI = (forw_multi_struct *)tabent->value; NULL != FORW_MULTI;				\
								FORW_MULTI = (forw_multi_struct *)FORW_MULTI->next)		\
			{													\
				if ((FORW_MULTI->time == REC_TIME) VMS_ONLY(&& (FORW_MULTI->image_count == REC_IMAGE_COUNT)))	\
				{												\
					FORW_MULTI->u.tabent = tabent;								\
					break;											\
				}												\
			}													\
		}														\
		assert((NULL == FORW_MULTI) || (FORW_MULTI->time == REC_TIME) && (FORW_MULTI->token == REC_TOKEN));		\
	} else															\
		FORW_MULTI = NULL;												\
}

#define	MUR_FORW_TOKEN_REMOVE(RCTL)				\
{								\
	reg_ctl_list	*n_rctl, *p_rctl;			\
								\
	n_rctl = RCTL->next_tp_rctl;				\
	p_rctl = RCTL->prev_tp_rctl;				\
	assert(NULL != p_rctl);					\
	assert(NULL != n_rctl);					\
	assert((RCTL != p_rctl) || (RCTL == n_rctl));		\
	if (RCTL != p_rctl)					\
	{							\
		assert(RCTL == p_rctl->next_tp_rctl);		\
		p_rctl->next_tp_rctl = n_rctl;			\
		assert(RCTL == n_rctl->prev_tp_rctl);		\
		n_rctl->prev_tp_rctl = p_rctl;			\
	}							\
	RCTL->forw_multi = NULL;				\
}

#define	MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl)									\
{														\
	inctn_opcode_t		opcode;										\
	struct_jrec_inctn	*inctn_rec;									\
														\
	inctn_rec = &rctl->mur_desc->jnlrec->jrec_inctn;							\
	opcode = (inctn_opcode_t)inctn_rec->detail.blks2upgrd_struct.opcode;					\
	if ((inctn_gdsfilext_gtm == opcode) || (inctn_gdsfilext_mu_reorg == opcode))				\
	{	/* Note down the number of bitmaps that were created during this file extension			\
		 * in V4 format. At the turn around point, blks_to_upgrd counter has to be			\
		 * increased by this amount to reflect the current state of the new bitmaps.			\
		 */												\
		 (rctl)->blks_to_upgrd_adjust += (inctn_rec)->detail.blks2upgrd_struct.blks_to_upgrd_delta;	\
	}													\
}

#define	MUR_WITHIN_ERROR_LIMIT(err_cnt, error_limit) ((++err_cnt <= error_limit) || (mur_options.interactive && mur_interactive()))

#ifdef DEBUG
#	define	MUR_DBG_SET_LAST_PROCESSED_JNL_SEQNO(TOKEN, RCTL)					\
	{												\
		if (NULL != RCTL)									\
		{											\
			assert(!mur_options.rollback || RCTL->last_processed_jnl_seqno <= TOKEN);	\
			RCTL->last_processed_jnl_seqno = TOKEN;						\
		}											\
	}
#else
#	define	MUR_DBG_SET_LAST_PROCESSED_JNL_SEQNO(TOKEN, RCTL)
#endif

#define	MUR_SET_JNL_FENCE_CTL_TOKEN(TOKEN, RCTL)			\
{									\
	MUR_DBG_SET_LAST_PROCESSED_JNL_SEQNO(TOKEN, RCTL);		\
	jnl_fence_ctl.token = TOKEN;					\
}

#if defined(UNIX)
#define MUR_TOKEN_LOOKUP(token, image_count, rec_time, fence) mur_token_lookup(token, rec_time, fence)
#elif defined(VMS)
#define MUR_TOKEN_LOOKUP(token, image_count, rec_time, fence) mur_token_lookup(token, image_count, rec_time, fence)
#endif

#define PRINT_VERBOSE_STAT(JCTL, MODULE)   									\
{										   				\
	GBLREF 	jnl_gbls_t	jgbl;			   							\
	UNIX_ONLY(                             									\
	uint4 days;                            									\
	time_t seconds;)											\
	VMS_ONLY(mval val;)											\
	if (mur_options.verbose)										\
	{													\
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4,   LEN_AND_LIT("Module"),				\
				LEN_AND_LIT(MODULE));								\
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4,   LEN_AND_LIT("    Journal file"),			\
			JCTL->jnl_fn_len, JCTL->jnl_fn);							\
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("    Record Offset"),			\
			JCTL->rec_offset, JCTL->rec_offset);							\
		if (!jgbl.forw_phase_recovery)									\
		{												\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("    Turn around Offset"), 	\
				JCTL->turn_around_offset, JCTL->turn_around_offset);   				\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("    Turn around timestamp"),	\
				JCTL->turn_around_time, JCTL->turn_around_time);  				\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("    Turn around transaction"),	\
				&JCTL->turn_around_tn, &JCTL->turn_around_tn);  				\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("    Turn around seqno"), 	\
				&JCTL->turn_around_seqno, &JCTL->turn_around_seqno);   				\
			UNIX_ONLY(										\
				dollarh(jgbl.mur_tp_resolve_time, &days, &seconds);        			\
				gtm_putmsg(VARLSTCNT(8) ERR_MUINFOUINT6, 6, LEN_AND_LIT("    Tp_resolve_time"), \
				jgbl.mur_tp_resolve_time, jgbl.mur_tp_resolve_time, days, seconds);		\
			)	 										\
			VMS_ONLY(										\
				op_horolog(&val);	                                                 	\
            			gtm_putmsg(VARLSTCNT(8) ERR_MUINFOUINT6, 6,  LEN_AND_LIT("    Tp_resolve_time"),\
                		jgbl.mur_tp_resolve_time, jgbl.mur_tp_resolve_time, val.str.len, val.str.addr)	\
			);     											\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("    Token total"), 		\
			murgbl.token_table.count, murgbl.token_table.count);					\
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("    Token broken"), 		\
				murgbl.broken_cnt, murgbl.broken_cnt);  					\
		}								   				\
	}									   				\
}

#define PRINT_VERBOSE_TAIL_BAD(JCTL)									\
{													\
	if (mur_options.verbose)									\
	{												\
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOSTR, 4,						\
			LEN_AND_LIT("Tail analysis found bad record for journal file"),			\
			JCTL->jnl_fn_len, JCTL->jnl_fn);						\
		gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Record Offset"),		\
			JCTL->rec_offset, JCTL->rec_offset);						\
	}												\
}

#define ASSERT_HOLD_REPLPOOL_SEMS				\
{								\
	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);		\
	assert(holds_sem[SOURCE][SRC_SERV_COUNT_SEM]);		\
	assert(holds_sem[RECV][RECV_POOL_ACCESS_SEM]);		\
	assert(holds_sem[RECV][RECV_SERV_COUNT_SEM]);		\
	assert(holds_sem[RECV][UPD_PROC_COUNT_SEM]);		\
	assert(holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);		\
}

#define ASSERT_DONOT_HOLD_REPLPOOL_SEMS				\
{								\
	assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);	\
	assert(!holds_sem[SOURCE][SRC_SERV_COUNT_SEM]);		\
	assert(!holds_sem[RECV][RECV_POOL_ACCESS_SEM]);		\
	assert(!holds_sem[RECV][RECV_SERV_COUNT_SEM]);		\
	assert(!holds_sem[RECV][UPD_PROC_COUNT_SEM]);		\
	assert(!holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);	\
}

/* Prototypes */
#ifdef UNIX
seq_num			mur_get_max_strm_reg_seqno(int strm_index);
#endif

void			jnlext_write(fi_type *file_info, char *buffer, int length);
uint4			mur_apply_pblk(boolean_t apply_intrpt_pblk);
boolean_t 		mur_back_process(boolean_t apply_pblk, seq_num *pre_resolve_seqno);
uint4			mur_back_processing_one_region(mur_back_opt_t *mur_back_options);
uint4 			mur_back_processing(jnl_ctl_list **jjctl, boolean_t apply_pblk, seq_num *pre_resolve_seqno,
				jnl_tm_t alt_tp_resolve_time);
uint4 			mur_block_count_correct(reg_ctl_list *rctl);
int4			mur_blocks_free(reg_ctl_list *rctl);
boolean_t		mur_close_files(void);
void			mur_close_file_extfmt(void);
int4			mur_cre_file_extfmt(jnl_ctl_list *jctl, int recstat);
boolean_t		mur_do_wildcard(char *jnl_str, char *pat_str, int jnl_len, int pat_len);
uint4			mur_forward(jnl_tm_t min_broken_time, seq_num min_broken_seqno, seq_num losttn_seqno);
uint4			mur_forward_play_cur_jrec(reg_ctl_list *rctl);
#ifdef GTM_TRIGGER
uint4			mur_forward_play_multireg_tp(forw_multi_struct *forw_multi, reg_ctl_list *rctl);
#endif
boolean_t		mur_fopen_sp(jnl_ctl_list *jctl);
boolean_t		mur_fopen(jnl_ctl_list *jctl);
boolean_t		mur_fclose(jnl_ctl_list *jctl);
void			mur_get_options(void);
uint4			mur_get_pini(jnl_ctl_list *jctl, off_jnl_t pini_addr, pini_list_struct **pplst);
void			mur_init(void);
void			mur_free(void);
void			mur_rctl_desc_alloc(reg_ctl_list *rctl);
void			mur_rctl_desc_free(reg_ctl_list *rctl);
boolean_t		mur_insert_prev(jnl_ctl_list **jjctl);
boolean_t		mur_interactive(void);
boolean_t		mur_jctl_from_next_gen(void);
void 			mur_multi_rehash(void);
uint4			mur_next(jnl_ctl_list *jctl, off_jnl_t dskaddr);
uint4			mur_next_rec(jnl_ctl_list **jjctl);
boolean_t		mur_open_files(void);
uint4			mur_output_pblk(reg_ctl_list *rctl);
uint4			mur_output_record(reg_ctl_list *rctl);
void			mur_output_show(void);
void			mur_pini_addr_reset(sgmnt_addrs *csa);
uint4			mur_pini_state(jnl_ctl_list *jctl, uint4 pini_addr, int state);
uint4			mur_prev(jnl_ctl_list *jctl, off_jnl_t dskaddr);
uint4			mur_prev_rec(jnl_ctl_list **jjctl);
uint4			mur_process_intrpt_recov(void);
void 			mur_process_seqno_table(seq_num *min_broken_seqno, seq_num *losttn_seqno);
void 			mur_process_timequal(jnl_tm_t max_lvrec_time, jnl_tm_t min_bov_time);
jnl_tm_t 		mur_process_token_table(boolean_t *ztp_broken);
void			mur_put_aimg_rec(jnl_record *rec);
uint4			mur_read(jnl_ctl_list *jctl);
void			mur_rem_jctls(reg_ctl_list *rctl);
boolean_t		mur_report_error(jnl_ctl_list *jctl, enum mur_error code);
#if defined(UNIX)
multi_struct 		*mur_token_lookup(token_num token, off_jnl_t rec_time, enum rec_fence_type fence);
int			gtmrecv_fetchresync(int port, seq_num *resync_seqno, seq_num max_reg_seqno);
#elif defined(VMS)
multi_struct 		*mur_token_lookup(token_num token, int4 image_count, off_jnl_t rec_time, enum rec_fence_type fence);
int			gtmrecv_fetchresync(int port, seq_num *resync_seqno);
#endif
void 			mur_tp_resolve_time(jnl_tm_t max_lvrec_time);
void			mur_show_header(jnl_ctl_list *jctl);
boolean_t		mur_select_rec(jnl_ctl_list *jctl);
void			mur_sort_files(void);
boolean_t		mur_ztp_lookback(void);

int	format_time(jnl_proc_time proc_time, char *string, int string_len, int time_format);

#endif /* MUPREC_H_INCLUDED */
