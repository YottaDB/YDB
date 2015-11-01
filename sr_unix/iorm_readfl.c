/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iotimer.h"
#include "iormdef.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "wake_alarm.h"
#include "min_max.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_conv.h"
#include "gtm_utf8.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	volatile bool	out_of_time;
GBLREF  boolean_t       gtm_utf8_mode;
#ifdef UNICODE_SUPPORTED
LITREF	UChar32		u32_line_term[];
LITREF	mstr		chset_names[];
GBLREF	UConverter	*chset_desc[];
#endif

#define fl_copy(a, b) (a > b ? b : a)

#define SETZACANCELTIMER					\
		io_ptr->dollar.za = 9;				\
		v->str.len = 0;					\
		if (timed && !out_of_time)			\
			cancel_timer(timer_id);

#ifdef UNICODE_SUPPORTED
/* Maintenance of $ZB on a badchar error and returning partial data (if any) */
void iorm_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend)
{
        int             tmplen, len;
        unsigned char   *delimend;
        io_desc         *iod;

	assert(0 <= datalen);
	iod = io_curr_device.in;
	vmvalptr->str.len = datalen;
	vmvalptr->str.addr = (char *)stringpool.free;
        if (0 < datalen)
		/* Return how much input we got */
		stringpool.free += vmvalptr->str.len;

        if (NULL != strend && NULL != delimptr)
        {       /* First find the end of the delimiter (max of 4 bytes) */
		if (0 == delimlen)
		{
			for (delimend = delimptr; GTM_MB_LEN_MAX >= delimlen && delimend < strend; ++delimend, ++delimlen)
			{
				if (UTF8_VALID(delimend, strend, tmplen))
					break;
			}
		}
                if (0 < delimlen)
                {       /* Set $ZB with the failing badchar */
                        memcpy(iod->dollar.zb, delimptr, MIN(delimlen, ESC_LEN - 1));
                        iod->dollar.zb[MIN(delimlen, ESC_LEN - 1)] = '\0';
                }
        }
}
#endif

