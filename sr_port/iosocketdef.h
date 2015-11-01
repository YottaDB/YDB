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

#ifndef __IOSOCKETDEF_H__
#define __IOSOCKETDEF_H__

/* iosocketdef.h */

/* one socket device may have more than one socket associate with it.
 * one socket may have more than one delimiter associate with it.
 */

#include <sys/types.h>
#include <netinet/in.h>

#define MAX_HANDLE_LEN			64
#define MAX_N_SOCKET			64
#define MAX_N_DELIMITER			64
#define MAX_DELIM_LEN			64
#define DEFAULT_LISTEN_DEPTH		1
#define	DEFAULT_SOCKET_BUFFER_SIZE	0x400
#define	MAX_SOCKET_BUFFER_SIZE		0x100000
#define	MAX_INTERNAL_SOCBUF_SIZE	0x100000

enum socket_state
{
	socket_connected,
	socket_listening,
	socket_bound,
	socket_created
};

enum socket_protocol
{
	socket_tcpip,
	socket_spx,
	n_socket_protocol
};

typedef struct socket_address_type
{
	struct sockaddr_in      	sin;     /* accurate one */
	unsigned short                  port;
	char            		saddr_ip[SA_MAXLEN];
	char				saddr_lit[SA_MAXLITLEN];
} socket_address;

typedef struct socket_struct_type
{
	int 				sd; 	/* socket descriptor */
	struct 	d_socket_struct_type	*dev; 	/* point back to the driver */
	bool				passive,
					ioerror,
					urgent;
	enum socket_state		state;
	enum socket_protocol		protocol;
	socket_address			local,
					remote;
	unsigned char			lastop;
        char                            handle[MAX_HANDLE_LEN];
        short                           handle_len;
	int				bufsiz;	/* OS internal buffer size */
        int4                            n_delimiter;
        mstr                            delimiter[MAX_N_DELIMITER];
	size_t				buffer_size;			/* size of the buffer for this socket */
	size_t				buffered_length;		/* length of stuff buffered for this socket */
	size_t				buffered_offset;		/* offset of the buffered stuff to buffer head */
	char				*buffer;			/* pointer to the the buffer of this socket */
	boolean_t			nodelay;
} socket_struct;

typedef struct d_socket_struct_type
{
	int4				current_socket;			/* current socket index */
	int4				n_socket;			/* number of sockets	*/
        char                            dollar_device[DD_BUFLEN];
	char				dollar_key[DD_BUFLEN];
	struct socket_struct_type 	*socket[MAX_N_SOCKET];
}d_socket_struct;

boolean_t iosocket_bind(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz);
boolean_t iosocket_connect(socket_struct *socketptr, int4 timepar, boolean_t update_bufsiz);
boolean_t iosocket_delimiter(char *delimiter_buffer, unsigned char delimiter_blen, 	 socket_struct *socketptr, boolean_t rm);
boolean_t iosocket_switch(char *handle, short handle_len, d_socket_struct *from, 	 d_socket_struct *to);
int4 iosocket_handle(char *handle, short *len, boolean_t newhandle, 	 d_socket_struct *dsocketptr);
socket_struct *iosocket_create(char *sockaddr, uint4 bfsize);
ssize_t iosocket_snr(socket_struct *socketptr, void *buffer, size_t maxlength, int flags, 	 ABS_TIME *time_for_read);

#endif
