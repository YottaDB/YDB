/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_limits.h"
#include <ssdef.h>
#include <stddef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ddphdr.h"
#include "ddpcom.h"
#include "send_msg.h"
#include "dcpsubs.h"
#include "dcp_a2c.h"
#include "five_bit.h"
#include "is_five_bit.h"
#include "five_2_ascii.h"
#include "subscript.h"
#include "str2gvkey.h"
#include "gvusr.h"
#include "min_max.h"
#include "is_proc_alive.h"

/* Retry count should be user parameter */
#define RETRYCNT 5

/* list of machines that this client is speaking to.  The message number
	is unique per client, per machine.  Since multiple database files
	can reside on a given machine, the message number can not be associated
	with a region structure.  Since the number must be in synch for each
	machine, it can't be in the client structure.  Hence the appropriate
	sequence number for each machine for this client must be kept separately.
*/

error_def(ERR_GVDATAFAIL);
error_def(ERR_GVGETFAIL);
error_def(ERR_GVKILLFAIL);
error_def(ERR_GVORDERFAIL);
error_def(ERR_GVPUTFAIL);
error_def(ERR_GVQUERYFAIL);
error_def(ERR_GVSUBOFLOW);
error_def(ERR_GVZPREVFAIL);
error_def(ERR_NETDBOPNERR);
error_def(ERR_NETFAIL);
error_def(ERR_NETLCKFAIL);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_DDPBADRESPONSE);
error_def(ERR_DDPCONGEST);
error_def(ERR_DDPNOCONNECT);
error_def(ERR_DDPSHUTDOWN);
error_def(ERR_DDPTOOMANYPROCS);
error_def(ERR_DDPNOSERVER);
error_def(ERR_REC2BIG);

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_altkey;
GBLREF gv_key		*gv_currkey;

GBLREF struct com_slot	*com_ptr;
GBLREF struct com_hdr	*com_area;

GBLREF int4		ddp_max_rec_size;
GBLREF int4		ddp_slot_size;

static unsigned short	my_jobno;
static ddp_info		*ddp_values;
static gv_key		**ckey_adr;
static gv_key		**akey_adr;
static gd_region	**creg_adr;
static unsigned char	*bufptr;

static condition_code	send_to_agent();
static int 		gvusr_o2(char trancode);

static void setup_global_pointers(char trancode)
{
	char			*inpt;
	unsigned char		*ptr, *lastsubstart;
	ddp_hdr_t		*dp;
	ddp_global_request_t	*gp;

	gv_currkey = *ckey_adr;
	gv_altkey = *akey_adr;
	gv_cur_region = *creg_adr;
	inpt = gv_currkey->base;
	ddp_values = &FILE_INFO(gv_cur_region)->file_id;
	dp = com_ptr->text;
	dp->trancode = trancode;
	dp->proto = DDP_PROTO_VERSION;
	/* source_circuit_name filled in by agent */
	dp->source_job_number = my_jobno;
	dp->remote_circuit_name = ddp_values->volset;	/* Actually the volset name.  Translated by the agent */
	dp->remote_job_number = 0;
	dp->filler1 = 0;
	dp->hdrlen = DDP_MSG_HDRLEN;
	gp = dp->txt;
	gp->naked_size = 0;
	gp->uci = ddp_values->uci;
	gp->vol = ddp_values->volset;
	gp->global_type = DDP_GLOBAL_TYPE;
	bufptr = gp->global;
	dcp_g2d(&inpt, &bufptr, (unsigned char *)com_ptr + ddp_slot_size - bufptr, &gp->naked_size);
	if (UCHAR_MAX >= bufptr - gp->global)
		gp->global_len = bufptr - gp->global;
	else
		rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
	return;
}

static condition_code send_to_agent(void)
{
	uint4	server;

	com_ptr->len = bufptr - (unsigned char *)com_ptr->text;
	dcpc_send2agent();
	if (0 == dcpc_rcv_from_agent())
		return SS$_NORMAL;
	return (0 == (server = com_area->server_pid) || !is_proc_alive(server, 0)) ? ERR_DDPNOSERVER : ERR_NETFAIL;
}

