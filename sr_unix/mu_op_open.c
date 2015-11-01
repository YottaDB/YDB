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

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "gtm_stat.h"
#include "gtm_iconv.h"

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "cryptdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "copy.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "mupip_io_dev_dispatch.h"
#ifdef __MVS__
#include "iormdef.h"
#endif
#include "eintr_wrappers.h"
#include "mmemory.h"
#include "mu_op_open.h"
#include "trans_log_name.h"

#define  LOGNAME_LEN 255

LITREF dev_dispatch_struct  	io_dev_dispatch_mupip[];
GBLREF bool			licensed;
GBLREF int4			lkid,lid;

LITREF	unsigned char		io_params_size[];
GBLREF	bool			run_time;
GBLDEF	io_desc			*active_device;
GBLREF	mstr			sys_input;
GBLREF	mstr			sys_output;

static bool mu_open_try(io_log_name *, io_log_name *, mval *, mval *);

/*	The third parameter is dummy to keep the inteface same as op_open	*/
int mu_op_open(mval *v, mval *p, int t, mval *mspace)
{
	char		buf1[MAX_TRANS_NAME_LEN]; /* buffer to hold translated name */
	io_log_name	*naml;		/* logical record for passed name */
	io_log_name	*tl;		/* logical record for translated name */
	uint4		stat;		/* status */
	mstr		tn;			/* translated name 	  */
	error_def(LP_NOTACQ);			/* bad license 		  */
	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	if (mspace)
		MV_FORCE_STR(mspace);

	if (t < 0)
		t = 0;
	assert((unsigned char)*p->str.addr < n_iops);
	naml = get_log_name(&v->str, INSERT);
	if (naml->iod != 0)
		tl = naml;
	else
	{
#ifdef	NOLICENSE
	licensed= TRUE ;
#else
		CRYPT_CHKSYSTEM;
		if (!licensed || LP_CONFIRM(lid,lkid)==LP_NOTACQ)
		{
			licensed= FALSE ;
		}
#endif
		switch(stat = trans_log_name(&v->str, &tn, &buf1[0]))
		{
		case SS_NORMAL:
			tl = get_log_name(&tn, INSERT);
			break;
		case SS_NOLOGNAM:
			tl = naml;
			break;
		default:
			rts_error(VARLSTCNT(1) stat);
		}
	}
	stat = mu_open_try(naml, tl, p, mspace);
	return (stat);
}


