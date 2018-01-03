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
#include "iormdef.h"
#include "iormdefsp.h"
#include "gtmio.h"
#include "min_max.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif
#include "gtmcrypt.h"
#include "send_msg.h"
#include "error.h"

GBLREF io_pair		io_curr_device;
#ifdef UNICODE_SUPPORTED
LITREF mstr		chset_names[];
#endif

error_def(ERR_CRYPTBADWRTPOS);
error_def(ERR_DEVICEREADONLY);
error_def(ERR_IOERROR);
error_def(ERR_NOPRINCIO);
error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_SYSCALL);

/* write ASCII characters converting to UTF16 if needed
 * 	returns bytes written
*/
int  iorm_write_utf_ascii(io_desc *iod, char *string, int len)
{
	int		outlen, mblen, status;
	wint_t		utf_code;
	unsigned char	*outstart, *out, *top, *outptr, *nextoutptr, *outptrtop, *nextmb;
	d_rm_struct	*rm_ptr;
	boolean_t	ch_set;

	ESTABLISH_RET_GTMIO_CH(&iod->pair, -1, ch_set);
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	assert(NULL != rm_ptr);
	if (CHSET_UTF8 != iod->ochset)
	{
		outstart = outptr = &rm_ptr->outbuf[rm_ptr->out_bytes];
		outptrtop = rm_ptr->outbuf + rm_ptr->recordsize;	/* buffer is larger than recordsize to allow for EOL */
		assert(len <= (&rm_ptr->outbuf[rm_ptr->outbufsize] - outstart));
		for (out = (unsigned char*)string, top = out + len, outlen = 0; out < top && outptr <= outptrtop;
				out = nextmb, outptr = nextoutptr)
		{
			nextmb = UTF8_MBTOWC(out, top, utf_code);
			assert(nextmb == (out + 1));
			if (WEOF == utf_code)
			{
				iod->dollar.za = 9;
				UTF8_BADCHAR((int)(nextmb - out), out, top, 0, NULL);
			}
			if (CHSET_UTF16BE == iod->ochset)
				nextoutptr = UTF16BE_WCTOMB(utf_code, outptr);
			else
				nextoutptr = UTF16LE_WCTOMB(utf_code, outptr);
			if (nextoutptr == outptr)
			{	/* invalid codepoint */
				iod->dollar.za = 9;
				UTF8_BADCHAR((int)(nextmb - out), out, top, chset_names[iod->ochset].len,
						chset_names[iod->ochset].addr);
			}
			mblen = (int)(nextoutptr - outptr);
			outlen += mblen;
		}
	} else
	{
		outstart = (unsigned char *)string;
		outlen = len;
	}
	if (0 < outlen)
	{
		if (rm_ptr->output_encrypted)
		{
			REALLOC_CRYPTBUF_IF_NEEDED(outlen);
			WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, outstart, outlen, pvt_crypt_buf.addr);
			outptr = (unsigned char *)pvt_crypt_buf.addr;
		} else
			outptr = outstart;
		DOWRITERC_RM(rm_ptr, outptr, outlen, status);
		ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
		rm_ptr->write_occurred = TRUE;
		rm_ptr->out_bytes += outlen;
	}
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return outlen;
}