void gvusr_init(gd_region *reg, gd_region **creg, gv_key **ckey, gv_key **akey)
/* 2nd argument is a pointer to gv_cur_region in gtmshr */
/* 3rd argument is a pointer to gv_currkey in gtmshr */
/* 4th argument is a pointer to gv_altkey in gtmshr */
{
	char		*cp;
	condition_code	status;
	gd_segment	*seg;
	mval cktnam_logi = DEFINE_MVAL_STRING(MV_STR, 0, 0, STR_LIT_LEN(DDP_CLIENT_CKTNAM_LOGI), DDP_CLIENT_CKTNAM_LOGI, 0, 0);
	static boolean_t shm_inited = FALSE;

	ckey_adr = ckey;
	akey_adr = akey;
	creg_adr = creg;
	gv_cur_region = reg;
	seg = reg->dyn.addr;
	/* NOTE: In a clean and better world, these structures shouldn't have to
		be set up this way for a dba_usr database.  Users should be able
		to define their own structures to put in seg->file_cntl.  But this
		is the real world, and GV_MATCH will get sick if we don't toe the
		line, and make a usr database look like a bg or mm database at
		this structure level.  No one is using the usr funcitionality except
		our implementation of DEC DDP, I believe, so it shouldn't be too
		embarrassing.
	*/
	FILE_CNTL_INIT_IF_NULL(seg);
	ddp_values = &FILE_INFO(reg)->file_id;
	if (0 == ddp_values->uci)
	{ /* Calculate UCI and Volume set names */
		cp = &reg->dyn.addr->fname;
		if (!is_five_bit(cp))
			rts_error(VARLSTCNT(6) ERR_NETDBOPNERR, 0, ERR_TEXT, 2, LEN_AND_LIT("Invalid VOLUME specification"));
		ddp_values->volset = five_bit(cp);
		cp += DDP_VOLUME_NAME_LEN;
		if (':' != *cp++ || ':' != *cp++)
			rts_error(VARLSTCNT(6) ERR_NETDBOPNERR, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Invalid separator between volume and uci"));
		if (!is_five_bit(cp))
			rts_error(VARLSTCNT(6) ERR_NETDBOPNERR, 0, ERR_TEXT, 2, LEN_AND_LIT("Invalid UCI specification"));
		ddp_values->uci = five_bit(cp);
	}
	if (NULL != com_ptr)
	{ /* Have already established communications with the agent */
		reg->open = TRUE;
		return;
	}
	if (NULL == com_area)
	{
		status = dcp_get_circuit(&cktnam_logi);
		if (0 == (status & 1))
			rts_error(VARLSTCNT(7) ERR_NETDBOPNERR, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Could not find circuit name to use"), status);
		status = dcp_get_maxrecsize();
		if (0 == (status & 1))
			rts_error(VARLSTCNT(7) ERR_NETDBOPNERR, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Could not find maximum record size"), status);
	}
	status = dcpc_shm_init(NULL == com_area);
	if (0 == (status & 1))
	{
		if (ERR_DDPTOOMANYPROCS == status)
			rts_error(VARLSTCNT(5) ERR_NETDBOPNERR, 0, ERR_DDPTOOMANYPROCS, 1, MAXIMUM_PROCESSES);
		else
			rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	my_jobno = ((((unsigned char *)com_ptr - (unsigned char *)com_area->slot) / ddp_slot_size) + 1) << 1;
			/* Note: no slot zero...slot 1 is for server/agent; normal users use $J*2 */
	reg->open = TRUE;
	return;
}

void gvusr_rundown(void)
{
	ddp_hdr_t		*dp;
	ddp_global_request_t	*gp;

	if (NULL == com_ptr)
		return;
	dp = com_ptr->text;
	dp->trancode = DDPTR_USEREXIT;
	dp->source_job_number = my_jobno;
	gp = dp->txt;
	com_ptr->len = gp->global - (unsigned char *)com_ptr->text;
	dcpc_send2agent();
	/* Deal with LOCK rundowns */
	com_ptr = NULL;
	return;
}

int gvusr_data(void)
{
	ddp_hdr_t	*msg;
	char		*cp, *cp_top;
	int		msglen;
	int		retval;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(DDPTR_DEFINE);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		/* Result can be at most two digits, each digit being the ASCII value of char '0' or '1' */
		/* Possible valid results : "0", "1", "00", "01", "10", or "11" - without the quotes, quotes shown here only
		 * to indicate that the result is a string */
		for (retval = 0, cp = msg->txt, cp_top = cp + MIN(2, msglen); cp < cp_top; cp++)
		{ /* only 2 digits for $D result */
			retval *= 10;
			if ('1' == *cp)
				retval++;
			else if ('0' != *cp)
			{	/* Note: perhaps this should be retry */
				send_msg(VARLSTCNT(8) ERR_GVDATAFAIL, 2, LEN_AND_LIT("DDP_FORMAT"), ERR_TEXT, 2, msglen, msg->txt);
				rts_error(VARLSTCNT(8) ERR_GVDATAFAIL, 2, LEN_AND_LIT("DDP_FORMAT"), ERR_TEXT, 2, msglen, msg->txt);
			}
		}
		return retval;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(4) ERR_GVDATAFAIL, 2, msglen, msg->txt);
		return 0;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return 0;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return 0;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return 0;
	default:
		send_msg(VARLSTCNT(11) ERR_GVDATAFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVDATAFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return 0;
	}
	return 0;
}

static int gvusr_o2(char trancode)
{
	ddp_hdr_t	*msg;
	int		msglen;
	mval		tv;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(trancode);
		if (0 == gv_currkey->prev)	/* Name level dollar orders are not supported */
			rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2, LEN_AND_LIT("Name level $ORDER"));
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		tv.mvtype = MV_STR;
		tv.str.addr = msg->txt;
		tv.str.len = msglen;
		memcpy(gv_altkey, gv_currkey, SIZEOF(*gv_currkey) + gv_currkey->prev);
		gv_altkey->end = gv_altkey->prev;
		gtm$mval2subsc(&tv, gv_altkey);
		return (0 != msglen) ? 1 : 0;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(4) ERR_GVORDERFAIL, 2, msglen, msg->txt);
		return 0;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return 0;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return 0;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return 0;
	default:
		send_msg(VARLSTCNT(11) ERR_GVORDERFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVORDERFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return 0;
	}
	return 0;
}

int gvusr_order(void)
{
	return(gvusr_o2(DDPTR_ORDER));
}

int gvusr_query(mval *v)
{
	ddp_hdr_t	*msg;
	int		msglen, retval;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(DDPTR_QUERY);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		if (0 != msglen)
		{
			v->mvtype = MV_STR;
			v->str.len = msglen;
			v->str.addr = msg->txt;
			return 1;
		}
		return 0;
	case DDPTR_ERRESPONSE:
		if (0 != memcmp(msg->txt, "<UNDEF>", STR_LIT_LEN("<UNDEF>")))
			rts_error(VARLSTCNT(4) ERR_GVQUERYFAIL, 2, msglen, msg->txt);
		return 0;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return 0;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return 0;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return 0;
	default:
		send_msg(VARLSTCNT(11) ERR_GVQUERYFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVQUERYFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return 0;
	}
	return 0;
}

int gvusr_zprevious(void)
{
	return(gvusr_o2(DDPTR_PREVIOUS));
}

int gvusr_get(mval *v)
{
	ddp_hdr_t	*msg;
	int		msglen;
	int		retval;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(DDPTR_GET);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		v->mvtype = MV_STR;
		v->str.len = msglen;
		v->str.addr = msg->txt;
		return 1;
	case DDPTR_ERRESPONSE:
		if (0 != memcmp(msg->txt, "<UNDEF>", STR_LIT_LEN("<UNDEF>")))
			rts_error(VARLSTCNT(4) ERR_GVGETFAIL, 2, msglen, msg->txt);
		return 0;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return 0;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return 0;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return 0;
	default:
		send_msg(VARLSTCNT(11) ERR_GVGETFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVGETFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return 0;
	}
	return 0;
}

void gvusr_kill(bool do_subtree)
{
	ddp_hdr_t	*msg;
	int		msglen;
	condition_code	status;
	int		retry_count;

	if (!do_subtree)
		rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(DDPTR_KILL);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		assert(0 == msglen);
		return;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(4) ERR_GVKILLFAIL, 2, msglen, msg->txt);
		return;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return;
	default:
		send_msg(VARLSTCNT(11) ERR_GVKILLFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVKILLFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return;
	}
	return;
}

