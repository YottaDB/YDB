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

#ifndef __JNL_H__
#define __JNL_H__

#ifndef JNL_S_TIME
#include "v11_jnlsp.h"
#endif

/* If you update JNL_LABEL_TEXT, you need to JNL_VER_THIS and repl_internal_filter array.
 * Also need to follow a set of directions (yet to be written 12/7/2000 -- nars) in case a new set of filters need to be written.
 */
#define JNL_LABEL_TEXT		"GDSJNL11" /* update JNL_VER_THIS and repl_internal_filter array if you update JNL_LABEL_TEXT */
#define JNL_VER_THIS		'\013' /* from GDSJNL11, octal equivalent of decimal 11 */
#define JNL_VER_EARLIEST_REPL	'\007' /* from GDSJNL07 (V4.1-000E), octal equivalent of decimal 07 */
#define	ALIGN_KEY		0xfedcba98

#define JNL_ALLOC_DEF		100
#define JNL_ALLOC_MIN		10
/* #define JNL_ALLOC_MAX		16777216  moved to jnlsp.h */
#ifdef UNIX
#define JNL_BUFFER_DEF		ROUND_UP2(128, IO_BLOCK_SIZE / DISK_BLOCK_SIZE)
#else
#define JNL_BUFFER_DEF		128
#endif
/*	JNL_BUFFER_MIN	database block size / 512 + 1	*/
#define JNL_BUFFER_MAX		2000
/*	JNL_EXTEND_DEF	allocation size / 10		*/
#define JNL_EXTEND_DEF_PERC	0.1
#define JNL_EXTEND_MIN		0
#define JNL_EXTEND_MAX		65535
#define JNL_MIN_WRITE		32768
#define JNL_MAX_WRITE		65536
#define JNL_EXT_DEF		".MJL"
#define JNL_REC_TRAILER		0xFE
#define	JNL_WRT_START_MODULUS	512
#define JNL_WRT_START_MASK	~(JNL_WRT_START_MODULUS - 1)
#define	JNL_WRT_END_MODULUS	8
#define JNL_WRT_END_MASK	~(JNL_WRT_END_MODULUS - 1)
#define	JNL_MIN_ALIGNSIZE	16384
#define JNL_REC_START_BNDRY	8
#define MAX_JNL_REC_SIZE	(DISK_BLOCK_SIZE * 32)
#define MAX_YIELD_LIMIT		2048

/* options (sizeof(char)) to wcs_flu() (currently flush_hdr, write_epoch, sync_epoch) are bit-wise ored */
#define	WCSFLU_NONE		0
#define	WCSFLU_FLUSH_HDR	1
#define	WCSFLU_WRITE_EPOCH	2
#define	WCSFLU_SYNC_EPOCH	4

/* In Unix the unit epoch-second is a second, but in VMS approx. 152.5 epoch-seconds make up a second.
 * This is done due to the following. In VMS, time is a 64-bit quantity whose unit is in 100 nanoseconds.
 * To convert it to a unit of time that has more relevance to the user (the most desirable would be a second)
 * 	and not lose many CPU cycles in the process, we extract the middle 32-bits. This gives us a unit
 *	(which we now refer to as an epoch-second) which is 64K times 100 nanoseconds which is 1/152.5th of a second.
 * Note that we approximate this to be 152 for computation considerations and this may cause an epoch-timer
 *	scheduled to come out say every 1000 seconds to every 996 seconds (note this is only in VMS).
 * Also note that the user interface for epoch-interval is in seconds both in terms of input and output. Only
 *	the internal representation is different.
 */

#ifdef VMS
#define	SECOND2EPOCH_SECOND	152		/* = 100 nanoseconds / 64K */
#else
#define SECOND2EPOCH_SECOND	1
#endif

/* Have epoch-interval of 30 seconds in PRO and 300 seconds in DBG */

#ifndef DEBUG
#define	DEFAULT_EPOCH_INTERVAL	300*SECOND2EPOCH_SECOND		/* in epoch-seconds */
#else
#define	DEFAULT_EPOCH_INTERVAL	 30*SECOND2EPOCH_SECOND 	/* exercise epoch-syncing code relatively more often in DBG */
#endif

