/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JNL_H_INCLUDED
#define JNL_H_INCLUDED

#ifndef JNLSP_H_INCLUDED
#include "v12_jnlsp.h"
#endif

#define JPV_LEN_NODE		16
#define JPV_LEN_USER		12
#define JPV_LEN_PRCNAM		16
#define JPV_LEN_TERMINAL	15

enum jpv_types
{
        CURR_JPV = 0,
        ORIG_JPV,
        SRVR_JPV,
        JPV_COUNT
};

typedef struct jnl_process_vector_struct	/* name needed since this is used in cmmdef.h for "pvec" member */
{
	uint4		jpv_pid;			/* Process id */
	int4		jpv_image_count;		/* Image activations [VMS only] */
	jnl_proc_time	jpv_time;			/* Journal record timestamp;  also used for process termination time */
	jnl_proc_time	jpv_login_time;			/* Used for process initialization time */
	char		jpv_node[JPV_LEN_NODE],		/* Node name */
			jpv_user[JPV_LEN_USER],		/* User name */
			jpv_prcnam[JPV_LEN_PRCNAM],	/* Process name */
			jpv_terminal[JPV_LEN_TERMINAL];	/* Login terminal */
	unsigned char	jpv_mode;			/* a la JPI$_MODE [VMS only] */
	int4		filler;
	/* sizeof(jnl_process_vector) must be a multiple of sizeof(int4) */
} jnl_process_vector;

/* If you update JNL_LABEL_TEXT, you need to JNL_VER_THIS and repl_internal_filter array.
 * Also need to follow a set of directions (yet to be written 12/7/2000 -- nars) in case a new set of filters need to be written.
 */
#define JNL_LABEL_TEXT		"GDSJNL14" /* update JNL_VER_THIS and repl_internal_filter array if you update JNL_LABEL_TEXT */
#define JNL_VER_THIS		'\016' /* octal equivalent of JNL_LABEL_TEXT */
#define JNL_VER_EARLIEST_REPL	'\007' /* from GDSJNL07 (V4.1-000E), octal equivalent of decimal 07 */
#define	ALIGN_KEY		0xdeadbeef

#define JNL_ALLOC_DEF		100
#define JNL_ALLOC_MIN		10

/*	JNL_BUFFER_MIN	database block size / 512 + 1	*/
#define JNL_BUFFER_MAX		2000

/*	JNL_EXTEND_DEF	allocation size / 10		*/
#define JNL_EXTEND_DEF_PERC	0.1

#define JNL_EXTEND_MIN		0
#define JNL_EXTEND_MAX		65535
#define JNL_MIN_WRITE		32768
#define JNL_MAX_WRITE		65536
#define JNL_REC_TRAILER		0xFE
#define	JNL_WRT_START_MODULUS	512
#define JNL_WRT_START_MASK	~(JNL_WRT_START_MODULUS - 1)
#define	JNL_WRT_END_MODULUS	8
#define JNL_WRT_END_MASK	~(JNL_WRT_END_MODULUS - 1)
#define	JNL_MIN_ALIGNSIZE	32
#define	JNL_MAX_ALIGNSIZE	32	/* should ideally be 8388608, but not possible until MUPIP RECOVER supports it */
#define JNL_REC_START_BNDRY	8
#define MAX_JNL_REC_SIZE	(DISK_BLOCK_SIZE * 32)

#define MIN_YIELD_LIMIT		0
#define MAX_YIELD_LIMIT		2048
#define DEFAULT_YIELD_LIMIT	8

/* Have a minimum jnl-file-auto-switch-limit of 64 align boundaries (currently each align boundary is 16K) */
#define	JNL_AUTOSWITCHLIMIT_MIN	(64 * JNL_MIN_ALIGNSIZE)

#define	ALQ_DEQ_AUTO_OVERRIDE_INVALID	2	/* some positive value other than 0 or 1 which gets checked in jnl_file_open */

/* options (sizeof(char)) to wcs_flu() (currently flush_hdr, write_epoch, sync_epoch) are bit-wise ored */
#define	WCSFLU_NONE		0
#define	WCSFLU_FLUSH_HDR	1
#define	WCSFLU_WRITE_EPOCH	2
#define	WCSFLU_SYNC_EPOCH	4
#define	WCSFLU_FSYNC_DB		8	/* Currently used only in Unix wcs_flu() */

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

