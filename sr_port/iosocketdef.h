/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_socket.h" /* for using sockaddr_storage */
#include "iotcpdef.h"

#ifndef GTM_MB_LEN_MAX
#include "gtm_utf8.h"
#endif

/* Debugging notes: Some debuging calls as as below. Note that DBGSOCK2 is *always* disabled.
 * disabled. As parts of the code work, the DBGSOCK calls are changed to DBGSOCK2 to get
 * them out of the way without removing them (they may be useful in the future).
 *
 * Uncomment the define for DEBUG_SOCK below to enable DBGSOCK() debugging
 */
/* #define DEBUG_SOCK */
#ifdef DEBUG_SOCK
#  define DBGSOCK(X) DBGFPF(X)
#  define DBGSOCK_ONLY(X) X
#else
#  define DBGSOCK(X)
#  define DBGSOCK_ONLY(X)
#endif
#define DBGSOCK2(X)
#define DBGSOCK_ONLY2(X)

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
#define	MAX_INTERNAL_SOCBUF_SIZE	0x100000
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

#define	ONE_COMMA			"1,"

#define SOCKERROR(iod, socketptr, gtmerror, syserror) 							\
{ 													\
	int	errlen; 										\
	char	*errptr; 										\
	iod->dollar.za = 9; 										\
	memcpy(iod->dollar.device, ONE_COMMA, SIZEOF(ONE_COMMA)); 					\
	errptr = (char *)STRERROR(syserror); 								\
	errlen = STRLEN(errptr); 									\
	memcpy(&iod->dollar.device[SIZEOF(ONE_COMMA) - 1], errptr, errlen + 1); /* + 1 for null */ 	\
	assert(ERR_SOCKWRITE == gtmerror);								\
	UNIX_ONLY(if (iod == io_std_device.out)								\
		{											\
			if (!prin_out_dev_failure)							\
				prin_out_dev_failure = TRUE;						\
			else										\
			{										\
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOPRINCIO);			\
				stop_image_no_core();							\
			}										\
		})											\
	if (socketptr->ioerror) 									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) gtmerror, 0, ERR_TEXT, 2, errlen, errptr); 	\
}

#define SOCKET_ALLOC(SOCKPTR)										\
{													\
	SOCKPTR = (socket_struct *)malloc(SIZEOF(socket_struct));					\
	memset(SOCKPTR, 0, SIZEOF(socket_struct));							\
}

#define SOCKET_ADDR(SOCKPTR, SOCKEND)										\
	((sockaddr_ptr)(SOCKPTR->SOCKEND.sa									\
		? SOCKPTR->SOCKEND.sa										\
		: (SOCKPTR->SOCKEND.sa = (struct sockaddr *)malloc(SIZEOF(struct sockaddr_storage)))))

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

#define SOCKET_FREE(SOCKPTR)							\
{										\
	if (NULL != SOCKPTR->buffer) 						\
		free(SOCKPTR->buffer);						\
	if (NULL != SOCKPTR->zff.addr)						\
		free(SOCKPTR->zff.addr);					\
	if (NULL != SOCKPTR->local.sa)						\
		free(SOCKPTR->local.sa);					\
	if (NULL != SOCKPTR->remote.sa)						\
		free(SOCKPTR->remote.sa);					\
	if (NULL != SOCKPTR->local.saddr_ip)					\
		free(SOCKPTR->local.saddr_ip);					\
	if (NULL != SOCKPTR->remote.saddr_ip)					\
		free(SOCKPTR->remote.saddr_ip);					\
	iosocket_delimiter((unsigned char *)NULL, 0, SOCKPTR, TRUE);		\
	free(SOCKPTR);								\
}

