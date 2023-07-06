/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IOSOCKETDEF_H
#define IOSOCKETDEF_H

/* iosocketdef.h */

/* one socket device may have more than one socket associate with it.
 * one socket may have more than one delimiter associate with it.
 */

#include <sys/types.h>
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include "gtm_un.h"
#include "min_max.h"
#include "gtm_socket.h" /* for using sockaddr_storage */
#ifndef GTM_MB_LEN_MAX
#include "gtm_utf8.h"
#endif

/* values for lastop in socket_struct */

#define TCP_NOOP		0
#define TCP_WRITE		1
#define TCP_READ		2

/* bits for readyforwhat in socket_struct */
#define SOCKREADY_READ		1
#define SOCKREADY_WRITE		2

/* bits for pendingevent in socket_struct */
#define SOCKPEND_READ		1	/* from poll */
#define SOCKPEND_WRITE		2
#define SOCKPEND_BUFFER		4	/* something in input buffer */

/* bits for options_state in socket_struct */
#define SOCKOPTIONS_SYSTEM	1	/* value from getsockopt */
#define SOCKOPTIONS_USER	2	/* value specified by user */
#define SOCKOPTIONS_PENDING	4	/* setsockopt needed */
#define	SOCKOPTIONS_FROM_STRUCT	-1	/* flag for iosocket_tcp_keepalive.c */

typedef struct
{
	uid_t   mem;
	gid_t   grp;
} uic_struct_int;

/* Debugging notes: Some debuging calls as as below. Note that DBGSOCK2 is *always* disabled.
 * disabled. As parts of the code work, the DBGSOCK calls are changed to DBGSOCK2 to get
 * them out of the way without removing them (they may be useful in the future).
 *
 * Uncomment the define for DEBUG_SOCK below to enable DBGSOCK() debugging
 */
/* #define DEBUG_SOCK */
#ifdef DEBUG_SOCK
#  include "gtmio.h"
#  define DBGSOCK(X) DBGFPF(X)
#  define DBGSOCK_ONLY(X) X
#else
#  define DBGSOCK(X)
#  define DBGSOCK_ONLY(X)
#endif
#ifdef DEBUG_SOCKWAIT
#  define DBGSOCKWAIT(X) DBGFPF(X)
#  define DBGSOCKWAIT_ONLY(X) X
#else
#  define DBGSOCKWAIT(X)
#  define DBGSOCKWAIT_ONLY(X)
#endif
/* #define DEBUG_SOCK2 */
#ifdef DEBUG_SOCK2
#  include "gtmio.h"
#  define DBGSOCK2(X) DBGFPF(X)
#  define DBGSOCK_ONLY2(X) X
#else
#  define DBGSOCK2(X)
#  define DBGSOCK_ONLY2(X)
#endif

/* About the length of the delimiter string. While we are allocating lots of space here for the maximum representation
 * of 64 delimiters each of 64 chars MB chars, the fact is that the iop option processing actually limits the string
 * containing all the delimiters to 255 bytes by its nature of having single byte length field imbedded in the buffer
 * stream. When the iop processing is modified to handle a larger string, these options will be useful. But for right
 * now, they are way overkill..
 */
#define MAX_N_SOCKET			64		/* Initial default for gtm_max_sockets in gbldefs.c */
#define MAX_MAX_N_SOCKET		(1024 * 1024)	/* Values higher than this absurd value are most likely wrong */
#define MAX_N_DELIMITER			64
#define MAX_DELIM_LEN			(MAX_N_DELIMITER * GTM_MB_LEN_MAX)  /* worst case byte length for 64 UTF-8 characters */

#define MAX_HANDLE_LEN			(64 * GTM_MB_LEN_MAX)		    /* worst case byte length for 64 UTF-8 characters */
#define MAX_ZFF_LEN			(64 * GTM_MB_LEN_MAX)		    /* worst case byte length for 64 UTF-8 characters */
#define DEFAULT_LISTEN_DEPTH		1
#define	DEFAULT_SOCKET_BUFFER_SIZE	0x400
#define	MAX_SOCKET_BUFFER_SIZE		0x100000
/* Next three fields relate to the time that a variable length unterminated read will wait to see
 * if there is more input coming in before it gives up and returns what it has to the user. This
 * time is specified in milliseconds. This value used to be 200ms but that was deemed too long on
 * modern systems yet now the user can change it if they wish to. Tradeoffs are longer waits for
 * variable reads versus potential CPU burner if the value gets too low.  The implementation now
 * waits INITIAL_MOREREAD_TIMEOUT time for the first read to occur and then switches to the
 * DEFAULT_MOREREAD_TIMEOUT.  This keeps CPU usage low during the potentially long period prior to
 * reading some data, while being more responsive for subsequent reads.
 */