void gvusr_put(mval *v)
{
	int		buff_remaining;
	condition_code	status;
	ddp_hdr_t	*msg;
	int		msglen;
	int		retry_count;

	msg = com_ptr->text;
	for (retry_count = 0; ; retry_count++)
	{
		setup_global_pointers(DDPTR_PUT);
		buff_remaining = (unsigned char *)com_ptr + ddp_slot_size - bufptr;
		assert(0 <= buff_remaining); /* we shouldn't have trashed someone else's slot */
		if (v->str.len + 1 > buff_remaining) /* + 1 for the mysterious last byte that the protocol seems to want */
			rts_error(VARLSTCNT(6) ERR_REC2BIG, 4, ((ddp_global_request_t *)(msg->txt))->global_len + v->str.len,
				  ddp_slot_size - offsetof(com_slot_t, text[0]) - DDP_MSG_HDRLEN
				  - offsetof(ddp_global_request_t, global[0]), REG_LEN_STR(gv_cur_region));
		memcpy(bufptr, v->str.addr, v->str.len);
		bufptr += v->str.len;
	 	bufptr++;	/* Not sure why the extra byte is here, but the protocol seems to have it */
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		assert(0 == msglen);
		return;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(4) ERR_GVPUTFAIL, 2, msglen, msg->txt);
		return;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return;
	default:
		send_msg(VARLSTCNT(11) ERR_GVPUTFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(11) ERR_GVPUTFAIL, 2, LEN_AND_LIT("DDP_NET_FAIL"),
			ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		return;
	}
	return;
}