#define	MAX_EPOCH_INTERVAL	32767	/* in seconds. Amounts to nearly 10 hours. Don't want to keep db stale so long */

#define JNL_ENABLED(X)		((X)->jnl_state == jnl_open)		/* If TRUE, journal records are to be written */
#define JNL_ALLOWED(X)		((X)->jnl_state != jnl_notallowed)	/* If TRUE, journalling is allowed for the file */
#define REPL_ENABLED(X)		((X)->repl_state == repl_open)		/* If TRUE, replication records are to be written */

#define MUEXTRACT_TYPE(A) 	(((A)[0]-'0')*10 + ((A)[1]-'0')) /* A is a character pointer */

#define PADDED			PADDING

#ifdef BIGENDIAN
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)((uchar_ptr_t)&x + 1))
#else
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)(&x))
#endif

enum jnl_record_type
{
#define JNL_TABLE_ENTRY(A,B,C,D)	A,
#include "v11_jnl_rec_table.h"
#undef JNL_TABLE_ENTRY

	JRT_RECTYPES		/* Total number of journal record types */
};


enum jnl_state_codes
{
	jnl_notallowed,
	jnl_closed,
	jnl_open
};

enum repl_state_codes
{
	repl_closed,
	repl_open
};

typedef struct
{
	int4			min_write_size,	/* if unwritten data gets to this size, write it */
				max_write_size, /* maximum size of any single write */
				size;		/* buffer size */
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	boolean_t		before_images;	/* If TRUE, before-image processing is enabled */
						/* end not volatile QUAD */
	volatile int4		free;		/* relative index of first byte to write in buffer */
	volatile uint4		freeaddr,	/* virtual on-disk address which will correspond to free, when it is written */
				lastaddr,	/* previous freeaddr */
				filesize;	/* highest virtual address available in the file */
						/* end mainline QUAD */
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
	int4			alignsize;		/* alignment size for JRT_ALIGN */
	volatile int4		qiocnt,			/* Number of qio's issued */
				bytcnt,			/* Number of bytes written */
				errcnt,			/* Number of errors during writing */
				reccnt[JRT_RECTYPES];	/* Number of records written per opcode */
	int			filler_align[28 - JRT_RECTYPES];	/* So buff below starts on even (QW) keel */
	/* Note the above filler will fail if JRT_RECTYPES grows beyond 27 elements. In that case, change the start num to
	   the next even number above JRT_RECTYPES.
	*/
	volatile trans_num	epoch_tn;		/* Transaction number for current epoch */
	volatile uint4		next_epoch_time;	/* Time when next epoch is to be written (in epoch-seconds) */
	volatile boolean_t	need_db_fsync;          /* need an fsync of the db file */
	volatile int4		io_in_prog;		/* VMS only: write in progress indicator (NOTE: must manipulate
										only with interlocked instructions */
	global_latch_t		io_in_prog_latch;	/* UNIX only: write in progress indicator */
	CACHELINE_PAD(sizeof(global_latch_t), 1)	/* ; supplied by macro */
	global_latch_t		fsync_in_prog_latch;	/* fsync in progress indicator */
        CACHELINE_PAD(sizeof(global_latch_t), 2)	/* ; supplied by macro */
	/************************************************************************************/
	/* Important: must keep header structure quadword aligned for buffers used in QIO's */
	/************************************************************************************/
	unsigned char		buff[1];		/* Actually buff[size] */
} jnl_buffer;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef jnl_buffer *jnl_buffer_ptr_t;

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
	gd_id			fileid;			/* used for UNIX only */
	vms_lock_sb		*jnllsb;		/* VMS only */
	boolean_t		free_update_inprog;	/* M VMS only */
	int4			regnum;			/* M index for 'tokens' */
	uint4			pini_addr,		/* virtual on-disk address for JRT_PINI record, if journalling */
				lastwrite,		/* M used by jnl_wait */
				new_freeaddr;
	int4			temp_free;		/* M Temp copy of free relative index until full write done */
	double			filler_q0;		/* reset QUAD end mainline */
	int4			new_dsk;		/* A VMS only */
	uint4			new_dskaddr,		/* A VMS only */
				status;			/* A for error reporting */
	volatile boolean_t	dsk_update_inprog;	/* A VMS only */
	volatile boolean_t	qio_active;		/* jnl buffer write in progress in THIS process (recursion indicator) */
	boolean_t		fd_mismatch;
	volatile boolean_t	sync_io;		/* TRUE if the process is using O_SYNC/O_DSYNC for this jnl (UNIX) */
} jnl_private_control;