void iorm_write_utf(mstr *v)
{
	int4		inchars, char_count;		/* in characters */
	int4		inlen, outbytes, mblen;		/* in bytes */
	int4		availwidth, usedwidth, mbwidth;	/* in display columns */
	int		status, padsize, fstat_res, save_errno;
	wint_t		utf_code;
	io_desc		*iod;
	d_rm_struct	*rm_ptr;
	unsigned char	*inptr, *top, *nextmb, *outptr, *nextoutptr, *outstart, temppad, temppadarray[2];
	char		*out_ptr;
	boolean_t	utf8_active = TRUE;		/* needed by GTM_IO_WCWIDTH macro */
	boolean_t	stream, wrap;
	struct stat	statbuf;
	boolean_t	ch_set;

	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	assert(NULL != rm_ptr);
	inptr = (unsigned char *)v->addr;
	inlen = v->len;
	top = inptr + inlen;
	if (!rm_ptr->fixed && 0 == iod->dollar.x)
		rm_ptr->out_bytes = 0;			/* user reset $X */
	inchars = UTF8_LEN_STRICT(v->addr, v->len);	/* validate and get good char count */
	if (0 >= inchars)
	{
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
		return;
	}
	usedwidth = 0;
	stream = rm_ptr->stream;
	wrap = iod->wrap;
	if (stream && !wrap)
	{	/* For STREAM and NOWRAP, allow the entire record to be written without any record truncations/terminations */
		availwidth = inlen;	/* calculate worst case requirement of width (in chars) to write out input bytes */
		rm_ptr->out_bytes = 0;
	} else
		availwidth = iod->width - iod->dollar.x;
	outbytes = 0;
	if (CHSET_UTF8 != iod->ochset)
	{
		outstart = nextoutptr = outptr = &rm_ptr->outbuf[rm_ptr->out_bytes];
		/* In case the CHSET changes from non-UTF-16 to UTF-16 and a read has already been done,
		 * there's no way to read the BOM bytes & to determine the variant. So default to UTF-16BE.
		 */
		if (rm_ptr->done_1st_write && ((CHSET_UTF16 == iod->ochset) && !IS_UTF16_CHSET(rm_ptr->ochset_utf16_variant)))
		{
			iod->ochset = rm_ptr->ochset_utf16_variant = CHSET_UTF16BE;
		}
		if (!rm_ptr->done_1st_write)
		{	/* get the file size  */
			FSTAT_FILE(rm_ptr->fildes, &statbuf, fstat_res);
			if (-1 == fstat_res)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fstat"),
					      CALLFROM, save_errno);
			}
			if (CHSET_UTF16 == iod->ochset)
			{	/* Write BOM but do not count it towards the bytes in the current record */
				/* write BOM if file is empty */
				if (0 == statbuf.st_size)
				{
					memcpy(outptr, UTF16BE_BOM, UTF16BE_BOM_LEN);
					outbytes = UTF16BE_BOM_LEN;
					if (rm_ptr->output_encrypted)
					{
						REALLOC_CRYPTBUF_IF_NEEDED(outbytes);
						WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, outstart, outbytes,
							pvt_crypt_buf.addr);
						out_ptr = pvt_crypt_buf.addr;
					} else
						out_ptr = (char *)outstart;
					DOWRITERC_RM(rm_ptr, out_ptr, outbytes, status);
					ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
					rm_ptr->write_occurred = TRUE;
					outptr = outstart;
					rm_ptr->out_bytes = outbytes = 0;
					/* save UTF16BE_BOM_LEN in bom_num_bytes until bom is checked, but don't
					 indicate that bom has been checked - which still needs to be done for reading
					 the exception is if the file was opened WRITEONLY */
					rm_ptr->bom_num_bytes = UTF16BE_BOM_LEN;
					if (rm_ptr->write_only)
						rm_ptr->bom_checked = TRUE;
				}
				iod->ochset = CHSET_UTF16BE;
				get_chset_desc(&chset_names[iod->ochset]);
			}
			if (!IS_UTF16_CHSET(rm_ptr->ochset_utf16_variant) &&
				(IS_UTF16_CHSET(iod->ochset) && (CHSET_UTF16 != iod->ochset)))
				rm_ptr->ochset_utf16_variant = iod->ochset;
		}
	} else
		outstart = (unsigned char *)v->addr;	/* write from the UTF-8 string */
	if (!rm_ptr->done_1st_write)
		rm_ptr->done_1st_write = TRUE;
	for (char_count = mblen = 0; inptr <= top ; )
	{
		if (inptr < top)
		{	/* get "mblen" and "mbwidth" of of next character in buffer */
			nextmb = UTF8_MBTOWC(inptr, top, utf_code);
			if (WEOF == utf_code)
			{
				iod->dollar.za = 9;
				UTF8_BADCHAR(0, inptr, top, 0, NULL);
			}
			if (CHSET_UTF8 != iod->ochset)
			{
				if (CHSET_UTF16BE == iod->ochset)
					nextoutptr = UTF16BE_WCTOMB(utf_code, outptr);
				else
					nextoutptr = UTF16LE_WCTOMB(utf_code, outptr);
				if (nextoutptr == outptr)
				{	/* invalid codepoint */
					iod->dollar.za = 9;
					UTF8_BADCHAR((int)((nextmb - inptr)), inptr, top, chset_names[iod->ochset].len,
							chset_names[iod->ochset].addr);
				}
				mblen  = (int)(nextoutptr - outptr);
				outptr = nextoutptr;
			} else
				mblen = (int)(nextmb - inptr);
			GTM_IO_WCWIDTH(utf_code, mbwidth);
			assert(mblen);
		}
		assert(rm_ptr->out_bytes <= rm_ptr->recordsize);
		/* Note that "mblen" and "mbwidth" are valid only if "inptr < top".
		 * This is why they are used after the "inptr >= top" check below
		 */
		if ((inptr >= top) || ((usedwidth + mbwidth) > availwidth) || ((rm_ptr->out_bytes + mblen) > rm_ptr->recordsize))
		{	/* filled to WIDTH or end of input or full record */
			if (0 < outbytes)
			{
				if (rm_ptr->output_encrypted)
				{
					REALLOC_CRYPTBUF_IF_NEEDED(outbytes);
					WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, outstart, outbytes, pvt_crypt_buf.addr);
					out_ptr = pvt_crypt_buf.addr;
				} else
					out_ptr = (char *)outstart;
				if (rm_ptr->fifo || rm_ptr->is_pipe)
				{
					WRITEPIPE(rm_ptr->fildes, rm_ptr->pipe_buff_size, out_ptr, outbytes, status);
				} else
				{
					DOWRITERC_RM(rm_ptr, out_ptr, outbytes, status);
				}
				ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
				rm_ptr->write_occurred = TRUE;
			}
			iod->dollar.x += usedwidth;
			if (inptr >= top)
				break;					/* end of input */
			if (char_count >= inchars)
				break;					/* end of adjusted input characters */
			if (!stream || wrap)
			{	/* implicit record termination for non-stream files or stream files with the "wrap" option. */
				if (!wrap)	/* non-stream device wants NOWRAP, so break right away without writing any more */
					break;
				if (!rm_ptr->fixed && wrap)
					iorm_write_utf_ascii(iod, RMEOL, RMEOL_LEN);
				else if (rm_ptr->fixed && rm_ptr->out_bytes < rm_ptr->recordsize)
				{	/* padding bytes needed */
					temppad = rm_ptr->padchar;
					if (CHSET_UTF16LE == iod->ochset)
					{
						temppadarray[0] = temppad;
						temppadarray[1] = '\0';
						padsize = 2;
					} else if (CHSET_UTF16BE == iod->ochset)
					{
						temppadarray[0] = '\0';
						temppadarray[1] = temppad;
						padsize = 2;
					} else
					{
						assert(CHSET_UTF8 == iod->ochset);
						temppadarray[0] = temppad;
						padsize = 1;
					}
					for ( ; rm_ptr->out_bytes < rm_ptr->recordsize; rm_ptr->out_bytes += padsize)
					{
						if (rm_ptr->output_encrypted)
						{
							REALLOC_CRYPTBUF_IF_NEEDED(padsize);
							WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, temppadarray, padsize,
								pvt_crypt_buf.addr);
							out_ptr = pvt_crypt_buf.addr;
						} else
							out_ptr = (char *)temppadarray;
						DOWRITERC_RM(rm_ptr, out_ptr, padsize, status);
						ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
						rm_ptr->write_occurred = TRUE;
					}
					assert(rm_ptr->out_bytes == rm_ptr->recordsize);
				}
				iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
				iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
				if (iod->length)	/* and fixed format requires no padding for wrapped records */
					iod->dollar.y %= iod->length;
				availwidth = iod->width;
			}
			/* else STREAM specified with NOWRAP.
			 * We can continue to write even if device width is exceeded since NOWRAP has been specified.
			 */
			rm_ptr->out_bytes = 0;
			/* For UTF16, since input and output streams ("inptr" and "outstart") point to different buffers,
			 * the last parsed UTF16 character in "inptr" has not yet been written to "outstart" so
			 * redo the parsing by continuing to the beginning of the loop without incrementing "inptr".
			 * For UTF8, both are the same so we can increment "inptr" without any issues and avoid reparsing.
			 */
			if (CHSET_UTF8 == iod->ochset)
			{
				outstart = outstart + outbytes;		/* advance within input string */
				outbytes = usedwidth = 0;
			} else
			{
				outstart = nextoutptr = outptr = &rm_ptr->outbuf[rm_ptr->out_bytes];
				outbytes = usedwidth = 0;
				continue;
			}
		}
		assert(mblen);
		assert(mblen <= rm_ptr->recordsize);	/* there is room in RECORDSIZE to write at least one character */
		rm_ptr->out_bytes += mblen;
		outbytes += mblen;
		inptr = nextmb;			/* next input byte */
		assert(inptr <= top);
		char_count++;
		usedwidth += mbwidth;
		assert(usedwidth <= availwidth);	/* there is room in display WIDTH to write at least one character */
	}
	iod->dollar.za = 0;
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
	return;
}

