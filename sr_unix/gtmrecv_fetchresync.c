/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include "gtm_socket.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"

#include <sys/un.h>
#include "gtm_time.h" /* needed for difftime() definition; if this file is not included, difftime returns bad values on AIX */
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <sys/shm.h>
#include <sys/wait.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "error.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "repl_comm.h"
#include "repl_msg.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "gtm_logicals.h"
#include "eintr_wrappers.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "io.h"
#include "trans_log_name.h"
#include "util.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "iotcpdef.h"
#include "gtmio.h"
#include "replgbl.h"

#define MAX_ATTEMPTS_FOR_FETCH_RESYNC	60 /* max-wait in seconds for source server response after connection is established */
#define MAX_WAIT_FOR_FETCHRESYNC_CONN	60 /* max-wait in seconds to establish connection with the source server */
#define FETCHRESYNC_PRIMARY_POLL	(MICROSEC_IN_SEC - 1) /* micro seconds, almost 1 second */

GBLREF	uint4			process_id;
GBLREF	int			recvpool_shmid;
GBLREF	int			gtmrecv_listen_sock_fd, gtmrecv_sock_fd;
GBLREF	struct sockaddr_in	primary_addr;
GBLREF	seq_num			seq_num_zero;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int			repl_max_send_buffsize, repl_max_recv_buffsize;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		repl_connection_reset;
GBLREF 	mur_gbls_t		murgbl;

error_def(ERR_PRIMARYNOTROOT);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLCOMM);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_TEXT);

CONDITION_HANDLER(gtmrecv_fetchresync_ch)
{
	int	rc;

	START_CH;
	if (FD_INVALID != gtmrecv_listen_sock_fd)
		CLOSEFILE_RESET(gtmrecv_listen_sock_fd, rc);	/* resets "gtmrecv_listen_sock_fd" to FD_INVALID */
	if (FD_INVALID != gtmrecv_sock_fd)
		CLOSEFILE_RESET(gtmrecv_sock_fd, rc);	/* resets "gtmrecv_sock_fd" to FD_INVALID */
	PRN_ERROR;
	NEXTCH;
}