typedef enum
{
	JNL_KILL,
	JNL_SET,
	JNL_ZKILL,
	JNL_INCTN
} jnl_action_code;

typedef enum
{
 /* 00 */ MUEXT_NULL,
 /* 01 */ MUEXT_PINI,
 /* 02 */ MUEXT_PFIN,
 /* 03 */ MUEXT_EOF,
 /* 04 */ MUEXT_KILL,
 /* 05 */ MUEXT_SET,
 /* 06 */ MUEXT_ZTSTART,
 /* 07 */ MUEXT_ZTCOMMIT,
 /* 08 */ MUEXT_TSTART,
 /* 09 */ MUEXT_TCOMMIT,
 /* 10 */ MUEXT_ZKILL,

	MUEXT_MAX_TYPES
} muextract_type;

typedef struct
{
	mval			*val;
	jnl_action_code		operation;
	gv_key			*key;
} jnl_action;

typedef struct
{
	sgmnt_addrs		*fence_list;
	int			level;
	short			total_regions,
				region_count;
	token_num		token;
} jnl_fence_control;

typedef struct
{
	char			label[sizeof(JNL_LABEL_TEXT) - 1];
	jnl_process_vector	who_created,
				who_opened;
	uint4			end_of_data;
	int4			max_record_length;
	uint4			bov_timestamp,
				eov_timestamp;
	bool			before_images;
	bool			crash;
	bool			update_disabled;
	bool			filler_bool[1];
	int4			alignsize;
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	unsigned short		data_file_name_length;
	unsigned short		prev_jnl_file_name_length;
	char			data_file_name[JNL_NAME_SIZE];
	char			prev_jnl_file_name[JNL_NAME_SIZE];
	int4			repl_state;	/* To state whether replication is turned on for this journal file */
	seq_num			start_seqno;	/* the reg_seqno when this journal file was created */
	trans_num		bov_tn, eov_tn;
} jnl_file_header;

typedef struct
{
	int4			status,
				alloc,
				extend,
				buffer;
	trans_num		tn;
	char			*fn,
				*jnl,
				*jnl_def;
	short			rsize,
				fn_len,
				jnl_len,
				jnl_def_len;
	bool			before_images;
	bool			filler_bool[3];
	int4			alignsize;
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	char			*prev_jnl;
	int4			filler_int4;
	int4			prev_jnl_len;
	int4			repl_state;
	seq_num			reg_seqno;
} jnl_create_info;

/* Journal record definitions */
typedef struct
{
	unsigned short		length;
	char			text[1];		/* Actually text[length] */
} jnl_string;

typedef struct
{
	jnl_process_vector	process_vector;
} struct_jrec_pini;

typedef struct
{
	jnl_process_vector	process_vector;
	uint4			pini_addr;
	trans_num		tn;
} struct_jrec_pfin;

typedef struct
{
	uint4			pini_addr;
	uint4			tc_short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	token_num		token;
	uint4			participants;
	uint4			ts_short_time;
} struct_jrec_tcom;

typedef struct
{
	uint4			pini_addr;	/* pini_addr and rec_seqno are just so that the other fields are the same offset */
	uint4			short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
} struct_jrec_null;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	jnl_string		mumps_node;
	/* Note: for SET, mumps data follows mumps_node */
} struct_jrec_kill_set;

