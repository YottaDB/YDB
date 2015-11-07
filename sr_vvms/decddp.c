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

#include "mdef.h"

#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>
#include <lnmdef.h>	/* for trans_log_name */
#include <stddef.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "vmsdtype.h"	/* for trans_log_name */
#include "nmadef.h"
#include "dcpsubs.h"
#include "decddp.h"
#include "ddp_trace_output.h"
#include "trans_log_name.h"
#include "five_bit.h"
#include "five_2_ascii.h"

/* Ethernet controller information.  */

#define XMTBUFLEN	MAX_ETHER_DATA_SIZE	/* size of a transmit buffer */
#define RETRY_LIMIT	3			/* number of attempts to get past an error (as of this writing used by send) */

GBLDEF unsigned char	decddp_multicast_addr[ETHERADDR_LENGTH] = {9, 0, 0x2b, 0, 0, 1};

GBLREF unsigned short	my_circuit;
GBLREF unsigned short	my_group_mask; /* filled in dcp_get_group_mask */
GBLREF boolean_t	ddp_trace;
GBLREF volset_tab	volset_table[];
GBLREF mstr		my_circuit_name;

typedef struct
{
	int4		buflen;
	unsigned char	*bufptr;
} quad_desc;

#ifdef __ALPHA
# pragma member_alignment save
# pragma nomember_alignment
#endif

static struct
{
	short	parm_id;
	int4	parm_val;
} setparm[] =
{ /* Keep NMA$C_PCLI_BFN as the first element */
	{NMA$C_PCLI_BFN, ETH_RCV_BUFCNT},	/* GT.CM default # of receive buffers to preallocate */
	{NMA$C_PCLI_FMT, NMA$C_LINFM_ETH},	/* packet format = ethernet packet format */
	{NMA$C_PCLI_PTY, DDP_ETH_PROTO_TYPE},	/* protocol type = Distributed Data Processing (DDP) protocol */
	{NMA$C_PCLI_PAD, NMA$C_STATE_OFF},	/* padding = no size field */
	{NMA$C_PCLI_MLT, NMA$C_STATE_ON},	/* multicast address state = accecpt all multicast addresses */
	{NMA$C_PCLI_BUS, ETHERNET_HEADER_SIZE + MAX_ETHER_DATA_SIZE}	/* maximum allowable port receive data size */
};

#ifdef __ALPHA
# pragma member_alignment restore
#endif

static quad_desc		setparmdsc = {SIZEOF(setparm), setparm};
static unsigned char		sensebuf[250]; /* recommeded size for attributes buffer, see LAN Device Drivers in I/O manual */
static quad_desc		sensedsc = {SIZEOF(sensebuf), sensebuf};
static unsigned char		xmtbuf[XMTBUFLEN], *xmtbufptr = NULL;
static struct iosb_struct	read_iosb, write_iosb;
static uint4			ethchan;	/* returned ethernet channel # (only low-order word used) */
static unsigned char		destination_address[ETHERADDR_LENGTH];
static unsigned char		my_ether_addr[ETHERADDR_LENGTH];
static boolean_t		input_pending = FALSE;
static unsigned char		max_ddp_request_credits = DDP_MAX_REQUEST_CREDITS;
/*	End of ethernet control information.  */

