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

#define NUM_IOVECS	2
#define NUM_IOVECS_BOTH	2
#define NUM_IOVECS_DATA	1

cmi_status_t cmj_read_start(struct CLB *lnk)
{
	struct NTD *tsk = lnk->ntd;
	cmi_status_t status = SS_NORMAL;
	int save_errno;
        ssize_t rval;
	error_def(CMI_DCNINPROG);
	error_def(CMI_LNKNOTIDLE);
	error_def(CMI_OVERRUN);

	if (-1 == lnk->mun)
		return ENOTCONN;
	if (CM_CLB_IDLE != lnk->sta)
		return (CM_CLB_DISCONNECT == lnk->sta) ? CMI_DCNINPROG : CMI_LNKNOTIDLE;
	lnk->sta = CM_CLB_READ;
	CMI_CLB_IOSTATUS(lnk) = SS_NORMAL;
	lnk->ios.xfer_count = 0;
	lnk->ios.len_len = 0;

	while ((-1 == (rval = recv(lnk->mun, (void *)lnk->ios.u.lenbuf, CMI_TCP_PREFIX_LEN, 0))) && EINTR == errno)
		;
	/*
	 * rval == 0 --> eof
	 * rval == 2 --> entire length read (2 == CMI_TCP_PREFIX_LEN)
	 * rval == 1 || (rval < 0 && (errno == EWOULDBLOCK || errno == EAGAIN) --> read stream empty
	 */
	/* weed out the bad things first */
	if (0 > rval && !CMI_IO_WOULDBLOCK(errno))
	{
		save_errno = errno;
		cmj_err(lnk, CMI_REASON_STATUS, save_errno);
		cmj_postevent(lnk);
		return save_errno;
	}
	if (0 == rval)
	{
		cmj_err(lnk, CMI_REASON_STATUS, ECONNRESET);
		cmj_postevent(lnk);
		return ECONNRESET;
	}
	if (CMI_TCP_PREFIX_LEN == rval)
	{
		/* got the entire length */
		lnk->ios.len_len = CMI_TCP_PREFIX_LEN;
		lnk->ios.u.len = ntohs(lnk->ios.u.len);
		if (lnk->ios.u.len > lnk->mbl)
			return CMI_OVERRUN;

		while ((-1 == (rval = recv(lnk->mun, (void *)lnk->mbf, (int)lnk->ios.u.len, 0))) && EINTR == errno)
			;
		if (0 > rval && !CMI_IO_WOULDBLOCK(errno))
		{
			save_errno = errno;
			cmj_err(lnk, CMI_REASON_STATUS, save_errno);
			cmj_postevent(lnk);
			return save_errno;
		}
		if (0 == rval)
		{
			cmj_err(lnk, CMI_REASON_STATUS, ECONNRESET);
			cmj_postevent(lnk);
			return ECONNRESET;
		}
		if (rval == lnk->ios.u.len)
		{
			lnk->ios.xfer_count = rval;
			cmj_fini(lnk);
		}
		else
		{
			/* partial read or no read of packet */
			if (0 < rval)
				lnk->ios.xfer_count = rval;
			status = cmj_clb_set_async(lnk);
		}
	}
	else
	{
		/* partial or no read of length */
		if (0 < rval)
		{
			assert(CMI_TCP_PREFIX_LEN > rval);
			lnk->ios.len_len = (int)rval;
		}
		status = cmj_clb_set_async(lnk);
	}
	return status;
}

void cmj_read_interrupt(struct CLB *lnk, int signo)
{
	ssize_t rval;
	cmi_status_t status = SS_NORMAL;
	error_def(CMI_OVERRUN);
	char peekchar;
	struct iovec vec[NUM_IOVECS];
	struct msghdr msg;

	if (-1 == lnk->mun)
		return;
	if (CM_CLB_IDLE == lnk->sta)
	{
		/* potential eof, error */
		/* just peek to see if a 0 or error comes back */
		while ((-1 == (rval = recv(lnk->mun, (void *)&peekchar, 1, MSG_PEEK))) && EINTR == errno)
			;
		if (0 == rval)
			cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)ECONNRESET);
		else
		{
			if (-1 ==  rval && !CMI_IO_WOULDBLOCK(errno))
				cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)errno);
		}
		/* no I/O outstand - return leaving data in socket */
		return;
	}
	/* setup I/O vec based on past history */
	memset(&msg, 0, SIZEOF(msg));
	msg.msg_iov = vec;
	if (CMI_TCP_PREFIX_LEN > lnk->ios.len_len)
	{
		/* length not entirely read */
		msg.msg_iovlen = NUM_IOVECS_BOTH;
		vec[0].iov_len = CMI_TCP_PREFIX_LEN - lnk->ios.len_len;
		vec[0].iov_base = (lnk->ios.u.lenbuf + lnk->ios.len_len);
		vec[1].iov_len = lnk->mbl;
		vec[1].iov_base = (caddr_t)lnk->mbf;
	}
	else
	{
		/* length had been read, need to complete data */
		msg.msg_iovlen = NUM_IOVECS_DATA;
		vec[0].iov_len = lnk->ios.u.len - lnk->ios.xfer_count;
		vec[0].iov_base = (caddr_t)(lnk->mbf + lnk->ios.xfer_count);
	}

	while ((-1 == (rval = recvmsg(lnk->mun, &msg, 0))) && EINTR == errno)
		;
	/* weed out the bad things first */
	if (0 == rval)
	{
		cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)ECONNRESET);
		return;
	}
	if (-1 == rval && !CMI_IO_WOULDBLOCK(errno))
	{
		cmj_err(lnk, CMI_REASON_STATUS, (cmi_status_t)errno);
		return;
	}

	/* now we either have a positive rval or a no data error */
	if (0 < rval)
	{
		if (NUM_IOVECS_BOTH == msg.msg_iovlen)
		{
			if ((CMI_TCP_PREFIX_LEN - lnk->ios.len_len) <= rval)
			{
				/* we now have the whole length */
				rval -= (CMI_TCP_PREFIX_LEN - lnk->ios.len_len);
				lnk->ios.len_len = CMI_TCP_PREFIX_LEN;
				lnk->ios.u.len = ntohs(lnk->ios.u.len);
			}
			else
			{
				/* we just read part of the length */
				assert(CMI_TCP_PREFIX_LEN > rval);
				lnk->ios.len_len +=  (int)rval;
				status = cmj_clb_set_async(lnk);
				if (CMI_ERROR(status))
					cmj_err(lnk, CMI_REASON_STATUS, status);
				return;
			}
		}
		if (rval == (lnk->ios.u.len - lnk->ios.xfer_count))
		{
			lnk->ios.xfer_count += rval;
			cmj_fini(lnk);
			return;
		}
		lnk->ios.xfer_count += rval;
	}
	status = cmj_clb_set_async(lnk);
	if (CMI_ERROR(status))
		cmj_err(lnk, CMI_REASON_STATUS, status);
}