#define SOCKET_DUP(SOCKPTR, NEWSOCKPTR)										\
{														\
	NEWSOCKPTR = (socket_struct *)malloc(SIZEOF(socket_struct));						\
	*NEWSOCKPTR = *SOCKPTR;											\
	if (NULL != SOCKPTR->buffer) 										\
		NEWSOCKPTR->buffer = (char *)malloc(SOCKPTR->buffer_size);					\
	if (NULL != SOCKPTR->zff.addr)										\
	{													\
		NEWSOCKPTR->zff.addr = (char *)malloc(MAX_ZFF_LEN);						\
		memcpy(NEWSOCKPTR->zff.addr, SOCKPTR->zff.addr, SOCKPTR->zff.len);				\
	}													\
	if (NULL != SOCKPTR->local.sa)										\
	{													\
		NEWSOCKPTR->local.sa = (struct sockaddr *)malloc(SOCKPTR->local.ai.ai_addrlen);			\
		memcpy(NEWSOCKPTR->local.sa, SOCKPTR->local.sa, SOCKPTR->local.ai.ai_addrlen);			\
	}													\
	if (NULL != SOCKPTR->remote.sa)										\
	{													\
		NEWSOCKPTR->remote.sa = (struct sockaddr *)malloc(SOCKPTR->remote.ai.ai_addrlen);		\
		memcpy(NEWSOCKPTR->remote.sa, SOCKPTR->remote.sa, SOCKPTR->remote.ai.ai_addrlen);		\
	}													\
	if (NULL != SOCKPTR->local.saddr_ip)									\
		STRNDUP(SOCKPTR->local.saddr_ip, SA_MAXLEN, NEWSOCKPTR->local.saddr_ip);			\
	if (NULL != SOCKPTR->remote.saddr_ip)									\
		STRNDUP(SOCKPTR->remote.saddr_ip, SA_MAXLEN, NEWSOCKPTR->remote.saddr_ip);			\
	iosocket_delimiter_copy(SOCKPTR, NEWSOCKPTR);								\
}

enum socket_state
{
	socket_connected,
	socket_listening,
	socket_bound,
	socket_created,
	socket_connect_inprogress
};

enum socket_protocol
{
	socket_tcpip,
	socket_spx,
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
	struct sockaddr		      	*sa;
	struct addrinfo			ai;
	struct addrinfo			*ai_head; /* store the head of addrinfo linked list */
	unsigned short                  port;
	char            		*saddr_ip;
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
        char                            handle[MAX_HANDLE_LEN];
        int                             handle_len;
	int				bufsiz;				/* OS internal buffer size */
        int	  	                n_delimiter;
	int				last_recv_errno;
        mstr                            delimiter[MAX_N_DELIMITER];
        mstr                            idelimiter[MAX_N_DELIMITER];
	mstr				odelimiter0;
	size_t				buffer_size;			/* size of the buffer for this socket */
	size_t				buffered_length;		/* length of stuff buffered for this socket */
	size_t				buffered_offset;		/* offset of the buffered stuff to buffer head */
	char				*buffer;			/* pointer to the the buffer of this socket */
	boolean_t			nodelay;
	boolean_t			first_read;
	boolean_t			first_write;
	boolean_t			def_moreread_timeout;	/* true if deviceparameter morereadtime defined in open or use */
	mstr				zff;
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
} socket_interrupt;

typedef struct d_socket_struct_type
{
	socket_interrupt		sock_save_state;		/* Saved state of interrupted IO */
	boolean_t                     	mupintr;			/* We were mupip interrupted */
	int4				current_socket;			/* current socket index */
	int4				n_socket;			/* number of sockets	*/
	struct io_desc_struct		*iod;				/* Point back to main IO descriptor block */
	struct socket_struct_type 	*socket[1];			/* Array size determined by gtm_max_sockets */
} d_socket_struct;

boolean_t iosocket_bind(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz);
boolean_t iosocket_connect(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz);
boolean_t iosocket_delimiter(unsigned char *delimiter_buffer, int4 delimiter_len, socket_struct *socketptr, boolean_t rm);
void iosocket_delim_conv(socket_struct *socketptr, gtm_chset_t to_chset);
void iosocket_delimiter_copy(socket_struct *from, socket_struct *to);
boolean_t iosocket_switch(char *handle, int handle_len, d_socket_struct *from, d_socket_struct *to);
int4 iosocket_handle(char *handle, int *len, boolean_t newhandle, d_socket_struct *dsocketptr);
socket_struct *iosocket_create(char *sockaddr, uint4 bfsize, int file_des);
ssize_t iosocket_snr(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, ABS_TIME *time_for_read);
void iosocket_unsnr(socket_struct *socketptr, unsigned char *buffer, size_t len);
ssize_t iosocket_snr_utf_prebuffer(io_desc *iod, socket_struct *socketptr, int flags, ABS_TIME *time_for_read,
				   boolean_t wait_for_input);
void iosocket_write_real(mstr *v, boolean_t convert_output);
void iosocket_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend);
#endif
