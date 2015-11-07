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
 *  omi_srvc_xct.c ---
 *
 *	Process a client's transaction.
 *
 */

#include "mdef.h"

#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_time.h"		/* for time() */
#include "gtmio.h"
#include "have_crit.h"

#include "omi.h"
#include "gtcm.h"
#include "error.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF int4	omi_errno;
GBLREF char	*omi_pklog;
GBLREF char	*omi_oprlist[];
GBLREF char	*omi_errlist[];
GBLREF char	*omi_pklog_addr;
GBLREF int	history;
GBLREF boolean_t	servtime_expired;

static omi_op	omi_dispatch_table[OMI_OP_MAX] =
{
	0,
	omi_prc_conn,
	omi_prc_stat,
	omi_prc_disc,
	0,    0,    0,    0,    0,    0,
	omi_prc_set,
/*	omi_prc_setp*/0,
/*	omi_prc_sete*/0,
	omi_prc_kill,
/*	omi_prc_incr*/0,
	0,    0,    0,    0,    0,
	omi_prc_get,
	omi_prc_def,
	omi_prc_ordr,
	omi_prc_next,
	omi_prc_qry,
	omi_prc_rord,
/*	omi_prc_rqry*/0,
	0,    0,    0,
	omi_prc_lock,
	omi_prc_unlk,
	omi_prc_unlc,
	omi_prc_unla
};

int	omi_srvc_xact (omi_conn *cptr)
{
	extern int4	omi_nxact, omi_nerrs, omi_brecv, omi_bsent, gtcm_stime;
	extern int4	gtcm_ltime, omi_nxact2;

	omi_vi		mlen, xlen;
	omi_li		nxact;
	omi_si		hlen;
	omi_req_hdr	rh;
	omi_err_hdr	eh;
	int4		rv, blen;
	int		bunches, xblk, i, fatal;
	char		buff[OMI_BUFSIZ], *bptr, *xend, *bend;

#ifdef BSD_TCP
	int		 cc, save_errno;

/*	If true, an error occurred */
	cc =(int)(&cptr->buff[cptr->bsiz] - &cptr->bptr[cptr->blen]);
	while (!servtime_expired && (cc = (int)(read(cptr->fd, &cptr->bptr[cptr->blen], cc))) < 0  &&  errno == EINTR)
			;
	save_errno = errno;
	if (servtime_expired)
		return -1;
	if (cc < 0)
	{
		if (errno == ETIMEDOUT)
		{
			OMI_DBG((omi_debug, "%s: connection %d to %s timed out.\n",
				 SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.ai)));
		}
		else if (errno == ECONNRESET)
		{
			OMI_DBG((omi_debug, "%s: connection %d to %s closed by remote client.\n",
				 SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.ai)));
		}
		else
		{
			char	msg[256];
			SPRINTF(msg, "Attempted read from connection %d to %s failed",
				cptr->stats.id, gtcm_hname(&cptr->stats.ai));
			gtcm_rep_err(msg, save_errno);
		}
		return -1;
	}

/*	If true, the connection has closed */
	else if (cc == 0)
	{
		return -1;
	}

/*	Buffer in use between &cptr->bptr[0] and &cptr->buff[cptr->bsiz] */
	cptr->blen             += cc;
	cptr->stats.bytes_recv += cc;
	omi_brecv              += cc;
#else /* defined(BSD_TCP) */

#ifdef FILE_TCP
	int		 cc;

	cc = (int)(&cptr->buff[cptr->bsiz] - &cptr->bptr[cptr->blen]);
/*	If true, an error occurred */
	if ((cc = (int)read(cptr->fd, &cptr->bptr[cptr->blen], 4)) <= 0)
	{
		return -1;
	}
	cptr->blen += 4;
	cptr->xptr  = cptr->bptr;
	OMI_VI_READ(&mlen, cptr->xptr);
	if ((cc = (int)read(cptr->fd, &cptr->bptr[cptr->blen], mlen.value)) <= 0)
	{
		return -1;
	}
	cptr->blen             += cc;
	cptr->stats.bytes_recv += cc + 4;
	omi_brecv              += cc + 4;
