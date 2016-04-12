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
 *  gtcm.h ---
 *
 *	Include file for the GTCM server.
 *
 *  $Header:$
 *
 */

#ifndef GTCM_H
#define GTCM_H

#include "gtcm_sysenv.h"
#include "omi.h"

/*  Names */
#define SRVR_NAME	"gtcm_server"
#define SRVC_NAME	"omi"
#define GTCM_STAT	"/tmp/gtcm_server.stat"
#define GTCM_ERLG	"/tmp/gtcm_server.erlg"

#ifdef __STDC__
#define P(X) X
#else /* defined(__STDC__) */
#define P(X) ()
#endif /* !defined(__STDC__) */

/*  Routines */
void		gtcm_init     P((int argc, char_ptr_t argv[]));
int		gtcm_prsopt   P((int, char **));
int		gtcm_maxfds   P((void));
int		gtcm_bgn_net  P((omi_conn_ll *));
void		gtcm_end_net  P((omi_conn_ll *));
void		gtcm_loop     P((omi_conn_ll *));
int		gtcm_cn_acpt  P((omi_conn_ll *, int));
void		gtcm_cn_disc  P((omi_conn *, omi_conn_ll *));
int		gtcm_term     P((int));
void		gtcm_exit_ch  P((int));
void		gtcm_exit     P((void));
int		gtcm_dmpstat  P((int));
void		gtcm_rep_err  P((char *, int));
int		omi_srvc_xact P((omi_conn *));
int		rc_srvc_xact  P((omi_conn *, char *));
char		*gtcm_hname    P((struct addrinfo *));
void		gtcm_cpktdmp  P((char *, int, char *));
void		gtcm_pktdmp   P((char *, int, char *));
void		init_hist     P((void));
void		init_omi_hist P((int));
void		save_omi_req  P((char *, int));
void		save_omi_rsp  P((char *, int));
void		init_rc_hist  P((int));
void		save_rc_req   P((char *, int));
void		save_rc_rsp   P((char *, int));
void		dump_omi_rq   P((void));
void		dump_rc_hist  P((void));

#undef P

#define HISTORY			10    /* save ten most recent packets */
#define OMI_HIST_BUFSIZ		8192
#define RC_HIST_BUFSIZ		4096


/* packet history stuff */
typedef struct omi_hist_rec_t
{
	int  conn;
	time_t timestamp;
	int  toobigflag;
	char req[OMI_HIST_BUFSIZ];
	char rsp[OMI_HIST_BUFSIZ];
	int  req_len;
	int  rsp_len;
} omi_hist_rec;


typedef struct rc_hist_rec_t
{
	int  conn;
	time_t timestamp;
	int  toobigflag;
	char req[RC_HIST_BUFSIZ];
	char rsp[RC_HIST_BUFSIZ];
	int  req_len;
	int  rsp_len;
} rc_hist_rec;

#ifdef GTCM_HIST_C
omi_hist_rec	*omi_hist = (void*)0; /* array of recently saved packets */
int		omi_hist_num= -1;     /* current/most recently saved packet */
rc_hist_rec	*rc_hist = (void*)0; /* array of recently saved RC packets */
int		rc_hist_num= -1;
#else

extern	omi_hist_rec	*omi_hist;
extern	int		omi_hist_num;
extern	rc_hist_rec	*rc_hist;
extern	int		rc_hist_num;
#endif

#endif /* !defined(GTCM_H) */
