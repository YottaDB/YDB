/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef V010_JNL_H_INCLUDED
#define V010_JNL_H_INCLUDED

#ifndef V010_JNLSP_H_INCLUDED
#include "v010_jnlsp.h"
#endif

#define JNL_LABEL_TEXT		"GDSJNL06A"

#define JNL_ALLOC_DEF		100
#define JNL_ALLOC_MIN		10
/* #define JNL_ALLOC_MAX		16777216  moved to jnlsp.h */
#define JNL_BUFFER_DEF		128
/*	JNL_BUFFER_MIN	database block size / 512 + 1	*/
#define JNL_BUFFER_MAX		2016
/*	JNL_EXTEND_DEF	allocation size / 10		*/
#define JNL_EXTEND_DEF_PERC	0.1
#define JNL_EXTEND_MIN		0
#define JNL_EXTEND_MAX		65535
#define JNL_MIN_WRITE		32768
#define JNL_MAX_WRITE		65536
#define JNL_EXT_DEF		".MJL"
#define JNL_REC_TRAILER		0xFE
#define JNL_MAX_FLUSH_TRIES	512
#define JNL_WRT_START_MASK	~511
#define JNL_WRT_END_MASK	~7

#define JNL_ENABLED(X)		((X)->jnl_state == jnl_open)		/* If TRUE, journal records are to be written */
#define JNL_ALLOWED(X)		((X)->jnl_state != jnl_notallowed)	/* If TRUE, journalling is allowed for the file */

#define PADDED			PADDING

#ifdef BIGENDIAN
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)((uchar_ptr_t)&x + 1))
#else
#define THREE_LOW_BYTES(x)	((uchar_ptr_t)(&x))
#endif


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
	jnl_open,
};

typedef struct
{
	int4			min_write_size,	/* if unwritten data gets to this size, write it */
				max_write_size, /* maximum size of any single write */
				size;		/* buffer size */
	unsigned short		epoch_size;	/* How many transactions per epoch */
	bool			before_images;	/* If TRUE, before-image processing is enabled */
	char			filler;
						/* end not volatile QUAD */
	volatile int4		free;		/* relative index of first byte to write in buffer */
	volatile uint4		freeaddr,	/* virtual on-disk address which will correspond to free, when it is written */
				lastaddr,	/* previous freeaddr */
				filesize;	/* highest virtual address available in the file */
						/* end mainline QUAD */
	volatile int4		blocked;
	int4			filler_int4;
	volatile int4		dsk;		/* relative index of 1st byte to write to disk;
						 * if free == dsk, buffer is empty */
	volatile int4		wrtsize;	/* size of write in progress */
	volatile uint4		dskaddr,	/* virtual on-disk address corresponding to dsk */
				now_writer,	/* current owner of io_in_prog */
				image_count;	/* for VMS is_proc_alive */
	volatile struct				/* must be at least word aligned for memory coherency */
	{
		short		cond;
		unsigned short	length;
		int4		dev_specific;
	}			iosb;
	int4			filler_int;		/* end ast QUAD */
	volatile int4		qiocnt,			/* Number of qio's issued */
				bytcnt,			/* Number of bytes written */
				errcnt,			/* Number of errors during writing */
				reccnt[JRT_RECTYPES];	/* Number of records written per opcode */
	volatile trans_num	epoch_tn;		/* Transaction number for current epoch */
	double			filler_q1;		/* QUAD reset to insure memory coherency of the following int */
	int			io_in_prog;		/* If 1, write is in progress (NOTE: must manipulate
										only with interlocked instructions */
	double			filler_q2;		/* QUAD reset to insure quadword alignment in comment below */
        global_latch_t          jb_latch;               /* needed by aswp on HPPA, 16 bytes */
        CACHELINE_PAD(SIZEOF(global_latch_t), 1)	/* ; supplied by macro */
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
	bool			free_update_inprog;	/* M VMS only */
	unsigned char		regnum;			/* M index for 'tokens' */
	unsigned char		filler_char[2];
	uint4			pini_addr,		/* virtual on-disk address for JRT_PINI record, if journalling */
				lastwrite,		/* M used by jnl_wait */
				new_freeaddr;		/* M VMS only */
	int4			temp_free;		/* M Temp copy of free relative index until full write done */
	double			filler_q0;		/* reset QUAD end mainline */
	int4			new_dsk;		/* A VMS only */
	uint4			new_dskaddr,		/* A VMS only */
				status;			/* A for error reporting */
	bool			dsk_update_inprog;	/* A VMS only */
	volatile bool		qio_active;		/* A This process, referenced but not used in UNIX */
	char			filler1[2];		/* prior 3 fields are maintained by AST - preserve memory coherency */
} jnl_private_control;

