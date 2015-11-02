/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CCP_H
#define CCP_H

/* requires gdsroot */

enum
{
	CCP_VALBLK_TRANS_HIST,
	CCP_VALBLK_JNL_ADDR,
	CCP_VALBLK_EPOCH_TN,
	CCP_VALBLK_LST_ADDR
};

typedef struct ccp_wait_struct
{
	struct ccp_wait_struct *next;
	uint4 pid;
} ccp_wait;

typedef struct
{
	ccp_wait *first,**last;
} ccp_wait_head;

typedef struct
{
	short cond;
	unsigned short length;
	int4 lockid;
} ccp_iosb;

typedef struct mem_list_struct {
	char *addr;
	uint4 pages;
	struct mem_list_struct *next;
	struct mem_list_struct *prev;
	bool free;
} mem_list;

typedef struct ccp_db_header_struct
{
	struct sgmnt_data_struct *glob_sec;	/* address of file header */
	struct gd_region_struct *greg;		/* address like gv_cur_region */
	struct sgmnt_addrs_struct *segment;	/* like cs_addrs */
	struct ccp_db_header_struct *next;	/* pointer to next db_header in list */
	mem_list *mem_ptr;			/* memory control structure */
	vms_lock_sb wm_iosb;			/* iosb for write mode $enq*/
	vms_lock_sb flush_iosb;			/* iosb for 'buffers flushed' $enq*/
	vms_lock_sb lock_iosb;			/* iosb for MUMPS LOCK $enq*/
	vms_lock_sb refcnt_iosb;		/* iosb for reference count of gtm process in database $enq*/
	ccp_iosb qio_iosb;			/* iosb for $QIO used during file opens and misc operations*/
	ccp_wait_head write_wait;		/* list of pid's to wake upon entering write mode */
	ccp_wait_head flu_wait;			/* list of pid's to wake upon prior machine finishing flush */
	ccp_wait_head exitwm_wait;		/* list of pid's to wake after finishing exitwm */
	ccp_wait_head reopen_wait;		/* list of pid's to wake upon finishing close */
	trans_num last_write_tn;		/* t.n. for last write on this node */
	trans_num master_map_start_tn;		/* t.n. for last master map update*/
	trans_num tick_tn;			/* t.n. as of last clock tick*/
	uint4 last_lk_sequence;		/* sequence for last lock update */
	short unsigned wc_rover;		/* used when flushing dirty buffers so that we don't have to start at the beginning
						   each time */
	struct ccp_db_header_struct *tick_timer_id;	/* unique values to use as timer id and pointer to db */
	struct ccp_db_header_struct *quantum_timer_id;
	struct ccp_db_header_struct *stale_timer_id;
	struct ccp_db_header_struct *wmcrit_timer_id;
	struct ccp_db_header_struct *close_timer_id;
	struct ccp_db_header_struct *exitwm_timer_id;
	struct ccp_db_header_struct *extra_tick_id;
	date_time start_wm_time;		/* Time entered write mode */
	unsigned char	drop_lvl;
	unsigned int quantum_expired : 1;	/* quantum is up*/
	unsigned int tick_in_progress : 1;	/* the tick timer is running*/
	unsigned int wmexit_requested: 1;	/* we have received a request to relinquish write mode*/
	unsigned int write_mode_requested: 1;	/* processing to get write mode has commenced*/
	unsigned int dirty_buffers_acquired : 1; /* set to zero when write mode is entered,
							and to one when ENQ on dirty buffers is acquired*/
	unsigned int filler : 1;
	unsigned int stale_in_progress : 1;	/* stale timer active */
	unsigned int blocking_ast_received : 1;	/* an exitwm blocking ast has been received */
	unsigned int remote_wakeup : 1; 	/* wakeup needed for processes blocked on mumps locks */
	unsigned int extra_tick_started : 1;	/* started timer for extra tick prior to write mode release */
	unsigned int extra_tick_done : 1;	/* extra tick done */
	unsigned int close_region : 1;		/* closing region */
} ccp_db_header;

typedef union
{
	gds_file_id		file_id;
	struct gd_region_struct	*reg;
	ccp_db_header		*h;
	struct FAB		*fab;
	struct
	{
		unsigned char	len;
		unsigned char	txt[23];
	}			str;
	struct
	{
		gds_file_id	fid;
		unsigned short	cycle;
	}			exreq; /* exit write mode request */
	struct
	{
		unsigned short	channel;
		unsigned char	*context;
	}			mbx;
} ccp_action_aux_value;

/* types used to discriminate ccp_action_aux_value union members */
#define CCTVNUL 0	/* no value */
#define CCTVSTR 1	/* string */
#define CCTVMBX 2	/* mailbox id */
#define CCTVFIL 3	/* file id */
#define CCTVDBP 4	/* data_base header pointer */
#define CCTVFAB 5	/* pointer to a FAB */

#define CCP_TABLE_ENTRY(A,B,C,D) A,
typedef enum
{
#include "ccpact_tab.h"
	CCPACTION_COUNT
} ccp_action_code;
#undef CCP_TABLE_ENTRY

typedef struct
{
	ccp_action_code		action;
	uint4		pid;
	ccp_action_aux_value	v;
} ccp_action_record;

typedef struct
{
	int4 fl;
	int4 bl;
} ccp_relque;

typedef struct
{
	ccp_relque q;
	date_time request_time, process_time;
	ccp_action_record value;
}ccp_que_entry;