#define MUEXTRACT_TYPE(A) 	(((A)[0]-'0')*10 + ((A)[1]-'0')) /* A is a character pointer */

#define PADDED			PADDING

#ifdef BIGENDIAN
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)((uchar_ptr_t)&x + 1))
#else
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)(&x))
#endif

#define JNL_S_TIME(rec, jrec_type)			MID_TIME((rec)->val.jrec_type.process_vector.jpv_time)
#define JNL_S_TIME_PINI(rec, jrec_type, which_pv)	MID_TIME((rec)->val.jrec_type.process_vector[which_pv].jpv_time)
#define MUR_OPT_MID_TIME(opt)				MID_TIME(mur_options.opt)
#define CMP_JNL_PROC_TIME(W1, W2)			((long)(MID_TIME(W1) - MID_TIME(W2)))
#define EXTTIME(S)					extract_len = exttime(S, mur_extract_buff, extract_len)

enum jnl_record_type
{
#define JNL_TABLE_ENTRY(A,B,C,D)	A,
#include "v12_jnl_rec_table.h"
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
				filesize;	/* highest virtual address available in the file (units in disk-blocks) */
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
	/* alignsize is removed and log2_of_alignsize introduced */
	uint4         		log2_of_alignsize;       /* Ceiling of log2(alignsize) */
 	uint4			autoswitchlimit;	/* limit in disk blocks (max 4GB) when jnl should be auto switched */
	uint4			cycle;			/* shared copy of the number of the current journal file generation */
	volatile int4		qiocnt,			/* Number of qio's issued */
				bytcnt,			/* Number of bytes written */
				errcnt,			/* Number of errors during writing */
				reccnt[JRT_RECTYPES];	/* Number of records written per opcode */
	int			filler_align[29 - JRT_RECTYPES];	/* So buff below starts on even (QW) keel */
	/* Note the above filler will fail if JRT_RECTYPES grows beyond 29 elements and give compiler warning in VMS
	 * if JRT_RECTYPES equals 29. In that case, change the start num to the next odd number above JRT_RECTYPES.
	*/
	volatile trans_num_4byte	epoch_tn;		/* Transaction number for current epoch */
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
	gd_id			fileid;			/* currently initialized and used only by source-server */
	vms_lock_sb		*jnllsb;		/* VMS only */
	boolean_t		free_update_inprog;	/* M VMS only */
	int4			regnum;			/* M index for 'tokens' */
	uint4			pini_addr,		/* virtual on-disk address for JRT_PINI record, if journaling */
				lastwrite,		/* M used by jnl_wait */
				new_freeaddr;
	int4			temp_free;		/* M Temp copy of free relative index until full write done */
	double			filler_q0;		/* reset QUAD end mainline */
	int4			new_dsk;		/* A VMS only */
	uint4			new_dskaddr,		/* A VMS only */
				status;			/* A for error reporting */
	volatile boolean_t	dsk_update_inprog;	/* A VMS only */
	volatile boolean_t	qio_active;		/* jnl buffer write in progress in THIS process (recursion indicator) */
	boolean_t		fd_mismatch;		/* TRUE when jpc->channel does not point to the active journal */
	volatile boolean_t	sync_io;		/* TRUE if the process is using O_SYNC/O_DSYNC for this jnl (UNIX) */
	boolean_t		alq_deq_auto_override;	/* set in jnl_file_open() and reset in set_jnl_info() */
	uint4			jnl_alq;		/* copies of journal allocation/extension/autoswitchlimit */
	uint4			jnl_deq;		/*	to be restored whenever a journal switch occurs   */
	uint4			autoswitchlimit;	/*	during forward phase of journal recovery/rollback */
	uint4			status2;		/* for secondary error status, currently used only in VMS */
	uint4			cycle;			/* private copy of the number of this journal file generation */
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
	jnl_proc_time		bov_timestamp,
				eov_timestamp;
	bool			before_images;
	bool			crash;
	bool			update_disabled;
	bool			filler_bool[1];
	int4			alignsize;
	int4			epoch_interval;	/* Time between successive epochs in epoch-seconds */
	unsigned short		data_file_name_length;
	unsigned short		prev_jnl_file_name_length;
	unsigned short		forw_phase_jnl_file_len; /* Length of forward phase journal file name */
	unsigned short		filler_short;	/* To make sure we have 4/8 byte alignment for the next field */
	char			data_file_name[JNL_NAME_SIZE];
	char			prev_jnl_file_name[JNL_NAME_SIZE];
	char			forw_phase_jnl_file_name[JNL_NAME_SIZE];	/* Forward phase journal file name */
	int4			repl_state;	/* To state whether replication is turned on for this journal file */
	seq_num			start_seqno;	/* the reg_seqno when this journal file was created */
	trans_num_4byte		bov_tn, eov_tn;
	uint4			ftruncate_len;	/* Length upto which journal file was truncated (turn around point stop address) */
	uint4			forw_phase_eof_addr;	/* Eof address and Last record of newly created forw_phase journal file, */
	uint4			forw_phase_last_record;	/* Used incase rollback is interrupted during forward phase */
	uint4			forw_phase_stop_addr;	/* The last stop addr in forward phase journal file name,
							 * Used incase of rollback is interrupted during forward phase */
 	uint4			autoswitchlimit;	/* limit in disk blocks (max 4GB) when jnl should be auto switched */
	uint4			jnl_alq;		/* initial allocation (in blocks) */
	uint4			jnl_deq;		/* extension (in blocks) */
} jnl_file_header;

