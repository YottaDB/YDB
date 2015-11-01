/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define ZAREQUEST_SENT		16
#define LREQUEST_SENT		8
#define REQUEST_PENDING		4
#define REMOTE_ZALLOCATES	2
#define REMOTE_LOCKS		1
#define REMOTE_CLR_MASK		(ZAREQUEST_SENT + LREQUEST_SENT + REMOTE_ZALLOCATES + REMOTE_LOCKS)
#define CM_BUFFER_OVERHEAD	20
#define CM_BLKPASS		40

#define S_PROTOCOL		"VAXVMSGTM023GCM010               "
#define S_HDRSIZE		1
#define S_PROTSIZE		33
#define S_REGINSIZE		6
#define S_LAFLAGSIZE		1
#define S_SUBLISTSIZE		1
#define CM_MINBUFSIZE		512 + CM_BUFFER_OVERHEAD

#define CMLCK_REQUEUE		0
#define CM_LOCKS		0
#define CM_ZALLOCATES		0x80
#define CM_NOLKCANCEL		256

#define CM_WRITE		1
#define CM_READ			0
#define CM_NOOP			2

#define CMMS_E_ERROR            1
#define CMMS_L_LKCANALL         2
#define CMMS_L_LKCANCEL         3
#define CMMS_L_LKDELETE	        4
#define CMMS_L_LKREQIMMED       5
#define CMMS_L_LKREQNODE        6
#define CMMS_L_LKREQUEST        7
#define CMMS_L_LKRESUME         8
#define CMMS_L_LKACQUIRE        9
#define CMMS_L_LKSUSPEND        10
#define CMMS_M_LKABORT          11
#define CMMS_M_LKBLOCKED        12
#define CMMS_M_LKGRANTED        13
#define CMMS_M_LKDELETED        14
#define CMMS_M_LKSUSPENDED	15
#define CMMS_Q_DATA             16
#define CMMS_Q_GET              17
#define CMMS_Q_KILL             18
#define CMMS_Q_ORDER            19
#define CMMS_Q_PREV             20
#define CMMS_Q_PUT              21
#define CMMS_Q_QUERY            22
#define CMMS_Q_ZWITHDRAW        23
#define CMMS_R_DATA             24
#define CMMS_R_GET              25
#define CMMS_R_KILL             26
#define CMMS_R_ORDER            27
#define CMMS_R_PREV             28
#define CMMS_R_PUT              29
#define CMMS_R_QUERY            30
#define CMMS_R_ZWITHDRAW	31
#define CMMS_R_UNDEF            32
#define CMMS_S_INITPROC         33
#define CMMS_S_INITREG          34
#define CMMS_S_TERMINATE        35
#define CMMS_S_INTERRUPT	36
#define CMMS_T_INITPROC         37
#define CMMS_T_REGNUM           38
#define CMMS_X_INQPROC          39
#define CMMS_X_INQPRRG          40
#define CMMS_X_INQREG           41
#define CMMS_Y_STATPROCREC      42
#define CMMS_Y_STATPRRGREC      43
#define CMMS_Y_STATREGREC       44
#define CMMS_U_LKEDELETE	45
#define CMMS_U_LKESHOW		46
#define CMMS_V_LKESHOW		47
#define CMMS_E_TERMINATE	48
#define CMMS_B_BUFRESIZE	49
#define CMMS_B_BUFFLUSH		50
#define CMMS_C_BUFRESIZE	51
#define CMMS_C_BUFFLUSH		52

typedef struct cm_region_list_struct
	{
		relque				regque;
		struct cm_region_list_struct	*next;
		unsigned char			regnum;
		unsigned char			oper;
		unsigned short			lks_this_cmd;
		bool				reqnode;
		char				filler[3];
		struct cm_region_head_struct	*reghead;
		struct cs_struct		*cs;
		struct mlk_pvtblk_struct	*blkd;
		struct mlk_pvtblk_struct	*lockdata;
		uint4			pini_addr;
	} cm_region_list;

typedef struct cs_struct
	{
		relque				qent;
		cm_region_list			*region_root;
		cm_region_list			*current_region;
		struct CLB			*clb_ptr;
		unsigned char			state;
		unsigned char			new_msg;
		unsigned char			maxregnum;
		bool				waiting_in_queue;
		uint4			connect[2];
		uint4			lastact[2];
		uint4			stats;
		unsigned short			procnum;
		unsigned short			transnum;
		unsigned short			lk_cancel;
		struct           /* hold info from interrupt cancel msg */
		  {              /* laflag can be 0, x40, x80 */
		    unsigned char               laflag;    /* + 1 if valid */
		    unsigned char               transnum;  /* for lk_cancel */
		  } int_cancel;
		struct jnl_process_vector_struct *pvec;
	} connection_struct;

typedef struct cm_region_head_struct
	{
		relque				head;
		struct cm_region_head_struct	*next;
		struct cm_region_head_struct	*last;
		connection_struct		*connect_ptr;
		struct gd_region_struct		*reg;
		uint4			refcnt;
		uint4			wakeup;
		struct htab_desc		*reg_hash;
	} cm_region_head;

typedef struct link_info_struct
	{
		unsigned char			neterr;
		unsigned char			lck_info;
		unsigned char			lnk_active;
		char				filler;
		struct mlk_pvtblk_struct	*netlocks;
		unsigned short			procnum;
		unsigned short			buffered_count;
		unsigned short			buffer_size;
		unsigned short			buffer_used;
		unsigned char			*buffer;
	} link_info;

typedef struct cm_lk_response_struct
	{
		struct cm_lk_response_struct	*next;
		struct CLB			*response;
	} cm_lk_response;

typedef struct
	{
		char	code;
		char	rnum;
		bool	all;
		bool	interactive;
		int4	pid;
		char	nodelength;
		char	node[32];
	} clear_request;

typedef struct
	{
		char	code;
		char	filler[3];
		int4	status;
		int4	locknamelength;
		char	lockname[256];
	} clear_reply;

typedef struct
	{
		char	code;
		bool	clear;
	} clear_confirm;

typedef struct
	{
		char	code;
		char	rnum;
		bool	all;
		bool	wait;
		int4	pid;
		char	nodelength;
		char	node[32];
	} show_request;

typedef struct
	{
		char	code;
		char	line[256];
	} show_reply;