#define CCP_MBX_NAME "GTM$CCP$MBX"
#define CCP_PRC_NAME "GT.CX_CONTROL"
#define CCP_LOG_NAME "GTM$CCPLOG:CCPERR.LOG"

enum ccp_state_code
{
	CCST_CLOSED,		/* Data base is closed */
	CCST_OPNREQ,		/* A request to open the data base is being processed */
	CCST_RDMODE,		/* The data base is in READ mode, all buffers have been written to disk */
	CCST_WRTREQ,		/* A Write Mode request on our machine is being processed */
	CCST_WRTGNT,		/* Write mode has been granted, but the last machine's dirty buffers are not available */
	CCST_DRTGNT,		/* Write mode has been granted and the dirty buffers are available */
	CCST_WMXREQ,		/* Another machine has requested write mode */
	CCST_WMXGNT		/* We have relinquished write mode, and are writing dirty buffers */
};

#define CCST_MASK_CLOSED (1 << CCST_CLOSED)
#define CCST_MASK_OPNREQ (1 << CCST_OPNREQ)
#define CCST_MASK_RDMODE (1 << CCST_RDMODE)
#define CCST_MASK_WRTREQ (1 << CCST_WRTREQ)
#define CCST_MASK_WRTGNT (1 << CCST_WRTGNT)
#define CCST_MASK_DRTGNT (1 << CCST_DRTGNT)
#define CCST_MASK_WMXREQ (1 << CCST_WMXREQ)
#define CCST_MASK_WMXGNT (1 << CCST_WMXGNT)

#define CCST_MASK_OPEN (~(CCST_MASK_CLOSED | CCST_MASK_OPNREQ))
#define CCST_MASK_WRITE_MODE (CCST_MASK_WRTGNT | CCST_MASK_DRTGNT)
#define CCST_MASK_HAVE_DIRTY_BUFFERS (CCST_MASK_DRTGNT | CCST_MASK_WMXREQ | CCST_MASK_WMXGNT | CCST_MASK_RDMODE)
#define CCP_SEGMENT_STATE(X,Y) (((1 << ((X)->ccp_state)) & (Y)) != 0)

#define CCP_CLOSE_REGION 0
#define CCP_OPEN_REGION 1

#ifdef VMS
#define CCP_FID_MSG(X,Y) (ccp_sendmsg(Y, &FILE_INFO(X)->file_id))
#else
#define CCP_FID_MSG(X,Y)
#endif

bool ccp_act_request(ccp_action_record *rec);
bool ccp_priority_request(ccp_action_record *rec);
short ccp_sendmsg(ccp_action_code action, ccp_action_aux_value *aux_value);
ccp_action_record *ccp_act_select(void);
ccp_db_header *ccp_get_reg_by_fab(struct FAB *fb);
ccp_db_header *ccp_get_reg(gds_file_id *name);
void ccp_exit(void);
void ccp_init(void);
void ccp_quemin_adjust(char oper);
void ccp_staleness(ccp_db_header **p);
short ccp_sendmsg(ccp_action_code action, ccp_action_aux_value *aux_value);
unsigned char *ccp_format_querec(ccp_que_entry *inrec, unsigned char *outbuf, 	 unsigned short outbuflen);
void cce_ccp_ch();
void cce_get_return_channel(ccp_action_aux_value *p);
void cce_dbdump(void);
void ccp_act_complete(void);
void ccp_act_init(void);
void ccp_add_reg(ccp_db_header *d);
void ccp_close1(ccp_db_header *db);
void ccp_dump(void);
void ccp_ewmwtbf_interrupt(ccp_db_header **p);
void ccp_exitwm1a(ccp_db_header *db);
void ccp_exitwm1(ccp_db_header *db);
void ccp_exitwm2a(ccp_db_header *db);
void ccp_exitwm2(ccp_db_header *db);
void ccp_exitwm3(ccp_db_header *db);
void ccp_exitwm_attempt(ccp_db_header *db);
void ccp_exitwm_blkast(ccp_db_header **pdb);
void ccp_extra_tick(ccp_db_header **p);
void ccp_gotdrt_tick(ccp_db_header *db);
void ccp_lkdowake_blkast(ccp_db_header *db);
void ccp_lkrqwake1(ccp_db_header *db);
void ccp_mbx_start(void);
void ccp_opendb1a(struct FAB *fb);
void ccp_opendb1(ccp_db_header *db);
void ccp_opendb1e(struct FAB *fb);
void ccp_opendb2(ccp_db_header *db);
void ccp_opendb3b(ccp_db_header *db);
void ccp_opendb3c(ccp_db_header *db);
void ccp_opendb3(ccp_db_header *db);
void ccp_opendb(ccp_action_record *rec);
void ccp_pndg_proc_add(ccp_wait_head *list, int4 pid);
void ccp_pndg_proc_wake(ccp_wait_head *list);
void ccp_request_write_mode(ccp_db_header *db);
void ccp_reqwm_interrupt(ccp_db_header **pdb);
void ccp_rundown(void);
void ccp_signal_cont(uint4 arg1, ...);
void ccp_tick_interrupt(ccp_db_header **p);
void ccp_tick_start(ccp_db_header *db);
void ccp_tr_checkdb(void);
void ccp_writedb4a(ccp_db_header *db);
void ccp_writedb4(ccp_db_header *db);
void ccp_writedb5(ccp_db_header *db);

#endif