static void setup_lock_message(char trancode, uint4 lock_len, unsigned char *lock_key, gd_region *creg)
{
	unsigned char	sub_len, *lock_top, *lptr, *lptr_top, sep, ch, *cptr, *buftop;
	ddp_info	*reg_values;
	ddp_hdr_t	*dp;
	boolean_t	is_string, seen_dot, timed_out;
 	int		tries;

	sep = '(';
	lock_top = lock_key + lock_len;
	sub_len = *lock_key++;
	reg_values = &FILE_INFO(creg)->file_id;
	dp = (ddp_hdr_t *)com_ptr->text;
	memset(dp, 0, SIZEOF(*dp));
	dp->trancode = trancode;
	dp->proto = DDP_PROTO_VERSION;
	dp->source_job_number = my_jobno;
	dp->remote_circuit_name = reg_values->volset;	/* Actually the volset name.  Translated by the agent */
	dp->remote_job_number = 0;
	dp->filler1 = 0;
	dp->hdrlen = DDP_MSG_HDRLEN;
	bufptr = dp->txt; /* beginning of the nref */
	if ('^' == *lock_key)
	{
		*bufptr++ = *lock_key++;
		sub_len--;
	}
	/* put a ["UCI","VOL"] in the buffer in front of the lock namespace */
	cptr = DSM_EXTREF_PREFIX;
	cptr++; /* go past the ^ */
	memcpy(bufptr, cptr, STR_LIT_LEN(DSM_EXTREF_PREFIX) - 1); /* -1 bcoz ^ has already been accounted for */
	bufptr += (STR_LIT_LEN(DSM_EXTREF_PREFIX) - 1);
	bufptr = five_2_ascii(&reg_values->uci, bufptr);
	memcpy(bufptr, DSM_UCI_VOL_SEPARATOR, STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR));
	bufptr += STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR);
	bufptr = five_2_ascii(&reg_values->volset, bufptr);
	memcpy(bufptr, DSM_EXTREF_SUFFIX, STR_LIT_LEN(DSM_EXTREF_SUFFIX));
	bufptr += STR_LIT_LEN(DSM_EXTREF_SUFFIX);
	buftop = (unsigned char *)com_ptr + ddp_slot_size;
	if (bufptr + sub_len + 2 >= buftop)
		rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
	memcpy(bufptr, lock_key, sub_len);
	bufptr += sub_len;
	lock_key += sub_len;
	if (lock_key < lock_top)
	{
		while (lock_key < lock_top)
		{
			*bufptr++ = sep;
			sub_len = *lock_key++;
			lptr = lock_key;
			lptr_top = lptr + sub_len;
			seen_dot = is_string = FALSE;
			if (0 != sub_len && '-' == *lptr)
				*lptr++;
			for ( ; lptr < lptr_top; )
			{
				ch = *lptr++;
				if ('.' == ch)
				{
					if (seen_dot)
					{
						is_string = TRUE;
						break;
					} else
					{
						seen_dot = TRUE;
						continue;
					}
				}
				if (!ISDIGIT(ch))
				{
					is_string = TRUE;
					break;
				}
			}
			if (is_string)
			{
				if (bufptr >= buftop)
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
				*bufptr++ = '\"';
			}
			lptr = lock_key;
			lptr_top = lptr + sub_len;
			for ( ; lptr < lptr_top; )
			{
				if (bufptr >= buftop)
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
				ch = *lptr++;
				*bufptr++ = ch;
				if ('\"' != ch)
					continue;
				else
				{
					if (bufptr >= buftop)
						rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
					*bufptr++ = ch;		/* double the quotes to make a legal reference */
				}
			}
			if (is_string)
			{
				if (bufptr >= buftop)
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
				*bufptr++ = '\"';
			}
			lock_key += sub_len;
			sep = ',';
		}
		if (bufptr >= buftop)
			rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
		*bufptr++ = ')';
	}
	*bufptr++ = DDP_MSG_TERMINATOR;
	return;
}

