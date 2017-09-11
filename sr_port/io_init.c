/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#ifdef UNIX
#include "gtmio.h"
#endif

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "error.h"
#include "op.h"
#include "term_setup.h"
#include "trans_log_name.h"

GBLREF io_pair		io_curr_device;		/* current device	*/
GBLREF io_pair		io_std_device;		/* standard device	*/
GBLREF io_log_name	*dollar_principal;	/* pointer to log name GTM$PRINCIPAL if defined */
GBLREF bool		prin_in_dev_failure;
GBLREF bool		prin_out_dev_failure;
GBLREF int		(*op_open_ptr)(mval *v, mval *p, const mval *t, mval *mspace);

GBLREF io_log_name	*io_root_log_name;	/* root of linked list	*/
GBLREF mstr		sys_input;
GBLREF mstr		sys_output;
GBLREF mstr		gtm_principal;
#ifdef UNIX
GBLREF boolean_t	err_same_as_out;
#endif

LITREF	mval	literal_zero;

error_def(ERR_FILEOPENFAIL);
error_def(ERR_LOGTOOLONG);
error_def(ERR_SYSCALL);

void io_init(boolean_t term_ctrl)
{
	static readonly unsigned char open_params_list[2] =
	{
		(unsigned char)iop_newversion,
		(unsigned char)iop_eol
	};
	static readonly unsigned char nolognam_params_list[] =
	{
#		ifdef UNIX
		(unsigned char)iop_stream,	/* open FILEs in Unix with STREAM option by default */
#		endif
		(unsigned char)iop_nl,
		(unsigned char)iop_eol
	};
#	ifdef UNIX
	static readonly unsigned char nowrap_params_list[] =
	{
		(unsigned char)iop_nowrap,
		(unsigned char)iop_eol
	};
#	endif
	static readonly unsigned char	no_params = (unsigned char)iop_eol;
	static readonly unsigned char	shr_params[] =
	{
		(unsigned char)iop_shared,
		(unsigned char)iop_readonly,
		(unsigned char)iop_eol
	};

	int4			status;
        mval			val;
	mstr			tn;
 	MSTR_CONST		(gtm_netout, "GTM_NETOUT");
 	MSTR_CONST		(sys_net, "SYS$NET");
	char			buf1[MAX_TRANS_NAME_LEN]; /* buffer to hold translated name */
	mval			pars;
	io_log_name		*inp, *outp;
	io_log_name		*ln;
	enum io_dev_type	dev_type;
#	ifdef UNIX
	int			fd, newfd;
	struct stat		statbuf, out_statbuf;
#	endif

#	ifdef UNIX
	/* Make sure we have valid descriptors on stdin/stdout/stderr.
	 * Otherwise we could end up "filling the hole" with a database file and writing an error message to it.
	 */
	for (fd = 0; fd < 3; fd++)
	{
		status = fstat(fd, &statbuf);
		if (-1 == status)
		{
			if (EBADF == errno)
			{
				OPENFILE("/dev/null", ((0 == fd) ? O_RDONLY : O_RDWR), newfd);
				if (-1 == newfd)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_SYSCALL, 5,
							LEN_AND_LIT("open /dev/null on std descriptor"), CALLFROM, errno, 0);
				assert(newfd == fd);
			}
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_SYSCALL, 5,
						LEN_AND_LIT("fstat of std descriptor"), CALLFROM, errno, 0);
		} else if (0 < fd)
		{
			if (1 == fd)
				out_statbuf = statbuf;
			else
				err_same_as_out = (statbuf.st_dev == out_statbuf.st_dev) && (statbuf.st_ino == out_statbuf.st_ino);
		}
	}
#	endif
	io_init_name();
	/* default logical names */
	io_root_log_name = (io_log_name *)malloc(SIZEOF(*io_root_log_name));
	memset(io_root_log_name, 0, SIZEOF(*io_root_log_name));
	val.mvtype = MV_STR;
	val.str.addr = "0";
	val.str.len = 1;
	ln = get_log_name(&val.str, INSERT);
	assert(ln != 0);
	val.str = gtm_principal;
	status = TRANS_LOG_NAME(&val.str, &tn, buf1, SIZEOF(buf1), dont_sendmsg_on_log2long);
	if (SS_NOLOGNAM == status)
		dollar_principal = 0;
	else if (SS_NORMAL == status)
		dollar_principal = get_log_name(&tn, INSERT);
#	ifdef UNIX
	else if (SS_LOG2LONG == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.str.len, val.str.addr, SIZEOF(buf1) - 1);