typedef struct
{
	int4			status,
				alloc,
				extend,
				buffer;
	trans_num_4byte		tn;
	char			*fn,
				*jnl;
	short			rsize,
				fn_len,
				jnl_len,
				jnl_def_len;
	bool			before_images;
	bool			filler_bool[3];
	int4			alignsize;
 	int4			autoswitchlimit;	/* limit in disk blocks (max 4GB or 8388608, hence int4)
							 * when jnl should be auto switched */
	int4			epoch_interval;		/* Time between successive epochs in epoch-seconds */
	char			*prev_jnl;
	int4			prev_jnl_len;
	int4			repl_state;
	seq_num			reg_seqno;
	uint4			status2;		/* for secondary error status information in VMS */
} jnl_create_info;

/* Journal record definitions */
typedef struct
{
	unsigned short		length;
	char			text[1];		/* Actually text[length] */
} jnl_string;

typedef struct
{
	jnl_process_vector	process_vector[JPV_COUNT];
} struct_jrec_pini;

typedef struct
{
	jnl_process_vector	process_vector;
	uint4			pini_addr;
	trans_num_4byte		tn;
} struct_jrec_pfin;

typedef struct
{
	uint4			pini_addr;
	uint4			tc_short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			tc_recov_short_time;
	uint4			ts_recov_short_time;
	token_num		token;
	uint4			participants;
	uint4			ts_short_time;
} struct_jrec_tcom;

typedef struct
{
	uint4			pini_addr;	/* pini_addr and rec_seqno are just so that the other fields are the same offset */
	uint4			short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			recov_short_time;
	uint4			filler_null;
} struct_jrec_null;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			recov_short_time;
	uint4			filler_int4;
	jnl_string		mumps_node;
	/* Note: for SET, mumps data follows mumps_node */
} struct_jrec_kill_set;

typedef struct 		/* this should be the same as jrec_kill_set_struct except for mumps_node */
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			recov_short_time;
	uint4			filler_int4;
} fixed_jrec_kill_set;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			recov_short_time;
	uint4			filler_int4;
	token_num		token;
	char			jnl_tid[8];
	jnl_string		mumps_node;
	/* Note: for FSET, GSET, TSET, and USET, mumps data follows mumps_node */
} struct_jrec_tp_kill_set;

typedef struct		/* this should be the same as jrec_tp_kill_set_struct except for mumps_node */
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num_4byte		tn;
	int4			rec_seqno;
	seq_num			jnl_seqno;
	uint4			recov_short_time;
	int4			filler_int4; /* To make the next field token to start at 8 byte boundary */
	token_num		token;
	char			jnl_tid[8];
} fixed_jrec_tp_kill_set;

/* Following is a for logical record. But no logical update to the database.  So always it has fixed length. */
typedef struct
{
        uint4                   pini_addr;
        uint4                   short_time;
        trans_num_4byte               tn;
	inctn_opcode_t		opcode;
} struct_jrec_inctn;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	block_id		blknum;
	unsigned short		bsiz;
	unsigned short 		filler_short;
	char			blk_contents[1];	/* Actually blk_contents[bsiz] */
} struct_jrec_pblk;