#define INITIAL_MOREREAD_TIMEOUT	200
#define DEFAULT_MOREREAD_TIMEOUT	10
#define MAX_MOREREAD_TIMEOUT		999
/* For buffered output, wait this long for socket to be ready to output */
#define DEFAULT_WRITE_WAIT		200

#define SOCKWRTERROR(IOD, SOCKPTR, GTMERR, SYSERR, ERRORTEXT, TLSLIT) 						\
MBSTART {													\
	int	ERRLEN; 											\
	char	*ERRPTR; 											\
	IOD->dollar.za = ZA_IO_ERR; 										\
														\
	ISSUE_NOPRINCIO_IF_NEEDED(IOD, TRUE, !SOCKPTR->ioerror);	/* TRUE indicates WRITE */		\
	assert((0 != SYSERR) || (NULL != ERRORTEXT));								\
	if (NULL == ERRORTEXT)											\
		ERRPTR = (char *)STRERROR(SYSERR); 								\
	else													\
		ERRPTR = (char *)ERRORTEXT;									\
	SET_DOLLARDEVICE_ONECOMMA_ERRSTR(IOD, ERRPTR, ERRLEN);							\
	assert((ERR_SOCKWRITE == GTMERR) || (ERR_TLSIOERROR == GTMERR));					\
	if (SOCKPTR->ioerror)											\
	{													\
		if (ERR_SOCKWRITE == GTMERR)									\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) GTMERR, 0, ERR_TEXT, 2, ERRLEN, ERRPTR);	\
		else												\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT(TLSLIT),	\
				ERR_TEXT, 2, ERRLEN, ERRPTR);							\
	}													\
} MBEND

#define SOCKET_ALLOC(SOCKPTR)										\
{													\
	SOCKPTR = (socket_struct *)malloc(SIZEOF(socket_struct));					\
	memset(SOCKPTR, 0, SIZEOF(socket_struct));							\
}

#define SOCKET_ADDR(SOCKPTR, SOCKEND)										\
	((sockaddr_ptr)(SOCKPTR->SOCKEND.sa									\
		? SOCKPTR->SOCKEND.sa										\
		: (SOCKPTR->SOCKEND.sa = (struct sockaddr *)malloc(SIZEOF(struct sockaddr_storage)),		\
			memset(SOCKPTR->SOCKEND.sa, 0, SIZEOF(struct sockaddr_storage)), SOCKPTR->SOCKEND.sa)))

#define SOCKET_LOCAL_ADDR(SOCKPTR)	SOCKET_ADDR(SOCKPTR, local)
#define SOCKET_REMOTE_ADDR(SOCKPTR)	SOCKET_ADDR(SOCKPTR, remote)

#define SOCKET_ADDR_COPY(SOCKADDRESS, SOCKADDRPTR, SOCKADDRLEN)			\
{										\
	if (SOCKADDRESS.sa)							\
		free(SOCKADDRESS.sa);						\
	SOCKADDRESS.sa = (struct sockaddr *)malloc(SOCKADDRLEN);		\
	memcpy(SOCKADDRESS.sa, SOCKADDRPTR, SOCKADDRLEN);			\
}

#define SOCKET_AI_TO_ADDR(SOCKPTR, AIPTR, SOCKEND)	SOCKET_ADDR_COPY((SOCKPTR)->SOCKEND, (AIPTR)->ai_addr, (AIPTR)->ai_addrlen)
#define SOCKET_AI_TO_LOCAL_ADDR(SOCKPTR, AIPTR)		SOCKET_AI_TO_ADDR(SOCKPTR, AIPTR, local)
#define SOCKET_AI_TO_REMOTE_ADDR(SOCKPTR, AIPTR)	SOCKET_AI_TO_ADDR(SOCKPTR, AIPTR, remote)

