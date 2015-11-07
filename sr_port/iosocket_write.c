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

/* iosocket_write.c */

#include "mdef.h"

#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "dollarx.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "stringpool.h"
#include "send_msg.h"
#include "error.h"

GBLREF io_pair			io_curr_device;
#ifdef UNIX
GBLREF io_pair			io_std_device;
GBLREF bool			prin_out_dev_failure;
#endif

GBLREF tcp_library_struct	tcp_routines;
GBLREF mstr			chset_names[];
GBLREF UConverter		*chset_desc[];
GBLREF spdesc			stringpool;

error_def(ERR_SOCKWRITE);
error_def(ERR_TEXT);
error_def(ERR_CURRSOCKOFR);
error_def(ERR_ZFF2MANY);
error_def(ERR_DELIMSIZNA);
error_def(ERR_ZINTRECURSEIO);
UNIX_ONLY(error_def(ERR_NOPRINCIO);)

void	iosocket_write(mstr *v)
{
	iosocket_write_real(v, TRUE);
}

void	iosocket_write_real(mstr *v, boolean_t convert_output)
{
	io_desc		*iod;
	mstr		tempv;
	char		*out, *c_ptr, *c_top;
	int		in_b_len, b_len, status, new_len, c_len, mb_len;
	int		flags;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;

	DBGSOCK2((stdout, "socwrite: ************************** Top of iosocket_write\n"));
	iod = io_curr_device.out;
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	if (dsocketptr->mupintr)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;		/* return EPIPE instead of SIGPIPE */
#else
	flags = 0;
#endif
	socketptr->lastop = TCP_WRITE;
	if (socketptr->first_write)
	{ /* First WRITE, do following
	     1. Transition to UTF16BE if ochset is UTF16 and WRITE a BOM
	     2. Convert ZFF into ochset format so we don't need to convert every time ZFF is output
	     3. Convert DELIMITER 0 to OCHSET format to avoid repeated conversions of DELIM0 on output
	   */
		if (CHSET_UTF16 == iod->ochset)
		{
			DBGSOCK2((stdout, "socwrite: First write UTF16 -- writing BOM\n"));
			iod->ochset = CHSET_UTF16BE; /* per Unicode standard, assume big endian when endian
							format is unspecified */
			get_chset_desc(&chset_names[iod->ochset]);
			DOTCPSEND(socketptr->sd, UTF16BE_BOM, UTF16BE_BOM_LEN, flags, status);
			DBGSOCK2((stdout, "socwrite: TCP send of BOM-BE with rc %d\n", status));
			if (0 != status)
			{
				SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
				return;
			}
#ifdef UNIX
			else if (iod == io_std_device.out)
				prin_out_dev_failure = FALSE;
#endif
		}
		if (CHSET_UTF16BE == iod->ochset || CHSET_UTF16LE == iod->ochset) /* need conversion of ZFF and DELIM0 */
		{
			if (0 < socketptr->zff.len)
			{
				new_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset], &socketptr->zff, NULL,
							NULL);
				if (MAX_ZFF_LEN < new_len)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZFF2MANY, 2, new_len, MAX_ZFF_LEN);
				if (NULL != socketptr->zff.addr) /* we rely on newsocket.zff.addr being set to NULL
								    in iosocket_create() */
					socketptr->zff.addr = (char *)malloc(MAX_ZFF_LEN);
				socketptr->zff.len = new_len;
				UNICODE_ONLY(socketptr->zff.char_len = 0); /* don't care */
				memcpy(socketptr->zff.addr, stringpool.free, new_len);
			}

			if (0 < socketptr->n_delimiter)
			{
				new_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset],
								&socketptr->delimiter[0], NULL, NULL);
				if (MAX_DELIM_LEN < new_len)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DELIMSIZNA);
					return;
				}
				socketptr->odelimiter0.len = new_len;
				UNICODE_ONLY(socketptr->odelimiter0.char_len = socketptr->delimiter[0].char_len);
				socketptr->odelimiter0.addr = malloc(new_len);
				memcpy(socketptr->odelimiter0.addr, stringpool.free, new_len);
			}
		}
		socketptr->first_write = FALSE;
	}
	memcpy(iod->dollar.device, "0", SIZEOF("0"));
	if (CHSET_M != iod->ochset)
	{ /* For ochset == UTF-8, validate the output,
	   * For ochset == UTF-16[B|L]E, convert the output (and validate during conversion)
	   */
		if (CHSET_UTF8 == iod->ochset)
		{
			UTF8_LEN_STRICT(v->addr, v->len); /* triggers badchar error for invalid sequence */
			tempv = *v;
		} else
		{
			assert(CHSET_UTF16BE == iod->ochset || CHSET_UTF16LE == iod->ochset);
			/* Certain types of writes (calls from iosocket_wteol or _wtff) already have their output
			   converted. Converting again just wrecks it so avoid that when necessary.
			*/
			if (convert_output)
			{
				new_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset], v, NULL, NULL);
				tempv.addr = (char *)stringpool.free;
				tempv.len = new_len;
				/* Since there is no dependence on string pool between now and when we send the data,
				   we won't bother "protecting" the stringpool value. This space can be used again
				   by whomever needs it without us forcing a garbage collection due to IO reformat.
				*/
				/* stringpool.free += new_len; */
			} else
				tempv = *v;
		}
	} else
		tempv = *v;
	if (0 != (in_b_len = tempv.len))
	{
		DBGSOCK2((stdout, "socwrite: starting output loop (%d bytes) - iodwidth: %d  wrap: %d\n",
			  in_b_len, iod->width, iod->wrap));
		for (out = tempv.addr;  ; out += b_len)
		{
			DBGSOCK2((stdout, "socwrite: ---------> Top of write loop $x: %d  $y: %d  in_b_len: %d\n",
				  iod->dollar.x, iod->dollar.y, in_b_len));
			if (!iod->wrap)
				b_len = in_b_len;
			else
			{
				if ((iod->dollar.x >= iod->width) && (START == iod->esc_state))
				{
					/* Should this really be iosocket_Wteol() (for FILTER)? IF we call iosocket_wteol(),
					 * there will be recursion iosocket_Write -> iosocket_Wteol ->iosocket_Write */
					if (0 < socketptr->n_delimiter)
					{
						DOTCPSEND(socketptr->sd, socketptr->odelimiter0.addr, socketptr->odelimiter0.len,
								(socketptr->urgent ? MSG_OOB : 0) | flags, status);
						DBGSOCK2((stdout, "socwrite: TCP send of %d byte delimiter with rc %d\n",
							  socketptr->odelimiter0.len, status));
						if (0 != status)
						{
							SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
							return;
						}
#ifdef UNIX
						else if (iod == io_std_device.out)
							prin_out_dev_failure = FALSE;
#endif
					}
					iod->dollar.y++;
					iod->dollar.x = 0;
					DBGSOCK2((stdout, "socwrite: $x > width - wrote delimiter: %d  $x: %d  $y: %d\n",
						  (0 < socketptr->n_delimiter), iod->dollar.x, iod->dollar.y));
				}
				if ((START != iod->esc_state) || ((int)(iod->dollar.x + in_b_len) <= (int)iod->width))
				{ /* enough room even in the worst case, i.e., if width - dollar.x can accommodate in_b_len chars,
				   * it certainly can accommodate in_b_len bytes */
					b_len = in_b_len;
				} else
				{
					c_len = iod->width - iod->dollar.x;
					for (c_ptr = out, c_top = out + in_b_len, b_len = 0;
					     (c_ptr < c_top) && c_len--;
					     b_len += mb_len, c_ptr += mb_len)
					{
						mb_len = (CHSET_M       == iod->ochset) ? 0 :
						         (CHSET_UTF8    == iod->ochset) ? UTF8_MBFOLLOW(c_ptr) :
							 (CHSET_UTF16BE == iod->ochset) ? UTF16BE_MBFOLLOW(c_ptr, c_top) :
							 UTF16LE_MBFOLLOW(c_ptr, c_top);
						assert(-1 != mb_len);
						mb_len++;
					}
					DBGSOCK2((stdout, "socwrite: computing string length in chars: in_b_len: %d  mb_len: %d\n",
						  in_b_len, mb_len));
				}
			}
			assert(0 != b_len);
			DOTCPSEND(socketptr->sd, out, b_len, (socketptr->urgent ? MSG_OOB : 0) | flags, status);
			DBGSOCK2((stdout, "socwrite: TCP data send of %d bytes with rc %d\n", b_len, status));
			if (0 != status)
			{
				SOCKERROR(iod, socketptr, ERR_SOCKWRITE, status);
				return;
			}
#ifdef UNIX
			else if (iod == io_std_device.out)
				prin_out_dev_failure = FALSE;
#endif
			dollarx(iod, (uchar_ptr_t)out, (uchar_ptr_t)out + b_len);
			DBGSOCK2((stdout, "socwrite: $x/$y updated by dollarx():  $x: %d  $y: %d  filter: %d  escape:  %d\n",
				  iod->dollar.x, iod->dollar.y, iod->write_filter, iod->esc_state));
			in_b_len -= b_len;
			if (0 >= in_b_len)
				break;
		}
		iod->dollar.za = 0;
	}
	DBGSOCK2((stdout, "socwrite: <--------- Leaving iosocket_write\n"));
	return;
}