typedef struct
{
	uint4			pini_addr;
	uint4			short_time;
	block_id		blknum;
	unsigned short		bsiz;
	unsigned short 		filler_short; /* To make next field at 8 byte alingement and to fix Unaligned access errors */
} fixed_jrec_pblk;

typedef struct
{
        uint4                   pini_addr;
        uint4                   short_time;
        trans_num_4byte               tn;
        block_id                blknum;
        unsigned short          bsiz;
	unsigned short 		filler_short; /* To make the 8 byte alignment requirement. It's ok to have 6 byte filler for the */
	unsigned int		filler_int; /* AIMG record as it's written less frequently */
        char                    blk_contents[1];        /* Actually blk_contents[bsiz] */
} struct_jrec_after_image;

typedef struct
{
        uint4                   pini_addr;
        uint4                   short_time;
        trans_num_4byte               tn;
        block_id                blknum;
        unsigned short          bsiz;
	unsigned short 		filler_short; /* To make the 8 byte alignment requirement. It's ok to have 6 byte filler for the
						AIMG record as it's written less frequently */
	unsigned int		filler_int;
} fixed_jrec_after_image;

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
	trans_num_4byte		tn;
	int4			filler_int4; /* To make the next field token to start at 8 byte boundary */
	seq_num			jnl_seqno;
} struct_jrec_epoch;

typedef struct
{
	jnl_process_vector	process_vector;
	trans_num_4byte		tn;
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

/* The enum codes below correspond to code-paths that can call set_jnl_file_close() in VMS */
typedef enum
{
        SET_JNL_FILE_CLOSE_BACKUP = 1,	/* just for safety a non-zero value to start with */
	SET_JNL_FILE_CLOSE_SETJNL,
	SET_JNL_FILE_CLOSE_EXTEND,
	SET_JNL_FILE_CLOSE_RUNDOWN,
        SET_JNL_FILE_CLOSE_INVALID_OP
} set_jnl_file_close_opcode_t;

/* jnl_ prototypes */
int4	v12_jnl_record_length(jnl_record *rec, int4 top);
int	jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size); /***type int added***/
void	jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat);
uint4	jnl_qio_start(jnl_private_control *jpc);
uint4	jnl_write_attempt(jnl_private_control *jpc, uint4 threshold);
void	jnl_format(jnl_format_buffer *jfb);
void	jnl_format_set(mval *v);
void	jnl_prc_vector(jnl_process_vector *pv);
void	jnl_send_oper(jnl_private_control *jpc, uint4 status);
void	jnl_setver(void);
void	jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *hdr_buffer);
void	jnl_extr_init(void);
void	jnlext_write(char *buffer, int length);
uint4	cre_jnl_file(jnl_create_info *info);
uint4	jnl_ensure_open(void);
void	set_jnl_info(gd_region *reg, jnl_create_info *set_jnl_info);
int 	exttime(uint4 time, char *buffer, int extract_len);

#ifdef VMS
void	finish_active_jnl_qio(void);
void	jnl_start_ast(jnl_private_control *jpc);
uint4	jnl_permit_ast(jnl_private_control *jpc);
void	jnl_qio_end(jnl_private_control *jpc);
#endif

void	detailed_extract_tcom(jnl_record *rec, uint4 pid);
void	wcs_defer_wipchk_ast(jnl_private_control *jpc);
boolean_t  mupip_set_journal_parse(set_jnl_options *jnl_options, jnl_create_info *jnl_info);
uint4	mupip_set_journal_newstate(set_jnl_options *jnl_options, jnl_create_info *jnl_info, mu_set_rlist *rptr);
uint4	set_jnl_file_close(set_jnl_file_close_opcode_t set_jnl_file_close_opcode);
void	mupip_set_journal_fname(jnl_create_info *jnl_info);
uint4	mupip_set_jnlfile_aux(jnl_file_header *header, char *jnl_fname);
char	*ext2jnlcvt(char *ext_buff, int4 ext_len, jnl_record *rec);
char	*ext2jnl(char *ptr, jnl_record *rec);
char	*jnl2extcvt(jnl_record *rec, int4 jnl_len, char *ext_buff);
char	*jnl2ext(char *jnl_buff, char *ext_buff);

