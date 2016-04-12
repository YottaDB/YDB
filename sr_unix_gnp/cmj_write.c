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
#include "gtm_socket.h"
#include "gtm_string.h"
#include "cmidef.h"
#include <errno.h>
#include <sys/uio.h>
#include "gtm_inet.h"

GBLREF struct NTD *ntd_root;

#define NUM_IOVECS	2
#define IOVEC_LEN	0
#define IOVEC_MSG	1

cmi_status_t cmj_write_start(struct CLB *lnk)
{
	cmi_status_t status = SS_NORMAL;
	struct msghdr msg;
	struct iovec vec[NUM_IOVECS];
	int save_errno;
        ssize_t rval;
	error_def(CMI_DCNINPROG);
	error_def(CMI_LNKNOTIDLE);
	error_def(CMI_OVERRUN);

	if (lnk->mun == -1)
		return ENOTCONN;
	if (lnk->sta != CM_CLB_IDLE)
		return (lnk->sta == CM_CLB_DISCONNECT) ? CMI_DCNINPROG : CMI_LNKNOTIDLE;
	lnk->sta = CM_CLB_WRITE;
	CMI_CLB_IOSTATUS(lnk) = SS_NORMAL;
	memset(&msg, 0, SIZEOF(msg));
	msg.msg_iovlen = NUM_IOVECS; /* two vectors */
	msg.msg_iov = vec;
	vec[IOVEC_LEN].iov_len = CMI_TCP_PREFIX_LEN;
	vec[IOVEC_LEN].iov_base = (caddr_t)lnk->ios.u.lenbuf;
	lnk->ios.u.len = htons((unsigned short)lnk->cbl); /* length of message is sent in network byte order */
	vec[IOVEC_MSG].iov_len = lnk->cbl;
	vec[IOVEC_MSG].iov_base = (caddr_t)lnk->mbf;
	/*
	 * Use sendmsg to cut down number of system calls
	 * and more importantly, since we probably have
	 * disabled the nagle algorithm, insure that the tcp socket
	 * is presented with an opportunity to write
	 * the entire packet.
	 *
	 * I didn't change the interrupt handler, since
	 * a partially successful write is an exception.
	 *
	 */
	while ((-1 == (rval = sendmsg(lnk->mun, &msg, 0))) && EINTR == errno)
		;
	if (-1 == rval)
	{
		save_errno = errno;
		CMI_DPRINT(("cmj_write_start: sendmsg error code : %d\n", save_errno));
		if (!CMI_IO_WOULDBLOCK(save_errno))
		{
			cmj_err(lnk, CMI_REASON_STATUS, save_errno);
			cmj_postevent(lnk);
			return save_errno;
		}
	}
	if (rval >= CMI_TCP_PREFIX_LEN)
	{
		/* we wrote at least the entire length */
		lnk->ios.len_len = CMI_TCP_PREFIX_LEN;
		rval -= CMI_TCP_PREFIX_LEN; /* subtract out length overhead */
		lnk->ios.xfer_count = rval;
		if (rval == lnk->cbl)
		{
			cmj_fini(lnk);		/* done */
			return status;
		}
		/* After successfully sending message length, convert length back to host order since we use this to compute
		 * length remaining for partial sends */
		lnk->ios.u.len = ntohs((unsigned short)lnk->ios.u.len);
	} else
	{
		/* partial or no write of length */
		if (rval > 0)
		{
			assert(CMI_TCP_PREFIX_LEN > rval);
			lnk->ios.len_len = (int)rval;
		}
	}
	CMI_DPRINT(("cmj_write_start: sendmsg partial send %ld bytes\n", rval));
	status = cmj_clb_set_async(lnk); /* more to write */
	return status;
}

cmi_status_t cmj_write_urg_start(struct CLB *lnk)
{
	cmi_status_t status = SS_NORMAL;
	ssize_t rval;
	error_def(CMI_DCNINPROG);
	error_def(CMI_LNKNOTIDLE);
	error_def(CMI_OVERRUN);

	if (lnk->mun == -1)
		return ENOTCONN;
	lnk->prev_sta = lnk->sta;
	lnk->sta = CM_CLB_WRITE_URG;
	while ((-1 == (rval = send(lnk->mun, (void *)&lnk->urgdata, 1, MSG_OOB))) && EINTR == errno)
		;
	if (-1 == rval && !CMI_IO_WOULDBLOCK(errno))
		return errno;
	if (1 == rval)
	{
		cmj_fini(lnk);
		return status;
	}
	status = cmj_clb_set_async(lnk); /* could not send 1 byte urgent data, try again. NOTE: no length prefix for urgent data */
	return status;
}

