/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define EXTQW(I)		ptr = &mur_extract_buff[extract_len]; \
	ptr = (char *)i2ascl((uchar_ptr_t)ptr, I); \
	extract_len += (int)(ptr - &mur_extract_buff[extract_len]); \
	mur_extract_buff[extract_len++]='\\'

#define EXTINT(I)		ptr = &mur_extract_buff[extract_len]; \
	ptr = (char *)i2asc((uchar_ptr_t)ptr, I); \
	extract_len += (int)(ptr - &mur_extract_buff[extract_len]); \
	mur_extract_buff[extract_len++]='\\'

int real_len(int length, char *str);

#define	EXTTXT(T,L)		actual = real_len(L, T); \
	memcpy (&mur_extract_buff[extract_len], T, actual); \
	extract_len += actual; \
	mur_extract_buff[extract_len++]= '\\'

#define EXT2BYTES(T)	\
	mur_extract_buff[extract_len++]= *(caddr_t)(T); \
	mur_extract_buff[extract_len++]= *((caddr_t)(T)+1); \
	mur_extract_buff[extract_len++]= '\\'

#define SHOW_HEADER		1
#define SHOW_STATISTICS		2
#define SHOW_BROKEN		4
#define SHOW_ALL_PROCESSES	8
#define SHOW_ACTIVE_PROCESSES	16
#define SHOW_ALL		31	/* All of the above */

#define TRANS_KILLS		1
#define TRANS_SETS		2

#define	MUR_MULTI_LIST_INIT_ALLOC		1024	/* initial allocation for mur_multi_list */
#define	MUR_MULTI_TOKEN_HASHTABLE_INIT_ELEMS	1024	/* initial elements in the mur_multi token hashtable */
#define	MUR_MULTI_SEQNO_HASHTABLE_INIT_ELEMS	1024	/* initial elements in the mur_multi seqno hashtable */
#define	MUR_CURRENT_LIST_INIT_ALLOC		  64	/* initial allocation for current_list in "ctl" structure */
#define	MUR_JNL_REC_BUDDY_LIST_INIT_ALLOC	  64	/* initial allocation for jnl_rec_buddy_list in mur_forward() */
#define	MUR_RAB_BUDDY_LIST_INIT_ALLOC	  	  (1024 * 4)	/* initial allocation for jnl_rec_buddy_list in mur_forward() */

#define DEFAULT_EXTR_BUFSIZE		(FORMATZWR_CONVERSION_FACTOR * MAX_JNL_REC_SIZE)
#define	MUR_PINI_IN_USE_INIT_ELEMS	64		/* initial no. of elements in hash table ctl->pini_in_use */

enum mur_error
{
	MUR_JUSTPINI,
	MUR_NOPINI,
	MUR_NOPFIN,
	MUR_BRKTRANS,
	MUR_INSUFLOOK,
	MUR_MISSING_EXTRACT,
	MUR_MISSING_FILES,
	MUR_MISSING_PREVLINK,
	MUR_MULTEOF,
	MUR_TNCHECK,
	MUR_UNFENCE,
	MUR_UNKNOWN
};

enum mur_fence_type
{
	FENCE_NONE,
	FENCE_PROCESS,
	FENCE_ALWAYS
};

typedef struct
{
	unsigned char		*recaddr;
	unsigned int		reclen;
	uint4			dskaddr;
	unsigned char		*recbuff;	/* copy of record in aligned buffer */
	struct mur_file_control *pvt;
} mur_rab;

typedef struct broken_element
{
	int4	count;		/* denotes broken or not. if -1 then unbroken, else broken */
} broken_struct;

struct pini_list_struct
{
	struct pini_list_struct	*next;
	uint4			pini_addr;
	jnl_process_vector	jpv;
};

struct current_struct
{
	que_ent			free_que;	/* should be the first member in this structure */
	struct current_struct	*next;
	uint4			pini_addr;
	token_num		token;
	uint4			pid;
	char			process[JPV_LEN_PRCNAM],
				user[JPV_LEN_USER];
	bool			broken;
};

typedef struct show_list_struct
{
	struct show_list_struct	*next;
	jnl_process_vector	jpv;
	bool			broken,
				recovered;
} show_list_type;