#else /* defined(FILE_TCP) */
	return -1;
#endif /* !defined(FILE_TCP) */
#endif /* !defined(BSD_TCP) */

/*	If true, we don't have all of the transaction length yet */
	if (cptr->blen < OMI_VI_SIZ)
	{
/*		If true, push the piece we have to the beginning of the buffer */
		if (cptr->bptr != cptr->buff)
		{
			memmove(cptr->buff, cptr->bptr, cptr->blen);
			cptr->bptr = cptr->buff;
		}
		return 0;
	}

/*	Place the transaction pointer so we can move through the data */
	cptr->xptr = cptr->bptr;

	OMI_VI_READ(&mlen, cptr->xptr);

	if (mlen.value + 4 > OMI_BUFSIZ)
	{
		char	msg[256];

		SPRINTF(msg, "OMI packet length (%d) larger than max (%d)", mlen.value+4, OMI_BUFSIZ);
		gtcm_cpktdmp((char *)cptr->bptr, cptr->blen, msg);
		return -1;
	}

/*	If true, we don't have the full transaction yet */
	if (cptr->blen < mlen.value + 4)
	{
/*		If true, push the piece we have to the beginning of the buffer */
		if (cptr->bptr != cptr->buff)
		{
			memmove(cptr->buff, cptr->bptr, cptr->blen);
			cptr->bptr = cptr->buff;
		}
		return 0;
	}

/*	We have the whole transaction in the buffer, decompose it. */
	if (history)
	{
		init_omi_hist(cptr->stats.id);
		save_omi_req(cptr->bptr, cptr->blen);
	}


#ifdef DEBUG
/*
 *	if (omi_pklog && !INV_FD_P(cptr->pklog))
 *		if (!omi_pklog_addr  ||  (!strcmp(omi_pklog_addr, strchr(cptr->stats.addr, '@')+1)))
 *			omi_dump_pkt(cptr);
 */
#endif /* defined(DEBUG) */

/*	This is set to indicate a fatal error in a bunch */
	fatal = 0;
/*	This is set to indicate we processed an xblk */
	xblk  = 0;

/*	If true, we need to worry about bunches */
	if ((bunches = (cptr->exts & OMI_XTF_BUNCH)))
	{
		OMI_LI_READ(&nxact, cptr->xptr);
		bptr = buff + OMI_VI_SIZ + OMI_LI_SIZ;
		if (nxact.value * (OMI_RH_SIZ + 1) > mlen.value) /* || nxact.value < 0 */ /* is commented as it is always FALSE */
		{
			char	msg[256];

			SPRINTF(msg, "invalid OMI packet (invalid nxact)");
			gtcm_cpktdmp((char *)cptr->bptr, cptr->blen, msg);
			return -1;
		}
	}
	else
	{
		nxact.value = 1;
		bptr        = buff;
		xlen        = mlen;
	}

/*	This is the end of the response buffer (for the *_prc_*() routines) */
	bend  = ARRAYTOP(buff);