int	iorm_readfl (mval *v, int4 width, int4 timeout) /* timeout in seconds */
{
	boolean_t	ret, timed, utf_active, line_term_seen = FALSE, rdone = FALSE;
	char		inchar, *temp, *temp_start;
	unsigned char	*nextmb, *char_ptr, *char_start;
	int		flags;
	int		fcntl_res;
	int4		msec_timeout;	/* timeout in milliseconds */
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, reclen;
	int4		buff_len, mblen, char_count, bytes_count, tot_bytes_read;
	int4		status, max_width, ltind, exp_width, from_bom;
	wint_t		utf_code;
	io_desc		*io_ptr;
	d_rm_struct	*rm_ptr;
	gtm_chset_t	chset;
	TID		timer_id;

	error_def(ERR_IOEOF);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	utf_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	if (io_ptr->dollar.x && rm_ptr->lastop == RM_WRITE)
	{
		if (!io_ptr->dollar.za)
			iorm_wteol(1, io_ptr);
		io_ptr->dollar.x = 0;
	}

	rm_ptr->lastop = RM_READ;
	timer_id = (TID)iorm_readfl;
	max_width = io_ptr->width - io_ptr->dollar.x;
	if (0 == width)
	{
		width = io_ptr->width;		/* called from iorm_read */
		if (!utf_active || !rm_ptr->fixed)
			max_width = width;	/* preserve prior functionality */
	} else if (-1 == width)
	{
		rdone = TRUE;			/* called from iorm_rdone */
		width = 1;
	}
	width = (width < max_width) ? width : max_width;
	tot_bytes_read = char_bytes_read = bytes_read = char_count = bytes_count = 0;
	ret = TRUE;
        /* if utf_active, need room for multi byte characters */
	exp_width = utf_active ? (GTM_MB_LEN_MAX * width) : width;
        if (stringpool.free + exp_width > stringpool.top)
                stp_gcol (exp_width);
	temp = (char *)stringpool.free;
	out_of_time = FALSE;
	if (timeout == NO_M_TIMEOUT)
	{
		timed = FALSE;
		msec_timeout = NO_M_TIMEOUT;
	} else
	{
		timed = TRUE;
		msec_timeout = timeout2msec(timeout);
		if (msec_timeout > 0)
			start_timer(timer_id, msec_timeout, wake_alarm, 0, NULL);
		 else
		{
			out_of_time = TRUE;
			FCNTL2(rm_ptr->fildes, F_GETFL, flags);
			FCNTL3(rm_ptr->fildes, F_SETFL, (flags | O_NDELAY), fcntl_res);
		}
	}
	errno = status = 0;
	chset = io_ptr->ichset;
        if (!utf_active)
	{
		if (rm_ptr->fixed)
                {       /* This is M mode - one character is one byte.
                         * Note the check for EINTR below is valid and should not be converted to an EINTR
                         * wrapper macro, since action is taken on EINTR, not a retry.
                         */
			DOREADRLTO(rm_ptr->fildes, temp, width, out_of_time, status);
			if (0 > status)
			{
				bytes_count = 0;
				if (errno == EINTR  &&  out_of_time)
					status = -2;
			} else
				tot_bytes_read = bytes_count = status;
		} else
		{
			do
			{
				if ((status = getc(rm_ptr->filstr)) != EOF)
				{
					inchar = (unsigned char)status;
					tot_bytes_read++;
					if (inchar == NATIVE_NL)
					{
						line_term_seen = TRUE;
						if (!rdone)
							break;
					}
					*temp++ = inchar;
					bytes_count++;
				} else
				{
					inchar = 0;
					if (errno == 0)
						status = 0;
					else if (errno == EINTR)
					{
						if (out_of_time)
							status = -2;
						else
							continue;		/* Ignore interrupt if not our wakeup */
					}
					break;
				}
			} while (bytes_count < width);
		}
	} else
	{	/* Unicode mode */
		assert(NULL != rm_ptr->inbuf);
		if (rm_ptr->fixed)
                {
			buff_len = rm_ptr->inbuf_top - rm_ptr->inbuf_off;
			if (0 == buff_len)
			{	/* need to refill the buffer */
				buff_len = iorm_get(io_ptr);
				if (0 > buff_len)
				{
					bytes_count = 0;
					if (errno == EINTR  &&  out_of_time)
						buff_len = -2;
				}
				chset = io_ptr->ichset;		/* in case UTF-16 was changed */
			}
			status = tot_bytes_read = buff_len;		/* for EOF checking at the end */
			char_ptr = rm_ptr->inbuf_off;
			if (0 < buff_len)
			{
				for (char_count = 0; char_count < width && char_ptr < rm_ptr->inbuf_top; char_count++)
				{	/* count chars and check for validity */
					switch (chset)
					{
						case CHSET_UTF8:
							if (UTF8_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
							{
								bytes_count += mblen;
								char_ptr += mblen;
							} else
							{
								SETZACANCELTIMER;
								iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    mblen, char_ptr, rm_ptr->inbuf_top);
								rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
								UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
									chset_names[chset].len, chset_names[chset].addr);
							}
							break;
						case CHSET_UTF16BE:
							if (UTF16BE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
							{
								bytes_count += mblen;
								char_ptr += mblen;
							} else
							{
								SETZACANCELTIMER;
								iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    mblen, char_ptr, rm_ptr->inbuf_top);
								rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
								UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
									chset_names[chset].len, chset_names[chset].addr);
							}
							break;
						case CHSET_UTF16LE:
							if (UTF16LE_VALID(char_ptr, rm_ptr->inbuf_top, mblen))
							{
								bytes_count += mblen;
								char_ptr += mblen;
							} else
							{
								SETZACANCELTIMER;
								iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    mblen, char_ptr, rm_ptr->inbuf_top);
								rm_ptr->inbuf_off = char_ptr + mblen;	/* mark as read */
								UTF8_BADCHAR(mblen, char_ptr, rm_ptr->inbuf_top,
									chset_names[chset].len, chset_names[chset].addr);
							}
							break;
						default:
							GTMASSERT;
					}
				}
				v->str.len = char_ptr - rm_ptr->inbuf_off;
				UNICODE_ONLY(v->str.char_len = char_count;)
				if (0 < v->str.len)
				{
					if (CHSET_UTF8 == chset)
						memcpy(stringpool.free, rm_ptr->inbuf_off, v->str.len);
					else
					{
						v->str.addr = (char *)rm_ptr->inbuf_off;
						v->str.len = gtm_conv(chset_desc[chset], chset_desc[CHSET_UTF8],
									&v->str, NULL, NULL);
					}
					v->str.addr = (char *)stringpool.free;
					rm_ptr->inbuf_off += char_ptr - rm_ptr->inbuf_off;
				}
			}
                } else
		{	/* VARIABLE or STREAM */
			assert(CHSET_M != chset);
			if (rm_ptr->inbuf_pos <= rm_ptr->inbuf_off)
			{	/* reset buffer pointers */
				rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
				bytes2read = (CHSET_UTF8 == chset) ? 1 : 2;
			} else
			{	 /* use bytes from buffer for character left over from last read */
				assert(rm_ptr->done_1st_read);
				bytes2read = 0;			/* use bytes from buffer left over from last read */
				bytes_read = char_bytes_read = rm_ptr->inbuf_pos - rm_ptr->inbuf_off;
				add_bytes = bytes_read - 1;	/* to satisfy asserts */
			}
			char_start = rm_ptr->inbuf_off;
			do
			{
				if (!rm_ptr->done_1st_read)
				{
					status = iorm_get_bom(io_ptr);	/* need to check BOM */
					chset = io_ptr->ichset;	/* UTF16 will have changed to UTF16BE or UTF16LE */
				}
				if (0 <= status && bytes2read && rm_ptr->bom_buf_cnt > rm_ptr->bom_buf_off)
				{
					from_bom = MIN((rm_ptr->bom_buf_cnt - rm_ptr->bom_buf_off), bytes2read);
					memcpy(rm_ptr->inbuf_pos, &rm_ptr->bom_buf[rm_ptr->bom_buf_off], from_bom);
					rm_ptr->bom_buf_off += from_bom;
					rm_ptr->inbuf_pos += from_bom;
					bytes2read -= from_bom;		/* now in buffer */
					bytes_read = from_bom;
					char_bytes_read += from_bom;
					status = 0;
				}
				if (0 <= status && 0 < bytes2read)
					DOREADRLTO(rm_ptr->fildes, rm_ptr->inbuf_pos, bytes2read, out_of_time, status);
				if (0 <= status)
				{
					rm_ptr->inbuf_pos += status;
					bytes_read += status;			/* bytes read this pass */
					char_bytes_read += status;		/* bytes for this character */
					tot_bytes_read += bytes_read;		/* total bytes read this command */
					if (0 < bytes2read && 0 == status)
					{	/* EOF  on read */
						if (0 == char_bytes_read)
							break;			/* nothing read for this char so treat as EOF */
						assert(1 < (bytes2read + bytes_read));	/* incomplete character */
						SETZACANCELTIMER;
						iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
								    bytes_read, char_start, rm_ptr->inbuf_pos);
						rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
						UTF8_BADCHAR(bytes_read, char_start, rm_ptr->inbuf_pos,
								chset_names[chset].len, chset_names[chset].addr);
					} else if (status < bytes2read)
					{
						bytes2read -= status;
						continue;
					}
					if (CHSET_UTF8 == chset)
					{
						if (1 == char_bytes_read)
						{
							add_bytes = UTF8_MBFOLLOW(char_start);
							if (0 < add_bytes)
							{
								bytes2read = add_bytes;
								continue;
							} else if (-1 == add_bytes)
							{
								SETZACANCELTIMER;
								iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
										    char_start, (char_start + char_bytes_read));
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start,
									char_start + char_bytes_read, 0, NULL);
							}
							if (u32_line_term[U32_LT_LF] == *char_start)
								if (rm_ptr->crlast)
								{	/* ignore LF following CR */
									rm_ptr->crlast = FALSE;
									rm_ptr->inbuf_pos = char_start;
									bytes2read = 1;				/* reset */
									bytes_read = char_bytes_read = 0;	/* start fresh */
									tot_bytes_read--;
									continue;
								} else
								{
									line_term_seen = TRUE;
									rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
									if (!rdone)
										break;
								}
							rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
							if (u32_line_term[U32_LT_CR] == *char_start)
							{
								rm_ptr->crlast = TRUE;
								line_term_seen = TRUE;
								if (!rdone)
									break;
							} else
								rm_ptr->crlast = FALSE;
							if (u32_line_term[U32_LT_FF] == *char_start)
							{
								line_term_seen = TRUE;
								if (!rdone)
									break;
							}
							*temp++ = *char_start;
						} else
						{
							assert(char_bytes_read == (add_bytes + 1));
							nextmb = UTF8_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
							if (WEOF == utf_code)
							{	/* invalid mb char */
								SETZACANCELTIMER;
								iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
										    char_start, rm_ptr->inbuf_pos);
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start,
									rm_ptr->inbuf_pos, 0, NULL);
							}
							assert(nextmb == rm_ptr->inbuf_pos);
							rm_ptr->inbuf_off = nextmb;	/* mark as read */
							rm_ptr->crlast = FALSE;
							if (u32_line_term[U32_LT_NL] == utf_code ||
									u32_line_term[U32_LT_LS] == utf_code ||
									u32_line_term[U32_LT_PS] == utf_code)
							{
								line_term_seen = TRUE;
								if (!rdone)
									break;
							}
							memcpy(temp, char_start, char_bytes_read);
							temp += char_bytes_read;
						}
						bytes_count += char_bytes_read;
						if (bytes_count > MAX_STRLEN)
						{	/* need to leave bytes for this character in buffer */
							bytes_count -= char_bytes_read;
							rm_ptr->inbuf_off = char_start;
							break;
						}
					} else if (CHSET_UTF16BE == chset || CHSET_UTF16LE == chset)
					{
						if (2 == char_bytes_read)
						{
							if (CHSET_UTF16BE == chset)
								add_bytes = UTF16BE_MBFOLLOW(char_start, rm_ptr->inbuf_pos);
							else
								add_bytes = UTF16LE_MBFOLLOW(char_start, rm_ptr->inbuf_pos);
							if (1 < add_bytes)
							{	/* UTF16xE_MBFOLLOW returns 1 or 3 if valid */
								bytes2read = add_bytes - 1;
								continue;
							} else if (-1 == add_bytes)
							{	/*  not valid */
								SETZACANCELTIMER;
                                                                iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
										    char_bytes_read,
                                                                                    char_start, rm_ptr->inbuf_pos);
								rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
								UTF8_BADCHAR(char_bytes_read, char_start, rm_ptr->inbuf_pos,
									chset_names[chset].len, chset_names[chset].addr);
							}
						}
						assert(char_bytes_read == (add_bytes + 1));
						if (CHSET_UTF16BE == chset)
							nextmb = UTF16BE_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
						else
							nextmb = UTF16LE_MBTOWC(char_start, rm_ptr->inbuf_pos, utf_code);
						if (WEOF == utf_code)
						{	/* invalid mb char */
							SETZACANCELTIMER;
							iorm_readfl_badchar(v, ((unsigned char *)temp - stringpool.free),
									    char_bytes_read, char_start, rm_ptr->inbuf_pos);
							rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
							UTF8_BADCHAR(char_bytes_read, char_start, rm_ptr->inbuf_pos,
								chset_names[chset].len, chset_names[chset].addr);
						}
						assert(nextmb == rm_ptr->inbuf_pos);
						if (u32_line_term[U32_LT_LF] == utf_code)
						{
							if (rm_ptr->crlast)
							{	/* ignore LF following CR */
								rm_ptr->crlast = FALSE;
								rm_ptr->inbuf_pos = char_start;
								bytes2read = 2;				/* reset */
								bytes_read = char_bytes_read = 0;	/* start fresh */
								tot_bytes_read -= 2;
								continue;
							}
						}
						rm_ptr->inbuf_off = rm_ptr->inbuf_pos;	/* mark as read */
						if (u32_line_term[U32_LT_CR] == utf_code)
							rm_ptr->crlast = TRUE;
						else
							rm_ptr->crlast = FALSE;
						for (ltind = 0; 0 < u32_line_term[ltind]; ltind++)
							if (u32_line_term[ltind] == utf_code)
							{
								line_term_seen = TRUE;
								break;
							}
						if (line_term_seen && !rdone)
							break;		/* out of do loop */
						temp_start = temp;
						temp = (char *)UTF8_WCTOMB(utf_code, temp_start);
						bytes_count += (temp - temp_start);
						if (bytes_count > MAX_STRLEN)
						{	/* need to leave bytes for this character in buffer */
							bytes_count -= (temp - temp_start);
							rm_ptr->inbuf_off = char_start;
							break;
						}
					} else
						GTMASSERT;
					char_count++;
					char_start = rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
					bytes_read = char_bytes_read = 0;
					bytes2read = (CHSET_UTF8 == chset) ? 1 : 2;
				} else
				{
					inchar = 0;
					if (errno == 0)
						status = 0;
					else if (errno == EINTR)
					{
						if (out_of_time)
							status = -2;
						else
							continue;		/* Ignore interrupt if not our wakeup */
					}
					break;
				}
			} while (char_count < width && bytes_count < MAX_STRLEN);
		}
 	}
	if (status == EOF && errno != EINTR)
	{
		io_ptr->dollar.za = 9;
		v->str.len = 0;
		v->str.addr = (char *)stringpool.free;		/* ensure valid address */
		if (timed && !out_of_time)
			cancel_timer(timer_id);
		rts_error(VARLSTCNT(1) errno);
	}

	if (timed)
	{
		if (msec_timeout == 0)
		{
			FCNTL3(rm_ptr->fildes, F_SETFL, flags, fcntl_res);
			if (rm_ptr->fifo  &&  status == 0)
				ret = FALSE;
		} else
		{
			if (out_of_time)
				ret = FALSE;
			else
				cancel_timer(timer_id);
		}
	}

	if (status == 0 && tot_bytes_read == 0 && !rm_ptr->fifo)
	{
		v->str.len = 0;
		v->str.addr = (char *)stringpool.free;		/* ensure valid address */
		UNICODE_ONLY(v->str.char_len = 0;)
		if (io_ptr->dollar.zeof == TRUE)
		{
			io_ptr->dollar.za = 9;
			rts_error(VARLSTCNT(1) ERR_IOEOF);
		}
		io_ptr->dollar.zeof = TRUE;
		io_ptr->dollar.x = 0;
		io_ptr->dollar.za = 0;
		io_ptr->dollar.y++;
		if (io_ptr->error_handler.len > 0)
			rts_error(VARLSTCNT(1) ERR_IOEOF);
	} else
	{
		if (!utf_active || !rm_ptr->fixed)
		{	/* if Unicode and fixed, already setup the mstr */
			v->str.len = bytes_count;
			v->str.addr = (char *)stringpool.free;
			UNICODE_ONLY(v->str.char_len = char_count;)
			NON_UNICODE_ONLY(char_count = bytes_count;)
		}
		if (!rm_ptr->fixed && line_term_seen)
		{
		    	io_ptr->dollar.x = 0;
			io_ptr->dollar.y++;
		} else
		{
			io_ptr->dollar.x += char_count;
			if (io_ptr->dollar.x >= io_ptr->width && io_ptr->wrap)
			{
				io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
				if(io_ptr->length)
					io_ptr->dollar.y %= io_ptr->length;
				io_ptr->dollar.x %= io_ptr->width;
			}
		}
	}
	io_ptr->dollar.za = 0;
	return ret;
}
