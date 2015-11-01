/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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

#define JNL_EXTR_LABEL		"GDSJEX01"
#define JNL_DET_EXTR_LABEL	"GDSJDX01"

#define EXTQW(I)							\
	ptr = &murgbl.extr_buff[extract_len];				\
	ptr = (char *)i2ascl((uchar_ptr_t)ptr, I);			\
	extract_len += (int)(ptr - &murgbl.extr_buff[extract_len]);	\
	murgbl.extr_buff[extract_len++]='\\'

#define EXTINT(I)							\
	ptr = &murgbl.extr_buff[extract_len];				\
	ptr = (char *)i2asc((uchar_ptr_t)ptr, I);			\
	extract_len += (int)(ptr - &murgbl.extr_buff[extract_len]);	\
	murgbl.extr_buff[extract_len++]='\\'

#define EXT_DET_PREFIX()									\
{												\
	SPRINTF(murgbl.extr_buff, "0x%08x [0x%04x] :: ", mur_jctl->rec_offset, mur_rab.jreclen);\
	extract_len = strlen(murgbl.extr_buff);							\
	memcpy(murgbl.extr_buff + extract_len, jrt_label[rec->prefix.jrec_type], LAB_LEN);	\
	extract_len += LAB_LEN;									\
	memcpy(murgbl.extr_buff + extract_len, LAB_TERM, LAB_TERM_SZ);				\
	extract_len += LAB_TERM_SZ;								\
}

int real_len(int length, char *str);

#define	EXTTXT(T,L)		actual = real_len(L, T);		\
	memcpy (&murgbl.extr_buff[extract_len], T, actual);		\
	extract_len += actual;						\
	murgbl.extr_buff[extract_len++]= '\\'

#define EXT2BYTES(T)							\
	murgbl.extr_buff[extract_len++]= *(caddr_t)(T);			\
	murgbl.extr_buff[extract_len++]= *((caddr_t)(T)+1);		\
	murgbl.extr_buff[extract_len++]= '\\'

/* extract jnl record "rec" using extraction routine "extract" into file "file_info" (extract/lost/broken transaction files) */
#define EXTRACT_JNLREC(rec, extract, file_info, status)			\
{									\
	pini_list_struct	*plst;					\
									\
	status = mur_get_pini((rec)->prefix.pini_addr, &plst);		\
	if (SS_NORMAL == status)					\
		(*extract)((file_info), (rec), plst);			\
}

#define	EXTPID(plst)					\
{							\
	EXTINT(plst->jpv.jpv_pid);			\
	EXTINT(plst->origjpv.jpv_pid);			\
}

