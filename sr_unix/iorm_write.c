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
#include "iormdef.h"
#include "iormdefsp.h"
#include "gtmio.h"
#include "min_max.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif

GBLREF io_pair		io_curr_device;
#ifdef UNICODE_SUPPORTED
LITREF	mstr		chset_names[];
#endif

#define MAX_WIDTH 65535

error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_DEVICEREADONLY);
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
		DOWRITERC(rm_ptr->fildes, outstart, outlen, status);
		if (0 != status)
		{
			DOLLAR_DEVICE_WRITE(iod, status);
			iod->dollar.za = 9;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
		rm_ptr->out_bytes += outlen;
	}
	return outlen;
}

void iorm_write_utf(mstr *v)
{
	int4		inchars, char_count;		/* in characters */
	int4		inlen, outbytes, mblen;		/* in bytes */
	int4		availwidth, usedwidth, mbwidth;	/* in display columns */
	int		status, padsize;
	wint_t		utf_code;
	io_desc		*iod;
	d_rm_struct	*rm_ptr;
	unsigned char	*inptr, *top, *nextmb, *outptr, *nextoutptr, *outstart, temppad, temppadarray[2];
	boolean_t	utf8_active = TRUE;		/* needed by GTM_IO_WCWIDTH macro */

	iod = io_curr_device.out;
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	assert(NULL != rm_ptr);
	inptr = (unsigned char *)v->addr;
	inlen = v->len;
	top = inptr + inlen;
	if (!rm_ptr->fixed && 0 == iod->dollar.x)
		rm_ptr->out_bytes = 0;			/* user reset $X */
	inchars = UTF8_LEN_STRICT(v->addr, v->len);	/* validate and get good char count */
	if (0 >= inchars)
		return;
	usedwidth = 0;
	if (rm_ptr->stream && !iod->wrap)
	{
		availwidth = iod->width;
		/* For STREAM and NOWRAP, allow a maximum of "rm_ptr->recordsize" bytes to be written as part of
		 * the current WRITE. Any number of future WRITEs to this same record are allowed as long as
		 * each of them is within "rm_ptr->recordsize" bytes (truncated otherwise). This means that
		 * it does not matter how many bytes we have already written as part of the current record
		 * which is "rm_ptr->out_bytes". Reset it to 0 so we can use it for the current WRITE calculations.
		 */
		rm_ptr->out_bytes = 0;
	} else
		availwidth = iod->width - iod->dollar.x;
	outbytes = 0;
	if (CHSET_UTF8 != iod->ochset)
	{
		outstart = nextoutptr = outptr = &rm_ptr->outbuf[rm_ptr->out_bytes];
		if (!rm_ptr->done_1st_write)
		{
			if (CHSET_UTF16 == iod->ochset)
			{	/* Write BOM but do not count it towards the bytes in the current record */
				memcpy(outptr, UTF16BE_BOM, UTF16BE_BOM_LEN);
				outbytes = UTF16BE_BOM_LEN;
				outptr += UTF16BE_BOM_LEN;
				DOWRITERC(rm_ptr->fildes, outstart, outbytes, status);
				if (0 != status)
				{
					DOLLAR_DEVICE_WRITE(iod, status);
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
				outptr = outstart;
				rm_ptr->out_bytes = outbytes = 0;
				iod->ochset = CHSET_UTF16BE;
				get_chset_desc(&chset_names[iod->ochset]);
			}
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
		if (inptr >= top || ((usedwidth + mbwidth) > availwidth) || ((rm_ptr->out_bytes + mblen) > rm_ptr->recordsize))
		{	/* filled to WIDTH or end of input or full record */
			if (0 < outbytes)
			{
				if (rm_ptr->fifo || rm_ptr->pipe)
				{
					WRITEPIPE(rm_ptr->fildes, rm_ptr->pipe_buff_size, outstart, outbytes, status);
				} else
				{
					DOWRITERC(rm_ptr->fildes, outstart, outbytes, status);
				}
				if (0 != status)
				{
					DOLLAR_DEVICE_WRITE(iod, status);
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
			}
			iod->dollar.x += usedwidth;
			if (inptr >= top)
				break;					/* end of input */
			if (char_count >= inchars)
				break;					/* end of adjusted input characters */
			if (!rm_ptr->stream || iod->wrap)
			{ /* implicit record termination for non-stream files or stream files with the "wrap" option. */
				if (!iod->wrap)	/* non-stream device wants NOWRAP, so break right away without writing any more */
					break;
				if (!rm_ptr->fixed && iod->wrap)
					iorm_write_utf_ascii(iod, RMEOL, STRLEN(RMEOL));
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
						DOWRITERC(rm_ptr->fildes, temppadarray, padsize, status);
						if (0 != status)
						{
							DOLLAR_DEVICE_WRITE(iod, status);
							iod->dollar.za = 9;
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
						}
					}
					assert(rm_ptr->out_bytes == rm_ptr->recordsize);
				}
				iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
				iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
				if (iod->length)	/* and fixed format requires no padding for wrapped records */
					iod->dollar.y %= iod->length;
				availwidth = iod->width;
			} else
			{	/* STREAM specified with NOWRAP */
				/* We can continue to write even if device width is exceeded since NOWRAP has
				 * been specified. But if RECORDSIZE limit has been exceeded then we need to
				 * automatically terminate this WRITE (not the RECORD though) and return right away.
				 * In order to allow further WRITEs to add to this RECORD, reset rm_ptr->out_bytes to 0.
				 */
				if ((rm_ptr->out_bytes + mblen) > rm_ptr->recordsize)
				{
					rm_ptr->out_bytes = 0;
					break;
				}
			}
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
	return;
}

void iorm_write(mstr *v)
{
	io_desc		*iod;
	char		*out;
	int		inlen, outlen, status, len;
	d_rm_struct	*rm_ptr;
	int		flags;
	int		fcntl_res;

	iod = io_curr_device.out;
#ifdef __MVS__
	if (NULL == iod->dev_sp)
		rm_ptr = (d_rm_struct *)(iod->pair.in)->dev_sp;
	else
		rm_ptr = (d_rm_struct *)(iod->pair.out)->dev_sp;
#else
	rm_ptr = (d_rm_struct *)iod->dev_sp;
#endif
	memcpy(iod->dollar.device, "0", SIZEOF("0"));

	if (rm_ptr->noread)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVICEREADONLY);
	if (!iod->dollar.zeof && !rm_ptr->fifo && !rm_ptr->pipe)
	{
	 	iod->dollar.za = 9;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}

	/* if it's a fifo and not system output/error, last operation was not a write and O_NONBLOCK is not set
	   then set it.  A read will turn it off */
	if (rm_ptr->fifo && (2 < rm_ptr->fildes) && (RM_WRITE != rm_ptr->lastop))
	{
		flags = 0;
		FCNTL2(rm_ptr->fildes, F_GETFL, flags);
		if (0 > flags)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
		if (!(flags & O_NONBLOCK))
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags | O_NONBLOCK), fcntl_res);
			if (0 > fcntl_res)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fcntl"), CALLFROM, errno);
		}
	}

	rm_ptr->lastop = RM_WRITE;
	if (IS_UTF_CHSET(iod->ochset))
	{
		iorm_write_utf(v);
		return;
	}
	inlen = v->len;
	if (rm_ptr->stream && !iod->wrap)
		outlen = iod->width;
	else
		outlen = iod->width - iod->dollar.x;

	if (!iod->wrap && inlen > outlen && outlen != MAX_WIDTH)
		inlen = outlen;
	if (!inlen)
		return;
	if (outlen > inlen)
		outlen = inlen;
	for (out = v->addr; ; out += len)
	{
		len = MIN(inlen, outlen);
		if (rm_ptr->fifo || rm_ptr->pipe)
		{
			WRITEPIPE(rm_ptr->fildes, rm_ptr->pipe_buff_size, out, len, status);
		} else
		{
			DOWRITERC(rm_ptr->fildes, out, len, status);
		}
		if (0 != status)
		{
			DOLLAR_DEVICE_WRITE(iod, status);
			iod->dollar.za = 9;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
		iod->dollar.x += len;
		if ((inlen -= len) <= 0)
			break;

		if (!rm_ptr->stream || iod->wrap)
		/* implicit record termination for non-stream files
		 * or stream files with the "wrap" option.
		 */
		{
			if (!rm_ptr->fixed && iod->wrap)
			{
				DOWRITERC(rm_ptr->fildes, RMEOL, STRLEN(RMEOL), status);
				if (0 != status)
				{
					DOLLAR_DEVICE_WRITE(iod, status);
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
			}

			iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
			iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
			if (iod->length)	/* and fixed format requires no padding for wrapped records */
				iod->dollar.y %= iod->length;

			outlen = iod->width;
		}
		if (outlen > inlen)
			outlen = inlen;
	}
	iod->dollar.za = 0;
        return;
}