typedef enum
{
	JNL_KILL,
	JNL_SET,
	JNL_ZKILL
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
	gv_key			*key;
	mstr			val;
	jnl_action_code		operation;
} jnl_action;


typedef struct
{
	sgmnt_addrs		*fence_list;
	int			level;
	short			total_regions,
				region_count;
	uint4			token;
} jnl_fence_control;

typedef struct
{
	char			label[SIZEOF(JNL_LABEL_TEXT) - 1];
	jnl_process_vector	who_created,
				who_opened;
	uint4			end_of_data;
	int4			max_record_length;
	uint4			bov_timestamp,
				eov_timestamp;
	bool			before_images;
	unsigned char		data_file_name_length;
	char			data_file_name[255];
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
} jnl_create_info;


/* Journal record definitions */

typedef struct
{
	unsigned short		length;
	char			text[1];		/* Actually text[length] */
} jnl_string;


struct jrec_pini_struct
{
	jnl_process_vector	process_vector;
};

struct jrec_pfin_struct
{
	jnl_process_vector	process_vector;
	uint4			pini_addr;
	trans_num		tn;
};

struct jrec_tcom_struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	uint4			token,
				participants;
};

struct jrec_kill_set_struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	jnl_string		mumps_node;
	/* Note: for SET, mumps data follows mumps_node */
};

struct jrec_tp_kill_set_struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
	uint4			token;
	jnl_string		mumps_node;
	/* Note: for FSET, GSET, TSET, and USET, mumps data follows mumps_node */
};

struct jrec_pblk_struct
{
	uint4			pini_addr;
	uint4			short_time;
	block_id		blknum;
	unsigned short		bsiz;
	char			blk_contents[1];	/* Actually blk_contents[bsiz] */
};

struct jrec_epoch_struct
{
	uint4			pini_addr;
	uint4			short_time;
	trans_num		tn;
};

struct jrec_eof_struct
{
	jnl_process_vector	process_vector;
	trans_num		tn;
};

typedef union
{
	struct	jrec_pini_struct	jrec_pini;
	struct	jrec_pfin_struct	jrec_pfin;
	struct	jrec_tcom_struct	jrec_tcom,
					jrec_ztcom;
	struct	jrec_kill_set_struct	jrec_kill,
					jrec_set,
					jrec_zkill;
	struct	jrec_tp_kill_set_struct jrec_fkill,
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
	struct	jrec_pblk_struct	jrec_pblk;
	struct	jrec_epoch_struct	jrec_epoch;
	struct	jrec_eof_struct		jrec_eof;
} jrec_union;


typedef struct
{
	char			jrec_type;			/* Actually, enum jnl_record_type */
	unsigned int		jrec_backpointer : 24;		/* Offset to beginning of last record */
} jrec_prefix;

typedef struct
{
	/* Prefix: */
	char			jrec_type;			/* Actually, enum jnl_record_type */
	unsigned int		jrec_backpointer : 24;		/* Offset to beginning of last record */
	/* Journal record: */
	jrec_union		val;
	/* Suffix follows journal record */
} jnl_record;

typedef struct
{
	unsigned int		backptr : 24;
	unsigned int		suffix_code : 8;
} jrec_suffix;

#define JREC_PREFIX_SIZE	SIZEOF(jrec_prefix)
#define JREC_SUFFIX_SIZE	SIZEOF(jrec_suffix)
#define JNL_SHARE_SIZE(X) (JNL_ALLOWED(X) ? ((DIVIDE_ROUND_UP(JNL_NAME_EXP_SIZE + SIZEOF(jnl_buffer), OS_PAGELET_SIZE) \
		+ ((sgmnt_data_ptr_t)X)->jnl_buffer_size) * OS_PAGELET_SIZE) : 0)
    /*  pass address of jnl_buffer to get address of expanded jnl file name */
#define JNL_NAME_EXP_PTR(X) ((sm_uc_ptr_t)(X) - JNL_NAME_EXP_SIZE)

#endif