#define JNL_PUT_MSG_PROGRESS(STR)								\
{												\
	now_t	now;	/* for GET_CUR_TIME macro */						\
	char	*time_ptr, time_str[CTIME_BEFORE_NL + 2];					\
												\
	GET_CUR_TIME;										\
	gtm_putmsg(VARLSTCNT(6) ERR_MUJNLSTAT, 4, LEN_AND_LIT(STR), CTIME_BEFORE_NL, time_ptr);	\
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

#if defined(UNIX)

#define	TIME_FORMAT_STRING	"YYYY/MM/DD HH:MM:SS"
#define LENGTH_OF_TIME		STR_LIT_LEN(TIME_FORMAT_STRING)
#define	GET_TIME_STR(input_time, time_str)								\
{													\
	time_t		short_time;									\
	struct tm	*tsp;										\
													\
	short_time = (time_t)input_time;								\
	tsp = localtime((const time_t *)&short_time);							\
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
	assert(LENGTH_OF_TIME < sizeof(time_str));							\
	JNL_WHOLE_FROM_SHORT_TIME(long_time, input_time);						\
	GET_LONG_TIME_STR(long_time, time_str, sizeof(time_str));					\
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

#define	REL2ABSTIME(deltatime, basetime, roundup)							\
	deltatime = mur_rel2abstime(deltatime, basetime, roundup);

#endif

#define IS_VALID_RECTYPE(JREC)											\
(														\
	(JREC)->prefix.jrec_type > JRT_BAD && (JREC)->prefix.jrec_type < JRT_RECTYPES /* valid rectype */	\
)

#define IS_VALID_LEN_FROM_PREFIX(JREC, JFH)			\
( /* length within range */					\
	(JREC)->prefix.forwptr > MIN_JNLREC_SIZE  &&		\
	(JREC)->prefix.forwptr <= (JFH)->max_phys_reclen	\
)

#define IS_VALID_LEN_FROM_SUFFIX(SUFFIX, JFH)			\
( /* length within range */					\
  	(SUFFIX)->backptr > MIN_JNLREC_SIZE &&			\
	(SUFFIX)->backptr <= (JFH)->max_phys_reclen		\
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
#define MUR_MULTI_LIST_INIT_ALLOC		1024	/* initial allocation for mur_multi_list */
#define MUR_MULTI_HASHTABLE_INIT_ELEMS		1024	/* initial elements in the token table */
#define MUR_PINI_LIST_INIT_ELEMS		 256	/* initial no. of elements in hash table mur_jctl->pini_list */

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

enum mur_error
{
	MUR_DUPTOKEN,
	MUR_PREVJNLNOEOF,
	MUR_JNLBADRECFMT
};

enum mur_fence_type
{
	FENCE_NONE,
	FENCE_PROCESS,
	FENCE_ALWAYS
};

enum pini_rec_stat
{
	IGNORE_PROC = 0,
	ACTIVE_PROC = 1,
	FINISHED_PROC = 2,
	BROKEN_PROC = 4
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
	boolean_t		db_updated;
	boolean_t		intrpt_recovery;
	int			reg_total;		/* total number of regions involved in actual mupip journal flow */
	int			reg_full_total;		/* total including those regions that were opened but were discarded */
	int	            	err_cnt;
	int			wrn_count;
	int			broken_cnt;		/* Number of broken entries */
	int			max_extr_record_length;	/* maximum size of zwr-format extracted journal record */
	jnl_tm_t		tp_resolve_time;	/* Time of the point upto which a region will be processed for
							   TP token resolution for backward or forward recover */
	seq_num			resync_seqno;		/* is 0, if consistent rollback and no interrupted recovery */
	seq_num			stop_rlbk_seqno;	/* Where fetch_resync/resync rollback stop to apply to database */
	seq_num			consist_jnl_seqno;	/* Simulate replication server's sequence number */
	htab_desc		token_table;
	buddy_list      	*multi_list;
	buddy_list     		*pini_buddy_list;	/* Buddy list for pini_list */
	char			*extr_buff;
	jnl_process_vector	*prc_vec;		/* for recover process */
	void			*file_info[TOT_EXTR_TYPES];/* for a pointer to a structure described in filestruct.h */
} mur_gbls_t;

typedef struct
{
	struct_jrec_pini	*pinirec;		/* It must be used only after mur_get_pini() call */
	jnl_record		*jnlrec;		/* This points to jnl record read */
	unsigned int		jreclen;		/* length of the journal record read */
} mur_rab_t;

typedef struct multi_element_struct
{
	token_num			token;
	int4				pid;
	VMS_ONLY(int4			image_count;)	/* Image activations */
	jnl_tm_t			time;
	int				regnum;		/* Last partner (region) seen */
	int				partner;	/* Number of regions involved in TP/ZTP */
	enum rec_fence_type 		fence;		/* NOFENCE or TPFENCE or ZTPFENCE */
	struct multi_element_struct	*next;
} multi_struct;

typedef struct pini_list
{
	uint4			pini_addr;
	uint4			new_pini_addr;	/* used in forward phase of recovery */
	jnl_process_vector	jpv;		/* CURR_JPV. Current process's JPV. For GTCM server we also use this. */
	jnl_process_vector	origjpv;	/* ORIG_JPV. Used for GTCM client only */
	enum pini_rec_stat	state;		/* used for show qualifier */
} pini_list_struct;

typedef struct jnl_ctl_list_struct
{
	unsigned char			jnl_fn[JNL_NAME_SIZE];	/* Journal file name */
	unsigned int			jnl_fn_len;		/* Length of journal fine name string */
	jnl_file_header			*jfh;			/* journal file header */
	jnl_tm_t 			lvrec_time;		/* Last Valid Journal Record's Time Stamp */
	off_jnl_t 			lvrec_off;		/* Last Valid Journal Record's Offset */
	off_jnl_t 			rec_offset;		/* Last processed record's offset */
	off_jnl_t			os_filesize;		/* OS file size in bytes  */
	off_jnl_t			eof_addr;		/* Offset of end of last valid record of the journal */
	off_jnl_t 			save_turn_around_offset;/* Turn around point journal record's offset for each region.
								   Necessary for noverify and interrupted recovery */
	off_jnl_t 			turn_around_offset; 	/* Turn around point journal record's offset for each region */
	jnl_tm_t			turn_around_time; 	/* Turn around time for this region */
	boolean_t 			properly_closed;	/* TRUE if journal was properly closed, having written EOF;
									FALSE otherwise */
        boolean_t                       tail_analysis;		/* true for mur_fread_eof */
	boolean_t 			before_image; 		/* True if the database has before image journaling enabled */
        boolean_t                       read_only;		/* TRUE if read_only for extract/show/verify */
	trans_num 			turn_around_tn;		/* Turn around point transaction number of EPOCH */
	seq_num 			turn_around_seqno;	/* Turn around point jnl_seqno of EPOCH */
	int				jnlrec_cnt[JRT_RECTYPES];/* Count of each type of record found in this journal  */
	int4				status;			/* Last status of the last operation done on this journal */
	uint4				status2;		/* Last secondary status of the last operation done on
									this journal */
	fd_type				channel;
#if defined(VMS)
	struct FAB			*fab;
#endif
	struct hashtab_t               	*pini_list;		/* hash table of pini_addr to pid list */
	struct reg_ctl_list_struct	*reg_ctl;		/* Back pointer to this region's reg_ctl_list */
	struct jnl_ctl_list_struct 	*next_gen;		/* next generation journal file */
	struct jnl_ctl_list_struct 	*prev_gen;		/* previous generation journal file */
} jnl_ctl_list;


typedef struct reg_ctl_list_struct
{
	struct gd_region_struct		*gd;			/* region info */
	sgmnt_addrs			*csa;			/* cs_addrs of the region */
	sgmnt_data_ptr_t		csd;			/* cs_data of the region */
	file_control 			*db_ctl;		/* To do dbfilop() */
	struct jnl_ctl_list_struct 	*jctl;			/* Current generation journal file control info */
	struct jnl_ctl_list_struct 	*jctl_head;		/* For forward recovery starting (earliest) generation
									journal file to be processed. */
	struct jnl_ctl_list_struct	*jctl_save_turn_around;	/* First pass turn around point journal file.
								   Necessary for for noverify and interrupted recovery */
	struct jnl_ctl_list_struct	*jctl_turn_around;	/* final pass turn around point journal file */
	struct jnl_ctl_list_struct 	*jctl_alt_head;		/* For backward recovery turn around point
									journal file of interrupted recovery. */
	void				*tab_ptr;      		/* actually htab_desc*, but due to dependency conflicts
									keep it void *. Used for gv_target info for
									globals in mur_output_record() */
	jnl_tm_t 			lvrec_time;		/* Last Valid Journal Record's Time Stamp across all generations */
	int				jnl_state;
	int				repl_state;
	int4 				lookback_count;
	boolean_t 			before_image;		/* True if the database has before image journaling enabled */
	boolean_t			standalone;		/* If standalone access was acheived for the region */
	boolean_t			recov_interrupted;	/* a copy of csd->recov_interrupted before resetting it to TRUE */
	boolean_t			jfh_recov_interrupted;	/* Whether latest generation journal file was created by recover */
	trans_num			db_tn;			/* database curr_tn when region is opened first by recover */
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

#define MUR_TOKEN_ADD(multi, rec_token, rec_pid, rec_image_count, rec_tok_time, rec_partner, rec_fence, rec_regno)	\
{												\
	char	new;										\
	multi = (multi_struct *)get_new_element(murgbl.multi_list, 1);				\
	multi->token = rec_token;								\
	multi->pid = rec_pid;									\
	VMS_ONLY(multi->image_count = rec_image_count;)						\
	multi->time = rec_tok_time;								\
	multi->partner = rec_partner;								\
	multi->fence = rec_fence;								\
	multi->regnum = rec_regno;								\
	hentry = ht_put(&murgbl.token_table, (mname *)&(multi->token), &new);			\
	assert(new || NULL != hentry->ptr);							\
	if (!new && hentry->ptr)								\
		multi->next = (multi_struct *)hentry->ptr;					\
	else											\
		multi->next = NULL;								\
	hentry->ptr = (char *)multi;								\
	if (rec_partner)									\
		murgbl.broken_cnt = murgbl.broken_cnt + 1;					\
}

#define	MUR_WITHIN_ERROR_LIMIT(err_cnt, error_limit) ((++err_cnt <= error_limit) || (mur_options.interactive && mur_interactive()))

#if defined(UNIX)
#define MUR_TOKEN_LOOKUP(token, pid, image_count, rec_time, fence) mur_token_lookup(token, pid, rec_time, fence)
#elif defined(VMS)
#define MUR_TOKEN_LOOKUP(token, pid, image_count, rec_time, fence) mur_token_lookup(token, pid, image_count, rec_time, fence)
#endif

/* Prototypes */
int			gtmrecv_fetchresync(int port, seq_num *resync_seqno);
void			jnlext_write(fi_type *file_info, char *buffer, int length);
uint4			mur_apply_pblk(boolean_t apply_intrpt_pblk);
boolean_t 		mur_back_process(boolean_t apply_pblk);
uint4 			mur_block_count_correct(void);
int4			mur_blocks_free(void);
void			mur_close_files(void);
void			mur_close_file_extfmt(void);
int4			mur_cre_file_extfmt(int recstat);
boolean_t		mur_do_wildcard(char *jnl_str, char *pat_str, int jnl_len, int pat_len);
uint4			mur_forward(jnl_tm_t min_broken_time, seq_num min_broken_seqno, seq_num losttn_seqno);
boolean_t		mur_fopen_sp(jnl_ctl_list *jctl);
boolean_t		mur_fopen (jnl_ctl_list *jctl);
boolean_t		mur_fclose(jnl_ctl_list *jctl);
void			mur_get_options(void);
uint4			mur_get_pini(off_jnl_t pini_addr, pini_list_struct **pplst);
void			mur_init(void);
boolean_t		mur_insert_prev(void);
boolean_t		mur_interactive(void);
boolean_t		mur_jctl_from_next_gen(void);
void			mur_master_map(void);
void 			mur_multi_rehash(void);
uint4			mur_next (off_jnl_t dskaddr);
uint4 			mur_next_rec(void);
boolean_t		mur_open_files(void);
uint4			mur_output_pblk(void);
uint4			mur_output_record(void);
void			mur_output_show(void);
void			mur_pini_addr_reset(void);
uint4			mur_pini_state(uint4 pini_addr, int state);
uint4			mur_prev(off_jnl_t dskaddr);
uint4			mur_prev_rec(void);
uint4			mur_process_intrpt_recov(void);
void 			mur_process_seqno_table(seq_num *min_broken_seqno, seq_num *losttn_seqno);
jnl_tm_t 		mur_process_token_table(boolean_t *ztp_broken);
void			mur_put_aimg_rec(jnl_record *rec);
uint4			mur_read(jnl_ctl_list *jctl);
void			mur_rem_jctls(reg_ctl_list *rctl);
boolean_t		mur_report_error(enum mur_error code);
#if defined(UNIX)
multi_struct 		*mur_token_lookup(token_num token, uint4 pid, off_jnl_t rec_time, enum rec_fence_type fence);
#elif defined(VMS)
multi_struct 		*mur_token_lookup(token_num token, uint4 pid, int4 image_count,
									off_jnl_t rec_time, enum rec_fence_type fence);
#endif
void			mur_show_header(jnl_ctl_list *jctl);
boolean_t		mur_select_rec(void);
void			mur_sort_files(void);
boolean_t		mur_ztp_lookback(void);
#endif /* MUPREC_H_INCLUDED */