/*	Loop through the transaction(s) */
	for (i = nxact.value;  i > 0;  i--)
	{

		if (bunches)
			OMI_VI_READ(&xlen, cptr->xptr);

/*		This pointer marks the end of this transaction */
		xend = cptr->xptr + xlen.value;

/*		Check the size of the operation-independent header */
		OMI_SI_READ(&hlen, cptr->xptr);
/*		Operation class and type */
		OMI_LI_READ(&rh.op_class, cptr->xptr);
		OMI_SI_READ(&rh.op_type, cptr->xptr);
/*		User and Group */
		OMI_LI_READ(&rh.user, cptr->xptr);
		OMI_LI_READ(&rh.group, cptr->xptr);
/*		Sequence number */
		OMI_LI_READ(&rh.seq, cptr->xptr);
/*		Reference ID */
		OMI_LI_READ(&rh.ref, cptr->xptr);

/*		Initialize in case of an error */
		eh.type = 0;
/*		Sanity check the transaction */
		if (xlen.value > mlen.value  ||  hlen.value != OMI_RH_SIZ)
		{
			eh.type = OMI_ER_PR_INVMSGFMT;
			fatal   = -1;
		}
		else if (cptr->state == OMI_ST_DISC  &&  rh.op_type.value != OMI_CONNECT)
		{
			char	msg[256];
			SPRINTF(msg, "Request (%x) sent before session was established",
				rh.op_type.value);
			eh.type = OMI_ER_SE_NOSESS;
		}
		else if (cptr->state == OMI_ST_CONN  &&  rh.op_type.value == OMI_CONNECT)
		{
			eh.type = OMI_ER_SE_CONNREQ;
			fatal   = 1;
		}
		else if (rh.op_class.value != 1)
		{
			if (!(cptr->exts & OMI_XTF_RC  &&  rh.op_class.value == 2))
			{
				eh.type = OMI_ER_PR_INVMSGFMT;
				fatal   = -1;
			}
		}

		else if (rh.op_type.value >= OMI_OP_MAX)
		{
			cptr->stats.xact[0]++;
			eh.type = OMI_ER_PR_INVOPTYPE;
		}
		else if (!omi_dispatch_table[rh.op_type.value])
		{
			cptr->stats.xact[rh.op_type.value]++;
			eh.type = OMI_ER_PR_INVOPTYPE;
		}

/*		Report any errors */
		if (eh.type != OMI_ER_NO_ERROR  ||  fatal)
		{
			if (eh.type == OMI_ER_NO_ERROR)
				eh.type = OMI_ER_PR_INVMSGFMT;
			eh.class    = 1;
			eh.modifier = 0;
			omi_buff_rsp(&rh, &eh, 0, bptr, 0);
			bptr       += OMI_HDR_SIZ;
			cptr->xptr  = xend;
			cptr->stats.errs[eh.type]++;
			omi_nerrs++;
			OMI_DBG((omi_debug, "gtcm_server: %6d: Error (PD): %s\n",
				cptr->stats.id, omi_errlist[eh.type]));
			if (fatal < 0)
			break;
			continue;
		}

		/* start counting server up time with the first transaction */
		if (!omi_nxact)
		  gtcm_stime = (int4)time(0);

/*		If true, this is an RC request; service it in the RC code */
		if (cptr->exts & OMI_XTF_RC && rh.op_class.value == 2)
		{
#ifdef GTCM_RC
			omi_nxact++;
			omi_nxact2++;
			if ((rv = rc_srvc_xact(cptr, xend)) >= 0)
			{
				xblk = 1;
				blen = rv + OMI_HDR_SIZ;
				bptr = cptr->bptr;
				bend = &cptr->buff[cptr->bsiz];
			}
#else /* defined(RC) */
			fatal = 1;
			rv    = -OMI_ER_PR_INVMSGFMT;
#endif /* !defined(GTCM_RC) */
		}

/*		Otherwise, deal with the OMI/EMI request */
		else
		{
/*			Update the stats, do the operation */
			cptr->stats.xact[rh.op_type.value]++;
			omi_nxact++;
			omi_nxact2++;
/*			OMI_DBG((omi_debug, "gtcm_server: %6d: %s\n", cptr->stats.id,
 *				omi_oprlist[rh.op_type.value]));*/
			omi_errno = OMI_ER_NO_ERROR;
/*			Service the transaction(s) */
			rv = (*omi_dispatch_table[rh.op_type.value])(cptr, xend, bptr + OMI_HDR_SIZ, bend);

/*			If true (or true if) an exception is raised */
			if (omi_errno != OMI_ER_NO_ERROR)
				rv = -omi_errno;
		}

/*		If true, an error occurred servicing this transaction */
		if (rv < 0)
		{
			eh.class    = 1;
			eh.type     = -rv;
			eh.modifier = 0;
			omi_buff_rsp(&rh, &eh, 0, bptr, 0);
			bptr       += OMI_HDR_SIZ;
			cptr->xptr  = xend;
			cptr->stats.errs[eh.type]++;
			omi_nerrs++;
			OMI_DBG((omi_debug, "gtcm_server: %6d: Error (AD): %s\n",
				cptr->stats.id, omi_errlist[eh.type]));
	        	omi_dump_pkt(cptr);

		}
		else
		{
			if (cptr->xptr != xend)
			{
/*				Only report this if we're not doing RC */
				if (!(cptr->exts & OMI_XTF_RC && rh.op_class.value == 2))
					NON_IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server: xptr != xend (%d)\n", xend - cptr->xptr)));
					IA64_ONLY(OMI_DBG((omi_debug, "gtcm_server: xptr != xend (%ld)\n", xend - cptr->xptr)));
				cptr->xptr = xend;
			}
			omi_buff_rsp(&rh, (omi_err_hdr *)0, 0, bptr, rv);
			bptr += OMI_HDR_SIZ + rv;
		}
		if (bptr >= bend)
		{
			gtcm_rep_err("Buffer overrun error", -1);
			return -1;
		}
	}

