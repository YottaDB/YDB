/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  omi.h ---
 *
 *	Include file for the GTCM server (OMI code).
 *
 *  $Header:$
 *
 */

#ifndef OMI_H
#define OMI_H

#include "gtcm_sysenv.h"

/*  Numbers */
#define OMI_PROTO_MAJOR	 1
#define MAX_USER_NAME	20
#define OMI_PROTO_MINOR	 0
#define OMI_EXT_N2F(NUM) (1 << ((NUM) - 1))
#define OMI_XTN_BUNCH	 1
#define OMI_XTF_BUNCH	 OMI_EXT_N2F(OMI_XTN_BUNCH)
#define OMI_XTN_GGR	 2	/* Not supported */
#define OMI_XTF_GGR	 OMI_EXT_N2F(OMI_XTN_GGR)
#define OMI_XTN_NEWOP	 3
#define OMI_XTF_NEWOP	 OMI_EXT_N2F(OMI_XTN_NEWOP)
#ifdef GTCM_RC
#ifdef LTL_END
#define OMI_XTN_RC	 4
#define OMI_XTF_RC	 OMI_EXT_N2F(OMI_XTN_RC)
#else /*defined(LTL_END) */
#ifdef BIG_END
#define OMI_XTN_RC	 5
#define OMI_XTF_RC	 OMI_EXT_N2F(OMI_XTN_RC)
#endif /* defined(BIG_END) */
#endif /* !defined(LTL_END) */
#else /* defined(RC) */
#define OMI_XTN_RC	 0
#define OMI_XTF_RC	 0
#endif /* !defined(GTCM_RC) */
#define OMI_EXTENSIONS	 (OMI_XTF_BUNCH | OMI_XTF_NEWOP | OMI_XTF_RC)
#define OMI_NUM_EXT	 5


#define OMI_DBG_STMP							\
	do								\
	{								\
		extern FILE	*omi_debug;				\
		time_t		clock;					\
		char		*time_ptr;				\
									\
		if (omi_debug)						\
		{							\
			clock = time((time_t *)0);			\
			GTM_CTIME(time_ptr, &clock);			\
			FPRINTF(omi_debug, "%s", time_ptr);		\
			FFLUSH(omi_debug);				\
		}							\
	} while (0)

#define OMI_DBG(X)							\
	do								\
	{								\
		extern FILE	*omi_debug;				\
		if (omi_debug)						\
		{							\
			FPRINTF X ;					\
			FFLUSH(omi_debug);				\
		}							\
	} while (0)


/*#define OMI_DBG(X)
#define OMI_DBG_STMP*/

/*  Operations */
#define OMI_CONNECT	  1
#define OMI_STATUS	  2
#define OMI_DISCONNECT	  3
#define OMI_SET		 10
#define OMI_SETPIECE	 11
#define OMI_SETEXTRACT	 12
#define OMI_KILL	 13
#define OMI_INCREMENT	 14
#define OMI_GET		 20
#define OMI_DEFINE	 21
#define OMI_ORDER	 22
#define OMI_NEXT	 23
#define OMI_QUERY	 24
#define OMI_REVERSEORDER 25
#define OMI_REVERSEQUERY 26
#define OMI_LOCK	 30
#define OMI_UNLOCK	 31
#define OMI_UNLOCKCLIENT 32
#define OMI_UNLOCKALL	 33
#define OMI_OP_MAX	 34

/*  Error codes */
#define OMI_ER_NO_ERROR		 0
#define OMI_ER_DB_USERNOAUTH	 1
#define OMI_ER_DB_NOSUCHENV	 2
#define OMI_ER_DB_INVGLOBREF	 3
#define OMI_ER_DB_LONGGLOBREF	 4
#define OMI_ER_DB_LONGGLOBVAL	 5
#define OMI_ER_DB_UNRECOVER	 6
#define OMI_ER_PR_INVGLOBREF	10
#define OMI_ER_PR_INVMSGFMT	11
#define OMI_ER_PR_INVOPTYPE	12
#define OMI_ER_PR_SRVCSUSPEND	13
#define OMI_ER_PR_SEQNUMERR	14
#define OMI_ER_SE_VRSNOTSUPP	20
#define OMI_ER_SE_LENMIN	21
#define OMI_ER_SE_LENMAX	22
#define OMI_ER_SE_CONNREQ	23
#define OMI_ER_SE_NOSESS	24
#define OMI_ER_MAX		25

