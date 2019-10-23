/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "io.h"
#include "io_params.h"
#include "gtmio.h"
#include "iottdef.h"
#include "iormdef.h"
#include "iosocketdef.h"
#include "send_msg.h"
#include "error.h"

GBLDEF boolean_t	exiting_on_dev_out_error = FALSE;

GBLREF io_pair		io_std_device;
GBLREF io_pair		io_curr_device;
GBLREF bool		prin_out_dev_failure;
GBLREF boolean_t	in_prin_gtmio;

STATICDEF int		writing_to_pio = 0;

#define SET_MUPINTR(MUPINTR, TYPE)	\
	MUPINTR = ((TYPE *)io_std_device.out->dev_sp)->mupintr;

/* Functions for optionally writing to and flushing the principal device. We do not flush if an error has already occurred, to give
 * user's $ZT or EXCEPTION a chance to execute. Keep in mind that some utility programs do not have devices to flush.
 */

error_def(ERR_NOPRINCIO);

void flush_pio(void)
{
	if (io_std_device.out && !prin_out_dev_failure)
		(io_std_device.out->disp_ptr->flush)(io_std_device.out);
}

void write_text_newline_and_flush_pio(mstr *text)
{
	io_pair		save_io_curr_device;
	int		i, status, msg_length;
	char		*msg_start, c;
	boolean_t	mupintr, encrypted;

	/* We have already tried to stop the image, so returning here should not be allowed. */
	if (exiting_on_dev_out_error)
		return;
	/* If we are not yet stopping the image, make sure we are not nesting on this function. */
	if (3 > writing_to_pio)
	{
		writing_to_pio++;
		if ((!prin_out_dev_failure) && (!in_prin_gtmio))
		{	/* It is unsafe to continue using the principal device if we are in the middle of some I/O on it or have
			 * already encountered an error.
			 */
			assert(NULL != io_std_device.out);
			switch (io_std_device.out->type)
			{
				case tt:
					SET_MUPINTR(mupintr, d_tt_struct);
					encrypted = FALSE;
					break;
				case rm:
					SET_MUPINTR(mupintr, d_rm_struct);
					/* Encryption only supported for files, pipes, and FIFOs. */
					encrypted = IS_GTM_IMAGE
						? ((d_rm_struct *)io_std_device.out->dev_sp)->output_encrypted : FALSE;
					break;
				case gtmsocket:
					SET_MUPINTR(mupintr, d_socket_struct);
					encrypted = FALSE;
					break;
				default:
					assert(FALSE);
					mupintr = encrypted = FALSE;
			}
			/* Do not use GT.M I/O if got MUPIP-interrupted. */
			if (!mupintr)
			{	/* The wteol() call may invoke write() under the covers, so the current device needs to be correct
				 * for all calls below.
				 */
				save_io_curr_device = io_curr_device;
				io_curr_device = io_std_device;
				if ((NULL != text) && (0 != text->len))
				{
					if (0 < io_std_device.out->dollar.x)
					{	/* If we are going to write something before the newline and $X is currently
						 * non-zero, then we need to insert one newline first.
						 */
						(io_std_device.out->disp_ptr->wteol)(1, io_std_device.out);
					}
					msg_start = text->addr;
					msg_length = text->len;
					/* Find all newlines and form feeds and convert them into appropriate GT.M I/O functions. */
					for (i = 0; i < msg_length; i++)
					{
						c = *(msg_start + i);
						if ('\n' == c)
						{
							text->len = i - (text->addr - msg_start);
							(io_std_device.out->disp_ptr->write)(text);
							(io_std_device.out->disp_ptr->wteol)(1, io_std_device.out);
							text->addr += text->len + 1;
						} else if ('\f' == c)
						{
							text->len = i - (text->addr - msg_start);
							(io_std_device.out->disp_ptr->write)(text);
							(io_std_device.out->disp_ptr->wtff)();
							text->addr += text->len + 1;
						}
					}
					text->len = i - (text->addr - msg_start);
					if (0 != text->len)
					{	/* If we still have something to write, potentially after a form feed or newline, do
						 * write it and follow up with a newline.
						 */
						(io_std_device.out->disp_ptr->write)(text);
						(io_std_device.out->disp_ptr->wteol)(1, io_std_device.out);
					} else if ('\f' != c)
						(io_std_device.out->disp_ptr->wteol)(1, io_std_device.out);
				} else
					(io_std_device.out->disp_ptr->wteol)(1, io_std_device.out);
				(io_std_device.out->disp_ptr->flush)(io_std_device.out);
				io_curr_device = save_io_curr_device;
				writing_to_pio--;
				return;
			}
		} else
			encrypted = FALSE;
		/* Encrypted messages can only be printed using GT.M I/O, and since mixing encrypted and plain-text messages results
		 * in unencryptable content.
		 */
		if (!encrypted)
		{
			if ((NULL != text) && (0 != text->len))
			{
				status = FPRINTF(stderr, "%.*s", text->len, text->addr);
				if (0 <= status)
				{
					c = *(text->addr + text->len - 1);
					if ('\f' != c)
						status = FPRINTF(stderr, "\n");
				}
			} else
				status = FPRINTF(stderr, "\n");
			if (0 <= status)
			{
				FFLUSH(stderr);
				writing_to_pio--;
				return;
			}
		}
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOPRINCIO);
	}
	exiting_on_dev_out_error = TRUE;
	stop_image_no_core();
	writing_to_pio--;
	/* No assert for writing_to_pio being non-negative because if we made it past the stop_image_no_core() call above, something
	 * is seriously wrong, and there is no point counting on an assert.
	 */
}
