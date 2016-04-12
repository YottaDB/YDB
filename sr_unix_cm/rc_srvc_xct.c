/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /*
 * rc_srvc_xact.c ---
 *
 * Process a client's RC transaction.
 *
 */

#ifndef lint
static char     rcsid[] = "$Header:$";
#endif

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <errno.h>

#include "gtcm.h"
#include "omi.h"
#include "gtmio.h"
#include "have_crit.h"
#include "rc.h"

typedef struct rc_clnt_err rc_clnt_err;
struct rc_clnt_err
{
	rc_clnt_err   *next;
	int4        pid;
};

GBLDEF int      rc_size_return = 0;
GBLDEF UINTPTR_T rc_auxown = 0;
GBLDEF int      rc_errno = 0;
GBLDEF int      rc_nxact = 0;
GBLDEF int      rc_nerrs = 0;
GBLDEF rof_struct *rc_overflow = (rof_struct *) 0;
GBLREF int	history;
GBLREF int	omi_pid;

static rc_op   rc_dispatch_table[RC_OP_MAX] = {
	0,
	rc_prc_opnd,
	rc_prc_clsd,
	 /* rc_prc_exch */ 0,
	rc_prc_lock,
	rc_prc_logn,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	rc_prc_getp,
	rc_prc_getr,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	rc_prc_kill,
	rc_prc_set,
	rc_prc_setf,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 /* rc_prc_ntrx */ 0
};