typedef struct 		/* this should be the same as jrec_kill_set_struct except for mumps_node */
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
} fixed_jrec_kill_set;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	token_num		token;
	char			jnl_tid[8];
	jnl_string		mumps_node;
	/* Note: for FSET, GSET, TSET, and USET, mumps data follows mumps_node */
} struct_jrec_tp_kill_set;

typedef struct		/* this should be the same as jrec_tp_kill_set_struct except for mumps_node */
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	token_num		token;
	char			jnl_tid[8];
} fixed_jrec_tp_kill_set;

/* Following is a for logical record. But no logical update to the database.  So always it has fixed length. */
typedef struct
{
        uint4                   pini_addr;
        uint4                   short_time;
        trans_num               tn;
	inctn_opcode_t		opcode;
} struct_jrec_inctn;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	block_id		blknum;
	unsigned short		bsiz;
	char			blk_contents[1];	/* Actually blk_contents[bsiz] */
} struct_jrec_pblk;

typedef struct
{
        uint4                   pini_addr;
        uint4                   short_time;
        trans_num               tn;
        block_id                blknum;
        unsigned short          bsiz;
        char                    blk_contents[1];        /* Actually blk_contents[bsiz] */
} struct_jrec_after_image;

typedef struct
{
	uint4			short_time;
	union
	{
		jnl_string	align_string;
		int4		filler4;		/* for the VAX, we need to ensure that this structure is 8-bytes long */
	}			align_str;
} struct_jrec_align;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	int4			filler_int4;	/* to make the next field (jnl_seqno) 8-byte aligned */
	seq_num			jnl_seqno;
} struct_jrec_epoch;

typedef struct
{
	jnl_process_vector	process_vector;
	trans_num		tn;
	int4			filler_int4;	/* to make the next field (jnl_seqno) 8-byte aligned */
	seq_num			jnl_seqno;
} struct_jrec_eof;

typedef union
{
	struct_jrec_pini		jrec_pini;
	struct_jrec_pfin		jrec_pfin;
	struct_jrec_tcom		jrec_tcom,
					jrec_ztcom;
	struct_jrec_kill_set		jrec_kill,
					jrec_set,
					jrec_zkill;
	struct_jrec_tp_kill_set		jrec_fkill,
					jrec_gkill,
					jrec_tkill,
					jrec_ukill,
					jrec_fset,
					jrec_gset,
					jrec_tset,
					jrec_uset,
					jrec_fzkill,
					jrec_gzkill,
					jrec_tzkill,
					jrec_uzkill;
	struct_jrec_inctn               jrec_inctn;
	struct_jrec_pblk		jrec_pblk;
	struct_jrec_after_image		jrec_aimg;
	struct_jrec_align		jrec_align;
	struct_jrec_null		jrec_null;
	struct_jrec_epoch		jrec_epoch;
	struct_jrec_eof			jrec_eof;
} jrec_union;


typedef struct
{
	char			jrec_type;			/* Actually, enum jnl_record_type */
	unsigned int		jrec_backpointer : 24;		/* Offset to beginning of last record */
	int4			filler_prefix;			/* to make jnl_record and jrec_union accept 8-byte jnl_seqno */
} jrec_prefix;

typedef struct
{
	/* Prefix: */
	char			jrec_type;			/* Actually, enum jnl_record_type */
	unsigned int		jrec_backpointer : 24;		/* Offset to beginning of last record */
	int4			filler_prefix;			/* to make jnl_record and jrec_union accept 8-byte jnl_seqno */
	/* Journal record: */
	jrec_union		val;
	/* Suffix follows journal record */
} jnl_record;

typedef struct
{
	int4			filler_suffix;
	unsigned int		backptr : 24;
	unsigned int		suffix_code : 8;
} jrec_suffix;

typedef struct jnl_format_buff_struct
{
	que_ent				free_que;
	struct  jnl_format_buff_struct	*next;
	enum jnl_record_type		rectype;
	int4				record_size;
	char 				*buff;
	jnl_action			ja;
} jnl_format_buffer;

