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

#ifndef REPL_COMM_H
#define REPL_COMM_H

#define REPL_CONN_RESET(err)	(ECONNRESET == (err) || EPIPE == (err) || EINVAL == (err) || ETIMEDOUT == (err))

/* To use REPL_SEND_LOOP following variables need to exist:
 *	unsigned char	*msg_ptr;
 *	int		tosend_len;
 *	int		sent_len;
 *	int		sent_this_iter;
 *	int		status;
 * On completion of an iteration, sent_len contains the number of bytes sent upto now, tosend_len contains the number of bytes
 * yet to be sent. Users of this macro must NOT count on msg_ptr pointing to BUFF + LEN. sent_this_iter is used as a
 * temporary, and users must NOT count on its value. The loop is terminated when
 * a. we send all of the requested length successfully, in which case sent_len will be LEN, and tosend_len will be 0, OR,
 * b. when repl_send() fails, in which case sent_len contains the number of bytes successfully sent, and tosend_len is the
 *    length that we failed to send.
 */
#define REPL_SEND_LOOP(SOCK_FD, BUFF, LEN, TIMEOUT) 							\
assert(LEN > 0);											\
for (msg_ptr = (unsigned char *)(BUFF), sent_len = 0, sent_this_iter = tosend_len = (LEN); 		\
     SS_NORMAL == (status = repl_send(SOCK_FD, msg_ptr, &sent_this_iter, TIMEOUT)) 			\
     && ((sent_len += sent_this_iter), (tosend_len -= sent_this_iter), (tosend_len > 0)); 		\
     msg_ptr += sent_this_iter, sent_this_iter = tosend_len)

/* Use REPL_RECV_LOOP when the length of the msg to be recieved is already known
 * To use REPL_RECV_LOOP, following variables need to exist:
 *	unsigned char	*msg_ptr;
 *	int		torecv_len;
 *	int		recvd_len;
 *	int		recvd_this_iter;
 *	int		status;
 * On completion of an iteration, recvd_len contains the number of bytes received upto now, torecv_len contains the number of bytes
 * yet to be received. Users of this macro must NOT count on msg_ptr pointing to BUFF + LEN. recvd_this_iter is used as a
 * temporary, and users must NOT count on its value. The loop is terminated when
 * a. we receive all of the requested length successfully, in which case recvd_len will be LEN, and torecv_len will be 0, OR,
 * b. when repl_recv() fails, in which case recvd_len contains the number of bytes successfully received, and torecv_len is the
 *    length that we failed to receive.
 */
#define REPL_RECV_LOOP(SOCK_FD, BUFF, LEN, TIMEOUT) 							\
for (msg_ptr = (unsigned char *)(BUFF), recvd_len = 0, recvd_this_iter = torecv_len = (LEN); 		\
     (SS_NORMAL == (status = repl_recv(SOCK_FD, msg_ptr, &recvd_this_iter, TIMEOUT))) 			\
     && ((recvd_len += recvd_this_iter), (torecv_len -= recvd_this_iter), (torecv_len > 0)); 		\
     msg_ptr += recvd_this_iter, recvd_this_iter = torecv_len)

#define REPL_COMM_MAX_INTR_CNT	3	/* # of iterations we'll let select() be interrupted before we give up and assume timeout */
#define REPL_COMM_LOG_EAGAIN_INTERVAL	10	/* every these many times select() returns EAGAIN, we log a message */
#define REPL_COMM_LOG_EWDBLCK_INTERVAL	10	/* every these many times send() returns EWOULDBLOCK, we log a message */
#define REPL_COMM_LOG_EMSGSIZE_INTERVAL	10	/* every these many times send() returns EWOULDBLOCK, we log a message */
#define REPL_COMM_MIN_SEND_SIZE		1024	/* try to keep the send size at least this */

#define VMS_MAX_TCP_IO_SIZE	(64 * 1024 - 512) /* Hard limit for TCP send or recv size. On some implementations, the limit is
						   * 64K - 1, on others it is 64K - 512. We take the conservative approach and
						   * choose the lower limit
						   */
#define VMS_MAX_TCP_SEND_SIZE	VMS_MAX_TCP_IO_SIZE
#define VMS_MAX_TCP_RECV_SIZE	VMS_MAX_TCP_IO_SIZE

#define GTMSOURCE_IDLE_POLL_WAIT	100			/* 100ms sleep in case nothing sent to the other side */
#define REPL_POLL_WAIT			(MILLISECS_IN_SEC - 1)	/* Maximum time (in ms) for select()/poll() to timeout */
#define REPL_POLL_NOWAIT		0			/* Forces poll()/select() to return immediately */

/* Replication communcation subsystem function prototypes */
int fd_ioready(int sock_fd, boolean_t pollin, int timeout);
int repl_send(int sock_fd, unsigned char *buff, int *send_len, int timeout);
int repl_recv(int sock_fd, unsigned char *buff, int *recv_len, int timeout);
int repl_close(int *sock_fd);
int get_send_sock_buff_size(int sockfd, int *buflen);
int get_recv_sock_buff_size(int sockfd, int *buflen);
int set_send_sock_buff_size(int sockfd, int buflen);
int set_recv_sock_buff_size(int sockfd, int buflen);
void repl_log_conn_info(int sock_fd, FILE *log_fp);

#endif /* REPL_COMM_H */