/*  Default buffer size, and fixed protocol portion */
#define OMI_BUFSIZ	 65535
#define OMI_RH_SIZ	 11
#define OMI_MAX_DATA	 32767	/* XXX */
#define OMI_MAX_SUBSCR	 255	/* XXX */
#define OMI_MAX_REF	 255	/* XXX */

/*  Data packing/unpacking information and structures */
typedef unsigned char	uns_char;
#define OMI_SI_SIZ	1
typedef unsigned short	uns_short;
#define OMI_LI_SIZ	2
typedef uint4		uns_long;
#define OMI_VI_SIZ	4

/*  Header before the specific information */
#define OMI_HDR_SIZ	(OMI_VI_SIZ + OMI_SI_SIZ + OMI_RH_SIZ)

typedef union	omi_si
{
	char		octet[OMI_SI_SIZ];
	uns_char	value;
} omi_si;

typedef union	omi_li
{
	char		octet[OMI_LI_SIZ];
	uns_short	value;
} omi_li;

typedef union	omi_vi
{
	char		octet[OMI_VI_SIZ];
	uns_long	value;
} omi_vi;

#ifdef BIG_END

#define OMI_SI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[0] = *(PTR)++;	\
	} while (0)

#define OMI_SI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_si	 obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[0];	\
	} while (0)

#define OMI_LI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[1] = *(PTR)++;	\
		(VO)->octet[0] = *(PTR)++;	\
	} while (0)

#define OMI_LI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_li	obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[1];	\
		*(PTR)++  = obj.octet[0];	\
	} while (0)

#define OMI_VI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[3] = *(PTR)++;	\
		(VO)->octet[2] = *(PTR)++;	\
		(VO)->octet[1] = *(PTR)++;	\
		(VO)->octet[0] = *(PTR)++;	\
	} while (0)

#define OMI_VI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_vi	obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[3];	\
		*(PTR)++  = obj.octet[2];	\
		*(PTR)++  = obj.octet[1];	\
		*(PTR)++  = obj.octet[0];	\
	} while (0)

#else /* defined(BIG_END) */

#ifdef LTL_END

#define OMI_SI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[0] = *(PTR)++;	\
	} while (0)

#define OMI_SI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_si	obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[0];	\
	} while (0)

#define OMI_LI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[0] = *(PTR)++;	\
		(VO)->octet[1] = *(PTR)++;	\
	} while (0)

#define OMI_LI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_li	obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[0];	\
		*(PTR)++  = obj.octet[1];	\
	} while (0)

#define OMI_VI_READ(VO, PTR)			\
	do					\
	{					\
		(VO)->octet[0] = *(PTR)++;	\
		(VO)->octet[1] = *(PTR)++;	\
		(VO)->octet[2] = *(PTR)++;	\
		(VO)->octet[3] = *(PTR)++;	\
	} while (0)

#define OMI_VI_WRIT(VAL, PTR)			\
	do					\
	{					\
		omi_vi	obj;			\
		obj.value = (VAL);		\
		*(PTR)++  = obj.octet[0];	\
		*(PTR)++  = obj.octet[1];	\
		*(PTR)++  = obj.octet[2];	\
		*(PTR)++  = obj.octet[3];	\
	} while (0)

#endif /* defined(LTL_END) */

#endif /* !defined(BIG_END) */

/*  OMI request header; common to all operations */
typedef struct	omi_req_hdr
{
	omi_li	op_class;
	omi_si	op_type;
	omi_li	user;
	omi_li	group;
	omi_li	seq;
	omi_li	ref;
} omi_req_hdr;

typedef struct	omi_err_hdr
{
	uns_short	class;
	uns_char	type;
	uns_short	modifier;
} omi_err_hdr;

typedef uns_short omi_status;