/* jnl_ prototypes */
int4 v11_jnl_record_length(jnl_record *rec, int4 top);
int jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size); /***type int added***/
void  jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat);
uint4 jnl_qio_start(jnl_private_control *jpc);
uint4 jnl_write_attempt(jnl_private_control *jpc, uint4 threshold);
void jnl_format(jnl_format_buffer *jfb);
void jnl_prc_vector(jnl_process_vector *pv);
void jnl_send_oper(jnl_private_control *jpc, uint4 status);
void jnl_setver(void);
void jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *hdr_buffer);
void jnl_extr_init(void);
void jnlext_write(char *buffer, int length);
uint4 cre_jnl_file(jnl_create_info *info);
uint4 jnl_ensure_open(void);
void  set_jnl_info(gd_region *reg, jnl_create_info *set_jnl_info);

#ifdef VMS
uint4 set_jnl_file_close(void);
void finish_active_jnl_qio(void);
void jnl_start_ast(jnl_private_control *jpc);
uint4 jnl_permit_ast(jnl_private_control *jpc);
void jnl_qio_end(jnl_private_control *jpc);
#endif

void detailed_extract_tcom(jnl_record *rec, uint4 pid, jnl_proc_time *ref_time);
void wcs_defer_wipchk_ast(jnl_private_control *jpc);

int4 mupip_set_jnlfile_aux(jnl_file_header *header);

char *ext2jnlcvt(char *ext_buff, int4 ext_len, jnl_record *rec);
char    *ext2jnl(char *ptr, jnl_record *rec);
char *jnl2extcvt(jnl_record *rec, int4 jnl_len, char *ext_buff);
char    *jnl2ext(char *jnl_buff, char *ext_buff);

#define JREC_PREFIX_SIZE	sizeof(jrec_prefix)
#define JREC_SUFFIX_SIZE	sizeof(jrec_suffix)
#define JNL_SHARE_SIZE(X)	(JNL_ALLOWED(X) ? 							\
				(ROUND_UP(JNL_NAME_EXP_SIZE + sizeof(jnl_buffer), OS_PAGE_SIZE)		\
                                + ROUND_UP(((sgmnt_data_ptr_t)X)->jnl_buffer_size * DISK_BLOCK_SIZE, 	\
					OS_PAGE_SIZE)) : 0)
    /*  pass address of jnl_buffer to get address of expanded jnl file name */
#define JNL_NAME_EXP_PTR(X) ((sm_uc_ptr_t)(X) - JNL_NAME_EXP_SIZE)
#define JNL_GDID_PVT(sa)        (sa->jnl->fileid)
/* Given a journal record, get_jnl_seqno returns the jnl_seqno field
 * NOTE : All replication type records have the same first fields
 * pini_addr, short_time, tn, rec_seqno and jnl_seqno.
 * jnl_seqno is at the same offset in all the records.
 * Modify this function if need be when changing any jrec structure.
 */
#define get_jnl_seqno(j)	((IS_REPL_RECTYPE(REF_CHAR(&(j)->jrec_type))) ? (j)->val.jrec_kill.jnl_seqno : seq_num_zero)
/* Given a journal record, get_tn returns the tn field
 * NOTE : All replication type records have the same first fields
 * pini_addr, short_time, tn, rec_seqno and jnl_seqno.
 * tn is at the same offset in all the records.
 * Modify this function if need be when changing any jrec structure.
 */
#define get_tn(j)		((IS_REPL_RECTYPE(REF_CHAR(&(j)->jrec_type))) ? (j)->val.jrec_kill.tn : 0)

/* In t_end(), we need to write the after-image if DSE or mupip recover/rollback is playing it.
 * But to write it out, we should have it already built before bg_update().
 * Hence, we pre-build the block here itself before invoking t_end().
 */
#define	BUILD_AIMG_IF_JNL_ENABLED(csa, csd, jfb, cse)					\
{											\
	if (JNL_ENABLED(csd))								\
	{										\
		cse = (cw_set_element *)(&cw_set[0]);					\
		cse->new_buff = jfb;							\
		gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, csa->ti->curr_tn);	\
		cse->done = TRUE;							\
	}										\
}

#endif