int
rc_srvc_xact(cptr, xend)
	omi_conn       *cptr;
	char           *xend;
{
	rc_xblk_hdr   *fxhdr;
	rc_q_hdr      *qhdr;
	rc_clnt_err   *elst, *eptr;
	rc_rsp_page   *rpg;
	char           *cpte;
	int             rv, rqlen;
	char	       *tptr;
	int 		dumped = 0;
	int		pkt_len = (int)(xend - (char *) cptr);

	/* Reserve space for the RC header in the response buffer */
	fxhdr = (rc_xblk_hdr *) cptr->xptr;
	cptr->xptr += SIZEOF(rc_xblk_hdr);

	/* If true, this not a known RC block type */
	if (fxhdr->method.value != RC_METHOD)
	{
#ifdef DEBUG
		gtcm_cpktdmp((char *)cptr, pkt_len,"Unknown RC block type.");
		dumped = 1;
#endif
		return -OMI_ER_PR_INVMSGFMT;
	}
	/* Check the endianism of the XBLK */
#ifdef LTL_END
	if (fxhdr->big_endian.value)
		return -OMI_ER_PR_INVMSGFMT;
#else				/* defined(LTL_END) */
#ifdef BIG_END
	if (!fxhdr->big_endian.value)
		return -OMI_ER_PR_INVMSGFMT;
#else				/* defined(BIG_END) */
	return -OMI_ER_PR_INVMSGFMT;
#endif				/* !defined(BIG_END) */
#endif				/* !defined(LTL_END) */


	/* Stash this for any lock operations */
	rc_auxown = (UINTPTR_T)cptr;
	/* This is the list of errors */
	elst = (rc_clnt_err *) 0;
	/* This is the offset of the first error Aq */
	fxhdr->err_aq.value = 0;

	tptr = cptr->xptr;

	/* Loop through the requests in the XBLK */
	for (qhdr = (rc_q_hdr *) 0; cptr->xptr < xend;)
	{

		qhdr = (rc_q_hdr *) tptr;
		rqlen = qhdr->a.len.value;

		if (history)
		{
			init_rc_hist(cptr->stats.id);
			save_rc_req((char *)qhdr,qhdr->a.len.value);
		}

		rv = -1;

		/* Check to see if this is a error'd PID, throw away packets
		   from these. */
		/* 9/8/94 VTF:  protocol change
		 * Process any LOCK/UNLOCK requests from an error'd PID
		 */
		for (eptr = elst; eptr; eptr = eptr->next)
			if (eptr->pid == ((qhdr->r.pid1.value << 16) | qhdr->r.pid2.value)
			    && qhdr->r.typ.value != RC_LOCK_NAMES)
				break;
		if (eptr)
			qhdr->a.erc.value = RC_NETREQIGN;
	/*	Do we do this yet? */
		else if (	/* (qhdr->r.typ.value < 0) || */ /* this is commented out as it always evaluates to FALSE */
			(qhdr->r.typ.value > RC_OP_MAX )
			|| (!rc_dispatch_table[qhdr->r.typ.value]))
		{
#ifdef DEBUG
			if (qhdr->r.typ.value == RC_EXCH_INFO)
			{
				dumped = 1;
				OMI_DBG((omi_debug, "Note: Unsupported request type (ExchangeInfo)\n"));
			}
			else
			{
				gtcm_cpktdmp((char *)qhdr, qhdr->a.len.value, "Unknown request type.");
				dumped = 1;
			}
#endif
			qhdr->a.erc.value = RC_NETERRNTIMPL;
		}
		/* Try it */
		else
		{
			/*
			 * Calculate the size available for a response
			 * page
			 */
			if (qhdr->r.typ.value == RC_GET_PAGE
			    || qhdr->r.typ.value == RC_GET_RECORD)
			{
				rc_size_return = (int4)((char *) fxhdr
					+ fxhdr->end.value - RC_MIN_CPT_SIZ - (char *) qhdr
					- SIZEOF(rc_rsp_page) + 1);
			}
			rc_overflow = cptr->of;
			rc_errno = RC_SUCCESS;
			if (fxhdr->free.value > fxhdr->end.value)
			{
				char msg[256];
				SPRINTF(msg,"invalid packet :  free (%x) > end (%x).  Dumped RC header + packet",
					fxhdr->free.value, fxhdr->end.value);
				/*GTM64: Assuming length of packet wont excced MAX_INT */
				gtcm_cpktdmp((char *)fxhdr, (int4)(((char *)xend) - ((char *)fxhdr)),msg);
				if (history)
				    dump_omi_rq();
				return -OMI_ER_PR_INVMSGFMT;
			}
			rv = (*rc_dispatch_table[qhdr->r.typ.value]) (qhdr);
			if (fxhdr->free.value > fxhdr->end.value)
			{
				char msg[256];
				char *p;
				SPRINTF(msg,"corrupted packet, free (%x) > end.value (%x).  Dumped RC header + packet",
					fxhdr->free.value,fxhdr->end.value);
				gtcm_cpktdmp((char *)fxhdr, (int)(((char *)xend) - ((char *)fxhdr)),msg);
				p = (char *)-1L;
				*p = 1;   /*guarenteed core dump */
			}
			if (qhdr->a.len.value > fxhdr->end.value)
			{
				char msg[256];
				char *p;
				SPRINTF(msg,"corrupted packet, qhdr.a.len.value=%x > fxhdr end (%x).  Dumped RC header + packet",
					qhdr->a.len.value,fxhdr->end.value);
				gtcm_cpktdmp((char *)qhdr, qhdr->a.len.value, msg);
				p = (char *)-1L;
				*p = 1;   /*guarenteed core dump */
			}
			rc_nxact++;
			if (rc_errno != RC_SUCCESS)
			{
				rc_nerrs++;
				qhdr->a.erc.value = rc_errno;
				rv = -1;
#ifdef DEBUG
				{
					char msg[256];

					SPRINTF(msg,"RC error (0x%x).",rc_errno);
					gtcm_cpktdmp((char *)qhdr, qhdr->a.len.value, msg);
					dumped = 1;
				}
#endif
			}
		}




		/* Keep track of erroneous PIDs */
		if (rv < 0)
		{
			/*
			 * OMI_DBG_STMP; OMI_DBG((omi_debug, "gtcm_server:
			 * rc_srvc_xct(error: %d)\n",
			 * qhdr->a.erc.value));
			 */
#ifdef DEBUG
			if (!dumped)
				gtcm_cpktdmp((char *)qhdr, qhdr->a.len.value,
					    "RC Request returned error.");
			dumped = 1;
#endif
			eptr = (rc_clnt_err *) malloc(SIZEOF(rc_clnt_err));
			eptr->pid = (qhdr->r.pid1.value << 16) | qhdr->r.pid2.value;
			eptr->next = elst;
			elst = eptr;
			if (fxhdr->err_aq.value == 0)
				fxhdr->err_aq.value = (char *) qhdr - (char *) fxhdr;
		}
		qhdr->r.typ.value |= 0x80;

		if (history)
			save_rc_rsp((char *)qhdr,qhdr->a.len.value);

		/* Move to the next request */
		cptr->xptr += rqlen;

		tptr = (char *)qhdr;
		tptr += RC_AQ_HDR;

		if (qhdr->r.typ.value ==
		    (RC_GET_PAGE | 0x80) ||
		    qhdr->r.typ.value == (RC_GET_RECORD | 0x80))
			break;	/* Reads are always the last request in
				 * the buffer */
		else {
			if (cptr->xptr < xend)
			{
				/* ensure that the length of the next request
				 * actually fits into this XBLK
				 */
				assert(((rc_q_hdr *)(cptr->xptr))
				       ->a.len.value <= xend - cptr->xptr);
				memcpy(tptr, cptr->xptr, ((rc_q_hdr
					   *)(cptr->xptr))->a.len.value);
			}

		  qhdr->a.len.value = RC_AQ_HDR;
		}
	}

	/* Forget the erroneous PIDs */
	if (elst)
	{
		for (eptr = elst->next; eptr; eptr = (elst = eptr)->next)
			free(elst);
		free(elst);
	}
	/*
	 * If true, there was an XBLK, so fill in some of the response
	 * fields
	 */

	/* There's no way that I can see to get to this point with a
	   null qhdr. */

	if (qhdr)
	{
		fxhdr->last_aq.value = (char *) qhdr - (char *) fxhdr;
		fxhdr->cpt_tab.value = ((char *) qhdr -
					(char *) fxhdr) + qhdr->a.len.value;
		fxhdr->cpt_siz.value = fxhdr->end.value - fxhdr->cpt_tab.value;
	} else
	{
		fxhdr->last_aq.value = 0;
		fxhdr->cpt_tab.value = fxhdr->free.value;
		fxhdr->cpt_siz.value = fxhdr->end.value - fxhdr->cpt_tab.value;
	}

	if (fxhdr->free.value > fxhdr->end.value)
	{
	    char msg[256];
	    SPRINTF(msg,"invalid packet :  free (%x) > end (%x).  Dumped RC header + packet",
		    fxhdr->free.value, fxhdr->end.value);
	    /*GTM64: Assuming length of packet wont excced MAX_INT */
	    gtcm_cpktdmp((char *)fxhdr, (int4)(((char *)xend) - ((char *)fxhdr)),msg);
	    if (history)
		dump_omi_rq();
	    return -OMI_ER_PR_INVMSGFMT;
	}
	else if (fxhdr->cpt_tab.value > fxhdr->end.value)
	{
	    char msg[256];
	    SPRINTF(msg,"invalid packet :  cpt_tab (%x) > end (%x).  Dumped RC header + packet",
		    fxhdr->cpt_tab.value, fxhdr->end.value);
	    /*GTM64: Assuming length of packet wont excced MAX_INT */
	    gtcm_cpktdmp((char *)fxhdr, (int)(((char *)xend) - ((char *)fxhdr)),msg);
	    if (history)
		dump_omi_rq();
	    return -OMI_ER_PR_INVMSGFMT;
	}
	else if (fxhdr->cpt_tab.value + fxhdr->cpt_siz.value
		                                      > fxhdr->end.value)
	{
	    char msg[256];
	    SPRINTF(msg,"invalid packet :  cpt_tab + cpt_siz (%x) > end (%x).  Dumped RC header + packet",
		    fxhdr->cpt_tab.value + fxhdr->cpt_siz.value,
		    fxhdr->end.value);
	    /*GTM64: Assuming length of packet wont excced MAX_INT */
	    gtcm_cpktdmp((char *)fxhdr, (int4)(((char *)xend) - ((char *)fxhdr)),msg);
	    if (history)
		dump_omi_rq();
	    return -OMI_ER_PR_INVMSGFMT;
	}
	rc_send_cpt(fxhdr, (rc_rsp_page *) qhdr);
	if (fxhdr->free.value > fxhdr->end.value)
	{
	    char msg[256];
	    SPRINTF(msg,"invalid Aq packet :  free (%x) > end (%x).  Dumped RC header + packet",
		    fxhdr->free.value, fxhdr->end.value);
  	    /*GTM64: Assuming length of packet wont excced MAX_INT */
	    gtcm_cpktdmp((char *)fxhdr, (int4)(((char *)xend) - ((char *)fxhdr)),msg);
	    if (history)
		dump_omi_rq();
	    assert(fxhdr->free.value <= fxhdr->end.value);

	    return -OMI_ER_PR_INVMSGFMT;
	}

	return fxhdr->free.value;

}