#define JREC_PREFIX_SIZE	sizeof(jrec_prefix)
#define JREC_SUFFIX_SIZE	sizeof(jrec_suffix)
#define JNL_SHARE_SIZE(X)	(JNL_ALLOWED(X) ? 							\
				(ROUND_UP(JNL_NAME_EXP_SIZE + sizeof(jnl_buffer), OS_PAGE_SIZE)		\
                                + ROUND_UP(((sgmnt_data_ptr_t)X)->jnl_buffer_size * DISK_BLOCK_SIZE, 	\
					OS_PAGE_SIZE)) : 0)

/* pass address of jnl_buffer to get address of expanded jnl file name */
#define JNL_NAME_EXP_PTR(X) ((sm_uc_ptr_t)(X) - JNL_NAME_EXP_SIZE)
#define JNL_GDID_PVT(CSA)        ((CSA)->jnl->fileid)

#ifdef UNIX
#define JNL_GDID_PTR(CSA)	((gd_id_ptr_t)(&((CSA)->nl->jnl_file.u)))
#else
#define JNL_GDID_PTR(CSA)	((gd_id_ptr_t)(&((CSA)->nl->jnl_file.jnl_file_id)))
#endif

/* Note that since "cycle" (in jpc and jb below) can rollover the 4G limit back to 0, it should
 * only be used to do "!=" checks and never to do ordered checks like "<", ">", "<=" or ">=".
 */
#define JNL_FILE_SWITCHED(JPC) 	((JPC)->cycle != (JPC)->jnl_buff->cycle)

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

/* define xxx_FIXED_SIZE macros to reflect the fixed size component of each journal record type */
#define	JRT_AIMG_FIXED_SIZE	sizeof(fixed_jrec_after_image)
#define	JRT_ALIGN_FIXED_SIZE	sizeof(struct_jrec_align)
#define	JRT_EOF_FIXED_SIZE	sizeof(struct_jrec_eof)
#define	JRT_EPOCH_FIXED_SIZE	sizeof(struct_jrec_epoch)
#define	JRT_INCTN_FIXED_SIZE	sizeof(struct_jrec_inctn)
#define	JRT_NULL_FIXED_SIZE	sizeof(struct_jrec_null)
#define	JRT_PBLK_FIXED_SIZE	sizeof(fixed_jrec_pblk)
#define	JRT_PFIN_FIXED_SIZE	sizeof(struct_jrec_pfin)
#define	JRT_PINI_FIXED_SIZE	sizeof(struct_jrec_pini)
#define	JRT_TCOM_FIXED_SIZE	sizeof(struct_jrec_tcom)
#define	JRT_ZTCOM_FIXED_SIZE	sizeof(struct_jrec_tcom)

#define	JRT_SET_FIXED_SIZE	sizeof(fixed_jrec_kill_set)
#define	JRT_TSET_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_USET_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_FSET_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_GSET_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)

#define	JRT_KILL_FIXED_SIZE	sizeof(fixed_jrec_kill_set)
#define	JRT_TKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_UKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_FKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_GKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)

#define	JRT_ZKILL_FIXED_SIZE	sizeof(fixed_jrec_kill_set)
#define	JRT_TZKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_UZKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_FZKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)
#define	JRT_GZKILL_FIXED_SIZE	sizeof(fixed_jrec_tp_kill_set)

/* define xxx_RECLEN macros to reflect the journal record length of each journal record type */
#define	AIMG_RECLEN		(JREC_PREFIX_SIZE + JRT_AIMG_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	ALIGN_RECLEN		(JREC_PREFIX_SIZE + JRT_ALIGN_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	EOF_RECLEN		(JREC_PREFIX_SIZE + JRT_EOF_FIXED_SIZE   + JREC_SUFFIX_SIZE)
#define	EPOCH_RECLEN		(JREC_PREFIX_SIZE + JRT_EPOCH_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	INCTN_RECLEN		(JREC_PREFIX_SIZE + JRT_INCTN_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	NULL_RECLEN		(JREC_PREFIX_SIZE + JRT_NULL_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	PBLK_RECLEN		(JREC_PREFIX_SIZE + JRT_PBLK_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	PFIN_RECLEN		(JREC_PREFIX_SIZE + JRT_PFIN_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	PINI_RECLEN		(JREC_PREFIX_SIZE + JRT_PINI_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	TCOM_RECLEN		(JREC_PREFIX_SIZE + JRT_TCOM_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	ZTCOM_RECLEN		(JREC_PREFIX_SIZE + JRT_ZTCOM_FIXED_SIZE + JREC_SUFFIX_SIZE)