#define SOCKET_ADDRLEN(SOCKPTR, AIPTR, SOCKEND)									\
		(((SOCKPTR)->SOCKEND.sa) ? ((AIPTR)->ai_addrlen) : (SIZEOF(struct sockaddr_storage)))

#define SOCKET_BUFFER_INIT(SOCKPTR, SIZE)					\
{										\
	SOCKPTR->buffer = (char *)malloc(SIZE);					\
	SOCKPTR->buffer_size = SIZE;						\
	SOCKPTR->buffered_length = SOCKPTR->buffered_offset = 0;		\
}

#define SOCKET_OBUFFER_INIT(SOCKETPTR, SIZE, WAIT_TIME, FLUSH_TIME)			\
{											\
	if (0 == SOCKETPTR->obuffer_size)						\
	{										\
		SOCKETPTR->obuffer_size = (0 != SIZE) ? SIZE : SOCKETPTR->buffer_size;	\
		SOCKETPTR->obuffer = (char *)malloc(SOCKETPTR->obuffer_size);		\
		SOCKETPTR->obuffer_length = SOCKETPTR->obuffer_offset = 0;		\
	}										\
	SOCKETPTR->obuffer_wait_time = WAIT_TIME;					\
	SOCKETPTR->obuffer_flush_time = FLUSH_TIME;					\
}

#define SOCKET_FREE(SOCKPTR)							\
{										\
	if (NULL != SOCKPTR)							\
	{									\
		if (NULL != SOCKPTR->buffer) 					\
		{								\
			free(SOCKPTR->buffer);					\
			SOCKPTR->buffer = NULL;					\
		}								\
		if (NULL != SOCKPTR->obuffer) 					\
		{								\
			free(SOCKPTR->obuffer);					\
			SOCKPTR->obuffer = NULL;				\
		}								\
		if (NULL != SOCKPTR->zff.addr)					\
		{								\
			if ((NULL != SOCKPTR->ozff.addr) && (SOCKPTR->ozff.addr != SOCKPTR->zff.addr))			\
				free(SOCKPTR->ozff.addr);			\
			free(SOCKPTR->zff.addr);				\
			SOCKPTR->zff.addr = SOCKPTR->ozff.addr = NULL;		\
		}								\
		if (NULL != SOCKPTR->local.sa)					\
		{								\
			free(SOCKPTR->local.sa);				\
			SOCKPTR->local.sa = NULL;				\
		}								\
		if (NULL != SOCKPTR->remote.sa)					\
		{								\
			free(SOCKPTR->remote.sa);				\
			SOCKPTR->remote.sa = NULL;				\
		}								\
		if (NULL != SOCKPTR->local.saddr_ip)				\
		{								\
			free(SOCKPTR->local.saddr_ip);				\
			SOCKPTR->local.saddr_ip = NULL;				\
		}								\
		if (NULL != SOCKPTR->remote.saddr_ip)				\
		{								\
			free(SOCKPTR->remote.saddr_ip);				\
			SOCKPTR->remote.saddr_ip = NULL;			\
		}								\
		if (NULL != SOCKPTR->parenthandle)				\
		{								\
			free(SOCKPTR->parenthandle);				\
			SOCKPTR->parenthandle = NULL;				\
		}								\
		iosocket_delimiter((unsigned char *)NULL, 0, SOCKPTR, TRUE);	\
		free(SOCKPTR);							\
		SOCKPTR = NULL;							\
	}									\
}

