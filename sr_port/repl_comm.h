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

#ifndef _REPL_COMM_H
#define _REPL_COMM_H

#define REPL_CONN_RESET(err)	(ECONNRESET == (err) || EPIPE == (err) || EINVAL == (err))

/* To use REPL_SEND_LOOP following variables need to exist:
 *	unsigned char	*msg_ptr;
 *	int		send_len;
 *	int		sent_len;
 *	int		status;
 */
#define REPL_SEND_LOOP(sock_fd, buff, len, timeout) \
	for (msg_ptr = (unsigned char *)(buff), send_len = sent_len = (len); \
	     SS_NORMAL == (status = repl_send(sock_fd, msg_ptr, &sent_len, timeout)) && sent_len < send_len; \
	     msg_ptr += sent_len, sent_len = send_len = send_len - sent_len)

/* Use REPL_RECV_LOOP when the length of the msg tobe recieved is already known
 * To use REPL_RECV_LOOP, following variables need to exist:
 *	unsigned char	*msg_ptr;
 *	int		recv_len;
 *	int		recvd_len;
 *	int		status;
 */
#define REPL_RECV_LOOP(sock_fd, buff, len, timeout) \
for (msg_ptr = (unsigned char *)(buff), recvd_len = recv_len = (len); \
     SS_NORMAL == (status = repl_recv(sock_fd, msg_ptr, &recvd_len, timeout)) && recvd_len < recv_len; \
     msg_ptr += recvd_len, recvd_len = recv_len = recv_len - recvd_len)

/* Replication communcation subsystem function prototypes */
int repl_send(int sock_fd, unsigned char *buff, int *send_len, struct timeval *timeout);
int repl_recv(int sock_fd, unsigned char *buff, int *recv_len, struct timeval *timeout);
int repl_close(int *sock_fd);
int get_sock_buff_size(int sockfd, uint4 *send_buffsize, uint4 *recv_buffsize);

#endif