#	endif
	else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);

	/* open devices */
	val.str = sys_input;
	inp = get_log_name(&val.str, INSERT);
	pars.mvtype = MV_STR;
	status = TRANS_LOG_NAME(&val.str, &tn, buf1, SIZEOF(buf1), dont_sendmsg_on_log2long);
	if (SS_NOLOGNAM == status)
	{
		pars.str.len = SIZEOF(nolognam_params_list);
		pars.str.addr = (char *)nolognam_params_list;
	} else if (SS_NORMAL == status)
	{
		UNIX_ONLY(assert(FALSE);)
		if (!io_is_rm(&val.str))
		{
			pars.str.len = SIZEOF(no_params);
			pars.str.addr = (char *)&no_params;
		} else  if (io_is_sn(&val.str))
		{
			pars.str.len = SIZEOF(open_params_list);
			pars.str.addr = (char *)open_params_list;
		} else
		{
			pars.str.len = SIZEOF(shr_params);
			pars.str.addr = (char *)shr_params;
		}
	}
#	ifdef UNIX
	else if (SS_LOG2LONG == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.str.len, val.str.addr, SIZEOF(buf1) - 1);
#	endif
	else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	ESTABLISH(io_init_ch);
	(*op_open_ptr)(&val, &pars, (mval *)&literal_zero, 0);
	io_curr_device.in  = io_std_device.in  = inp->iod;
	val.str = sys_output;
	if ((SS_NORMAL == TRANS_LOG_NAME(&gtm_netout, &tn, buf1, SIZEOF(buf1), do_sendmsg_on_log2long))
			&& (SS_NORMAL == TRANS_LOG_NAME(&sys_net, &tn, buf1, SIZEOF(buf1), do_sendmsg_on_log2long))
			&& io_is_sn(&sys_net))
		val.str = sys_net;
	outp = get_log_name(&val.str, INSERT);
	status = TRANS_LOG_NAME(&val.str, &tn, buf1, SIZEOF(buf1), dont_sendmsg_on_log2long);
	if ((SS_NORMAL != status) && (SS_NOLOGNAM != status))
	{
#		ifdef UNIX
		if (SS_LOG2LONG == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.str.len, val.str.addr, SIZEOF(buf1) - 1);
		else
#		endif
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
	if ((val.str.addr == sys_net.addr) && (pars.str.addr == (char *)open_params_list))
		/* sys$net is the only input thing that uses open_params_list */
		outp->iod = io_curr_device.in;
	/* For terminals and mailboxes and sockets, SYS$INPUT and SYS$OUTPUT may point to
		the same device.  If input is one of those, then check translated
		name for output against translated name for input;
		in that case they should be joined by their logical names */
	dev_type = io_curr_device.in->type;
	if (((tt == dev_type) || (mb == dev_type) || (gtmsocket == dev_type)) && same_device_check(tn, buf1))
		outp->iod = io_curr_device.in;
	if (!outp->iod)
	{
		if (status == SS_NOLOGNAM)
		{
			pars.str.len = SIZEOF(nolognam_params_list);
			pars.str.addr = (char *)nolognam_params_list;
		} else  if (status == SS_NORMAL)
		{
			pars.str.len = SIZEOF(open_params_list);
			pars.str.addr = (char *)open_params_list;
		}
		(*op_open_ptr)(&val, &pars, (mval *)&literal_zero, 0);
	}
	io_curr_device.out = io_std_device.out = outp->iod;
	term_setup(term_ctrl);
	io_std_device.out->pair = io_std_device;
	io_std_device.in->pair = io_std_device;
	io_std_device.out->perm = io_std_device.in->perm = TRUE;
	for (ln = io_root_log_name;  ln;  ln = ln->next)
		ln->iod = io_std_device.in;

	if (dollar_principal)
		dollar_principal->iod = io_std_device.in;
	pars.str.len = SIZEOF(no_params);
	pars.str.addr = (char *)&no_params;
#	ifdef UNIX
	/* If Unix and input/output device is one of rm (FILE) or ff (FIFO) or pi (PIPE) or gtmsocket (SOCKET),
	 * open device by default with NOWRAP option.
	 */
	if ((rm == dev_type) || (ff == dev_type) || (pi == dev_type) || (gtmsocket == dev_type))
	{
		pars.str.len = SIZEOF(nowrap_params_list);
		pars.str.addr = (char *)nowrap_params_list;
	}
#	endif
	val.str.len = io_curr_device.in->trans_name->len;
	val.str.addr = io_std_device.in->trans_name->dollar_io;
	op_use(&val, &pars);
	REVERT;
	return;
}