/*	If true, this was not an RC XBLK, so we need to calculate the
 *	size of the response */
	if (!xblk)
	{
		blen = (int)(bptr - buff);
		bptr = buff;
		if (bunches)
		{
			OMI_VI_WRIT(blen - OMI_VI_SIZ, bptr);
			OMI_LI_WRIT(nxact.value, bptr);
			bptr = buff;
		}
	}
	else
		bptr = cptr->bptr;

/*	Send the response(s) back to the client */
	if (history)
		save_omi_rsp(bptr, blen);

#ifdef BSD_TCP
/*	Write out the response (in pieces if necessary) */
	while (blen > 0)
	{
		while(!servtime_expired && (cc = (int)(write(cptr->fd, bptr, blen))) < 0)
			;
		save_errno = errno;
		if (cc < 0)
		{
			if (errno == ETIMEDOUT)
			{
				OMI_DBG((omi_debug, "%s: connection %d to %s timed out.\n",
					 SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.ai)));
			}
			else if (errno == ECONNRESET)
			{
				OMI_DBG((omi_debug, "%s: connection %d to %s closed by remote client.\n",
					 SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.ai)));
			}
			else if (errno == EPIPE)
			{
				OMI_DBG((omi_debug, "%s: remote client no longer attached to connection %d, %s.\n",
					 SRVR_NAME, cptr->stats.id, gtcm_hname(&cptr->stats.ai)));
			}
			else if (errno == EINTR)
				continue;
			else
			{
				char	msg[256];
				SPRINTF(msg, "Write attempt to connection %d failed", cptr->stats.id);
				gtcm_rep_err(msg, save_errno);
			}
			return -1;
		}
		bptr                   += cc;
		blen                   -= cc;
		cptr->stats.bytes_send += cc;
		omi_bsent              += cc;
	}
#else  /* defined(BSD_TCP) */

#ifdef FILE_TCP
		bptr                   += blen;
		cptr->stats.bytes_send += blen;
		omi_bsent              += blen;
		blen                   -= blen;
#endif /* defined(FILE_TCP) */
#endif /* !defined(BSD_TCP) */

/*	If true, a fatal error occurred with the last transaction */
/*	also exitt if we received a disconnect request */
	if (fatal  ||  rh.op_type.value == OMI_DISCONNECT)
	{
		return -1;
	}

/*	Get ready for the next transaction */
	cptr->bptr += mlen.value + 4;
	cptr->blen -= mlen.value + 4;
	if (cptr->xptr > cptr->bptr)
	{
		gtcm_rep_err("buffer size error", errno);
		return -1;
	}

/*	If true, move the partial buffer to the beginning of the buffer */
	if (cptr->blen > 0  &&  cptr->bptr != cptr->buff)
		memmove(cptr->buff, cptr->bptr, cptr->blen);
/*	Reset to the beginning of the buffer */
	cptr->bptr = cptr->buff;

	return 0;

}