int gvusr_lock(uint4 lock_len, unsigned char *lock_key, gd_region *creg)
{
	ddp_hdr_t	*msg;
	int		msglen;
	int		retval;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_lock_message(DDPTR_ZALLOC, lock_len, lock_key, creg);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		assert(0 != msglen);
		if ('S' == msg->txt[0])
			return 0;	/* 0 is a successful lock */
		assert('F' == msg->txt[0]);
		return 1;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(1) ERR_NETLCKFAIL);
		return 1;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return 1;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return 1;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return 1;
	default:
		send_msg(VARLSTCNT(9) ERR_NETLCKFAIL, 0, ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(9) ERR_NETLCKFAIL, 0, ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen,msg->txt);
		return 1;
	}
	return 1;
}

void gvusr_unlock(uint4 lock_len, unsigned char *lock_key, gd_region *creg)
{
	ddp_hdr_t	*msg;
	int		msglen;
	condition_code	status;
	int		retry_count;

	for (retry_count = 0; ; retry_count++)
	{
		setup_lock_message(DDPTR_ZDEALLOC, lock_len, lock_key, creg);
		status = send_to_agent();
		if (0 != (status & 1))
			break;
		if (RETRYCNT <= retry_count)
			rts_error(VARLSTCNT(1) status);
	}
	msg = com_ptr->text;
	msglen = msg->message_length - msg->hdrlen - 1; /* -1 to strip the terminator byte */
	if (0 > msglen)
		msglen = 0;
	switch(msg->trancode)
	{
	case DDPTR_RESPONSE:
		assert(0 == msglen);
		return;
	case DDPTR_ERRESPONSE:
		rts_error(VARLSTCNT(1) ERR_NETLCKFAIL);
		return;
	case DDPTRX_CONGESTION:
		rts_error(VARLSTCNT(1) ERR_DDPCONGEST);
		return;
	case DDPTRX_SHUTDOWN:
		rts_error(VARLSTCNT(1) ERR_DDPSHUTDOWN);
		return;
	case DDPTRX_NOCONNECT:
		rts_error(VARLSTCNT(4) ERR_DDPNOCONNECT, 2, DB_LEN_STR(gv_cur_region));
		return;
	default:
		send_msg(VARLSTCNT(9) ERR_NETLCKFAIL, 0, ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen, msg->txt);
		rts_error(VARLSTCNT(9) ERR_NETLCKFAIL, 0, ERR_DDPBADRESPONSE, 1, (long)msg->trancode, ERR_TEXT, 2, msglen,msg->txt);
		return;
	}
	return;
}
