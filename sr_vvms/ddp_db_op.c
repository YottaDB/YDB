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

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_time.h"

#include <descrip.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "decddp.h"
#include "route_table.h"
#include "subscript.h"
#include "seven_2_ascii.h"

#define DDP_LOG_ZSTATUS								\
{										\
	gtm$zstatus(&ddp_result);						\
	DDP_LOG_ERROR(ddp_result.dsc$w_length, ddp_result.dsc$a_pointer);	\
}

GBLDEF int4 			subscript_count;
GBLDEF struct dsc$descriptor	ddp_result;
GBLDEF struct dsc$descriptor	subscript_array[1 + 1 + 1 + MAX_GVSUBSCRIPTS]; /* worst case (SET operation) -
										* 1 - input data (for SET)
										* 1 - global directory
										* 1 - global name
										* MAX_GVSUBSCRIPTS - max # subscripts
										*/
GBLREF bool                     dec_nofac;
error_def(ERR_ZGBLDIRACC);
error_def(ERR_DBOPNERR);
error_def(ERR_GVUNDEF);
error_def(ERR_REC2BIG);
error_def(ERR_LCKSTIMOUT);
error_def(ERR_DDPOUTMSG2BIG);
error_def(ERR_DDPLOGERR);

void	gtm$zstatus(struct dsc$descriptor *zstatus);

static unsigned char spool[MAX_ETHER_DATA_SIZE];