typedef struct ctl_list_struct
{
	struct ctl_list_struct	*next,
				*prev;
	struct pini_list_struct	*pini_list;
	struct current_struct	*broken_list,
				*current_list;
	struct file_control_struct
				*db_ctl;
	struct gd_region_struct *gd;
	void			*tab_ptr;	/* actually htab_desc *, but due to dependency conflicts keep it void * */
	mur_rab			*rab;
	show_list_type		*show_list;
	trans_num		jnl_tn,
				db_tn,
				turn_around_tn,
				bov_tn,
				eov_tn;
	int4			db_id;
	uint4			stop_addr;
	uint4			consist_stop_addr;
	int4			broken_entries,
				lookback_count,
				jnlrec_cnt[JRT_RECTYPES];
	short			jnl_fn_len;
	char			jnl_fn[256],
				jnl_state,
				repl_state;
	bool			concat_next,
				concat_prev,
				bypass,
				found_eof,
				clustered,
				reached_lookback_limit;
	jnl_proc_time		lookback_time;
	jnl_proc_time		bov_timestamp,
				eov_timestamp;
	seq_num			stop_jnl_seqno;
	seq_num			jnl_seqno;
	struct hashtab_t	*pini_in_use; /* Store hash table entries for pini_addr */
	boolean_t		before_image; /* True if the database has before image journalling enabled */
	uint4			turn_around_epoch_time; /* Used in forward processing for restoring turn around epoch time stamp */
} ctl_list;


typedef struct upd_ctl_list_struct
{
        struct upd_ctl_list_struct      *next;

	struct file_control_struct
	                                *db_ctl;
	struct gd_region_struct         *gd;


}upd_proc_ctl;

bool upd_open_files(upd_proc_ctl **upd_db_files);
upd_proc_ctl *read_db_files_from_gld(gd_addr *addr);

typedef struct redirect_list_struct
{
	struct redirect_list_struct
				*next;
	short			org_name_len,
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
				since_time;
	redirect_list		*redirect;
	select_list		*user,
				*database,		/* UNIX only? */
				*global,
				*process;
	long_list		*id;
	void                    *brktrans_file_info;    /* for a pointer to a structure described in filestruct.h */
	void			*extr_file_info;	/* for a pointer to a structure described in filestruct.h */
	void			*losttrans_file_info;	/* for a pointer to a structure described in filestruct.h */
	enum mur_fence_type	fences;
	int4			error_limit,
				show,
				lookback_opers,
				fetchresync, epoch_limit;
	char			transaction;
	bool			forward,
				update,
				verify,
				interactive,
				before,
				since,
				lookback_time_specified,
				lookback_opers_specified,
				log,
				detail,
				selection,
				rollback,
				notncheck,
				losttrans,
				apply_after_image,
				preserve_jnl,
				chain;
} mur_opt_struct;

bool mur_back_process(ctl_list *ctl);
bool mur_brktrans_open_files(ctl_list *ctl);
bool mur_check_jnlfiles_present(ctl_list **jnl_files);
bool mur_do_record(ctl_list *ctl);
bool mur_do_wildcard(char *jnl_str, char *pat_str, int jnl_len, int pat_len);
bool mur_forward_process(ctl_list *ctl);
bool mur_insert_prev(ctl_list *ctl, ctl_list **jnl_files);
bool mur_interactive(void);
bool mur_jnlhdr_bov_check(jnl_file_header *header, int jnl_fn_len, char *jnl_fn);
bool mur_jnlhdr_multi_bov_check(jnl_file_header *prev_header, int prev_jnl_fn_len,
	char *prev_jnl_fn, jnl_file_header *header, int jnl_fn_len, char *jnl_fn, boolean_t ordered);