#define	SET_RECLEN		(JREC_PREFIX_SIZE + JRT_SET_FIXED_SIZE   + JREC_SUFFIX_SIZE)
#define	TSET_RECLEN		(JREC_PREFIX_SIZE + JRT_TSET_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	USET_RECLEN		(JREC_PREFIX_SIZE + JRT_USET_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	FSET_RECLEN		(JREC_PREFIX_SIZE + JRT_FSET_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	GSET_RECLEN		(JREC_PREFIX_SIZE + JRT_GSET_FIXED_SIZE  + JREC_SUFFIX_SIZE)

#define	KILL_RECLEN		(JREC_PREFIX_SIZE + JRT_KILL_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	TKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_TKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	UKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_UKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	FKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_FKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	GKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_GKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)

#define	ZKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_ZKILL_FIXED_SIZE  + JREC_SUFFIX_SIZE)
#define	TZKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_TZKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	UZKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_UZKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	FZKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_FZKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)
#define	GZKILL_RECLEN		(JREC_PREFIX_SIZE + JRT_GZKILL_FIXED_SIZE + JREC_SUFFIX_SIZE)

#define	JNL_FILE_FIRST_RECORD	ROUND_UP(sizeof(jnl_file_header), DISK_BLOCK_SIZE)
#define	HDR_LEN			ROUND_UP(sizeof(jnl_file_header), DISK_BLOCK_SIZE)
#define	EOF_BACKPTR		(JREC_PREFIX_SIZE + JRT_EOF_FIXED_SIZE)
#define	EPOCH_BACKPTR		(JREC_PREFIX_SIZE + JRT_EPOCH_FIXED_SIZE)
#define	EPOCH_SIZE		1000	/* to be changed later when we allow the user to modify it */

/* the maximum required journal file size (in 512-byte blocks) if the current transaction was the only one in a fresh journal file
 * JNL_FILE_FIRST_RECORD takes into account the size of the journal file header
 * DISK_BLOCK_SIZE takes into account the first epoch record and
 * 	the DISK_BLOCK_SIZE-aligned EOF record that gets written as part of a cre_jnl_file()
 */
#define	MAX_REQD_JNL_FILE_SIZE(tot_jrec_size)											\
	DIVIDE_ROUND_UP((JNL_FILE_FIRST_RECORD + DISK_BLOCK_SIZE + ROUND_UP(tot_jrec_size, DISK_BLOCK_SIZE)), DISK_BLOCK_SIZE)

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
#ifdef UNIX
#define JPC_ALLOC(csa)								\
{										\
	csa->jnl = (jnl_private_control *)malloc(sizeof(*csa->jnl));		\
	memset(csa->jnl, 0, sizeof(*csa->jnl));					\
}
#else
#define JPC_ALLOC(csa)								\
{										\
	vms_lock_sb	*tmp_jnllsb;						\
	if (NULL == csa->jnl)							\
	{									\
		csa->jnl = (jnl_private_control *)malloc(sizeof(*csa->jnl));	\
		memset(csa->jnl, 0, sizeof(*csa->jnl));				\
		csa->jnl->jnllsb = malloc(sizeof(vms_lock_sb));			\
	} else									\
	{									\
		tmp_jnllsb = csa->jnl->jnllsb;					\
		memset(csa->jnl, 0, sizeof(*csa->jnl));				\
		csa->jnl->jnllsb = tmp_jnllsb;					\
	}									\
	memset(csa->jnl->jnllsb, 0, sizeof(vms_lock_sb));			\
}
#endif
#define JNL_INIT(csa, reg, csd)								\
{											\
	csa->jnl_state = csd->jnl_state;						\
	csa->jnl_before_image = csd->jnl_before_image;					\
	csa->repl_state = csd->repl_state;						\
	if JNL_ALLOWED(csa)								\
	{										\
		JPC_ALLOC(csa);								\
		csa->jnl->region = reg;							\
		csa->jnl->jnl_buff = (jnl_buffer_ptr_t)((sm_uc_ptr_t)(csa->nl) + NODE_LOCAL_SPACE + JNL_NAME_EXP_SIZE);\
		csa->jnl->channel = NOJNL;						\
	} else										\
		csa->jnl = NULL;							\
}
#endif /* JNL_H_INCLUDED */