static bool mu_open_try(io_log_name *naml, io_log_name *tl, mval *pp, mval *mspace)
{
	uint4		status;
	int4		size;
	mstr		tn;		/* translated name */
	int		oflag;
	unsigned char	ch;
	int		file_des;
	struct stat	outbuf;
	char		*buf, namebuf[LOGNAME_LEN + 1];
	d_mt_struct	*mt_ptr;
	int		umask_orig, umask_creat;
	int		char_or_block_special;
	int		fstat_res;
	int		save_errno;
	int		p_offset;

error_def(ERR_SYSCALL);

	mt_ptr = NULL;
	char_or_block_special = FALSE;
	file_des = -2;
	oflag = 0;
	tn.len = tl->len;
	if (tn.len > LOGNAME_LEN)
		tn.len = LOGNAME_LEN;
	tn.addr = tl->dollar_io;
	memcpy(namebuf, tn.addr, tn.len);
	namebuf[tn.len] = '\0';
	buf = namebuf;
	if (0 == naml->iod)
	{
		if (0 == tl->iod)
		{
			tl->iod =  (io_desc *)malloc(sizeof(io_desc));
			memset((char*)tl->iod, 0, sizeof(io_desc));
			tl->iod->pair.in  = tl->iod;
			tl->iod->pair.out = tl->iod;
			tl->iod->trans_name = tl;
			tl->iod->type = n_io_dev_types;
			p_offset = 0;
			while(iop_eol != *(pp->str.addr + p_offset))
			{
				ch = *(pp->str.addr + p_offset++);
				if (iop_sequential == ch)
					tl->iod->type = rm;

				if (IOP_VAR_SIZE == io_params_size[ch])
					p_offset += *(pp->str.addr + p_offset) + 1;
				else
					p_offset += io_params_size[ch];
			}
		}
		if ((n_io_dev_types == tl->iod->type) && mspace && mspace->str.len)
		{
			tl->iod->type = us;
		}
		if (n_io_dev_types == tl->iod->type)
		{
			if (0 == memvcmp(tn.addr, tn.len, sys_input.addr, sys_input.len))
			{
				file_des = 0;
				FSTAT_FILE(file_des, &outbuf, fstat_res);
				if (-1 == fstat_res)
				{
				  save_errno = errno;
				  rts_error(VARLSTCNT(8) ERR_SYSCALL, 5,
						  RTS_ERROR_LITERAL("fstat()"),
						  CALLFROM, save_errno);
				}
			} else
			{
				if (0 == memvcmp(tn.addr, tn.len, sys_output.addr, sys_output.len))
				{
					file_des = 1;
					FSTAT_FILE(file_des, &outbuf, fstat_res);
					if (-1 == fstat_res)
				{
				  save_errno = errno;
				  rts_error(VARLSTCNT(8) ERR_SYSCALL, 5,
						  RTS_ERROR_LITERAL("fstat()"),
						  CALLFROM, save_errno);
				}
				} else  if (0 == memvcmp(tn.addr, tn.len, "/dev/null", 9))
					tl->iod->type = nl;
				else  if ((-1 == Stat(buf, &outbuf)) && (n_io_dev_types == tl->iod->type))
				{

					if (ENOENT == errno)
						tl->iod->type = rm;
					else
					{
				  	     save_errno = errno;
					     rts_error(VARLSTCNT(8) ERR_SYSCALL, 5,
						  RTS_ERROR_LITERAL("fstat()"),
						  CALLFROM, save_errno);
					}
				}
			}
		}
		if (n_io_dev_types == tl->iod->type)
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
				case S_IFREG:
				case S_IFDIR:
					tl->iod->type = rm;
					break;
				case S_IFSOCK:
				case 0:
					tl->iod->type = ff;
					break;
				default:
					break;
			}
		}
		naml->iod = tl->iod;
	}
	active_device = naml->iod;

	if ((-2 == file_des) && (dev_open != naml->iod->state) && (us != naml->iod->type) && (tcp != naml->iod->type))
	{
		oflag |= (O_RDWR | O_CREAT | O_NOCTTY);
		size = 0;
		p_offset = 0;
		while(iop_eol != *(pp->str.addr + p_offset))
		{
			assert((params) *(pp->str.addr + p_offset) < (params)n_iops);
			switch ((ch = *(pp->str.addr + p_offset++)))
			{
				case iop_allocation:
					if (rm == naml->iod->type)
					{
						GET_LONG(size, pp->str.addr + p_offset);
						size *= 512;
					}
					break;
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
					oflag  &= ~(O_RDWR | O_RDONLY);
					oflag  |= O_WRONLY | O_CREAT;
					break;
				case iop_ipchset:
					{
						if ( (iconv_t)0 != naml->iod->input_conv_cd )
						{
							ICONV_CLOSE_CD(naml->iod->input_conv_cd);
						}
						SET_CODE_SET(naml->iod->in_code_set,
								(char *)(pp->str.addr + p_offset + 1));
						if (DEFAULT_CODE_SET != naml->iod->in_code_set)
							ICONV_OPEN_CD(naml->iod->input_conv_cd,
								(char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
						break;
					}
				case iop_opchset:
					{
						if ( (iconv_t)0 != naml->iod->output_conv_cd)
						{
							ICONV_CLOSE_CD(naml->iod->output_conv_cd);
						}
						SET_CODE_SET(naml->iod->out_code_set,
								(char *)(pp->str.addr + p_offset + 1));
						if (DEFAULT_CODE_SET != naml->iod->out_code_set)
							ICONV_OPEN_CD(naml->iod->output_conv_cd, INSIDE_CH_SET,
								(char *)(pp->str.addr + p_offset + 1));
						break;
					}
				default:
					break;
			}
			if (IOP_VAR_SIZE == io_params_size[ch])
				p_offset += *(pp->str.addr + p_offset) + 1;
			else
				p_offset += io_params_size[ch];
		}
		/* RW permissions for owner and others as determined by umask. */
		umask_orig = umask(000);	/* determine umask (destructive) */
		(void)umask(umask_orig);	/* reset umask */
		umask_creat = 0666 & ~umask_orig;
               /*
                * the check for EINTR below is valid and should not be converte d to an EINTR
                * wrapper macro, due to the other errno values being checked.
                */
		while ((-1 == (file_des = OPEN4(buf, oflag, umask_creat, size))))
		{
			if (   EINTR == errno
			    || ETXTBSY == errno
			    || ENFILE == errno
			    || EBUSY == errno
			    || ((mb == naml->iod->type) && (ENXIO == errno)))
				continue;
			else
				break;
		}

		if (-1 == file_des)
			return FALSE;
	}

	assert (tcp != naml->iod->type);
	SET_CODE_SET(naml->iod->in_code_set, OUTSIDE_CH_SET);
	if (DEFAULT_CODE_SET != naml->iod->in_code_set)
		ICONV_OPEN_CD(naml->iod->input_conv_cd, OUTSIDE_CH_SET, INSIDE_CH_SET);
	SET_CODE_SET(naml->iod->out_code_set, OUTSIDE_CH_SET);
	if (DEFAULT_CODE_SET != naml->iod->out_code_set)
		ICONV_OPEN_CD(naml->iod->output_conv_cd, INSIDE_CH_SET, OUTSIDE_CH_SET);

	/* smw 99/12/18 not possible to be -1 here */
	if (-1 == file_des)
	{
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5,
			RTS_ERROR_LITERAL("open()"),
			CALLFROM, save_errno);
	}

	if (n_io_dev_types == naml->iod->type)
	{
	        if (isatty(file_des))
			naml->iod->type = tt;
		else if (char_or_block_special && file_des > 2)
			/* assume mag tape */
			naml->iod->type = mt;
		else
			naml->iod->type = rm;
	}
	assert(naml->iod->type < n_io_dev_types);
	naml->iod->disp_ptr = &io_dev_dispatch_mupip[naml->iod->type];
	active_device = naml->iod;
	if (dev_never_opened == naml->iod->state)
	{
		naml->iod->wrap = TRUE;
		naml->iod->width = 80;
		naml->iod->length = 55;
	}
	if (dev_open != naml->iod->state)
	{
		naml->iod->dollar.x = 0;
		naml->iod->dollar.y = 0;
		naml->iod->dollar.za = 0;
		naml->iod->dollar.zb[0] = 0;
	}
	status = (naml->iod->disp_ptr->open)(naml, pp, file_des, mspace, NO_M_TIMEOUT);
	if (TRUE == status)
		naml->iod->state = dev_open;
	else  if (dev_open == naml->iod->state)
		naml->iod->state = dev_closed;
	if (1 == file_des)
		naml->iod->dollar.zeof = TRUE;
	active_device = 0;

	if (run_time)
		return (status);
	else
		return TRUE;
}