void iorm_write(mstr *v)
{
	io_desc		*iod;
	char		*out, *out_ptr;
	int		inlen, outlen, status, len;
	d_rm_struct	*rm_ptr;
	int		flags;
	int		fcntl_res;
	boolean_t	stream, wrap;
	struct stat	statbuf;
	int		fstat_res, save_errno;
	boolean_t	ch_set;
	unsigned int	save_dollarx;

	iod = io_curr_device.out;
	ESTABLISH_GTMIO_CH(&io_curr_device, ch_set);
#ifdef __MVS__
	if (NULL == iod->dev_sp)
		rm_ptr = (d_rm_struct *)(iod->pair.in)->dev_sp;
	else
		rm_ptr = (d_rm_struct *)(iod->pair.out)->dev_sp;
#else
	rm_ptr = (d_rm_struct *)iod->dev_sp;
#endif
	memcpy(iod->dollar.device, "0", SIZEOF("0"));

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

	/* if it's a fifo and not system output/error, last operation was not a write and O_NONBLOCK is not set
	   then set it.  A read will turn it off */
	if (rm_ptr->fifo && (2 < rm_ptr->fildes) && (RM_WRITE != rm_ptr->lastop))
	{
		flags = 0;
		FCNTL2(rm_ptr->fildes, F_GETFL, flags);
		if (0 > flags)
		{
			save_errno = errno;
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, save_errno);
		}
		if (!(flags & O_NONBLOCK))
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags | O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM,
					save_errno);
			}
		}
	}

	if (!rm_ptr->fifo && !rm_ptr->is_pipe && !rm_ptr->fixed && (2 < rm_ptr->fildes) && (RM_WRITE != rm_ptr->lastop))
	{
		/* need to do an lseek to set current location in file */
		if ((off_t)-1 == (lseek(rm_ptr->fildes, rm_ptr->file_pos, SEEK_SET)))
		{
			save_errno = errno;
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
				      RTS_ERROR_LITERAL("iorm_write()"), CALLFROM, save_errno);
		}
	}


	/* if current file position is less than bom_num_bytes and it is a disk in utf mode and last op not a WRITE
	   skip past the BOM */
	if (!rm_ptr->fifo && !rm_ptr->is_pipe && IS_UTF_CHSET(iod->ochset) && (rm_ptr->file_pos < rm_ptr->bom_num_bytes) &&
	    (2 < rm_ptr->fildes) && (RM_WRITE != rm_ptr->lastop))
	{
		/* need to do lseek to skip the BOM before writing*/
		if ((off_t)-1 == (lseek(rm_ptr->fildes, (off_t)rm_ptr->bom_num_bytes, SEEK_SET)))
		{
			save_errno = errno;
			SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
				      RTS_ERROR_LITERAL("iorm_write()"), CALLFROM, save_errno);
		}
	}


	rm_ptr->lastop = RM_WRITE;
	if (IS_UTF_CHSET(iod->ochset))
	{
		iorm_write_utf(v);
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
		return;
	}
	inlen = v->len;
	if (!inlen)
	{
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
		return;
	}
	stream = rm_ptr->stream;
	wrap = iod->wrap;
	if (stream && !wrap)
		outlen = inlen;
	else
	{
		if (iod->width < iod->dollar.x)
			save_dollarx = iod->width;
		else
			save_dollarx = iod->dollar.x;
		outlen = iod->width - save_dollarx;
		if (!wrap && !stream && (inlen > outlen))
			inlen = outlen; /* implicit input truncation for non-stream files with the "nowrap" option. */
	}
	for (out = v->addr; ; out += len)
	{
		len = MIN(inlen, outlen);
		if (rm_ptr->output_encrypted)
		{
			REALLOC_CRYPTBUF_IF_NEEDED(len);
			WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out, len, pvt_crypt_buf.addr);
			out_ptr = pvt_crypt_buf.addr;
		} else
			out_ptr = out;
		if (rm_ptr->fifo || rm_ptr->is_pipe)
		{
			WRITEPIPE(rm_ptr->fildes, rm_ptr->pipe_buff_size, out_ptr, len, status);
		} else
		{
			DOWRITERC_RM(rm_ptr, out_ptr, len, status);
		}
		ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
		rm_ptr->write_occurred = TRUE;
		iod->dollar.x += len;
		if (0 >= (inlen -= len))
			break;
		if (!stream || wrap)
		{	/* implicit record termination for non-stream files or stream files with the "wrap" option. */
			if (!rm_ptr->fixed && wrap)
			{
				out_ptr = RMEOL;
				if (rm_ptr->output_encrypted)
				{
					assert(pvt_crypt_buf.len >= RMEOL_LEN);
					WRITE_ENCRYPTED_DATA(rm_ptr, iod->trans_name, out_ptr, RMEOL_LEN, pvt_crypt_buf.addr);
					out_ptr = pvt_crypt_buf.addr;
				}
				DOWRITERC_RM(rm_ptr, out_ptr, RMEOL_LEN, status);
				ISSUE_NOPRINCIO_IF_NEEDED_RM(status, ==, iod);
				rm_ptr->write_occurred = TRUE;
			}
			iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
			iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
			if (iod->length)	/* and fixed format requires no padding for wrapped records */
				iod->dollar.y %= iod->length;
			outlen = iod->width;
		}
	}
	iod->dollar.za = 0;
	REVERT_GTMIO_CH(&io_curr_device, ch_set);
        return;
}
