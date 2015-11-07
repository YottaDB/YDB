#/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_time.h"

#include <stddef.h>
#include <ssdef.h>
#include <lnmdef.h>
#include <descrip.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <errno.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "vmsdtype.h"	/* for trans_log_name */
#include "io.h"
#include "dcpsubs.h"
#include "decddp.h"
#include "dcp_a2c.h"
#include "route_table.h"
#include "ddp_trace_output.h"
#include "cce_output.h"
#include "trans_log_name.h"
#include "logical_truth_value.h"
#include "gtm_ctype.h"
#include "cli.h"
#include "eintr_wrappers.h"
#include "gtm_env_init.h"     /* for gtm_env_init() prototype */

GBLDEF boolean_t 	ddp_trace;
GBLDEF unsigned short	my_group_mask = DDP_DEFAULT_GROUP_MASK;
GBLDEF volset_tab	volset_table[DDP_MAX_VOLSETS];

GBLREF unsigned short		my_circuit;
GBLREF com_hdr_t		*com_area;
GBLREF struct dsc$descriptor	ddp_result;
GBLREF unsigned char		decddp_multicast_addr[ETHERADDR_LENGTH];
GBLREF mval			dollar_zproc;
GBLREF int4			ddp_slot_size;

error_def(ERR_DDPOUTMSG2BIG);

static char		ddp_input_buffer[offsetof(struct in_buffer_struct, dh) + MAX_ETHER_DATA_SIZE];
static struct
{
	int4	link;
	void	(*exit_hand)();
	int4	arg_cnt;
	int4	*cond_val;
} exi_desblk;
static int4 exi_condition;

static void 	self_wake(void);

void 		ddp_return_error(struct in_buffer_struct *bp, char *msg);
void 		reply_to_client(int chn, char *buff, int bufflen);
void		declare_client_error(int chn, char m);
void		declare_err_msg(ddp_hdr_t *ddptr_client, ddp_hdr_t *ddptr_server, char *error);
void		ddp_exi_rundown(void);

mainx()
{
	/* server automatic declarations */
	condition_code	gtm$gbldata();
	condition_code	gtm$gblget();
	condition_code	gtm$gblkill();
	condition_code	gtm$gblorder();
	condition_code	gtm$gblprevious();
	condition_code	gtm$gblput();
	condition_code	gtm$gblquery();
	condition_code	gtm$lock2();
	condition_code	gtm$zdealloc2();
	condition_code	status;
	int		ckt;
	boolean_t	sendit, input_pending, circuit_entered;
	mstr		trace_logical = {LEN_AND_LIT(DDP_TRACE_ENV)};
	char		*addr, *topaddr, *errmsg;
	ddp_hdr_t	*ddptr;
	routing_tab	*remote_node;
	struct frame_hdr	*frame;
	struct in_buffer_struct *bufptr = (struct in_buffer_struct *)ddp_input_buffer;
	ddp_announce_msg_t	*ap;
	ddp_global_request_t	*gp;
	char		errstr[1024];
	DCL_THREADGBL_ACCESS;

	/* Agent automatic declarations */
	unsigned char	*buff;
	short		len;
	int4		jobno;
	com_slot_t	*cptr;

	GTM_THREADGBL_INIT;
	gtm_env_init();	/* read in all environment variables before any function call (particularly malloc) */
	if (ddp_trace = logical_truth_value(&trace_logical, FALSE, NULL))
		cce_out_open();

	getzprocess();
	status = dcp_get_circuit(&dollar_zproc);
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Circuit Name initialization failed", 0, 0);
		return status;
	}
	status = dcp_get_volsets();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Volume Set initialization failed", 0, 0);
		return status;
	}

	status = dcp_get_groups();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Group mask initialization failed", 0, 0);
		return status;
	}

	status = dcp_get_maxrecsize();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Max record size could not be found", 0, 0);
		return status;
	}

	ddp_result.dsc$b_dtype = DSC$K_DTYPE_T;
	ddp_result.dsc$b_class = DSC$K_CLASS_D;
	ddp_result.dsc$w_length = 0;

	gtm$init();

	status = decddp_init();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Ethernet controller initialization failed", 0, 0);
		return status;
	}
	decddp_shdr(DDPTR_ANNOUNCE, 1, 1, 0, 0, decddp_multicast_addr);
	decddp_sinit("WI");
	status = decddp_send();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Network connection failed", 0, 0);
		return status;
	}
	decddp_log_error(0, "Server has connected to network", 0, 0);

	/* Establish exit handler */
	exi_desblk.exit_hand = ddp_exi_rundown;
	exi_desblk.arg_cnt = 1;
	exi_desblk.cond_val = &exi_condition;
	sys$dclexh(&exi_desblk);

	status = dcpa_shm_init();
	if (0 == (status & 1))
	{
		decddp_log_error(status, "Agent buffer initialization failed", 0, 0);
		return status;
	}