#define SOCKET_DUP(SOCKPTR, NEWSOCKPTR)										\
{														\
	NEWSOCKPTR = (socket_struct *)malloc(SIZEOF(socket_struct));						\
	*NEWSOCKPTR = *SOCKPTR;											\
	if (NULL != SOCKPTR->buffer) 										\
	{													\
		NEWSOCKPTR->buffered_length = NEWSOCKPTR->buffered_offset = 0;					\
		NEWSOCKPTR->buffer = (char *)malloc(SOCKPTR->buffer_size);					\
	}													\
	assert((NULL == SOCKPTR->obuffer) && (0 == SOCKPTR->obuffer_size));					\
	if ((0 != SOCKPTR->zff.len) && (NULL != SOCKPTR->zff.addr))						\
	{													\
		NEWSOCKPTR->zff.addr = (char *)malloc(MAX_ZFF_LEN);						\
		memcpy(NEWSOCKPTR->zff.addr, SOCKPTR->zff.addr, SOCKPTR->zff.len);				\
		if ((NULL != SOCKPTR->ozff.addr) && (SOCKPTR->zff.addr != SOCKPTR->ozff.addr))			\
		{												\
			NEWSOCKPTR->ozff.addr = (char *)malloc(MAX_ZFF_LEN);					\
			memcpy(NEWSOCKPTR->ozff.addr, SOCKPTR->ozff.addr, SOCKPTR->ozff.len);			\
			NEWSOCKPTR->ozff.len = SOCKPTR->ozff.len;						\
		} else												\
			NEWSOCKPTR->ozff = NEWSOCKPTR->zff;							\
	} else													\
	{													\
		NEWSOCKPTR->zff.len = NEWSOCKPTR->ozff.len = 0;							\
		NEWSOCKPTR->zff.addr = NEWSOCKPTR->ozff.addr = NULL;							\
	}													\
	if (NULL != SOCKPTR->local.sa)										\
	{													\
		NEWSOCKPTR->local.sa = (struct sockaddr *)malloc(SOCKPTR->local.ai.ai_addrlen);			\
		memcpy(NEWSOCKPTR->local.sa, SOCKPTR->local.sa, SOCKPTR->local.ai.ai_addrlen);			\
		NEWSOCKPTR->local.ai.ai_addr = NEWSOCKPTR->local.sa;						\
	}													\
	if (NULL != SOCKPTR->remote.sa)										\
	{													\
		NEWSOCKPTR->remote.sa = (struct sockaddr *)malloc(SOCKPTR->remote.ai.ai_addrlen);		\
		memcpy(NEWSOCKPTR->remote.sa, SOCKPTR->remote.sa, SOCKPTR->remote.ai.ai_addrlen);		\
		NEWSOCKPTR->remote.ai.ai_addr = NEWSOCKPTR->remote.sa;						\
	}													\
	if (NULL != SOCKPTR->local.saddr_ip)									\
		STRNDUP(SOCKPTR->local.saddr_ip, SA_MAXLEN, NEWSOCKPTR->local.saddr_ip);			\
	if (NULL != SOCKPTR->remote.saddr_ip)									\
		STRNDUP(SOCKPTR->remote.saddr_ip, SA_MAXLEN, NEWSOCKPTR->remote.saddr_ip);			\
	if (NULL != SOCKPTR->parenthandle)									\
		NEWSOCKPTR->parenthandle = NULL;									\
	iosocket_delimiter_copy(SOCKPTR, NEWSOCKPTR);								\
}

enum socket_pass_type
{
	sockpass_new,
	sockpass_data,
	sockpass_sock
};

#define		ENSURE_DATA_SOCKET(SOCKPTR)					\
{										\
	if (socket_local == (SOCKPTR)->protocol)				\
	{									\
		if (sockpass_new == (SOCKPTR)->passtype)			\
			(SOCKPTR)->passtype = sockpass_data;			\
		else if (sockpass_sock == (SOCKPTR)->passtype)			\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_SOCKPASSDATAMIX, 0);	\
	}									\
}

#define		ENSURE_PASS_SOCKET(SOCKPTR)					\
{										\
	if (socket_local == (SOCKPTR)->protocol)				\
	{									\
		if (sockpass_new == (SOCKPTR)->passtype)			\
			(SOCKPTR)->passtype = sockpass_sock;			\
		else if (sockpass_data == (SOCKPTR)->passtype)			\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_SOCKPASSDATAMIX, 0);	\
	}                                                                      \
}

enum socket_state
{
	socket_connected,
	socket_listening,
	socket_bound,
	socket_created,
	socket_connect_inprogress
};

enum socket_creator
{
	creator_listen,
	creator_accept,
	creator_connect,
	creator_principal,
	creator_passed
};

enum socket_protocol
{
	socket_tcpip,
	socket_spx,
	socket_local,
	n_socket_protocol
};

enum socket_which		/* which module saved the interrupted info */
{
	sockwhich_invalid,
	sockwhich_readfl,
	sockwhich_wait,
	sockwhich_connect
};