/*  OMI connection state */
typedef enum	omi_cn_st
{
	OMI_ST_CONN,
	OMI_ST_DISC,
	OMI_ST_CLOS
} omi_cn_st;

/*  OMI per-connection statistics */
typedef struct	omi_cn_stat
{
	int			id;
	time_t			start;
	struct addrinfo		ai;
	struct sockaddr_storage sas;
	int			bytes_recv;
	int			bytes_send;
	int			xact[OMI_OP_MAX];
	int			errs[OMI_ER_MAX];
} omi_cn_stat;

/*  Incomplete structure reference ("gd_addr" in gdsfhead.h) */
typedef struct gd_addr_struct	 ga_struct;
/*  Incomplete structure reference ("rc_oflow" in rc/rc_oflow.h) */
typedef struct rc_oflow	 oof_struct;


/* PING parameters */
#define MIN_TIMEOUT_INTERVAL		60 /* seconds */
#define TIMEOUT_INTERVAL	       180 /* seconds */
#define PING_TIMEOUT			10
#define MAX_PING_CNT			5

/*  OMI connection information */
typedef struct omi_conn	 omi_conn;
struct omi_conn
{
	omi_conn	*next;
	omi_fd		fd;
	int		ping_cnt;	/* number of pings we have sent to machine */
	int		timeout;	/* time when we should next check this connection */
	int		bsiz;
	char		*buff;
	char		*bptr;
	char		*xptr;
	char		ag_name[MAX_USER_NAME];
	int		blen;
	int		exts;
	omi_cn_st	state;
	ga_struct	*ga;
	oof_struct	*of;
	omi_fd		pklog;
	omi_cn_stat	stats;
};

/*  OMI meta-connection statistics */
typedef struct	omi_cl_stat
{
	int	conn;
	int	disc;
	int	clos;
} omi_cl_stat;

/* OMI connection linked list */
typedef struct	omi_conn_ll
{
	omi_fd		nve;
	omi_conn	*head;
	omi_conn	*tail;
	omi_cl_stat	stats;
	omi_cn_stat	st_cn;
} omi_conn_ll;

/*  Operation pointers */
typedef int (*omi_op)(/* omi_conn *, char * */);

/*  Routines */
int		omi_prc_conn (omi_conn *, char *, char *, char *);
int		omi_prc_stat (omi_conn *, char *, char *, char *);
int		omi_prc_disc (omi_conn *, char *, char *, char *);
int		omi_prc_set  (omi_conn *, char *, char *, char *);
int		omi_prc_setp (omi_conn *, char *, char *, char *);
int		omi_prc_sete (omi_conn *, char *, char *, char *);
int		omi_prc_kill (omi_conn *, char *, char *, char *);
int		omi_prc_incr (omi_conn *, char *, char *, char *);
int		omi_prc_get  (omi_conn *, char *, char *, char *);
int		omi_prc_def  (omi_conn *, char *, char *, char *);
int		omi_prc_ordr (omi_conn *, char *, char *, char *);
int		omi_prc_next (omi_conn *, char *, char *, char *);
int		omi_prc_qry  (omi_conn *, char *, char *, char *);
int		omi_prc_rord (omi_conn *, char *, char *, char *);
int		omi_prc_lock (omi_conn *, char *, char *, char *);
int		omi_prc_unlk (omi_conn *, char *, char *, char *);
int		omi_prc_unlc (omi_conn *, char *, char *, char *);
int		omi_prc_unla (omi_conn *, char *, char *, char *);
void		omi_buff_rsp (omi_req_hdr *, omi_err_hdr *, omi_status, char *, int);
int		omi_gvextnam (omi_conn *, uns_short, char *);
int		omi_lkextnam (omi_conn *, uns_short, char *, char *);
void		omi_dump_pkt (omi_conn *);

#ifdef __STDC__
#define P(X) X
#else /* defined(__STDC__) */
#define P(X) ()
#endif /* !defined(__STDC__) */

int		get_ping_rsp  P((void));
int		icmp_ping     P((int conn));
int		init_ping     P((void));
int		in_cksum      P((u_short *addr, int len));

#undef P
#endif /* !defined(OMI_H) */