/**************
	self_wake();
*/
	for (;;)
	{
		/* The input + output loop is structured in this way in order to make input and output 'fair' */
		input_pending = dcp_get_input_buffer(bufptr, SIZEOF(ddp_input_buffer));
		if (NULL != (cptr = dcpa_read()))
		{ /* Service local client request */
			len = cptr->len;
			jobno = ((unsigned char *)cptr - (unsigned char *)com_area->slot) / ddp_slot_size;
			ddptr = buff = cptr->text;
			assert((jobno + 1) == (ddptr->source_job_number >> 1));
			if (DDPTR_USEREXIT != ddptr->trancode)
			{
				ddptr->source_circuit_name = my_circuit;
				/* client gives us the name of the volume set.  we translate it to the node supporting it */
				if (0 == (ckt = find_circuit(ddptr->remote_circuit_name)))
				{
					declare_client_error(jobno, DDPTRX_NOCONNECT);
					continue;
				}
				ddptr->remote_circuit_name = ckt;
				remote_node = find_route(ckt);
				if (!remote_node)
				{
					declare_client_error(jobno, DDPTRX_NOCONNECT);
					continue;
				}
				ddptr->message_number = remote_node->outgoing_users[jobno];
				decddp_set_etheraddr(remote_node->ether_addr);
				status = dcp_send_message(buff, len, &cptr->iosb);
				if (0 == (status & 1))
				{
					decddp_log_error(status, "Server transmission failed", &ckt, 0);
					if (0 != cptr->state)
					{
						declare_err_msg(ddptr, ddptr, DDP_XMIT_FAIL);
						dcpa_send(cptr);
					}
				}
			} else
			{
				reset_user_count(jobno);
				dcpa_free_user(cptr);
			}
		} else
		{
			if (!input_pending)
				sys$hiber();
		}
		if (input_pending)
		{ /* Service remote request */
			sendit = TRUE;
			errmsg = NULL;
			frame = &bufptr->fh;
			ddptr = &bufptr->dh;
			switch (ddptr->trancode)
			{
			case DDPTR_GET:
				errmsg = ddp_db_op(bufptr, gtm$gblget, NULL, 0);
				break;
			case DDPTR_PUT:
				gp = ddptr->txt;
				addr = gp->global + gp->global_len;
				topaddr = ((char *)ddptr) + ddptr->message_length;
				if (addr > topaddr)
				{
					errmsg = "<FORMT>";
					break;
				}
				errmsg = ddp_db_op(bufptr, gtm$gblput, addr, topaddr - addr - 1); /* -1 to get rid of the mysterious
												   * last byte in PUT message */
				break;
			case DDPTR_KILL:
				errmsg = ddp_db_op(bufptr, gtm$gblkill,(char *)1 ,0);
				break;
			case DDPTR_ORDER:
				errmsg = ddp_db_op(bufptr, gtm$gblorder, NULL, 0);
				break;
			case DDPTR_PREVIOUS:
				errmsg = ddp_db_op(bufptr, gtm$gblprevious, NULL, 0);
				break;
			case DDPTR_DEFINE:
				errmsg = ddp_db_op(bufptr, gtm$gbldata, NULL, 0);
				break;
			case DDPTR_QUERY:
				errmsg = ddp_db_op(bufptr, gtm$gblquery, NULL, 0);
				break;
			case DDPTR_ZALLOC:
				errmsg = ddp_lock_op(bufptr, gtm$lock2, 0);
				break;
			case DDPTR_ZDEALLOC:
				errmsg = ddp_lock_op(bufptr, gtm$zdealloc2, 1);
				break;
			case DDPTR_ANNOUNCE:
				ap = ddptr->txt;
				if ('W' != ap->code[0] || 'I' != ap->code[1])
				{
					sendit = FALSE;
					if ('I' == ap->code[0] && 'S' == ap->code[1])
					{
						remove_circuits(ddptr);
						enter_circuits(ddptr);
						decddp_log_error(0, "Volume set configuration has changed",
								 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
					} else if ('I' == ap->code[0] && 'D' == ap->code[1])
					{
						remove_circuits(ddptr);
						decddp_log_error(0, "Circuit has shut-down",
								 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
					} else if (('I' == ap->code[0] && 'I' == ap->code[1]) ||
						   ('W' == ap->code[0] && 'A' == ap->code[1]))
					{
						if (FALSE != (circuit_entered = enter_circuits(ddptr))
						    && ('W' != ap->code[0] || 'A' != ap->code[1]))
							decddp_log_error(0,"System connected in response to our connection request",
								 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
					}
				} else if (FALSE != (sendit = enter_circuits(ddptr)))
				{ /* code == WI and we belong to some group that the announcer also belongs to */
					decddp_log_error(0, "System is now connected",
							 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
					decddp_shdr(DDPTR_ANNOUNCE, 1, 1, 0, 0, frame->source_address);
					decddp_sinit("II");
				}
				break;
			case DDPTR_RESPONSE:
			case DDPTR_ERRESPONSE:
				sendit = FALSE;
				errmsg = NULL;
				jobno = (ddptr->remote_job_number >> 1);
				jobno -= 1;
				assert(jobno >= 0 && jobno < MAX_USERS_PER_NODE);
				remote_node = find_route(ddptr->source_circuit_name);
				/* ignore if we can't match the message sequence numbers */
				if (remote_node && remote_node->outgoing_users[jobno] == ddptr->message_number)
				{
					remote_node->outgoing_users[jobno]++;
					reply_to_client(jobno, ddptr, ddptr->message_length);
				}
				break;
			default:
				SNPRINTF(errstr, SIZEOF(errstr), "Unknown transaction code type: %d", ddptr->trancode);
				decddp_log_error(0, errstr, &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
				errmsg = "<FORMT>"; /* Let the server know that this was a badly formatted message */
				break;/* throw away unkown transaction types */
			}
			if (NULL != errmsg)
			{
				if ('\0' != *errmsg)
					ddp_return_error(bufptr, errmsg);
				else
				{	 /* a null error msg indicates a sequencing error - the message is ignored */
					SNPRINTF(errstr, SIZEOF(errstr),
						"Sequencing error - expected: %d, got: %d", remote_node->incoming_users[jobno] + 1,
						bufptr->dh.message_number);
					decddp_log_error(0, errstr,
							 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
				}
			} else if (sendit)
			{
				status = decddp_send();
				if (0 == (status & 1))
				{
					decddp_log_error(status, "Server response transmission failed",
							 &bufptr->dh.source_circuit_name, &bufptr->dh.source_job_number);
					if (ERR_DDPOUTMSG2BIG == status)
						ddp_return_error(bufptr, "<DBDGD>"); /* should ideally be something like
										      * "<MSG2BIG>", but we don't yet know the
										      * code DSM uses - Vinaya 06/28/02 */
				}
			}
		}
	}
}

/* Client subroutines */
void ddp_return_error(struct in_buffer_struct *bp, char *msg)
{
	condition_code	status;

	decddp_shdr(DDPTR_ERRESPONSE, 1, bp->dh.source_circuit_name, bp->dh.source_job_number,
			bp->dh.message_number, bp->fh.source_address);
	(void)decddp_s8bit(msg);
	decddp_putbyte(DDP_MSG_TERMINATOR);
	status = decddp_send();
	if (0 == (status & 1))
	{
		assert(ERR_DDPOUTMSG2BIG != status);
		decddp_log_error(status, "Server error response transmission failed",
				 &bp->dh.source_circuit_name, &bp->dh.source_job_number);
	}
}

static void self_wake(void)
{
	static readonly int4 timeout[2] = {-50000000, -1}; /* 5 seconds */
	sys$wake(0, 0);
	sys$setimr(0, timeout, self_wake, 0, 0);
}

/* Agent subroutines */

void reply_to_client(int chn, char *buff, int bufflen)
{
	com_slot_t 	*cp;
	ddp_hdr_t	*ddptr_r, *ddptr_s;
	DEBUG_ONLY(unsigned char  *extref;)
	DEBUG_ONLY(ddp_global_request_t	*gp;)

	cp = (com_slot_t *)((unsigned char *)com_area->slot + chn * ddp_slot_size);
	assert(0 != cp->pid);
	/* Only reply once.  If state is zero, then there is alreay a message
	in the buffer or a time out.  Don't overlay message.
	State may also be zero because we have timed out, in which case we
	don't want to overlay the retry message */
	if (0 == cp->state)
		return;
	ddptr_r = (ddp_hdr_t *)buff;
	ddptr_s = (ddp_hdr_t *)cp->text; /* until we overwrite this com slot, the slot contains the message we sent */
	if ((DDPTR_QUERY != ddptr_s->trancode) || 1 == bufflen || (1 == (ddptr_r->message_length - ddptr_r->hdrlen)))
	{ /* non QUERY operation, OR,
	   * came here from declare_client_error, OR
	   * "" result from QUERY (message contains 0xFF, just one byte) */
		if (ddp_slot_size - offsetof(com_slot_t, text[0]) >= bufflen)
		{
			cp->len = bufflen;
			memcpy(ddptr_s, ddptr_r, bufflen);
		} else
		{ /* we don't want to trash someone else's slot; declare error instead */
			assert(1 != bufflen); /* if we came into this function from declare_client_error, we should have sufficient
						 space in slot. Also, calling declare_err_msg is not good when there is no message
						 that has been formatted in the buffer. */
			declare_err_msg(ddptr_s, ddptr_r, DDP_MSG2BIG);
		}
	} else
	{ /* remove the extended reference from the response */
		assert(DSM_EXTREF_FORM_LEN < ddptr_r->message_length - ddptr_r->hdrlen);
		DEBUG_ONLY(extref = ddptr_r->txt);
		DEBUG_ONLY(gp = ddptr_s->txt);
		assert(0 == memcmp(extref, DSM_EXTREF_PREFIX, STR_LIT_LEN(DSM_EXTREF_PREFIX)));
		assert(gp->uci == five_bit(&extref[STR_LIT_LEN(DSM_EXTREF_PREFIX)]));
		assert(0 == memcmp(&extref[STR_LIT_LEN(DSM_EXTREF_PREFIX) + DDP_UCI_NAME_LEN],
				   DSM_UCI_VOL_SEPARATOR, STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR)));
		assert(gp->vol ==
			five_bit(&extref[STR_LIT_LEN(DSM_EXTREF_PREFIX) + DDP_UCI_NAME_LEN + STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR)]));
		assert(0 == memcmp(&extref[STR_LIT_LEN(DSM_EXTREF_PREFIX) + DDP_UCI_NAME_LEN +
					   STR_LIT_LEN(DSM_UCI_VOL_SEPARATOR) + DDP_VOLUME_NAME_LEN],
				   DSM_EXTREF_SUFFIX, STR_LIT_LEN(DSM_EXTREF_SUFFIX)));
		assert(DDP_MSG_TERMINATOR == ddptr_r->txt[ddptr_r->message_length - ddptr_r->hdrlen - 1]);
		if (ddp_slot_size - ddptr_r->message_length >= offsetof(com_slot_t, text[0]) - (DSM_EXTREF_FORM_LEN - 1))
		{ /* we don't want to trash someone else's slot; declare error instead */
			memcpy((char *)ddptr_s, (char *)ddptr_r, ddptr_r->hdrlen); /* copy the header */
			ddptr_s->message_length -= (DSM_EXTREF_FORM_LEN - 1); /* adjust for the removal of the extref prefix */
									      /* -1 because we want to retain ^ */
			ddptr_s->txt[0] = '^'; /* global indicator */
			memcpy(&ddptr_s->txt[1],
			       &ddptr_r->txt[DSM_EXTREF_FORM_LEN], /* global without the extref prefix */
			       ddptr_r->message_length - ddptr_r->hdrlen - DSM_EXTREF_FORM_LEN);
		} else
			declare_err_msg(ddptr_s, ddptr_r, DDP_MSG2BIG);
	}
	dcpa_send(cp);
	return;
}

void declare_client_error(int chn, char m)
{
	reply_to_client(chn, &m, SIZEOF(m));
	return;
}

void declare_err_msg(ddp_hdr_t *ddptr_client, ddp_hdr_t *ddptr_server, char *error)
{ /* error should be '\0' terminated */
	int	error_len;

	error_len = strlen(error) + 1;  /* we want the terminating null to be copied due to the way we compute the message length
					 * for response messages in ddpgvusr */
	memcpy((char *)ddptr_client, (char *)ddptr_server, ddptr_server->hdrlen); /* copy the header */
	memcpy(ddptr_client->txt, error, error_len); /* Change the message text */
	ddptr_client->message_length = ddptr_client->hdrlen + error_len;
	ddptr_client->trancode = DDPTR_ERRESPONSE;
}

void ddp_exi_rundown(void)
{

	decddp_shdr(DDPTR_ANNOUNCE, 1, 1, 0, 0, decddp_multicast_addr);
	decddp_sinit("ID");
	decddp_send();
}
