/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * ---------------------------------------------------
 * CM structures
 * ---------------------------------------------------
 */

#define CM_MSG_BUF_SIZE 	512		/* Message buffer size */
#define MAX_CONN_IND		5	/* Max. simultaneous conn. requests */

#define SERV_PROVIDER		"/dev/tcpip"	/* Service provider */
#define GTCM_SERVER_NAME	"gtcm"	/* Service provider */
#define GTCM_SERVER_PROTOCOL	"tcp"	/* Service provider */

/*
 * Connection States
 */
#define CM_CLB_IDLE		0
#define CM_CLB_READ		1
#define CM_CLB_WRITE		2
#define CM_CLB_CONNECT		3
#define CM_CLB_DISCONNECT	4

#define CMI_CLB_OPENOK          0
#define CMI_CLB_OPENFAIL        1
#define CMI_CLB_OPENREJECT      2

#define CMI_IO_ACCEPT		0
#define CMI_IO_REJECT		1
#define CMI_IO_NODATA		2

#define CM_CONN_ACK		1

/* Mode - ASYNC, SYNC */
#define NO_MODE			0
#define SYNC			1
#define ASYNC			2

/* Protocols supported */
#define OMIL0                   0
#define OMIL1                   1
#define GVCM                    2

#define ALIGN_QUAD _align(quadword)

enum nsn_type
{
	connect_op, discon_op, abort_op, exit_op, pathlost, protocol,
	thirdparty, timeout, netshut, intmsg, reject, confirm,
	nsn_type_count
};

typedef struct
{
	unsigned short msg;
	unsigned short unit;           /* File descriptor for us */
	unsigned char netnum;
	unsigned char netnam[3];
	unsigned char len;
	unsigned char text[3];
} cm_mbx;

typedef struct
{
	unsigned short status;		/* Async I/O completion status */
	unsigned short xfer_count;	/* Amount actually done */
	uint4 dev_info;
} qio_iosb;

typedef struct
{
  unsigned short len;
  char *addr;
} cmi_str;

typedef struct
{
	int4 fl;
	int4 bl;
} relque;

/*
 * Node Connections doubly linked list
 */
struct NTD
{
	relque cqh;		/* forward/backward links */
	int proto_fd;		/* Protocol file descriptor */
	qio_iosb mst;		/* I/O operation status */
	cmi_str mbx;
	int listen_fd;		/* Server's listen file descriptor */
	int (*crq)();		/* add users routine (gtcm_init_ast) */
	void (*err)();		/* Error handler (gtcm_neterr) */
	void (*sht)();		/* shutdown processes routine */
	void (*dcn)();		/* not used */
	void (*mbx_ast)();	/* interrupt messages (gtcm_mbx_read_ast) */
	bool (*acc)();		/* accept connections (gtcm_link_accept) */
	short unsigned stt[nsn_type_count];
};

typedef struct clb_stat_struct
{
	struct
	{
		uint4 msgs,errors,bytes,last_error;
	} read,write;
} clb_stat;

/*
 * Individual link connections doubly linked list
 */
struct CLB
{
	relque cqe;			/* forward/backward links */
	struct NTD *ntd;
	cmi_str nod;
	cmi_str tnd;
	int dch;                        /* endpoint's file descriptor */
	int proto;                      /* Session protocol */
	void *protoptr;                 /* Prototype specfic block */
	int client_pid;			/* Client's process pid */
	void *usr;			/* Connection structure, updated by
					   (gtcm_init_ast()) */
	void (*err)();			/* link status errors */
	qio_iosb ios;
	unsigned short cbl;
	unsigned short mbl;
	unsigned char *mbf;
	unsigned char *cmbf;
	int remain_cnt;			/* # bytes of msg. still to be read */
	unsigned int sta : 4;		/* Connection state, updated by
					   cmi_read, cmi_write, cmj_fini */
	unsigned int io_mode : 2;	/* Mode - ASYNC, SYNC */
	void (*ast)();
	struct clb_stat_struct stt;
};

#define RELQUE2PTR(X) ( ((unsigned char *) &(X)) + ((int4) (X)))

#define PTR2RELQUE(DESTINATION,TARGET) \
	(DESTINATION = (((unsigned char *) &(TARGET)) \
	- ((unsigned char *) &(DESTINATION))))