static struct dsc$descriptor_s ethdsc[] =
{
	{STR_LIT_LEN("ECA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "ECA0"},
	{STR_LIT_LEN("ESA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "ESA0"},
	{STR_LIT_LEN("ETA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "ETA0"},
	{STR_LIT_LEN("EWA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "EWA0"},
	{STR_LIT_LEN("EXA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "EXA0"},
	{STR_LIT_LEN("EZA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "EZA0"},
	{STR_LIT_LEN("XEA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "XEA0"},
	{STR_LIT_LEN("XQA0"), DSC$K_DTYPE_T, DSC$K_CLASS_S, "XQA0"}
};

condition_code decddp_init(void)
{
	condition_code		status;
	char			*sbp, logical_buffer[MAX_TRANS_NAME_LEN], translation_buffer[MAX_TRANS_NAME_LEN];
	mstr			logical, translation;
	int4			eth_recv_bufcnt, credits;
	struct dsc$descriptor_s	ethdsc1;
	int			eth_index;

	assert(0 == (SIZEOF(xmtbuf) & 1)); /* even buffer size for padding odd length outbound message */
	logical.addr = logical_buffer;
	memcpy(logical.addr, DDP_ETHER_DEV_PREFIX, STR_LIT_LEN(DDP_ETHER_DEV_PREFIX));
	memcpy(&logical.addr[STR_LIT_LEN(DDP_ETHER_DEV_PREFIX)], my_circuit_name.addr, my_circuit_name.len);
	logical.len = STR_LIT_LEN(DDP_ETHER_DEV_PREFIX) + my_circuit_name.len;
	if (SS$_NORMAL == trans_log_name(&logical, &translation, translation_buffer))
	{
		ethdsc1.dsc$w_length = translation.len;
		ethdsc1.dsc$b_dtype = DSC$K_DTYPE_T;
		ethdsc1.dsc$b_class = DSC$K_CLASS_S;
		ethdsc1.dsc$a_pointer = translation.addr;
		status = SYS$ASSIGN(&ethdsc1, &ethchan, 0, 0);
	} else
		status = SS$_NOSUCHDEV;

	for (eth_index = 0;
	     (SS$_NOSUCHDEV == status) && (SIZEOF(ethdsc) / SIZEOF(struct dsc$descriptor_s) > eth_index);
	     status = SYS$ASSIGN(&ethdsc[eth_index++], &ethchan, 0, 0));
	if (0 != (status & 1))	/* if any of the SYS$ASSIGN's succeeded */
	{
		logical.addr = logical_buffer;
		memcpy(logical.addr, DDP_ETH_RCV_BUFCNT_PREFIX, STR_LIT_LEN(DDP_ETH_RCV_BUFCNT_PREFIX));
		memcpy(&logical.addr[STR_LIT_LEN(DDP_ETH_RCV_BUFCNT_PREFIX)], my_circuit_name.addr, my_circuit_name.len);
		logical.len = STR_LIT_LEN(DDP_ETH_RCV_BUFCNT_PREFIX) + my_circuit_name.len;
		if (SS$_NORMAL == trans_log_name(&logical, &translation, translation_buffer) && 0 < translation.len)
		{
			eth_recv_bufcnt = asc2i((uchar_ptr_t)translation_buffer, translation.len);
			if (MIN_ETH_RECV_BUFCNT > eth_recv_bufcnt || MAX_ETH_RECV_BUFCNT < eth_recv_bufcnt)
				eth_recv_bufcnt = ETH_RCV_BUFCNT;
			assert(NMA$C_PCLI_BFN == setparm[0].parm_id);
			setparm[0].parm_val = eth_recv_bufcnt;		/* OK to overwrite static since once per run */
		}
		status = SYS$QIOW(EFN$C_ENF, ethchan, (IO$_SETMODE | IO$M_CTRL | IO$M_STARTUP), &write_iosb, 0, 0,
                                  0, &setparmdsc, 0, 0, 0, 0);
		if (0 != (status & 1))
			status = write_iosb.status;
		if (0 != (status & 1))
		{
			status = SYS$QIOW(EFN$C_ENF, ethchan, (IO$_SENSEMODE | IO$M_CTRL), &write_iosb, 0, 0, 0, &sensedsc,
					  0, 0, 0, 0);
			if (0 != (status & 1))
				status = write_iosb.status;
			if (0 != (status & 1))
			{
				/* Locate the PHA parameter.  */
				status = SS$_NOSUCHDEV;		/* default in case we don't find PHA parameter */
				for (sbp = sensebuf;  sbp < sensebuf + SIZEOF(sensebuf);)
				{
					if (0 == (*(short *)sbp & 0x1000))	/* if not string parameter, must be longword */
						sbp += SIZEOF(short) + SIZEOF(int4);	/* skip longword parameter */
					else if (NMA$C_PCLI_PHA != (*(short *)sbp & 0x0fff))	/* compare without flag bits */
					{
						sbp += SIZEOF(short);			/* skip to string count */
						sbp += SIZEOF(short) + *(short *)sbp;	/* skip over string count and string */
					} else	/* must be NMA$C_PCLI_PHA */
					{
						sbp += 2 * SIZEOF(short);	/* skip over parameter ID and string count */
						memcpy(my_ether_addr, sbp, ETHERADDR_LENGTH);	/* get ethernet address */
						sbp += ETHERADDR_LENGTH;
						status = decddp_start_ast();
						break;
					}
				}
				logical.addr = logical_buffer;
				memcpy(logical.addr, DDP_MAXREQCREDITS_PREFIX, STR_LIT_LEN(DDP_MAXREQCREDITS_PREFIX));
				memcpy(&logical.addr[STR_LIT_LEN(DDP_MAXREQCREDITS_PREFIX)], my_circuit_name.addr,
												my_circuit_name.len);
				logical.len = STR_LIT_LEN(DDP_MAXREQCREDITS_PREFIX) + my_circuit_name.len;
				if (SS$_NORMAL == trans_log_name(&logical, &translation, translation_buffer)
							&& 0 < translation.len)
				{
					credits = asc2i((uchar_ptr_t)translation_buffer, translation.len);
				      	if (DDP_LEAST_MAXREQCREDITS <= credits && DDP_LARGEST_MAXREQCREDITS >= credits)
						max_ddp_request_credits = (unsigned char)credits;
				}
			}
		}
	}
	return status;
}

condition_code decddp_start_ast(void)
{ /* Enable attention AST.  */
	return SYS$QIOW(EFN$C_ENF, ethchan, (IO$_SETMODE | IO$M_ATTNAST), &read_iosb, 0, 0, decddp_ast_handler, 0, 0, 0, 0, 0);
}


void decddp_ast_handler(void)
{
	input_pending = TRUE;
	SYS$WAKE(0, 0);
	return;
}

boolean_t dcp_get_input_buffer(struct in_buffer_struct *input_buffer, size_t inbufsiz)
{
	bool		retval;
	condition_code	status;

	if (FALSE == input_pending)
		status = 0;
	else
	{
		input_pending = FALSE;
		status = SYS$QIOW(EFN$C_ENF, ethchan, IO$_READVBLK, &read_iosb, 0, 0, &input_buffer->dh,
				  inbufsiz - offsetof(struct in_buffer_struct, dh), 0, 0, input_buffer->fh.destination_address, 0);
		input_buffer->fh.frame_length = read_iosb.length;
		/* At this point, input_buffer should point to:
			2 bytes containing the number of bytes read	(in_buffer_struct.len)
			14 bytes of Ethernet packet header information	(in_buffer_struct.fh)
			data starting at byte 16			(in_buffer_struct.dh followed by ddp message contents)
		*/
		if (ddp_trace)
			ddp_trace_output(&input_buffer->dh, input_buffer->fh.frame_length, DDP_RECV);
		status = decddp_start_ast();
	}
	return (0 != (status & 1));
}

/* decddp_shdr - initialize transmit buffer with DDP packet header */
void decddp_shdr(unsigned char trancode, short jobno, unsigned short remote_circuit, short return_jobno, unsigned char msgno,
		 unsigned char *etheraddr)
/* trancode is DDP transaction code */
/* jobno is job number */
/* remote_circuit is 5-bit format of remote circuit name.  However, a 0001 if this is a multicast announcement.  */
/* return_jobno is for response, job number of client; for request, 0 */
/* msgno is message number */
/* etheraddr is pointer to destination ethernet address (6 bytes) */
{
	ddp_hdr_t	*dp;

	assert(SIZEOF(xmtbuf) - 1 > DDP_MSG_HDRLEN); /* buffer overflow check; - 1 to accommodate terminator byte */
	dp = (ddp_hdr_t *)xmtbuf;
	dp->trancode			= trancode;		/* transaction code */
	dp->proto			= DDP_PROTO_VERSION;
	dp->source_circuit_name		= my_circuit; 		/* 5-bit format of local circuit name */
	dp->source_job_number		= jobno;		/* job number << 1 */
	dp->remote_circuit_name		= remote_circuit;	/* 5-bit format of remote circuit name */
	dp->remote_job_number		= return_jobno;		/* response: $J; request: 0 */
	dp->message_number		= msgno;		/* sequence number; must be in order */
	dp->filler1			= 0x00;			/* literal 00 */
	/* leave message_length unset; filled later */
	dp->hdrlen			= DDP_MSG_HDRLEN;	/* DDP header length */

	xmtbufptr = &xmtbuf[DDP_MSG_HDRLEN];	/* first free position in buffer */
	memcpy(destination_address, etheraddr, ETHERADDR_LENGTH);
	return;
}

void decddp_set_etheraddr(unsigned char *adr)
{ /* set up Ethernet address */
	memcpy(destination_address, adr, ETHERADDR_LENGTH);
	return;
}

unsigned char *decddp_s5bit(unsigned char *cp)
{ /* convert 3 bytes to '5-bit format' and store in the transmit buffer; return pointer to next free transmit buffer location */
	*((unsigned short *)xmtbufptr)++ = five_bit(cp);
	return xmtbufptr;
}

unsigned char *decddp_s5asc(unsigned short fivebit)
{ /* convert 5-bit format number to ASCII and store in the transmit buffer; return pointer to next free transmit buffer location */
	xmtbufptr = five_2_ascii(&fivebit, xmtbufptr);
	return xmtbufptr;
}

unsigned char *decddp_s7bit(unsigned char *cp)
{ /* convert zero terminated string to '7-bit format' and store in the transmit buffer;
     return pointer to next free transmit buffer location
   */
	unsigned char	*mycp;

	if ('\0' != *cp)
	{ /* Copy characters, shifting low-order 7 bits to high-order 7 bits and set low-order bit.  */
		for (mycp = cp;  '\0' != *mycp;  mycp++)
			*xmtbufptr++ = ((*mycp << 1) & 0xff) | 0x01;
		*(xmtbufptr - 1) &= 0xfe;	/* clear low order bit of last byte signifying end of string */
						/* N.B., shortest allowable string is zero bytes with no terminator */
	}
	return xmtbufptr;
}


unsigned char *decddp_s8bit(unsigned char *cp)
{ /* transcribe zero terminated string to the transmit buffer; return pointer to next free transmit buffer location */
	unsigned char	*mycp;

	mycp = cp;
	do	/* just copy the bytes, including terminating zero byte; shortest string is 1 byte (terminator) */
		*xmtbufptr++ = *mycp++;
	while ('\0' != *mycp);

	return xmtbufptr;
}

void decddp_s8bit_counted(char *cp, int len)
{
	if (len)
		memcpy(xmtbufptr, cp, len);
	xmtbufptr += len;
	return;
}

void decddp_putbyte(unsigned char ch)		/* note the VAX MACRO version declared this of type (char *), but never set r0! */
{ /* copy single character to transmit buffer */

	*xmtbufptr++ = ch;
	return;
}

condition_code decddp_send(void)
{ /* transmit message */

	return(dcp_send_message(xmtbuf, (xmtbufptr - xmtbuf), &write_iosb));
}

condition_code dcp_send_message(unsigned char *buffer, int length, struct iosb_struct *iosbp)
{
	int		buflen, retry;
	condition_code	status;
	error_def(ERR_DDPOUTMSG2BIG);

	((ddp_hdr_t *)buffer)->message_length = buflen = length;
	if (0 != (buflen & 1))
		buffer[buflen++] = 0xff;	/* pad to even number of bytes with 0xff */
	if (buflen < DDP_MIN_MSG_LEN)
		buflen = DDP_MIN_MSG_LEN;	/* pad to min len */
	if (ddp_trace)
		ddp_trace_output(buffer, buflen, DDP_SEND);
	if (MAX_ETHER_DATA_SIZE < buflen)
		return ERR_DDPOUTMSG2BIG;
	for (retry = 0, status = 0; (0 == (status & 1)) && (RETRY_LIMIT > retry); retry++)
	{
		status = SYS$QIOW(EFN$C_ENF, ethchan, IO$_WRITEVBLK, iosbp, 0, 0, buffer, buflen, 0, 0, &destination_address, 0);
		if (0 != (status & 1))
			status = iosbp->status;
	}
	return status;
}

void decddp_sinit(unsigned char *response_code)
{ /* DDP session initialization message (must have called decddp_shdr() first) */
	ddp_announce_msg_t	*ap;
	int			volset_index;

	assert(DDP_ANNOUNCE_MSG_LEN <= SIZEOF(xmtbuf) - (xmtbufptr - xmtbuf)); /* buffer overflow check */
	ap = (ddp_announce_msg_t *)xmtbufptr; /* xmtbufptr points to the byte past the header */

	ap->filler0 = 0;
	ap->code[0] = response_code[0];
	ap->code[1] = response_code[1];
	memcpy(ap->ether_addr, my_ether_addr, ETHERADDR_LENGTH);
	ap->circuit_name = my_circuit;
	ap->filler1 = 0;
	ap->filler2 = 0;
	ap->max_job_no = MAX_USERS_PER_NODE;
	ap->group_mask = my_group_mask;
	ap->advertise_interval = DDP_ADVERTISE_INTERVAL;
	ap->max_request_credits = max_ddp_request_credits;	/* used for flow control */
	ap->cpu_type = DDP_CPU_TYPE;
	ap->version = DDP_SOFTWARE_VERSION;
	ap->cpu_load_rating = DDP_CPU_LOAD_RATING;
	ap->proto_version = DDP_PROTO_VERSION;
	ap->node_status = DDP_NODE_STATUS_ALL_CLEAR;
	ap->autoconfigure_version = DDP_AUTOCONFIGURE_VERSION;
	for (volset_index = 0; volset_index < DDP_MAX_VOLSETS; volset_index++)		/* copy volset names to the buffer */
		ap->volset[volset_index] = volset_table[volset_index].vol;
	/*********************** BEGIN STUFF THAT IS YET TO BE UNDERSTOOD ************************/
	memset(ap->filler3, 0, SIZEOF(ap->filler3));
	*((uint4 *)&ap->filler3[32]) = DDP_ANNOUNCE_FILLER3_LITERAL;
	/*********************** END   STUFF THAT IS YET TO BE UNDERSTOOD ************************/
	ap->terminator = DDP_MSG_TERMINATOR;

	xmtbufptr += DDP_ANNOUNCE_MSG_LEN;
	return;
}

unsigned char *decddp_xmtbuf(void)
{
	return xmtbuf;
}

int decddp_bufavail(void)
{
	return SIZEOF(xmtbuf) - (xmtbufptr - xmtbuf);
}
