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

#ifndef DDPCOM_H_INCLUDED
#define DDPCOM_H_INCLUDED

struct iosb_struct
{
	unsigned short	status;
	unsigned short	length;
	uint4		devinfo;
};

struct queue_entry
{
	int4 fl;
	int4 bl;
};

typedef struct com_slot
{
	struct queue_entry	q;
	struct iosb_struct	iosb;
	uint4			pid;
	short			len;
	short			state;	/* 1 means no message for client, 0 means message is ready for client */
	char			text[1]; /* actually, ddp_slot_size bytes */
} com_slot_t;

/* The state flag is actually used to mean that the client may proceed to reuse the message space.
   Once the client places a message in the slot, the flag remains 1 until the server has returned
   a reply.  It is necessary to check that the server has replied as there is no mutex protecting the
   message slot.  If the client aborts prior to the server replying, and were to then start a new
   transaction, placing a new message in the slot, the server's reply to the previous request could
   overlay the new message.
*/

typedef struct com_hdr
{
	struct queue_entry	unused_slots;
	struct queue_entry	outbound_pending;
	uint4			server_pid;
	int4			filler;	/* we must be quad-word aligned */
	com_slot_t		slot[1];
} com_hdr_t;

#define MAXIMUM_CIRCUITS	32
#define MAX_USERS_PER_NODE 	MAXIMUM_PROCESSES /* Only MAXIMUM_PROCESSES supported at this time */

typedef struct
{
	unsigned short	circuit_name;
	char		ether_addr[ETHERADDR_LENGTH];
	unsigned char	incoming_users[MAX_USERS_PER_NODE];
	unsigned char	outgoing_users[MAX_USERS_PER_NODE];
} routing_tab;

typedef struct
{
	unsigned short volset_name;
	unsigned short circuit_name;
} circuit_tab;

typedef struct uci_gld_pair_struct
{
	unsigned short			uci;
	mstr				gld;
	struct uci_gld_pair_struct	*next;
} uci_gld_pair;

typedef struct
{
	unsigned short	vol;
	uci_gld_pair	*ug;
} volset_tab;

#define VUG_CONFIG_COMMENT_CHAR '#'

typedef union
{
	uint4 auxid;
	struct
	{
		short circuit;
		short job;
	}as;
} auxvalue;

#define DDP_XMIT_FAIL		"<DDP_ETHER_IO_FAIL>"
#define DDP_MSG2BIG		"<DDP_MSG2BIG>"
#define DDP_AGENT_BUFF_NAME	"GTMDDP$AGENTBUFF$XXX" /* XXX - DDP_CIRCUIT_NAME_LEN characters that will be filled in
							* by init_section */

#endif /* DDPCOM_H_INCLUDED */