char *ddp_db_op(
	struct in_buffer_struct *bp,
	condition_code (*func)(),	/* function to dispatch to*/
	unsigned char *addr,		/* if == 0, then receive the result in 'ddp_result'
				   	   if == 1, then there is no result
				   	   if > 1, set the first descriptor to be equal to the data pointed to at addr */
	int len)			/* if addr > 1, then this is the length of the data to set-up */
{
	ddp_hdr_t		*dp;	/* pointer to input buffer */
	unsigned char		*cp, *inkeyptr, *keytop, *extref;
	int			ch, index, length, jobno, subscript_type, bufavail, gtm_prefix_len;
	condition_code		status;
	routing_tab		*remote_node;
	mstr			*gld;
	ddp_global_request_t	*gp;

	dp = &bp->dh;
	gp = dp->txt;
	jobno = dp->source_job_number;
	assert((2 <= jobno) && (0 == (jobno & 1)));
	jobno >>= 1;
	jobno -= 1;
	assert(0 <= jobno && MAX_USERS_PER_NODE >= jobno);
	if (NULL == (remote_node = find_route(dp->source_circuit_name))) /* couldn't find sender in our tables */
		return "";
	if (dp->message_number > remote_node->incoming_users[jobno] + 1) /* sequencing error, incoming message number is too big */
		return "";
	remote_node->incoming_users[jobno] = dp->message_number;
	/* Vinaya, 07/11/02 -  Based on ether traces, we noticed that the field global_type is always 0x02. Since we don't know
	 * what 0x02 really means, we don't want to sanity check the incoming message for the value of this field in the
	 * incoming message. Hence the disabling of the check. Note, the check had been disabled before the DDP revamp
	 * exercise in 2002. */
/*** temorarily noop this
*	if (DDP_GLOBAL_TYPE != gp->global_type)
*		return "<FORMT>";
*****/
	cp = spool;
	subscript_count = 0;	/* incremented after every add of a subscript to subscript_array.
				 * this is usually done with the INIT_DESCRIP and DESCRIP_LENGTH usage */
	if (addr > (char *)1)
	{ /* set-up pointer to input data */
		INIT_DESCRIP(subscript_array[subscript_count], addr);
		subscript_array[subscript_count].dsc$w_length = len;
		subscript_count++;
	}
	/* set-up global directory */
	INIT_DESCRIP(subscript_array[subscript_count], cp);
	if (NULL != (gld = find_gld(gp->vol, gp->uci)))
	{
		memcpy(cp, gld->addr, gld->len);
		cp += gld->len;
	} else
		return "<NOUCI>";
	DESCRIP_LENGTH(subscript_array[subscript_count], cp);
	subscript_count++;
	inkeyptr = gp->global;
	keytop = inkeyptr + (int)gp->global_len;
	if (keytop > (char *)dp + dp->message_length) /* check to see that key length doesnt send us off the end of the record */
		return "<FORMT>";
	/* set-up global name */
	INIT_DESCRIP(subscript_array[subscript_count], cp);
	length = seven_2_ascii(inkeyptr, cp);
	cp += length;
	inkeyptr += length;
	if (inkeyptr > keytop)
		return "<FORMT>";
	DESCRIP_LENGTH(subscript_array[subscript_count], cp);
	subscript_count++;
	/* set-up subscripts */
	/* our subscript format takes less space than DSM's subscript format; so we don't have to check for buffer overflow */
	for ( ; inkeyptr < keytop; )
	{
		subscript_type = *inkeyptr++;
		if (1 < subscript_type && 0x80 > subscript_type)
		{
			subscript_type = 0x7F - subscript_type;
			*--inkeyptr = '-';
			INIT_DESCRIP(subscript_array[subscript_count], inkeyptr);
			inkeyptr++;
			while(subscript_type--)
			{
				ch = *inkeyptr;
				ch = 0x69 - ch;
				assert(ISDIGIT(ch));
				*inkeyptr++ = ch;
			}
			assert('.' == *inkeyptr || 0xFE == *inkeyptr);
			if ('.' == *inkeyptr)
				inkeyptr++;
			while (0xFE != (ch = *inkeyptr))
			{
				ch = 0x69 - ch;
				assert(ISDIGIT(ch));
				*inkeyptr++ = ch;
			}
			inkeyptr++;
			DESCRIP_LENGTH(subscript_array[subscript_count], inkeyptr - 1);
			subscript_count++;
			assert('\0' != *inkeyptr);
			inkeyptr++;
		} else
		{
			INIT_DESCRIP(subscript_array[subscript_count], inkeyptr);
			while ('\0' != *inkeyptr++)
				;
			DESCRIP_LENGTH(subscript_array[subscript_count], inkeyptr - 1);
			subscript_count++;
		}
		if (inkeyptr > keytop)
			return "<FORMT>";
	}
	/* perform operation */
	status = ddp_dal_dispatch(func, (addr == 0) ? &ddp_result : NULL);
	if (0 == (status & 1))
	{
		DDP_LOG_ZSTATUS;
		if (status == ERR_GVUNDEF)
			return "<UNDEF>";
		if (status == ERR_ZGBLDIRACC)
			return "<NOUCI>";
		if (status == ERR_DBOPNERR)
			return "<DKHER>";
/****** This is displayed as a SYNTX error
		if (status == ERR_REC2BIG)
			return "<MXSTR>";
********/
		/*default:*/
		decddp_log_error(status, "Undecoded status message, returned as <DBDGD>", &bp->dh.source_circuit_name,
				 &bp->dh.source_job_number);
		return "<DBDGD>";
	}
	/* This is the message received from the client, so reply to the source of this message */
	decddp_shdr(DDPTR_RESPONSE, 1, dp->source_circuit_name, dp->source_job_number, dp->message_number, bp->fh.source_address);
	if (NULL == addr && 0 < ddp_result.dsc$w_length)
	{ /* note, input buffer and output buffer are different and so we can test the incoming message header fields */
		bufavail = decddp_bufavail();
		if (DDPTR_QUERY != dp->trancode && bufavail > ddp_result.dsc$w_length)
			decddp_s8bit_counted(ddp_result.dsc$a_pointer, ddp_result.dsc$w_length);
		else if (DDPTR_QUERY == dp->trancode &&
			 bufavail > DSM_EXTREF_FORM_LEN + ddp_result.dsc$w_length -
				    (gtm_prefix_len = STR_LIT_LEN(GTM_EXTREF_PREFIX) + gld->len + STR_LIT_LEN(GTM_EXTREF_SUFFIX)))
		{ /* form of extref is        ^|"globaldirectory"|global */
		  /* form of extref should be       ^["UCI","VOL"]global */
			extref = ddp_result.dsc$a_pointer;
			assert(0 == memcmp(extref, GTM_EXTREF_PREFIX, STR_LIT_LEN(GTM_EXTREF_PREFIX)));
			assert(0 == memcmp(&extref[STR_LIT_LEN(GTM_EXTREF_PREFIX)], gld->addr, gld->len));
			assert(0 == memcmp(&extref[STR_LIT_LEN(GTM_EXTREF_PREFIX) + gld->len], GTM_EXTREF_SUFFIX,
					   STR_LIT_LEN(GTM_EXTREF_SUFFIX)));
			decddp_s8bit_counted(LIT_AND_LEN(DSM_EXTREF_PREFIX));
			decddp_s5asc(gp->uci);
			decddp_s8bit_counted(LIT_AND_LEN(DSM_UCI_VOL_SEPARATOR));
			decddp_s5asc(gp->vol);
			decddp_s8bit_counted(LIT_AND_LEN(DSM_EXTREF_SUFFIX));
			decddp_s8bit_counted(&extref[gtm_prefix_len], ddp_result.dsc$w_length - gtm_prefix_len);
		} else /* result is larger than our outbound buffer size */
		{
			decddp_log_error(ERR_DDPOUTMSG2BIG, "Undecoded status message, returned as <DBDGD>",
					 &bp->dh.source_circuit_name, &bp->dh.source_job_number);
			return "<DBDGD>";
		}
	}
	decddp_putbyte(DDP_MSG_TERMINATOR);
	return NULL;
}