void cmj_write_interrupt(struct CLB *lnk, int signo)
{
	int save_errno;
        ssize_t rval;
	cmi_status_t status = SS_NORMAL;

	if (lnk->mun == -1)
		return;
	if (lnk->sta == CM_CLB_WRITE_URG)
	{
		while ((-1 == (rval = send(lnk->mun, (void *)&lnk->urgdata, 1, MSG_OOB))) && EINTR == errno)
			;
		if (-1 == rval)
		{
			save_errno = errno;
			CMI_DPRINT(("cmj_write_interrupt : send URGENT error code : %d\n", save_errno));
			if (!CMI_IO_WOULDBLOCK(save_errno))
			{
				cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)save_errno);
				return;
			}
		}
		if (1 == rval)
		{
			cmj_fini(lnk);
			return;
		}
		status = cmj_clb_set_async(lnk);
		if (CMI_ERROR(status))
			cmj_err(lnk, CMI_REASON_STATUS, status);
		return;
	}
	if (lnk->ios.len_len < CMI_TCP_PREFIX_LEN)
	{
		while ((-1 == (rval = send(lnk->mun, (void *)(lnk->ios.u.lenbuf + lnk->ios.len_len),
					CMI_TCP_PREFIX_LEN - lnk->ios.len_len, 0))) && EINTR == errno)
			;
		if (-1 == rval)
		{
			save_errno = errno;
			CMI_DPRINT(("cmj_write_interrupt : send error code : %d\n", save_errno));
			if (!CMI_IO_WOULDBLOCK(save_errno))
			{
				cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)save_errno);
				return;
			}
		}
		if (rval > 0)
		{
			lnk->ios.len_len += (int)rval;
			assert(CMI_TCP_PREFIX_LEN >= lnk->ios.len_len);
			if (lnk->ios.len_len == CMI_TCP_PREFIX_LEN) /* prefix length successfully sent */
			{
				/* After successfully sending message length, convert length back to host order since we use
				 * this to compute length remaining for partial sends */
				lnk->ios.u.len = ntohs((unsigned short)lnk->ios.u.len);
			}
		} else
		{
			CMI_DPRINT(("cmj_write_interrupt : send wrote 0 bytes\n"));
		}
	}
	if (lnk->ios.len_len == CMI_TCP_PREFIX_LEN)
	{
		assert(lnk->ios.u.len > lnk->ios.xfer_count); /* we shouldn't be wasting system calls on doing 0 byte output */
		while ((-1 == (rval = send(lnk->mun, (void *)(lnk->mbf + lnk->ios.xfer_count),
					(int)(lnk->ios.u.len - lnk->ios.xfer_count), 0))) && EINTR == errno)
			;
		if (-1 == rval && !CMI_IO_WOULDBLOCK(errno))
		{
			cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)errno);
			return;
		}
		if (rval > 0)
			lnk->ios.xfer_count += rval;
		if (0 == (int)(lnk->ios.u.len - lnk->ios.xfer_count)) /* entire message successfully sent */
			cmj_fini(lnk);
		else
		{
			status = cmj_clb_set_async(lnk);
			if (CMI_ERROR(status))
				cmj_err(lnk, CMI_REASON_STATUS, status);
			CMI_DPRINT(("cmj_write_interrupt: send partial (1) send %ld bytes\n", rval));
		}
	}
	else
	{
		/* partial or no write of length */
		if (rval > 0)
		{
			lnk->ios.len_len += (int)rval;
			assert(CMI_TCP_PREFIX_LEN > lnk->ios.len_len);
		}
		status = cmj_clb_set_async(lnk);
		if (CMI_ERROR(status))
			cmj_err(lnk, CMI_REASON_STATUS, status);
		CMI_DPRINT(("cmj_write_interrupt: send partial (2) send %ld bytes\n", rval));
	}
}