bool mur_lookback_process(ctl_list *ctl);
bool mur_lookup_broken(ctl_list *ctl, uint4 pini_addr, token_num token);
bool mur_lookup_multi(ctl_list *ctl, uint4 pini, token_num token, seq_num seqno);
bool mur_multi_extant(void);
bool mur_multi_missing(int4 epid);
bool mur_open_files(ctl_list **jnl_files);
bool mur_report_error(ctl_list *ctl, enum mur_error code);
bool mur_sort_and_checktn(ctl_list **jnl_files);
bool mur_sort_files(ctl_list **jnl_file_list_ptr);
ctl_list *mur_get_jnl_files(void);
int4 mur_blocks_free(ctl_list *ctl);
int4 mur_decrement_multi(int4 epid, token_num token, seq_num seqno);
jnl_file_header *mur_get_file_header(mur_rab *r);
jnl_process_vector *mur_get_pini_jpv(ctl_list *ctl, uint4 pini_addr);
mur_rab *mur_rab_create(int buffer_size);
struct current_struct *mur_lookup_current(ctl_list *ctl, uint4 pini_addr);
uint4 mur_read(mur_rab *, uint4);
uint4 mur_fill_with_zeros(short channel, uint4 from, uint4 to);
uint4 mur_forward(ctl_list *ctl);
uint4 mur_lookup_lookback_time(void);
uint4 mur_next(mur_rab *r, uint4 dskaddr);
uint4 mur_previous(mur_rab *r, uint4 dskaddr);
void mur_close_files(void);
void mur_cre_broken(ctl_list *ctl, uint4 pini_addr, token_num token);
void mur_cre_current(ctl_list *ctl, uint4 pini_addr, token_num token, jnl_process_vector *pv, bool broken);
void mur_cre_multi(int4 epid, token_num token, int4 count, uint4 lookback_time, seq_num seqno);
void mur_current_initialize(void);
void mur_delete_current(ctl_list *ctl, uint4 pini_addr);
void mur_do_show(ctl_list *ctl);
void mur_empty_current(ctl_list *ctl);
void mur_forward_buddy_list_init(void);
void mur_get_options(void);
void mur_include_broken(ctl_list *ctl);
void mur_jnl_read_error(ctl_list *ctl, uint4 status, bool ok);
void mur_master_map(ctl_list *ctl);
void mur_move_curr_to_broken(ctl_list *ctl, struct current_struct *curr);
void mur_multi_initialize(void);
void mur_output_record(ctl_list *ctl);
void mur_output_show(ctl_list *ctl);
void mur_recover_write_epoch_rec(ctl_list **jnl_files);
void mur_rollback_truncate(ctl_list **jnl_files);
seq_num rlbk_lookup_seqno(void);
void jnlext1_write(ctl_list *ctl);
int	gtmrecv_fetchresync(ctl_list *jnl_files, int port, seq_num *resync_seqno);
uint4	mur_block_count_correct(void);
boolean_t mur_crejnl_forwphase_file(ctl_list **jnl_files);
void mur_put_aimg_rec(jnl_record *rec);


#include "muprecsp.h"

struct mur_buffer_desc
{
	unsigned char		*txtbase;
	int4			txtlen;
	unsigned char		*bufbase;
	int4			buflen;
	uint4			dskaddr;   /* on disk, in bytes, block start */
#ifdef VMS
	struct
	{
		unsigned short	cond;
		unsigned short	length;
		int4		dev_specific;
	}			iosb;
#endif
	struct mur_file_control	*backptr;
};

struct mur_file_control
{
	unsigned char		*seq_recaddr;
	unsigned int		seq_reclen;
#ifdef VMS
	struct FAB		*fab;
#endif
	int4			blocksize;
	uint4			eof_addr;
	int4			txt_remaining;
	unsigned char		*alloc_base;
	int4			alloc_len;
	uint4			last_record;
	struct mur_buffer_desc	random_buff;
	struct mur_buffer_desc	seq_buff[2];
	jnl_file_header		*jfh;
	int			bufindex;
#if defined(VMS)
	short			channel;
#elif defined(UNIX)
	int			fd;
#endif
};

#define MINIMUM_BUFFER_SIZE (DISK_BLOCK_SIZE * 32)

#ifdef UNIX
#define BKUP_CMD	"cp -p "
#define RECOVERSUFFIX	"_recover"
#define RLBKSUFFIX	"_roll_bak"
#define NO_FD_OPEN     -1
#endif
#define FORWSUFFIX	"_forw_phase"

#ifdef VMS
#define BKUP_CMD	"BACKUP/IGNORE=INTERLOCK "
#define RNM_CMD		"RENAME "
#define RECOVERSUFFIX	"_RECOVER"
#define RLBKSUFFIX	"_ROLL_BAK"
#define RNMSUFFIX	"_ROLLED_BAK"
#endif

#define COPY_BOV_FIELDS_FROM_JNLHDR_TO_CTL(header, ctl)			\
	ctl->bov_timestamp = header->bov_timestamp;			\
	ctl->eov_timestamp = header->eov_timestamp;			\
	ctl->bov_tn = header->bov_tn;					\
	ctl->eov_tn = header->eov_tn;

#define DUMMY_FILE_ID	"123456"		/* needed only in VMS, but included here for lack of a better generic place */
#define	COPY_MUR_JREC_FIXED_FIELDS(mur_jrec_fixed_field, jrec)		\
	mur_jrec_fixed_field.short_time = jrec->short_time;		\
	mur_jrec_fixed_field.rec_seqno = jrec->rec_seqno;		\
	mur_jrec_fixed_field.jnl_seqno = jrec->jnl_seqno;		\
	cur_logirec_short_time = jrec->short_time;