typedef struct socket_address_type
{
	struct sockaddr			*sa;
	struct addrinfo			ai;
	struct addrinfo			*ai_head; /* store the head of addrinfo linked list */
	unsigned short			port;
	pid_t				process;	/* for LOCAL passfd */
	char				*saddr_ip;
} socket_address;

typedef struct socket_struct_type
{
	int 				sd; 		/* socket descriptor */
	int				temp_sd;	/* a temp socket descriptor only to test whether IPv6 can be created */
	struct 	d_socket_struct_type	*dev; 		/* point back to the driver */
	boolean_t			passive,
					ioerror,
					urgent,
					delim0containsLF;
	enum socket_state		state;
	enum socket_protocol		protocol;
	socket_address			local,
					remote;
	uint4				lastop;
	uint4				moreread_timeout;		/* timeout to see if more data available (ms) */
	char				handle[MAX_HANDLE_LEN];
	int				handle_len;
	int				bufsiz;				/* OS internal buffer size */
	int				n_delimiter;
	int				last_recv_errno;
	mstr				delimiter[MAX_N_DELIMITER];
	mstr				idelimiter[MAX_N_DELIMITER];
	mstr				odelimiter0;
	size_t				buffer_size;			/* size of the buffer for this socket */
	size_t				buffered_length;		/* length of stuff buffered for this socket */
	size_t				buffered_offset;		/* offset of the buffered stuff to buffer head */
	char				*buffer;			/* pointer to the buffer of this socket */
	boolean_t			nodelay;
	boolean_t			first_read;
	boolean_t			first_write;
	boolean_t			def_moreread_timeout;	/* true if deviceparameter morereadtime defined in open or use */
	enum socket_pass_type		passtype;		/* prevent mix of data and socket passing on LOCAL sockets */
	uint				filemode;		/* for LOCAL */
	uint				filemode_mask;		/* to tell which modes specified */
	uic_struct_int			uic;
	mstr				zff;
	mstr				ozff;			/* UTF-16 if chset is UTF-16 else copy  of zff */
	uint4				lastaction;		/* waitcycle  count */
	uint4				readycycle;		/* when was ready */
	uint4				pendingevent;		/* bitmask, if listening, needs accept */
	enum socket_creator		howcreated;
	char				*parenthandle;		/* listening socket this created from */
	size_t				obuffer_size;		/* size of the output buffer for this socket */
	size_t				obuffer_length;		/* length of output in this buffer */
	size_t				obuffer_offset;		/* offset of the buffered output to buffer head */
	size_t				obuffer_stopped_at;	/* non blocking output blocked at this offset */
	size_t				obuffer_stopped_left;	/* non blocking remaining output */
	volatile boolean_t		obuffer_in_use;		/* obuffer should be used for TLS or non blocking */
	volatile boolean_t		obuffer_timer_set;	/* timer scheduled to flush buffer */
	volatile boolean_t		obuffer_output_active;	/* in buffer output now */
	int				obuffer_flush_time;	/* flush output buffer after this many milliseconds */
	int				obuffer_wait_time;	/* wait for output ready this many milliseconds */
	int				obuffer_errno;		/* save status from timed output attempt */
	char				*obuffer;		/* pointer to the output buffer of this socket */
	boolean_t			nonblocking;		/* socket has been set O_NONBLOCK */
#ifdef GTM_TLS
	boolean_t			tlsenabled;
	void				*tlssocket;		/* actually gtm_tls_socket_t */
	boolean_t			tlsreadblocked;
	boolean_t			tlswriteblocked;
	short				tlspolldirection;	/* what TLS wants */
#endif
	boolean_t			nonblocked_output;	/* default is blocking output */
	boolean_t			output_blocked;		/* if set error all writes */
	int				output_failures;	/* when nonblocked output */
	int				max_output_retries;	/* when nonblocked output */
	int				args_written;		/* number of WRITE arguments output since CLEAR */
	size_t				lastarg_size;		/* number of bytes in last arg processed */
	ssize_t				lastarg_sent;		/* number of bytes actually written */
	int				readyforwhat;		/* bit mask SOCKREADY_READ and/or SOCKREADY_WRITE */
	uint4				current_events;		/* bitmask SOCKPEND_ only within iosocket_wait, see pendingevent */
	struct
	{
		unsigned int		alive	: 3;
		unsigned int		idle	: 3;
		unsigned int		cnt	: 3;
		unsigned int		intvl	: 3;
		unsigned int		sndbuf	: 3;
		unsigned int		rcvbuf	: 3;
	} options_state;
	int				keepalive;		/* SO_KEEPALIVE */
	int				keepidle;		/* TCP_KEEPIDLE */
	int				keepcnt;		/* TCP_KEEPCNT */
	int				keepintvl;		/* TCP_KEEPINTVL */
	int				iobfsize;		/* SO_SNDBUF */
} socket_struct;

