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

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_iconv.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdlib.h"

#include <errno.h>
#include <sys/ioctl.h>

#include "io.h"
#include "iosp.h"
#include "iotimer.h"
#include "io_params.h"
#include "gt_timer.h"
#include "copy.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "io_dev_dispatch.h"
#ifdef __MVS__
#include "iormdef.h"
#endif
#include "eintr_wrappers.h"
#include "mmemory.h"
#include "gtm_caseconv.h"
#include "outofband.h"
#include "wake_alarm.h"
#include "stringpool.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "gtmimagename.h"

#define  LOGNAME_LEN 255
/* avoid calling getservbyname for shell and Kerberos shell */
#define	RSHELL_PORT		514
#define	KSHELL_PORT		544

LITREF	unsigned char		io_params_size[];
GBLREF	dev_dispatch_struct	io_dev_dispatch[];
GBLREF	io_desc			*active_device;
GBLREF	mstr			sys_input;
GBLREF	mstr			sys_output;
GBLREF	bool			out_of_time;
GBLREF	int4			outofband;
GBLREF	int4			write_filter;
LITREF	mstr			chset_names[];

error_def(ERR_GETNAMEINFO);
error_def(ERR_GTMEISDIR);
error_def(ERR_TEXT);

bool io_open_try(io_log_name *naml, io_log_name *tl, mval *pp, int4 timeout, mval *mspace)	/* timeout in seconds */
{
	uint4		status;
	int4		msec_timeout;	/* timeout in milliseconds */
	mstr		tn;		/* translated name */
	int		oflag;
	params		*param;
	unsigned char	ch;
	bool		timed = FALSE;
	int		file_des;
#ifdef __MVS__
	int		file_des_w = -2;	/*	fifo write	*/
	uint4		status_w;
	d_rm_struct	*d_rm_out, *d_rm_in;
#endif
	boolean_t	filecreated = FALSE;
	TID		timer_id;
	struct stat	outbuf;
	char		*buf, namebuf[LOGNAME_LEN + 1];
	unsigned char	dev_type[MAX_DEV_TYPE_LEN];
	d_mt_struct	*mt_ptr;
	iosb		io_status_blk;
	mstr		chset_mstr;
	int		umask_orig, umask_creat;
	int		char_or_block_special;
	int		stat_res;
	int		fstat_res;

	int		p_offset, len;
	boolean_t	mknod_err , stat_err, dir_err;
	int 		save_mknod_err, save_stat_err;

	int		sockstat, sockoptval;
	in_port_t	sockport;
	GTM_SOCKLEN_TYPE	socknamelen;
	struct sockaddr_storage	sockname;
	GTM_SOCKLEN_TYPE	sockoptlen;
	boolean_t	ichset_specified, ochset_specified;
	int		errcode;
	unsigned int	port_len;
	char		port_buffer[NI_MAXSERV], *port_ptr;

	mt_ptr = NULL;
	char_or_block_special = FALSE;
	out_of_time = FALSE;
	file_des = -2;
	oflag = 0;
	mknod_err = FALSE;
	stat_err = FALSE;
	dir_err = FALSE;
	tn.len = tl->len;
	if (tn.len > LOGNAME_LEN)
		tn.len = LOGNAME_LEN;
	tn.addr = tl->dollar_io;
	memcpy(namebuf, tn.addr, tn.len);
	namebuf[tn.len] = '\0';
	buf = namebuf;
	timer_id = (TID)io_open_try;
	if (NO_M_TIMEOUT == timeout)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (!msec_timeout)
			timed = FALSE;
	}
	if (0 == naml->iod)
	{
		if (0 == tl->iod)
		{
			tl->iod =  (io_desc *)malloc(SIZEOF(io_desc));
			memset((char*)tl->iod, 0, SIZEOF(io_desc));
			tl->iod->pair.in  = tl->iod;
			tl->iod->pair.out = tl->iod;
			tl->iod->trans_name = tl;
			tl->iod->type = n_io_dev_types;
			p_offset = 0;
			while(iop_eol != *(pp->str.addr + p_offset))
			{
				ch = *(pp->str.addr + p_offset++);
				if (iop_fifo == ch)
					tl->iod->type = ff;
				else  if (iop_sequential == ch)
					tl->iod->type = rm;
				p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
					     (unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
			}
			if (ff == tl->iod->type)
			{
				/* fifo with RW permissions for owner, group, other */
				if ((-1 != MKNOD(buf, FIFO_PERMISSION, 0))
#ifdef __MVS__
				    || (EEXIST == errno)
#endif
				    )
				{
#ifdef __MVS__
					if (EEXIST != errno)
						filecreated = TRUE;
					/*	create another one for fifo write	*/
					tl->iod->pair.out = (io_desc *)malloc(SIZEOF(io_desc));
					(tl->iod->pair.out)->pair.in = tl->iod;
					(tl->iod->pair.out)->pair.out = (tl->iod->pair.out);
					(tl->iod->pair.out)->trans_name = tl;
					(tl->iod->pair.out)->type = ff;
#else
					filecreated = TRUE;
#endif
				} else  if (EEXIST != errno)
				{
					mknod_err = TRUE;
					save_mknod_err = errno;
					/* This save_err will be checked immediately after error_handler is set;
					   In that way error handler will get the chance to handle any error */
				}
			}
		}
		if ((n_io_dev_types == tl->iod->type) && mspace && mspace->str.len)
		{
			lower_to_upper(dev_type, (uchar_ptr_t)mspace->str.addr, mspace->str.len);

			if (((SIZEOF("SOCKET") - 1) == mspace->str.len)
				 && (0 == MEMCMP_LIT(dev_type, "SOCKET")))
				tl->iod->type = gtmsocket;
			else if (((SIZEOF("PIPE") - 1) == mspace->str.len)
			    && (0 == MEMCMP_LIT(dev_type, "PIPE")))
				tl->iod->type = pi;
			else if (((SIZEOF("TCP") - 1) == mspace->str.len)
			    && (0 == MEMCMP_LIT(dev_type, "TCP")))
				tl->iod->type = tcp;
			else
				tl->iod->type = us;
		}
		if (n_io_dev_types == tl->iod->type)
		{
			if (0 == memvcmp(tn.addr, tn.len, sys_input.addr, sys_input.len))
				file_des = 0;
			else if (0 == memvcmp(tn.addr, tn.len, sys_output.addr, sys_output.len))
				file_des = 1;
			if (0 == file_des || 1 == file_des)
			{
				FSTAT_FILE(file_des, &outbuf, fstat_res);
				if (-1 == fstat_res)
				{	/* save errno to be handled by exception handler if set */
					save_stat_err = errno;
					stat_err = TRUE;
				}
			}
			else if (0 == memvcmp(tn.addr, tn.len, LIT_AND_LEN("/dev/null")))
				tl->iod->type = nl;
			else
			{
				STAT_FILE(buf, &outbuf, stat_res);
				if ((-1 == stat_res) && (n_io_dev_types == tl->iod->type))
				{
					if (ENOENT == errno)
					{
						tl->iod->type = rm;
						filecreated = TRUE;
					} else
					{	/* save errno to be checked later */
						save_stat_err = errno;
						stat_err = TRUE;
					}
				}
			}
		}

		if (n_io_dev_types == tl->iod->type)
		{
			if (!stat_err)
			{
				switch(outbuf.st_mode & S_IFMT)
				{
					case S_IFCHR:
					case S_IFBLK:
						char_or_block_special = TRUE;
						break;
					case S_IFIFO:
						tl->iod->type = ff;
						break;
					case S_IFDIR:
						dir_err = TRUE;			/* directories should not be opened */
						/* no break in order to set iod->type value */
					case S_IFREG:
						tl->iod->type = rm;
						break;
					case S_IFSOCK:
						/*	If SOCK_STREAM, AF_INET, and not [kr]shell port assume we were
							started by inetd.
							We are not able to trigger exception until active_device set
							and exception parsed which is not done for fd == 0/1 which
							is from io_init called from gtm_startup before condition handlers
							are setup so we can't report errors, just will be treated as rm.
							Note the fall through if the above conditions are not true to
							maintain compatibility for rshd startup PER 3252.
							28-JUL-1995 14:24:31 FERTIG REPLACE IO_OPEN_TRY.C(18)
							"PER 3252: fix problems starting up mumps from an ""rsh"" or ""remsh"""
						*/
						sockoptlen = SIZEOF(sockoptval);
						sockstat = getsockopt(file_des, SOL_SOCKET, SO_TYPE, &sockoptval,
								      (GTM_SOCKLEN_TYPE *)&sockoptlen);
						if (!sockstat && SOCK_STREAM == sockoptval)
						{
							socknamelen = SIZEOF(sockname);
							sockstat = getsockname(file_des,
									       (struct sockaddr *)&sockname,
									       (GTM_SOCKLEN_TYPE *)&socknamelen);
							if (!sockstat && ((AF_INET == ((sockaddr_ptr)&sockname)->sa_family)
									 || (AF_INET6 == ((sockaddr_ptr)&sockname)->sa_family)))
							{
								port_len = NI_MAXSERV;
								if (0 != (errcode = getnameinfo((struct sockaddr *)&sockname,
												socknamelen, NULL, 0,
												port_buffer, port_len,
												NI_NUMERICHOST)))
								{
									RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
									return FALSE;
								}
								sockport=atoi(port_buffer);
								if (RSHELL_PORT != sockport && KSHELL_PORT != sockport)
								{
									tl->iod->type = gtmsocket;
									break;
								}
							}
						}
						/* fall through */
					case 0:
						tl->iod->type = ff;
						break;
					default:
						break;
				}
			}
		}
		naml->iod = tl->iod;
	}
	assert((0 <= naml->iod->state) && (n_io_dev_states > naml->iod->state));
	active_device = naml->iod;

	if ((-2 == file_des) && (dev_open != naml->iod->state) && (us != naml->iod->type)
	    && (tcp != naml->iod->type) && (gtmsocket != naml->iod->type) && (pi != naml->iod->type))
	{
		oflag |= (O_RDWR | O_CREAT | O_NOCTTY);
		p_offset = 0;
		ichset_specified = ochset_specified = FALSE;
		while(iop_eol != *(pp->str.addr + p_offset))
		{
			assert((params) *(pp->str.addr + p_offset) < (params)n_iops);
			switch ((ch = *(pp->str.addr + p_offset++)))
			{
				case iop_exception:
				{
					naml->iod->error_handler.len = *(pp->str.addr + p_offset);
					naml->iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
					s2pool(&naml->iod->error_handler);
					break;
				}
				case iop_append:
					if (rm == naml->iod->type)
						oflag |= O_APPEND;
					break;
				case iop_contiguous:
					break;
				case iop_newversion:
					if ((dev_open != naml->iod->state) && (rm == naml->iod->type))
						oflag |= O_TRUNC;
					break;
				case iop_readonly:
					oflag  &=  ~(O_RDWR | O_CREAT | O_WRONLY);
					oflag  |=  O_RDONLY;
					break;
				case iop_writeonly:
#					ifdef __MVS__
					if (ff != naml->iod->type)
#					endif
					{
						oflag  &= ~(O_RDWR | O_RDONLY);
						oflag  |= O_WRONLY | O_CREAT;
					}
					break;
				case iop_ipchset:
#ifdef KEEP_zOS_EBCDIC
					if ( (iconv_t)0 != naml->iod->input_conv_cd )
					{
						ICONV_CLOSE_CD(naml->iod->input_conv_cd);
					}
					SET_CODE_SET(naml->iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
					if (DEFAULT_CODE_SET != naml->iod->in_code_set)
						ICONV_OPEN_CD(naml->iod->input_conv_cd,
							      (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#endif
					break;
				case iop_opchset:
#ifdef KEEP_zOS_EBCDIC
					if ( (iconv_t)0 != naml->iod->output_conv_cd)
					{
						ICONV_CLOSE_CD(naml->iod->output_conv_cd);
					}
					SET_CODE_SET(naml->iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
					if (DEFAULT_CODE_SET != naml->iod->out_code_set)
						ICONV_OPEN_CD(naml->iod->output_conv_cd, INSIDE_CH_SET,
							      (char *)(pp->str.addr + p_offset + 1));
#endif
					break;
				case iop_m:
				case iop_utf8:
				case iop_utf16:
				case iop_utf16be:
				case iop_utf16le:
					break;
				default:
					break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				     (unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}

		/* Check the saved error from mknod() for fifo, also saved error from fstat() or stat()
		   so error handler (if set)  can handle it */
		if (ff == tl->iod->type  && mknod_err)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_mknod_err);
		/* Error from either stat() or fstat() function */
		if (stat_err)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_stat_err);
		/* Error from trying to open a dir */
		if (dir_err)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_GTMEISDIR, 2, LEN_AND_STR(buf));


		if (timed)
			start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);

		/* RW permissions for owner and others as determined by umask. */
		umask_orig = umask(000);	/* determine umask (destructive) */
		(void)umask(umask_orig);	/* reset umask */
		umask_creat = 0666 & ~umask_orig;

		/*
		 * no OPEN EINTR macros in the following while loop  due to complex error checks and processing between
		 * top of while and calls to OPEN3
		 */
		while ((-1 == (file_des = OPEN3(buf, oflag, umask_creat))) && !out_of_time)
		{
			if (timed && (0 == msec_timeout))
				out_of_time = TRUE;
			if (outofband)
				outofband_action(FALSE);
			if (   EINTR == errno
			       || ETXTBSY == errno
			       || ENFILE == errno
			       || EBUSY == errno
			       || ((mb == naml->iod->type) && (ENXIO == errno)))
				continue;
			else
			{
#ifdef __MVS__
				if (EINVAL == errno && ff == tl->iod->type && (oflag & O_RDONLY) && (oflag & O_WRONLY))
				{
					int	oflag_clone;
					int	fcntl_res;
					/* oflag must be O_RDWR, set it to be O_RDONLY 	*/
					oflag &= ~O_WRONLY;
					oflag |= O_NONBLOCK;
					while ((-1 == (file_des = OPEN3(buf, oflag, umask_creat))) && !out_of_time)
					{
						if (0 == msec_timeout)
							out_of_time = TRUE;
						if (outofband)
							outofband_action(FALSE);
						if (   EINTR == errno
						       || ETXTBSY == errno
						       || ENFILE == errno
						       || EBUSY == errno
						       || ((mb == naml->iod->type) && (ENXIO == errno)))
							continue;
						else
							break;
					}
					FCNTL2(file_des, F_GETFL, oflag_clone);
					FCNTL3(file_des, F_SETFL, (oflag_clone & ~O_NONBLOCK), fcntl_res);

					/* oflag was just made O_RDONLY, now set it to be O_WRONLY */
					oflag |= O_WRONLY;
					oflag &= ~O_RDONLY;
					while ((-1 == (file_des_w = OPEN3(buf, oflag, umask_creat))) && !out_of_time)
					{
						if (0 == msec_timeout)
							out_of_time = TRUE;
						if (outofband)
							outofband_action(FALSE);
						if (   EINTR == errno
						       || ETXTBSY == errno
						       || ENFILE == errno
						       || EBUSY == errno
						       || ((mb == naml->iod->type) && (ENXIO == errno)))
							continue;
						else
							break;
					}
				}
#endif
				break;
			}
		}
		if (out_of_time && (-1 == file_des))
		{
			if ((ff == tl->iod->type) && filecreated)
			{	/* The FIFO didn't really exist so remove all traces of it */
				UNLINK(tl->iod->trans_name->dollar_io);
				tl->iod->type = rm;
				remove_rms(tl->iod);
			}
			return FALSE;
		}
	}
	if (timed && !out_of_time)
		cancel_timer(timer_id);

#ifdef KEEP_zOS_EBCDIC
	if ((tcp != naml->iod->type) && (gtmsocket != naml->iod->type))
	{
		SET_CODE_SET(naml->iod->in_code_set, OUTSIDE_CH_SET);
		if (DEFAULT_CODE_SET != naml->iod->in_code_set)
			ICONV_OPEN_CD(naml->iod->input_conv_cd, OUTSIDE_CH_SET, INSIDE_CH_SET);
		SET_CODE_SET(naml->iod->out_code_set, OUTSIDE_CH_SET);
		if (DEFAULT_CODE_SET != naml->iod->out_code_set)
			ICONV_OPEN_CD(naml->iod->output_conv_cd, INSIDE_CH_SET, OUTSIDE_CH_SET);
	} else
	{
		SET_CODE_SET(naml->iod->out_code_set, INSIDE_CH_SET);
		if (DEFAULT_CODE_SET != naml->iod->out_code_set)
			ICONV_OPEN_CD(naml->iod->output_conv_cd, OUTSIDE_CH_SET, INSIDE_CH_SET);
		SET_CODE_SET(naml->iod->in_code_set, INSIDE_CH_SET);
		if (DEFAULT_CODE_SET != naml->iod->in_code_set)
			ICONV_OPEN_CD(naml->iod->input_conv_cd, INSIDE_CH_SET, OUTSIDE_CH_SET);
	}
#endif
	if (-1 == file_des)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);

	if (n_io_dev_types == naml->iod->type)
	{
		/* On AIX, /dev/{,*}random are of 'terminal type'.Hence define its type before
		 * calling isatty()
		 */
		if ((0 == memvcmp(tn.addr, tn.len, LIT_AND_LEN("/dev/random")))
			|| (0 == memvcmp(tn.addr, tn.len, LIT_AND_LEN("/dev/urandom"))))
				tl->iod->type = rm;
		else if (isatty(file_des))
			naml->iod->type = tt;
		else if (char_or_block_special && file_des > 2)
			if (0 == memvcmp(tn.addr, tn.len, LIT_AND_LEN("/dev/zero")))
				tl->iod->type = rm;
			else
				/* assume mag tape */
				naml->iod->type = mt;
		else
			naml->iod->type = rm;
	}
	assert(naml->iod->type < n_io_dev_types);
	naml->iod->disp_ptr = &io_dev_dispatch[naml->iod->type];
	if (dev_never_opened == naml->iod->state)
	{
		naml->iod->wrap = DEFAULT_IOD_WRAP;
		naml->iod->width = DEFAULT_IOD_WIDTH;
		naml->iod->length = DEFAULT_IOD_LENGTH;
		naml->iod->write_filter = write_filter;
	}
	if (dev_open != naml->iod->state)
	{
		naml->iod->dollar.x = 0;
		naml->iod->dollar.y = 0;
		naml->iod->dollar.za = 0;
		naml->iod->dollar.zb[0] = 0;
		naml->iod->dollar.key[0] = 0;
		naml->iod->dollar.device[0] = 0;
	}
#ifdef __MVS__
	/*	copy over the content of tl->iod(naml->iod) to (tl->iod->pair.out)	*/
	if (ff == tl->iod->type)
	{
		*(tl->iod->pair.out) = *(tl->iod);
		(tl->iod->pair.out)->pair.in = tl->iod;
		(tl->iod->pair.out)->pair.out = (tl->iod->pair.out);
		assert((0 <= tl->iod->pair.out->state) && (n_io_dev_states > tl->iod->pair.out->state));
		if (-1 < file_des_w)
		{
			io_desc		*io_ptr;
			io_log_name	dev_name;	/*	dummy	*/

			io_ptr = (tl->iod->pair.out);
			assert(io_ptr != tl->iod->pair.in);
			assert(NULL != io_ptr);
			assert(io_ptr->state >= 0 && io_ptr->state < n_io_dev_states);
			assert(ff == io_ptr->type);
			dev_name.iod = tl->iod->pair.out;
			if (TRUE == ioff_open(&dev_name, pp, file_des_w, mspace, timeout))
				(tl->iod->pair.out)->state = dev_open;
			else if (dev_open == (tl->iod->pair.out)->state)
				(tl->iod->pair.out)->state = dev_closed;
		}
		else if ((-2 == file_des_w) && (O_WRONLY == (oflag & O_ACCMODE)))
			active_device = naml->iod = tl->iod->pair.out;
		if (0 == file_des_w)
			(tl->iod->pair.out)->dollar.zeof = TRUE;
	}
#endif
	naml->iod->newly_created = filecreated;
	status = (naml->iod->disp_ptr->open)(naml, pp, file_des, mspace, timeout);
	if (TRUE == status)
		naml->iod->state = dev_open;
	else
	{
		if ((dev_open == naml->iod->state) && (gtmsocket != naml->iod->type))
			naml->iod->state = dev_closed;
		if ((gtmsocket == naml->iod->type) && naml->iod->newly_created)
		{
			assert (naml->iod->state != dev_open);
			iosocket_destroy(naml->iod);
			active_device = 0;
			return status;
		}
	}
#ifdef __MVS__
	d_rm_out = tl->iod->pair.out->dev_sp;
	d_rm_in = tl->iod->pair.in->dev_sp;
	if ((ff == tl->iod->pair.in->type) || (ff == tl->iod->pair.out->type))
	{
		if (NULL == d_rm_in)
		{
			tl->iod->pair.in->type = rm;
			tl->iod->pair.in->state = tl->iod->pair.out->state;
			tl->iod->pair.in->dev_sp = d_rm_out;
		}
		else if (NULL == d_rm_out)
		{
			tl->iod->pair.out->type = rm;
			tl->iod->pair.out->state = tl->iod->pair.in->state;
			tl->iod->pair.out->dev_sp = d_rm_in;
			d_rm_in->read_fildes = FD_INVALID;
		}
	}
	if (-1 < file_des_w)
	{
		d_rm_out->read_fildes = d_rm_in->fildes;
		d_rm_out->read_filstr = d_rm_in->filstr;
		free(d_rm_in);
		tl->iod->pair.in->dev_sp = d_rm_out;
	}

#endif
	if (1 == file_des)
		naml->iod->dollar.zeof = TRUE;
	active_device = 0;
	naml->iod->newly_created = FALSE;

	if ((NO_M_TIMEOUT != timeout) && IS_MCODE_RUNNING)
		return (status);
	else
		return TRUE;
}
