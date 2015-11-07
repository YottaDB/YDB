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
#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iosp.h"
#include "iormdef.h"
#include "iormdefsp.h"
#include "gtmio.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif

#ifdef UNICODE_SUPPORTED
LITREF	mstr		chset_names[];
GBLREF	UConverter	*chset_desc[];
#endif
error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_DEVICEREADONLY);

void iorm_wteol(int4 x,io_desc *iod)
{
	int		i, fixed_pad, fixed_pad_bytes, bytes_per_char, avail_bytes, pad_size, res_size;
	int		status, outbytes;
	char		*outstr, temppad, temppadarray[2];
	d_rm_struct	*rm_ptr;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;

#ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (((d_rm_struct *)iod->dev_sp)->fifo)
	{
		dollarx_ptr = &(iod->pair.out->dollar.x);
		dollary_ptr = &(iod->pair.out->dollar.y);
		rm_ptr = (d_rm_struct *) (iod->pair.out)->dev_sp;
		iod = iod->pair.out;
	} else
#endif
	{
		dollarx_ptr = &(iod->dollar.x);
		dollary_ptr = &(iod->dollar.y);
		rm_ptr = (d_rm_struct *)iod->dev_sp;
	}
	if (rm_ptr->noread)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVICEREADONLY);
	if (!iod->dollar.zeof && !rm_ptr->fifo && !rm_ptr->pipe)
	{
		iod->dollar.za = 9;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}
	rm_ptr->lastop = RM_WRITE;
#ifdef __MVS__
	if (CHSET_BINARY != iod->process_chset)
	{
#endif
	for (i = 0; i < x ; i++)
	{
#ifdef UNICODE_SUPPORTED
		if (IS_UTF_CHSET(iod->ochset))
		{
			if (!rm_ptr->done_1st_write)
			{
				if (CHSET_UTF16 == iod->ochset)
				{	/* write BE BOM this is in raw bytes */
					DOWRITERL(rm_ptr->fildes, UTF16BE_BOM, UTF16BE_BOM_LEN, res_size);
					if (-1 == res_size)
					{
						int real_errno = errno;
						DOLLAR_DEVICE_WRITE(iod, real_errno);
						iod->dollar.za = 9;
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) real_errno);
					}
					iod->ochset = CHSET_UTF16BE;
					get_chset_desc(&chset_names[iod->ochset]);
				}
				rm_ptr->done_1st_write = TRUE;
			}
			if (rm_ptr->fixed)
			{
				fixed_pad = iod->width - *dollarx_ptr;
				bytes_per_char = (CHSET_UTF8 == iod->ochset) ? 1 : 2;
				fixed_pad_bytes = fixed_pad * bytes_per_char;
				avail_bytes = rm_ptr->recordsize - rm_ptr->out_bytes;
				if (avail_bytes < fixed_pad_bytes)
					fixed_pad = avail_bytes / bytes_per_char;
				for ( ; fixed_pad > 0; fixed_pad -= pad_size)
				{
					pad_size = (fixed_pad > TAB_BUF_SZ) ? TAB_BUF_SZ : fixed_pad;
					res_size = iorm_write_utf_ascii(iod, (char *)RM_SPACES_BLOCK, pad_size);
					assert(res_size == pad_size * bytes_per_char);
				}
				if (rm_ptr->out_bytes < rm_ptr->recordsize)
				{	/* padding bytes needed */
					temppad = rm_ptr->padchar;
					if (CHSET_UTF16LE == iod->ochset)
					{
						temppadarray[0] = temppad;
						temppadarray[1] = '\0';
						assert(2 == bytes_per_char);
					} else if (CHSET_UTF16BE == iod->ochset)
					{
						temppadarray[0] = '\0';
						temppadarray[1] = temppad;
						assert(2 == bytes_per_char);
					} else
					{
						assert(CHSET_UTF8 == iod->ochset);
						temppadarray[0] = temppad;
						assert(1 == bytes_per_char);
					}
					for ( ; rm_ptr->out_bytes < rm_ptr->recordsize; rm_ptr->out_bytes += bytes_per_char)
					{
						DOWRITERC(rm_ptr->fildes, temppadarray, bytes_per_char, status);
						if (0 != status)
						{
							DOLLAR_DEVICE_WRITE(iod, status);
							iod->dollar.za = 9;
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
						}
					}
					assert(rm_ptr->out_bytes == rm_ptr->recordsize);
				}
			} else
			{
				iorm_write_utf_ascii(iod, RMEOL,STRLEN(RMEOL));
			}
			rm_ptr->out_bytes = 0;
		} else
#endif
		if (rm_ptr->fixed)
		{
			for (fixed_pad = iod->width - *dollarx_ptr; fixed_pad > 0; fixed_pad -= res_size)
			{
				pad_size = (fixed_pad > TAB_BUF_SZ) ? TAB_BUF_SZ : fixed_pad;
				DOWRITERL(rm_ptr->fildes, RM_SPACES_BLOCK, pad_size, res_size);
				if (-1 == res_size)
				{
					int real_errno = errno;
					DOLLAR_DEVICE_WRITE(iod, real_errno);
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) real_errno);
				}
				assert(res_size == pad_size);
			}
		} else
		{
			DOWRITERC(rm_ptr->fildes, RMEOL, STRLEN(RMEOL), status);
			if (0 != status)
			{
				DOLLAR_DEVICE_WRITE(iod, status);
				iod->dollar.za = 9;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
		}
		*dollarx_ptr = 0;
	}
#ifdef __MVS__
	} else
		*dollarx_ptr = 0;	/* just reset $X for BINARY */
#endif
	iod->dollar.za = 0;
	*dollary_ptr += x;
	if (iod->length)
		*dollary_ptr %= iod->length;
	return;
}