typedef struct socket_interrupt_type
{
	ABS_TIME			end_time;
	enum socket_which		who_saved;
	int				max_bufflen;
	int				bytes_read;
	int				chars_read;
	boolean_t                       end_time_valid;
	boolean_t			ibfsize_specified;
	struct d_socket_struct_type	*newdsocket;
	int				wait_for_what;		/* bit mask */
} socket_interrupt;

typedef struct d_socket_struct_type
{
	socket_interrupt		sock_save_state;		/* Saved state of interrupted IO */
	boolean_t                     	mupintr;			/* We were mupip interrupted */
	int4				current_socket;			/* current socket index */
	int4				n_socket;			/* number of sockets	*/
	uint4				waitcycle;			/* count waits */
	boolean_t			ichset_specified;
	boolean_t			ochset_specified;
	gtm_chset_t			ichset_utf16_variant;		/* Save the UTF-16 variant (BE/LE) for reuse */
	gtm_chset_t			ochset_utf16_variant;		/* Save the UTF-16 variant (BE/LE) for reuse */
	struct io_desc_struct		*iod;				/* Point back to main IO descriptor block */
	struct socket_struct_type 	*socket[1];			/* Array size determined by gtm_max_sockets */
} d_socket_struct;

boolean_t iosocket_bind(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz, boolean_t newversion);
boolean_t iosocket_connect(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz);
boolean_t iosocket_delimiter(unsigned char *delimiter_buffer, int4 delimiter_len, socket_struct *socketptr, boolean_t rm);
void iosocket_idelim_conv(socket_struct *socketptr, gtm_chset_t to_chset);
void iosocket_odelim_conv(socket_struct *socketptr, gtm_chset_t to_chset);
void iosocket_delimiter_copy(socket_struct *from, socket_struct *to);
boolean_t iosocket_switch(char *handle, int handle_len, d_socket_struct *from, d_socket_struct *to);
int4 iosocket_handle(char *handle, int *len, boolean_t newhandle, d_socket_struct *dsocketptr);
socket_struct *iosocket_create(char *sockaddr, uint4 bfsize, int file_des, boolean_t listen_specified);
ssize_t iosocket_snr(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read);
void iosocket_unsnr(socket_struct *socketptr, unsigned char *buffer, size_t len);
ssize_t iosocket_snr_utf_prebuffer(io_desc *iod, socket_struct *socketptr, int flags, ABS_TIME *time_for_read,
				   boolean_t wait_for_input);
void iosocket_write_real(mstr *v, boolean_t convert_output);
void iosocket_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend);
boolean_t iosocket_listen_sock(socket_struct *socketptr, unsigned short len);
void iosocket_close_one(d_socket_struct *dsocketptr, int index);
int iosocket_accept(d_socket_struct *dsocketptr, socket_struct *socketptr, boolean_t selectfirst);
ssize_t iosocket_output_buffer(socket_struct *socketptr);
int iosocket_buffer_error(socket_struct *socketptr);
boolean_t iosocket_tcp_keepalive(socket_struct *socketptr, int keepalive_opt, char * act, boolean_t freesocket);
boolean_t iosocket_getsockopt(socket_struct *socketptr, char *optname, int option,
	int level, void *optvalue,  GTM_SOCKLEN_TYPE *optvaluelen, boolean_t freesocket);
int iosocket_setsockopt(socket_struct *socketptr, char *optname, int option,
	int level, void *optvalue, GTM_SOCKLEN_TYPE optvaluelen, boolean_t freesocket);
#ifdef GTM_TLS
void    iosocket_tls(mval *optionmval, int4 timeoutarg, mval *tlsid, mval *password, mval *extraarg);
#endif
#endif
