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

/*
 *  rc.h ---
 *
 *	Include file for the GTCM server (RC code).
 *
 *  $Header:$
 *
 */

#ifndef RC_H
#define RC_H

#define RC_METHOD	3
#define RC_MIN_CPT_SIZ	512
#define RC_AQ_HDR      14

/*  Operations */
#define RC_OPEN_DS	0x0001
#define RC_CLOSE_DS	0x0002
#define RC_EXCH_INFO	0x0003
#define RC_LOCK_NAMES	0x0004
#define RC_LOGIN	0x0005
#define RC_GET_PAGE	0x0011
#define RC_GET_RECORD	0x0012
#define RC_KILL	0x0021
#define RC_SET		0x0022
#define RC_SET_FRAG	0x0023
#define RC_NETTRAX	0x0040
#define RC_OP_MAX	0x0041

/*  Modes */
#define RC_MODE_WRITE		0x001
#define RC_MODE_WAIT		0x002
#define RC_MODE_CLEARLOCK	0x020
#define RC_MODE_DECRLOCK	0x040
#define RC_MODE_NEXT		0x100
#define RC_MODE_PREV		0x200
#define RC_MODE_TREE		0x400
#define RC_MODE_GET		0x002
#define RC_MODE_SET		0x003
#define RC_MODE_KILL		0x403

/*  Error codes */
#define RC_ER_NO_ERROR		 0
#define RC_ER_MAX		 1
#define RC_SUCCESS		0x00000
#define RC_NOFILE		0x000CB
#define RC_BADFILESPEC		0x000CD
#define RC_FILEACCESS		0x000DB
#define RC_GLOBERRUNSPEC	0x0C000
#define RC_DSALREADYMNT	0x0C022
#define RC_NETERRNTIMPL	0x0C106
#define RC_BADXBUF		0x0C131
#define RC_NETERRDBEDGE	0x0C151
#define RC_MUMERRUNDEFVAR	0x0D031
#define RC_KEYTOOLONG		0x0D042
#define RC_SUBTOOLONG		0x0D044
#define RC_BADSUBSCRIPT	0x0D047
#define RC_NETERRRETRY		0x0C10F
#define RC_NETREQIGN		0x0C11F
#define RC_LOCKCONFLICT	0x0C0A1
#define RC_UNDEFNAMSPC		0x0D063
#define RC_GLOBERRPAGENOTFOUND 0x0C032
#define RC_GLOBERRINVPAGEADDR  0x0C033
#define RC_XBLKOVERFLOW	0x0C035


/*  Data structures */

typedef union rc_byte {
    char		 octet[SIZEOF(unsigned char)];
    unsigned char	 value;
} rc_byte;

typedef union rc_word {
    char		 octet[SIZEOF(unsigned short)];
    unsigned short	 value;
} rc_word;

typedef union rc_lword {
    char		 octet[SIZEOF(uint4)];
    uint4	 value;
} rc_lword;

/*  Operation pointers */
typedef int (*rc_op)(/* char *, short */);

/*  XBLK header */
typedef struct rc_xblk_hdr {
    rc_word	 end;
    rc_word	 free;
    rc_byte	 method;
    rc_byte	 alert;
    rc_word	 sync;
    rc_word	 cpt_tab;
    rc_word	 cpt_siz;
    rc_word	 aq_res;
    rc_word	 resp_max;
    rc_lword	 client;
    rc_byte	 alive;
    char	 filler0;
    rc_word	 err_aq;
    rc_word	 last_aq;
    rc_byte	 big_endian;
    char	 filler1[5];
    char	 asm1[32];
} rc_xblk_hdr;

typedef struct rc_xdsid {
    rc_word	 dsid;
    rc_word	 node;
} rc_xdsid;

typedef struct rc_xnsid {
    char	value[4];
}rc_xnsid;

typedef struct rc_rq_hdr {
    rc_word	 len;
    rc_word	 typ;
    rc_word	 fmd;
    rc_word	 pid1;
    rc_word	 pid2;
    rc_xdsid	 xdsid;
} rc_rq_hdr;

typedef struct rc_aq_hdr {
    rc_word	 len;
    rc_word	 typ;
    rc_word	 erc;
    rc_word	 pid1;
    rc_word	 pid2;
    rc_xdsid	 xdsid;
} rc_aq_hdr;

typedef union rc_q_hdr {
    rc_rq_hdr	 r;
    rc_aq_hdr	 a;
} rc_q_hdr;

typedef struct rc_sbkey {
    rc_byte	 len;
    char	 key[1];
} rc_sbkey;

typedef struct rc_swstr {
    rc_word	 len;
    char	 str[1];
} rc_swstr;

typedef struct rc_lknam {
	rc_xdsid	xdsid;
	rc_word	node_handle;
	rc_sbkey	sb_key;
} rc_lknam;

typedef struct {
    rc_q_hdr	 hdr;
    rc_word	 nlocks;
    rc_lknam	 dlocks[1];
} rc_req_lock;

typedef struct {
    rc_q_hdr	 hdr;
    char	 pageaddr[4];
    rc_word	 offset;
} rc_req_getp;

typedef struct {
    rc_q_hdr	 hdr;
    rc_sbkey	 key;
} rc_req_getr;

typedef struct {
    rc_q_hdr	 hdr;
    char	 pageaddr[4];
    rc_word	 frag_offset;
    rc_word	 size_return;
    rc_word	 size_remain;
    rc_word	 before;
    rc_word	 after;
    rc_word	 xcc;
    rc_byte	 rstatus;
    rc_byte	 zcode;
    char	 page[1];
} rc_rsp_page;

typedef struct {
    rc_q_hdr	 hdr;
    rc_xnsid	 xnsid;
    rc_sbkey	 key;
} rc_kill;

typedef struct {
    rc_q_hdr	 hdr;
    rc_xnsid	 xnsid;
    rc_sbkey	 key;
} rc_set;

typedef struct {
    rc_q_hdr	 hdr;
    char	 license_num[12];
    char	 license_blk[224];
} rc_req_logn;

typedef struct {
    rc_q_hdr	 hdr;
    rc_word	 version;
    rc_word	 method;
    rc_word	 session;
    rc_word	 date;
    char	 time[4];
    char	 license_blk[224];
} rc_rsp_logn;

/*  Incomplete structure reference ("rc_oflow" in rc/rc_oflow.h) */
typedef struct rc_oflow	 rof_struct;

#ifdef __STDC__
#define P(X) X
#else /* defined(__STDC__) */
#define P(X) ()
#endif /* !defined(__STDC__) */

/*  Routines */
int		 rc_prc_opnd P((rc_q_hdr *));
int		 rc_prc_lock P((rc_q_hdr *));
int		 rc_prc_clsd P((rc_q_hdr *));
int		 rc_prc_logn P((rc_q_hdr *));
int		 rc_prc_getp P((rc_q_hdr *));
int		 rc_prc_getr P((rc_q_hdr *));
int		 rc_prc_kill P((rc_q_hdr *));
int		 rc_prc_set  P((rc_q_hdr *));
int		 rc_prc_setf P((rc_q_hdr *));
rof_struct	*rc_oflow_alc P((void));
void		 rc_oflow_fin P((rof_struct *));
void		 rc_send_cpt P((rc_xblk_hdr *, rc_rsp_page *));
short		 rc_fnd_file P((rc_xdsid *));
int		 rc_frmt_lck P((char *, int4 , unsigned char *, short , short *));
void		 rc_gbl_ord P((rc_rsp_page *));
void		 rc_rundown P((void));

#undef P

#endif /* !defined(RC_H) */