int gtmrecv_fetchresync(int port, seq_num *resync_seqno, seq_num max_reg_seqno)
{
	GTM_SOCKLEN_TYPE		primary_addr_len;
	repl_resync_msg_t		resync_msg;
	repl_msg_t			msg;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				torecv_len, recvd_len, recvd_this_iter;	/* needed for REPL_RECV_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	fd_set				input_fds;
	int				wait_count;
	char				seq_num_str[32], *seq_num_ptr;
	pid_t				rollback_pid;
	int				rollback_status;
	int				wait_status;
	time_t				t1, t2;
	struct timeval			gtmrecv_fetchresync_max_wait, gtmrecv_fetchresync_poll, sel_timeout_val;
	repl_instinfo_msg_t		instinfo_msg;
	repl_needinst_msg_ptr_t		need_instinfo_msg;
	repl_needtriple_msg_ptr_t	need_tripleinfo_msg;
	int4				triple_num;
	repl_triple			triple;
	char				assumed_remote_proto_ver; /* Protocol version of the source server with which receiver
								   * server communicates. Need to be "signed char" in order to be
								   * able to do signed comparisons of this with the macros
								   * REPL_PROTO_VER_DUALSITE(0) and REPL_PROTO_VER_UNINITIALIZED(-1)
								   */
	seq_num				triple_seqnum;
	short				retry_num;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assumed_remote_proto_ver = REPL_PROTO_VER_MULTISITE;
	repl_log(stdout, TRUE, TRUE, "Assuming primary supports multisite functionality. Connecting "
		"using multisite communication protocol.\n");
	ESTABLISH_RET(gtmrecv_fetchresync_ch, (!SS_NORMAL));
	do
	{
		QWASSIGN(*resync_seqno, seq_num_zero);
		gtmrecv_fetchresync_max_wait.tv_sec = MAX_WAIT_FOR_FETCHRESYNC_CONN;
		gtmrecv_fetchresync_max_wait.tv_usec = 0;
		gtmrecv_fetchresync_poll.tv_sec = 0;
		gtmrecv_fetchresync_poll.tv_usec = FETCHRESYNC_PRIMARY_POLL;
		gtmrecv_comm_init(port);

		primary_addr_len = SIZEOF(primary_addr);
		murgbl.remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
		repl_log(stdout, TRUE, TRUE, "Waiting for a connection...\n");
		FD_ZERO(&input_fds);
		FD_SET(gtmrecv_listen_sock_fd, &input_fds);
		/* Note - the following call to select checks for EINTR. The SELECT macro is not used because
		 * the code also checks for EAGAIN and takes action before retrying the select.
		 */
		t1 = time(NULL);
		while ((status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL, NULL, &gtmrecv_fetchresync_max_wait)) < 0)
		{
			if ((EINTR == errno)  || (EAGAIN == errno))
			{
				t2 = time(NULL);
				if (0 >= (int)(gtmrecv_fetchresync_max_wait.tv_sec =
						(MAX_WAIT_FOR_FETCHRESYNC_CONN - (int)difftime(t2, t1))))
				{
					status = 0;
					break;
				}
				gtmrecv_fetchresync_max_wait.tv_usec = 0;
				FD_SET(gtmrecv_listen_sock_fd, &input_fds);
				continue;
			} else
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error in select on listen socket"), errno);
		}
		if (status == 0)
		{
			repl_log(stdout, TRUE, TRUE, "Waited about %d seconds for connection from primary source server\n",
				MAX_WAIT_FOR_FETCHRESYNC_CONN);
			rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Waited too long to get a connection request. Check if primary is alive."));
		}
		ACCEPT_SOCKET(gtmrecv_listen_sock_fd, (struct sockaddr *)&primary_addr,
									(GTM_SOCKLEN_TYPE *)&primary_addr_len, gtmrecv_sock_fd);
		if (0 > gtmrecv_sock_fd)
		{
#ifdef __hpux
		/* ENOBUFS in HP-UX is either because of a memory problem or when we have received a RST just
		after a SYN before an accept call. Normally this is not fatal and is just a transient state.Hence
		exiting just after a single error of this kind should not be done. So retry in case of HP-UX and ENOBUFS error*/
			if (ENOBUFS == errno)
			{
 				  retry_num = 0;
			  /*In case of succeeding with select in first go, accept will still get 5ms time difference*/
				while (HPUX_MAX_RETRIES > retry_num)
				{
					SHORT_SLEEP(5);
					FD_ZERO(&input_fds);
					FD_SET(gtmrecv_listen_sock_fd, &input_fds);
                          /*Since we use Blocking socket, check before re-trying whether there is a connection to be accepted*/
                          /*Timeout of HPUX_SEL_TIMEOUT.  In case the earlier connection is not available there can be
                            some time gap between the time the error occured and the new client requests coming in*/
                                  	for ( ; HPUX_MAX_RETRIES > retry_num; retry_num++)
					{
	                                          FD_ZERO(&input_fds);
	                                          FD_SET(gtmrecv_listen_sock_fd, &input_fds);
						  sel_timeout_val.tv_sec = 0;
        	                                  sel_timeout_val.tv_usec = HPUX_SEL_TIMEOUT;
                	                          status = select(gtmrecv_listen_sock_fd + 1, &input_fds, NULL,
								NULL, &sel_timeout_val);
                        	                  if (0 < status)
	                                                  break;
        	                                  else
                	                                 SHORT_SLEEP(5);
					}
					if (0 > status)
                                                 rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
                                              RTS_ERROR_LITERAL("Error in select on listen socket after ENOBUFS error"), errno);
					else
					{
	                                         ACCEPT_SOCKET(gtmrecv_listen_sock_fd, (struct sockaddr *)&primary_addr,
                                                                       (GTM_SOCKLEN_TYPE *)&primary_addr_len, gtmrecv_sock_fd);
						if ((0 > gtmrecv_sock_fd) && (errno == ENOBUFS))
							retry_num++;
						else
							break;
					}
				  }
                        }

			if (0 > gtmrecv_sock_fd)
#endif
			{
				rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error accepting connection from Source Server"), errno);
			}
		}
		repl_close(&gtmrecv_listen_sock_fd);
		if (0 != (status = get_send_sock_buff_size(gtmrecv_sock_fd, &repl_max_send_buffsize))
			|| 0 != (status = get_recv_sock_buff_size(gtmrecv_sock_fd, &repl_max_recv_buffsize)))
		{
			rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Error getting socket send/recv buffsizes"), status);
			return ERR_REPLCOMM;
		}
		repl_log(stdout, TRUE, TRUE, "Connection established, using TCP send buffer size %d receive buffer size %d\n",
				repl_max_send_buffsize, repl_max_recv_buffsize);
		repl_log_conn_info(gtmrecv_sock_fd, stdout);

		/* Send REPL_FETCH_RESYNC message */
		memset(&resync_msg, 0, SIZEOF(resync_msg));
		/* If we assume remote primary is multisite capable, we need to send the journal seqno of this instance
		 * for comparison. If on the other hand, it is assumed to be only dualsite capable, we need to send the
		 * dualsite_resync_seqno of this instance which is maintained in "jgbl.max_dualsite_resync_seqno".
		 */
		if (REPL_PROTO_VER_DUALSITE != assumed_remote_proto_ver)
			resync_msg.resync_seqno = max_reg_seqno;
		else
			resync_msg.resync_seqno = jgbl.max_dualsite_resync_seqno;
		assert(resync_msg.resync_seqno);
		resync_msg.proto_ver = REPL_PROTO_VER_THIS;
		resync_msg.node_endianness = NODE_ENDIANNESS;
		(TREF(replgbl)).src_node_same_endianness = TRUE;
		(TREF(replgbl)).src_node_endianness_known = FALSE;
		gtmrecv_repl_send((repl_msg_ptr_t)&resync_msg, REPL_FETCH_RESYNC, MIN_REPL_MSGLEN,
					"REPL_FETCH_RESYNC", resync_msg.resync_seqno);
		if (repl_connection_reset)
		{	/* Connection got reset during the above send */
			rts_error(VARLSTCNT(1) ERR_REPLCOMM);
			return ERR_REPLCOMM;
		}
		/* Wait for REPL_RESYNC_SEQNO (if dual-site primary) or REPL_NEED_INSTANCE_INFO (if multi-site primary) message */
		do
		{
			wait_count = MAX_ATTEMPTS_FOR_FETCH_RESYNC;
			REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_fetchresync_poll)
			{
				if (0 >= wait_count)
					break;
				repl_log(stdout, TRUE, TRUE, "Waiting for REPL_NEED_INSTANCE_INFO or REPL_NEED_TRIPLE_INFO"
					" or REPL_RESYNC_SEQNO\n");
				wait_count--;
			}
			if (status != SS_NORMAL)
			{
				if (EREPL_RECV == repl_errno)
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error receiving RESYNC JNLSEQNO. Error in recv"), status);
				if (EREPL_SELECT == repl_errno)
					rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error receiving RESYNC JNLSEQNO. Error in select"), status);
			}
			if (wait_count <= 0)
				rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Waited too long to get message from primary. Check if primary is alive."));
			if (!(TREF(replgbl)).src_node_endianness_known)
			{
				(TREF(replgbl)).src_node_endianness_known = TRUE;
				if ((REPL_MSGTYPE_LAST < msg.type) && (REPL_MSGTYPE_LAST > GTM_BYTESWAP_32(msg.type)))
					(TREF(replgbl)).src_node_same_endianness = FALSE;
				else
					(TREF(replgbl)).src_node_same_endianness = TRUE;
			}
			if (!(TREF(replgbl)).src_node_same_endianness)
			{
				msg.type = GTM_BYTESWAP_32(msg.type);
				msg.len = GTM_BYTESWAP_32(msg.len);
			}
			switch(msg.type)
			{
				case REPL_NEED_INSTANCE_INFO:
					need_instinfo_msg = (repl_needinst_msg_ptr_t)&msg;
					repl_log(stdout, TRUE, TRUE, "Received REPL_NEED_INSTANCE_INFO message from primary "
						"instance [%s]\n", need_instinfo_msg->instname);
					murgbl.remote_proto_ver = need_instinfo_msg->proto_ver;
					assert(REPL_PROTO_VER_DUALSITE != murgbl.remote_proto_ver);
					assert(REPL_PROTO_VER_UNINITIALIZED != murgbl.remote_proto_ver);
					assert(REPL_PROTO_VER_MULTISITE <= murgbl.remote_proto_ver);
					memset(&instinfo_msg, 0, SIZEOF(instinfo_msg));
					assert(NULL != jnlpool.repl_inst_filehdr);
					memcpy(instinfo_msg.instname, jnlpool.repl_inst_filehdr->this_instname,
						MAX_INSTNAME_LEN - 1);
					instinfo_msg.was_rootprimary = (unsigned char)repl_inst_was_rootprimary();
					murgbl.was_rootprimary = instinfo_msg.was_rootprimary;
					gtmrecv_repl_send((repl_msg_ptr_t)&instinfo_msg, REPL_INSTANCE_INFO,
								MIN_REPL_MSGLEN, "REPL_INSTANCE_INFO", MAX_SEQNO);
					if (instinfo_msg.was_rootprimary && !need_instinfo_msg->is_rootprimary)
						rts_error(VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
							LEN_AND_STR((char *)need_instinfo_msg->instname));
					break;

				case REPL_NEED_TRIPLE_INFO:
					need_tripleinfo_msg = RECAST(repl_needtriple_msg_ptr_t)&msg;
					if ((TREF(replgbl)).src_node_same_endianness)
						triple_seqnum = need_tripleinfo_msg->seqno;
					else
						triple_seqnum = GTM_BYTESWAP_64(need_tripleinfo_msg->seqno);
					repl_log(stdout, TRUE, TRUE, "Received REPL_NEED_TRIPLE_INFO message for seqno "
						"%llu [0x%llx]\n", triple_seqnum, triple_seqnum);
					assert(REPL_PROTO_VER_UNINITIALIZED != murgbl.remote_proto_ver);
					assert(NULL != jnlpool.jnlpool_dummy_reg);
					repl_inst_ftok_sem_lock();
					status = repl_inst_wrapper_triple_find_seqno(triple_seqnum, &triple, &triple_num);
					repl_inst_ftok_sem_release();
					if (0 != status)
					{	/* Close the connection. The function call above would have issued the error. */
						assert(ERR_REPLINSTNOHIST == status);
						repl_log(stdout, TRUE, TRUE, "Connection reset due to REPLINSTNOHIST error\n");
						repl_connection_reset = TRUE;
						repl_close(&gtmrecv_sock_fd);
						return status;
					}
					gtmrecv_send_triple_info(&triple, triple_num);
					break;

				case REPL_INST_NOHIST:
					repl_log(stdout, TRUE, TRUE, "Originating instance encountered a REPLINSTNOHIST error."
						" JNL_SEQNO of this replicating instance precedes the current history in the "
						"originating instance file. Rollback exiting.\n");
					status = ERR_REPLINSTNOHIST;
					repl_log(stdout, TRUE, TRUE, "Connection reset due to REPLINSTNOHIST error on primary\n");
					repl_connection_reset = TRUE;
					repl_close(&gtmrecv_sock_fd);
					return status;
					break;

				case REPL_RESYNC_SEQNO:
					repl_log(stdout, TRUE, TRUE, "Received REPL_RESYNC_SEQNO message\n");
					if (REPL_PROTO_VER_UNINITIALIZED == murgbl.remote_proto_ver)
						murgbl.remote_proto_ver = REPL_PROTO_VER_DUALSITE;
					assert(REPL_PROTO_VER_DUALSITE <= murgbl.remote_proto_ver);
					break;

				default:
					repl_log(stdout, TRUE, TRUE, "Message of unknown type (%d) received\n", msg.type);
					assert(FALSE);
					rts_error(VARLSTCNT(1) ERR_REPLCOMM);
					break;
			}
		} while (!repl_connection_reset && (REPL_RESYNC_SEQNO != msg.type));
		if (repl_connection_reset)
		{	/* Connection got reset during the above send */
			rts_error(VARLSTCNT(1) ERR_REPLCOMM);
			return ERR_REPLCOMM;
		}
		if ((TREF(replgbl)).src_node_same_endianness)
			QWASSIGN(*resync_seqno, *(seq_num *)&msg.msg[0]);
		else
			QWASSIGN(*resync_seqno, GTM_BYTESWAP_64(*(seq_num *)&msg.msg[0]));
		/* Wait till connection is broken or REPL_CONN_CLOSE is received */
		REPL_RECV_LOOP(gtmrecv_sock_fd, &msg, MIN_REPL_MSGLEN, FALSE, &gtmrecv_fetchresync_poll)
		{
			REPL_DPRINT1("FETCH_RESYNC : Waiting for source to send CLOSE_CONN or connection breakage\n");
		}
		repl_close(&gtmrecv_sock_fd);
		/* Check if our assumed remote protocol version matches the actual. If so, no need for further communication.
		 * If not, we need to reset our assumed remote protocol version, and reconnect using the newly assumed protocol
		 * version. This is because if the remote side is dualsite, we will send the resync seqno across and if it is
		 * multisite, we will send the jnl seqno across. But if the resync seqno and jnl seqno are not different, there
		 * is no need to disconnect. Keep retrying until the assumed and actual protocol versions match.
		 */
		if (REPL_PROTO_VER_DUALSITE != assumed_remote_proto_ver)
		{
			if (REPL_PROTO_VER_DUALSITE != murgbl.remote_proto_ver)
				break;	/* Both assumed and actual is multisite */
			/* Assumed multisite, but actual is dualsite. */
			if (max_reg_seqno == jgbl.max_dualsite_resync_seqno)
				break;	/* avoid disconnect/reconnect if both jnl and resync seqnos are same */
			assumed_remote_proto_ver = REPL_PROTO_VER_DUALSITE;
			repl_log(stdout, TRUE, TRUE, "Primary does not support multisite functionality. Reconnecting "
				"using dualsite communication protocol.\n");
		} else
		{
			if (REPL_PROTO_VER_DUALSITE == murgbl.remote_proto_ver)
				break;	/* Both assumed and actual is dualsite */
			/* Assumed dualsite, but actual is multisite. */
			if (max_reg_seqno == jgbl.max_dualsite_resync_seqno)
				break;	/* avoid disconnect/reconnect if both jnl and resync seqnos are same */
			assumed_remote_proto_ver = REPL_PROTO_VER_MULTISITE;
			repl_log(stdout, TRUE, TRUE, "Primary supports multisite functionality. Reconnecting "
				"using multisite communication protocol.\n");
		}
	} while (TRUE);
	REVERT;

	repl_log(stdout, TRUE, TRUE, "Received RESYNC SEQNO is %llu [0x%llx]\n", *resync_seqno, *resync_seqno);
	assert((*resync_seqno <= max_reg_seqno) || (REPL_PROTO_VER_DUALSITE == murgbl.remote_proto_ver));
	return SS_NORMAL;
}
