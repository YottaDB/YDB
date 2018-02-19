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
#include "gtmcrypt.h"
#include "send_msg.h"
#include "error.h"

#ifdef UNICODE_SUPPORTED
LITREF mstr		chset_names[];
GBLREF UConverter	*chset_desc[];
#endif

error_def(ERR_CRYPTBADWRTPOS);
error_def(ERR_DEVICEREADONLY);
error_def(ERR_NOPRINCIO);
error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_SYSCALL);

void iorm_wteol(int4 x,io_desc *iod)
{
	int		i, fixed_pad, fixed_pad_bytes, bytes_per_char, avail_bytes, pad_size, res_size;
	int		status, outbytes, len;
	char		*outstr, temppad, temppadarray[2], *out_ptr;
	d_rm_struct	*rm_ptr;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;
	struct stat	statbuf;
	int		fstat_res, save_errno;
	boolean_t	ch_set;

	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
#	ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (((d_rm_struct *)iod->dev_sp)->fifo)
	{
		dollarx_ptr = &(iod->pair.out->dollar.x);
		dollary_ptr = &(iod->pair.out->dollar.y);
		rm_ptr = (d_rm_struct *) (iod->pair.out)->dev_sp;
		iod = iod->pair.out;
	} else
#	endif
	{
		dollarx_ptr = &(iod->dollar.x);
		dollary_ptr = &(iod->dollar.y);
		rm_ptr = (d_rm_struct *)iod->dev_sp;
	}
	if (rm_ptr->read_only)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVICEREADONLY);
	if ((!rm_ptr->fifo) && (!rm_ptr->is_pipe) && rm_ptr->output_encrypted)
	{
		if (!iod->dollar.zeof)
		{
	 		iod->dollar.za = 9;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTTOEOFONPUT);
		} else
		{	/* If there have not been any writes, and input encryption attributes are different from those for output,
			 * and the file is not empty, disallow the write.
			 */
			if ((!rm_ptr->write_occurred)
			    && ((!rm_ptr->input_encrypted)
				|| (rm_ptr->input_iv.len != rm_ptr->output_iv.len)
				|| memcmp(rm_ptr->input_iv.addr, rm_ptr->output_iv.addr, rm_ptr->input_iv.len)
				|| (((rm_ptr->input_key.len != rm_ptr->output_key.len)
				    || memcmp(rm_ptr->input_key.addr, rm_ptr->output_key.addr, rm_ptr->input_key.len))
					&& (!GTMCRYPT_SAME_KEY(rm_ptr->input_cipher_handle, rm_ptr->output_cipher_handle)))))
			{
				FSTAT_FILE(rm_ptr->fildes, &statbuf, fstat_res);
				if (-1 == fstat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"),
						CALLFROM, save_errno);
				}
				if (0 != statbuf.st_size)
				{
					SET_DOLLARDEVICE_ERRSTR(iod, ONE_COMMA_CRYPTBADWRTPOS);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CRYPTBADWRTPOS);
				}
			}
		}
	}
	rm_ptr->lastop = RM_WRITE;
#	ifdef __MVS__
	if (CHSET_BINARY != iod->process_chset)
	{
#	endif
	for (i = 0; i < x; i++)
	{
#		ifdef UNICODE_SUPPORTED
		if (IS_UTF_CHSET(iod->ochset))
		{
			if (!rm_ptr->done_1st_write)
			{
				if (CHSET_UTF16 == iod->ochset)
				{	/* write BE BOM this is in raw bytes */
					out_ptr = UTF16BE_BOM;
					if (rm_ptr->output_encrypted)
					{
						REALLOC_CRYPTBUF_IF_NEEDED(UTF16BE_BOM_LEN);
						WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out_ptr, UTF16BE_BOM_LEN,
							pvt_crypt_buf.addr);
						out_ptr = pvt_crypt_buf.addr;
					}
					DOWRITERL_RM(rm_ptr, out_ptr, UTF16BE_BOM_LEN, res_size);
					ISSUE_NOPRINCIO_IF_NEEDED_RM(res_size, <=, iod);
					rm_ptr->write_occurred = TRUE;
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
						out_ptr = temppadarray;
						if (rm_ptr->output_encrypted)
						{
							REALLOC_CRYPTBUF_IF_NEEDED(bytes_per_char);
							WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out_ptr, bytes_per_char,
								pvt_crypt_buf.addr);
							out_ptr = pvt_crypt_buf.addr;
						}
						DOWRITERC_RM(rm_ptr, out_ptr, bytes_per_char, status);
						ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
						rm_ptr->write_occurred = TRUE;
					}
					assert(rm_ptr->out_bytes == rm_ptr->recordsize);
				}
			} else
			{
				iorm_write_utf_ascii(iod, RMEOL, STRLEN(RMEOL));
			}
			rm_ptr->out_bytes = 0;
		} else
#		endif
		if (rm_ptr->fixed)
		{
			for (fixed_pad = iod->width - *dollarx_ptr; fixed_pad > 0; fixed_pad -= res_size)
			{
				pad_size = (fixed_pad > TAB_BUF_SZ) ? TAB_BUF_SZ : fixed_pad;
				out_ptr = (char *)(RM_SPACES_BLOCK);
				if (rm_ptr->output_encrypted)
				{
					REALLOC_CRYPTBUF_IF_NEEDED(pad_size);
					WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out_ptr, pad_size, pvt_crypt_buf.addr);
					out_ptr = pvt_crypt_buf.addr;
				}
				DOWRITERL_RM(rm_ptr, out_ptr, pad_size, res_size);
				ISSUE_NOPRINCIO_IF_NEEDED_RM(res_size, <=, iod);
				rm_ptr->write_occurred = TRUE;
				assert(res_size == pad_size);
			}
		} else
		{
			out_ptr = RMEOL;
			if (rm_ptr->output_encrypted)
			{
				REALLOC_CRYPTBUF_IF_NEEDED(RMEOL_LEN);
				WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out_ptr, RMEOL_LEN, pvt_crypt_buf.addr);
				out_ptr = pvt_crypt_buf.addr;
			}
			DOWRITERC_RM(rm_ptr, out_ptr, RMEOL_LEN, status);
			ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
			rm_ptr->write_occurred = TRUE;
		}
		*dollarx_ptr = 0;
	}
#	ifdef __MVS__
	} else
		*dollarx_ptr = 0;	/* just reset $X for BINARY */
#	endif
	iod->dollar.za = 0;
	*dollary_ptr += x;
	if (iod->length)
		*dollary_ptr %= iod->length;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