char *ddp_lock_op(
	struct in_buffer_struct *bptr,
	condition_code (*func)(),	/* function to dispatch to*/
	int unlock_code)		/* if =1, then this is an unlock */
{
	ddp_hdr_t	*dp;		/* pointer to input buffer */
	unsigned char	*bp, *btop, *uci_p, *vol_p;
	unsigned char	*cp;
	int 		ch, jobno;
	condition_code	status;
	boolean_t	success;
	routing_tab	*remote_node;
	auxvalue	aux;
	mstr		*gld;

	dp = &bptr->dh;
	jobno = dp->source_job_number;
	assert((2 <= jobno) && (0 == (jobno & 1)));
	jobno >>= 1;
	jobno -= 1;
	assert(0 <= jobno && MAX_USERS_PER_NODE >= jobno);
	if (NULL == (remote_node = find_route(dp->source_circuit_name))) /* couldn't find sender in our tables */
		return "";
	if (dp->message_number > remote_node->incoming_users[jobno] + 1) /* sequencing error, incoming message number is too big */
		return "";
	remote_node->incoming_users[jobno] = dp->message_number;
	bp = dp->txt;
	btop = (char *)dp + dp->message_length - 1;
	if ('^' != *bp++ || '[' != *bp++ || '\"' != *bp++)
		return "<FORMT>";
	subscript_count = 0;	/* incremented after every add of a subscript to subscript_array.
				 * this is usually done with the INIT_DESCRIP and DESCRIP_LENGTH usage */
	cp = spool;
	/* set-up global directory */
	INIT_DESCRIP(subscript_array[subscript_count], cp);
	uci_p = bp;
	bp += DDP_UCI_NAME_LEN;
	if (*bp++ != '\"' || *bp++ != ',' || *bp++ != '\"')
		return "<FORMT>";
	vol_p = bp;
	bp += DDP_VOLUME_NAME_LEN;
	if (*bp++ != '\"' || *bp++ != ']')
		return "<FORMT>";
	if (NULL != (gld = find_gld(five_bit(vol_p), five_bit(uci_p))))
	{
		memcpy(cp, gld->addr, gld->len);
		cp += gld->len;
	} else
		return "<NOUCI>";
	DESCRIP_LENGTH(subscript_array[subscript_count], cp);
	subscript_count++;
	/* Load global name */
	if (bp > btop)
		return "<FORMT>";
	INIT_DESCRIP(subscript_array[subscript_count], bp);
	for (; '(' != *bp && bp < btop; bp++)
		;
	DESCRIP_LENGTH(subscript_array[subscript_count], bp);
	subscript_count++;
	if (bp < btop && '(' != *bp++)
		return "<FORMT>";
	for ( ; bp < btop; )
	{
		ch = *bp++;
		if (')' == ch)
			break;
		if ('\"' != ch)
		{
			INIT_DESCRIP(subscript_array[subscript_count], bp - 1);
			if ('-' == ch)
				ch = *bp++;
			for (; ;)
			{
				ch = *bp++;
				if (bp > btop)
					break;
				if (ISDIGIT(ch) || ch == '.')
					continue;
				break;
			}
			DESCRIP_LENGTH(subscript_array[subscript_count], bp - 1);
			subscript_count++;
		} else
		{
			INIT_DESCRIP(subscript_array[subscript_count], bp);
			for (; ;)
			{
				ch = *bp++;
				if ('\"' == ch)
				{ /* CAUTION:  MUST REMOVE DOUBLE QUOTES */
					ch = *bp++;
					if ('\"' != ch)
						break;
				}
			}
			DESCRIP_LENGTH(subscript_array[subscript_count], bp -  2);
			subscript_count++;
		}
		if (')' == ch)
			break;
		if (',' != ch || ch >= btop)
			return "<FORMT>";
	}
	success = TRUE;
	aux.as.circuit = dp->source_circuit_name;
	aux.as.job = jobno;
	status = ddp_lock_dispatch(func, unlock_code, aux.auxid);
	if (0 == (status & 1))
	{
		if (status == ERR_LCKSTIMOUT)
			success = FALSE;
		else
		{
			DDP_LOG_ZSTATUS;
			if (ERR_ZGBLDIRACC == status)
				return "<NOUCI>";
			if (ERR_DBOPNERR == status)
				return "<DKHER>";
			/*default:*/
			decddp_log_error(status, "Undecoded status message, returned as <DBDGD>", &bptr->dh.source_circuit_name,
					 &bptr->dh.source_job_number);
			return "<DBDGD>";
		}
	}
	/* This is the message received from the client, so reply to the source of this message */
	decddp_shdr(DDPTR_RESPONSE, 1, dp->source_circuit_name, dp->source_job_number, dp->message_number, bptr->fh.source_address);
	if (!unlock_code)
		decddp_putbyte(success ? 'S' : 'F');
	decddp_putbyte(DDP_MSG_TERMINATOR);
	return NULL;
}
