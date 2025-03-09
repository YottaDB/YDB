/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <wctype.h>
#include <wchar.h>
#include <gtm_signal.h>
#include "gtm_string.h"
#include "gtm_poll.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iottdef.h"
#include "iott_edit.h"
#include "trmdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "error.h"
#include "std_dev_outbndset.h"
#include "wake_alarm.h"
#include "svnames.h"
#include "op.h"
#include "util.h"
#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF	bool			out_of_time;
GBLREF	boolean_t		gtm_utf8_mode, hup_on, prin_in_dev_failure, prin_out_dev_failure;
GBLREF	int4			exi_condition;
GBLREF	io_pair			io_curr_device, io_std_device;
GBLREF	mval			dollar_zstatus;
GBLREF	mv_stent		*mv_chain;
GBLREF	stack_frame		*frame_pointer;
GBLREF	unsigned char		*msp, *stackbase, *stacktop, *stackwarn;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	volatile int4		outofband;

LITREF	unsigned char	lower_to_upper_table[];

error_def(ERR_IOEOF);
error_def(ERR_NOPRINCIO);
error_def(ERR_TERMHANGUP);
error_def(ERR_ZINTRECURSEIO);

int	iott_rdone (mint *v, uint8 nsec_timeout)	/* timeout in nanoseconds */
{
	ABS_TIME	cur_time, end_time;
	boolean_t	ch_set, first_time, ret = FALSE, timed, utf8_active, zint_restart;
	char		dc1, dc3;
	d_tt_struct	*tt_ptr;
	int		inchar_width, msk_in, msk_num, rdlen, selstat, status, utf8_more;
	io_desc		*io_ptr;
	mv_stent	*mv_zintdev;
	short int	i;
	boolean_t	echo_mode;  			//kt added
	ttio_state	temp_io_state; 			//kt added
	boolean_t	char_is_terminator;		//kt added
	TID		timer_id;
	tt_interrupt	*tt_state;
	unsigned char	inbyte, *zb_ptr, *zb_top;
	unsigned char	more_buf[GTM_MB_LEN_MAX + 1], *more_ptr;	/* to build up multi byte for character */
	wint_t		inchar;
	int		poll_timeout;
	nfds_t		poll_nfds;
	struct pollfd	poll_fdlist[1];
#ifdef __MVS__
	wint_t		asc_inchar;
#endif
	int		utf8_seen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	io_ptr = io_curr_device.in;
	if (ERR_TERMHANGUP == error_condition)
	{
		TERMHUP_NOPRINCIO_CHECK(FALSE);				/* FALSE for READ */
		io_ptr->dollar.za = ZA_IO_ERR;
		return FALSE;
	}
	ESTABLISH_RET_GTMIO_CH(&io_curr_device, -1, ch_set);
	assert(io_ptr->state == dev_open);
	iott_flush(io_curr_device.out);
	tt_ptr = (d_tt_struct*) io_ptr->dev_sp;
	//kt NOTE: reading just 1 character is not compatible with canonical mode, which holds onto all input until LF (or CR) entered etc.
	iott_setterm_for_no_canonical(io_ptr, &temp_io_state);	//kt added.  Establishes temp_io_state for use here in this function
	echo_mode = temp_io_state.ydb_echo;  		//kt added
	*v = -1;
	dc1 = (char) 17;
	dc3 = (char) 19;
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	zint_restart = FALSE;
	if (tt_ptr->mupintr)
	{	/* restore state to before job interrupt */
		tt_state = &tt_ptr->tt_state_save;
		assertpro(ttwhichinvalid != tt_state->who_saved);
		if (dollar_zininterrupt)
		{
			tt_ptr->mupintr = FALSE;
			tt_state->who_saved = ttwhichinvalid;
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
		}
		assertpro(ttrdone == tt_state->who_saved);	/* ZINTRECURSEIO should have caught */
		mv_zintdev = io_find_mvstent(io_ptr, FALSE);
		assert(NULL != mv_zintdev);
		mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
		mv_zintdev->mv_st_cont.mvs_zintdev.io_ptr = NULL;
		if (mv_chain == mv_zintdev)
			POP_MV_STENT();         /* pop if top of stack */
		/* The below two asserts ensure the invocation of "iott_rdone" after a job interrupt has
		 * the exact same "nsec_timeout" as well as "timed" variable context. This is needed to
		 * ensure that the "end_time" usages in the post-interrupt invocation always happen
		 * only if the pre-interrupt invocation had initialized "end_time".
		 * Note: Since "timed" is not yet set, we cannot use it but instead use the variables that it derives from.
		 */
		assert((NO_M_TIMEOUT != nsec_timeout) == tt_state->timed);
		assert((nsec_timeout) == tt_state->nsec_timeout);
		end_time = tt_state->end_time;
		if (utf8_active)
		{
			utf8_more = tt_state->utf8_more;
			if (utf8_more)
			{
				utf8_seen = tt_state->utf8_seen;
				assert(0 < utf8_seen);
				assert(GTM_MB_LEN_MAX >= (utf8_seen + utf8_more));
				memcpy(more_buf, tt_state->more_buf, utf8_seen);
				more_ptr = more_buf + utf8_seen;
			}
		}
		zb_ptr = tt_state->zb_ptr;
		zb_top = tt_state->zb_top;
		tt_state->who_saved = ttwhichinvalid;
		tt_ptr->mupintr = FALSE;
		zint_restart = TRUE;
	}
	if (!zint_restart)
	{
		utf8_more = 0;
		/* ---------------------------------------------------------
		 * zb_ptr is used to fill-in the value of $zb as we go
		 * If we drop-out with error or otherwise permaturely,
		 * consider $zb to be null.
		 * ---------------------------------------------------------
		 */
		zb_ptr = io_ptr->dollar.zb;
		zb_top = zb_ptr + SIZEOF(io_ptr->dollar.zb) - 1;
		*zb_ptr = 0;
		io_ptr->esc_state = START;
		io_ptr->dollar.za = 0;
		io_ptr->dollar.zeof = FALSE;
		if (temp_io_state.no_type_ahead)  //kt mod
			TCFLUSH(tt_ptr->fildes, TCIFLUSH, status);
		if (temp_io_state.readsync)  //kt mod
		{
			DOWRITERC(tt_ptr->fildes, &dc1, 1, status);
			if (0 != status)
			{
				io_ptr->dollar.za = ZA_IO_ERR;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
		}
	}
	out_of_time = FALSE;
	if (NO_M_TIMEOUT == nsec_timeout)
	{
		timed = FALSE;
		poll_timeout = 100 * MILLISECS_IN_SEC;
	} else
	{
		timed = TRUE;
		poll_timeout = DIVIDE_ROUND_UP(nsec_timeout, NANOSECS_IN_MSEC);
		if (0 == nsec_timeout)
		{
			if (!zint_restart)
				iott_mterm(io_ptr);
		} else
		{
			sys_get_curr_time(&cur_time);
			if (!zint_restart)
				add_uint8_to_abs_time(&cur_time, nsec_timeout, &end_time);
		}
	}
	first_time = TRUE;
	do
	{
		if (outofband)
		{
			if (jobinterrupt == outofband)
			{	/* save state if jobinterrupt */
				tt_state = &tt_ptr->tt_state_save;
				tt_state->exp_length = 0;
				tt_state->who_saved = ttrdone;
				/* Note that "end_time" might be uninitialized in some cases (e.g. if "nsec_timeout" is
				 * NO_M_TIMEOUT or 0) but it is okay since when it is restored, we are guaranteed that
				 * the post-interrupt invocation of "iott_rdone" (after the jobinterrupt is handled)
				 * will use the restored "end_time" only if "nsec_timeout" (which will have the exact
				 * same value as the pre-interrupt invocation of "iott_rdone" thanks to the xf_restartpc
				 * invocation in OC_RDONE in ttt.txt) is not NO_M_TIMEOUT or 0.
				 */
				tt_state->end_time = end_time;
				if (utf8_active)
				{
					tt_state->utf8_more = utf8_more;
					if (utf8_more)
					{
						utf8_seen = (int)((UINTPTR_T)more_ptr - (UINTPTR_T)more_buf);
						assert(0 < utf8_seen);
						assert(GTM_MB_LEN_MAX >= (utf8_seen + utf8_more));
						tt_state->utf8_seen = utf8_seen;
						memcpy(tt_state->more_buf, more_buf, utf8_seen);
					}
				}
				tt_state->zb_ptr = zb_ptr;
				tt_state->zb_top = zb_top;
#				ifdef DEBUG
				/* Store debug-only context used later to assert when restoring this context */
				tt_state->timed = timed;
				tt_state->nsec_timeout = nsec_timeout;
#				endif
				PUSH_MV_STENT(MVST_ZINTDEV);	/* used as a flag only */
				mv_chain->mv_st_cont.mvs_zintdev.buffer_valid = FALSE;
				mv_chain->mv_st_cont.mvs_zintdev.io_ptr = io_ptr;
				tt_ptr->mupintr = TRUE;
			} else
			{
				if (timed && (0 == nsec_timeout))
					iott_rterm(io_ptr);
			}
			REVERT_GTMIO_CH(&io_curr_device, ch_set);
			async_action(FALSE);
			break;
		}
		if (!first_time)
		{
			if (timed)
			{
				if (0 != nsec_timeout)
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (0 > cur_time.tv_sec)
					{
						ret = FALSE;
						break;
					}
					poll_timeout = (long)((cur_time.tv_sec * MILLISECS_IN_SEC) +
						DIVIDE_ROUND_UP((gtm_tv_usec_t)cur_time.tv_nsec, NANOSECS_IN_MSEC));
				}
			} else
			{
				poll_timeout = 100 * MILLISECS_IN_SEC;
			}
		} else
			first_time = FALSE;
		/* the checks for EINTR below are valid and should not be converted to EINTR
		 * wrapper macros, since the select/read is not retried on EINTR.
		 */
		poll_fdlist[0].fd = tt_ptr->fildes;
		poll_fdlist[0].events = POLLIN;
		poll_nfds = 1;
		selstat = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
		if (0 > selstat)
		{
			if (EINTR != errno)
			{
				io_ptr->dollar.za = ZA_IO_ERR;
				if (timed && (0 == nsec_timeout))
					iott_rterm(io_ptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				break;
			}
			eintr_handling_check();
		} else if (0 == selstat)
		{
			if (timed)
			{
				wake_alarm();	/* sets out_of_time to be true for zero as well as non-zero timeouts */
				break;
			}
			continue;	/* select() timeout; try again */
		} else if ((rdlen = (int)(read(tt_ptr->fildes, &inbyte, 1))) == 1)	/* This read is protected */
		{
			HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			/* --------------------------------------------------
			 * set prin_in_dev_failure to FALSE to indicate that
			 * input device is working now.
			 * --------------------------------------------------
			 */
			prin_in_dev_failure = FALSE;
#			ifdef UTF8_SUPPORTED
			if (utf8_active)
			{
				if (temp_io_state.discard_lf)  				//kt mod
				{	/* saw CR last time so ignore following LF */
					temp_io_state.discard_lf = FALSE; 		//kt mod
					if (NATIVE_LF == inbyte)
						continue;
				}
				if (utf8_more)
				{	/* needed extra bytes */
					*more_ptr++ = inbyte;
					if (--utf8_more)
						continue;	/* get next byte */
					UTF8_MBTOWC(more_buf, more_ptr, inchar);
					if (WEOF == inchar)
					{	/* invalid char */
						io_ptr->dollar.za = ZA_IO_ERR;
						/* No data to return */
						iott_readfl_badchar(NULL, NULL, 0,
								    (int)((more_ptr - more_buf)), more_buf, more_ptr, NULL);
						utf8_badchar((int)(more_ptr - more_buf), more_buf, more_ptr, 0, NULL); /* BADCHAR */
						break;
					}
				} else
				{
					more_ptr = more_buf;
					if (0 < (utf8_more = UTF8_MBFOLLOW(&inbyte)))	/* assignment */
					{
						if ((0 > utf8_more) || (GTM_MB_LEN_MAX < utf8_more))
						{	/* invalid character */
							io_ptr->dollar.za = ZA_IO_ERR;
							*more_ptr++ = inbyte;
							 /* No data to return */
							iott_readfl_badchar(NULL, NULL, 0, 1, more_buf, more_ptr, NULL);
							utf8_badchar(1, more_buf, more_ptr, 0, NULL);	/* ERR_BADCHAR */
							break;
						} else
						{
							*more_ptr++ = inbyte;
							continue;	/* get next byte */
						}
					} else
					{	/* single byte */
						*more_ptr++ = inbyte;
						UTF8_MBTOWC(more_buf, more_ptr, inchar);
						if (WEOF == inchar)
						{	/* invalid char */
							io_ptr->dollar.za = ZA_IO_ERR;
							 /* No data to return */
							iott_readfl_badchar(NULL, NULL, 0, 1, more_buf, more_ptr, NULL);
							utf8_badchar(1, more_buf, more_ptr, 0, NULL);   /* ERR_BADCHAR */
							break;
						}
					}
				}
				if (temp_io_state.case_convert) 		 //kt mod
					inchar = u_toupper(inchar);
				GTM_IO_WCWIDTH(inchar, inchar_width);
			} else
			{
#			endif
				if (temp_io_state.case_convert)  		//kt mod
					NATIVE_CVT2UPPER(inbyte, inbyte);
				inchar = inbyte;
				inchar_width = 1;
#			ifdef UTF8_SUPPORTED
			}
#			endif
			GETASCII(asc_inchar, inchar);
			if (INPUT_CHAR < ' '  &&  ((1 << INPUT_CHAR) & tt_ptr->enbld_outofbands.mask))
			{
				std_dev_outbndset(INPUT_CHAR);
				if (timed)
				{
					if (0 == nsec_timeout)
				  		iott_rterm(io_ptr);
				}
				async_action(FALSE);
				ret = FALSE;
				break;
			}
			else if ((temp_io_state.escape_processing)  //kt mod
				 && (inchar == NATIVE_ESC  ||  io_ptr->esc_state != START))
			{
				//kt --- start mod ----
				//Starting Subloop for escape sequence input
				//
				//Will change timeout to 100 milliseconds, because state is either:
				//   1) in an escape sequence read -- in which case rest of sequence should quickly be in buffer, or...
				//   2) we got isolated ESC from user.
				//
				//If input soruce is a slow serial connection, then even at 300 baud (very very slow), max delay between
				//   chars would be 30 ms/char. So if 100 ms pass, and nothing follows ESC, then consider this to
				//   be an isolated ESC from user.
				//NOTE: Even if an initial timeout was provided, we should override here.
				//      For example:
				//        If READ *C:30, and user presses isolated ESC, then select() below would otherwise wait
				//        30 seconds for further input--though it won't be coming. So input would seem to hang,
				//        even though user has entered character (ESC).
				poll_timeout = 100 * MILLISECS_IN_SEC;  // 0.100 sec
				//kt --- end mod ----
				*v = INPUT_CHAR;
				ret = FALSE;
				do
				{
					if (zb_ptr >= zb_top UTF8_ONLY(|| (utf8_active && ASCII_MAX < inchar)))
					{
						/* -------------
						 * $zb overflow
						 * -------------
						 */
						io_ptr->dollar.za = 2;
						break;
					}
					*zb_ptr++ = (unsigned char)inchar;
					iott_escape(zb_ptr - 1, zb_ptr, io_ptr);
					/*	RESTORE_DOLLARZB(asc_inchar);	*/
					*(zb_ptr - 1) = (unsigned char)INPUT_CHAR;	/* need to store ASCII value	*/
					if (io_ptr->esc_state == FINI)
					{
						ret = TRUE;
						break;
					}
					if (io_ptr->esc_state == BADESC)
					{
						/* ------------------------------
						 * Escape sequence failed parse.
						 * ------------------------------
						 */
						io_ptr->dollar.za = 2;
						break;
					}
					//kt Start mod -----------
					poll_fdlist[0].fd = tt_ptr->fildes;
					poll_fdlist[0].events = POLLIN;
					poll_nfds = 1;
					selstat = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
					if (0 == selstat) {
						//IO buffer empty, nothing found during timeout interval. Bad escape sequence, or user entered isolated ESC key
						io_ptr->esc_state == BADESC; //Is this needed?  Copying 'Escape sequence failed parse' above.
						io_ptr->dollar.za = 2;       //Is this needed?  Copying 'Escape sequence failed parse' above.
						break;
					}
					//kt End mod -------------
					DOREADRL(tt_ptr->fildes, &inbyte, 1, rdlen);
					inchar = inbyte;
					GETASCII(asc_inchar, inchar);
				} while (1 == rdlen);
				*zb_ptr++ = 0;
				if (rdlen != 1  &&  io_ptr->dollar.za == 0)
					io_ptr->dollar.za = ZA_IO_ERR;
				/* -------------------------------------------------
				 * End of escape sequence...do not process further.
				 * -------------------------------------------------
				 */
				break;
			} else
			{	/* may need to deal with terminators > ASCII_MAX and/or LS and PS if default_mask_term */
				ret = TRUE;
				char_is_terminator = IS_TERMINATOR(temp_io_state.mask_term.mask, INPUT_CHAR, utf8_active);
				//KT NOTE: Doesn't handle special terminators like other READ functions does.
				//         Consider handling with IS_SPECIAL_TERMINATOR() in future?
				if (!char_is_terminator  &&  echo_mode) //kt mod
				{
					status = iott_write_raw(tt_ptr->fildes,
						utf8_active ? (void *)&inchar : (void *)&inbyte, 1);
					if (0 >= status)
					{
						status = errno;
						io_ptr->dollar.za = ZA_IO_ERR;
						if (timed)
						{
							if (0 == nsec_timeout)
								iott_rterm(io_ptr);
						}
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1)  status);
					}
				}
				break;
			}
		} else if (rdlen < 0)
		{
			if (EINTR != errno)
			{
				HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
				io_ptr->dollar.za = ZA_IO_ERR;
				if (timed && (0 == nsec_timeout))
					iott_rterm(io_ptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				break;
			}
			eintr_handling_check();
		} else
		/* ------------
		 * rdlen == 0
		 * ------------
		 */
		{	/* ---------------------------------------------------------
			 * poll() says there's something to read, but
			 * read() found zero characters; assume connection dropped.
			 * ---------------------------------------------------------
			 */
			ISSUE_NOPRINCIO_IF_NEEDED(io_ptr, FALSE, FALSE);	/* FALSE, FALSE: READ, not socket*/
			if (io_ptr->dollar.zeof)
			{
				io_ptr->dollar.za = ZA_IO_ERR;
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
			} else
			{
				io_ptr->dollar.zeof = TRUE;
				io_ptr->dollar.za   = 0;
				if (io_ptr->error_handler.len > 0)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_IOEOF);
			}
			break;
		}
	} while (!out_of_time);
	if (timed)
	{
		if (0 == nsec_timeout)
			iott_rterm(io_ptr);
	}
	if (temp_io_state.readsync)  //kt mod
	{
		DOWRITERC(tt_ptr->fildes, &dc3, 1, status);
		if (0 != status)
		{
			io_ptr->dollar.za = ZA_IO_ERR;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}
	}
	if (outofband && jobinterrupt != outofband)
	{
		io_ptr->dollar.za = ZA_IO_ERR;
		REVERT_GTMIO_CH(&io_curr_device, ch_set);
		RESETTERM_IF_NEEDED(io_ptr, EXPECT_SETTERM_DONE_TRUE);
		return FALSE;
	}
	io_ptr->dollar.za = 0;
	if (ret  &&  io_ptr->esc_state != FINI)
	{
		*v = INPUT_CHAR;
		if ( BIT_FLAG_IS_ON(TT_EDITING, temp_io_state.ext_cap) && (temp_io_state.passthru == FALSE) && echo_mode )	 //kt mod
		{	/* keep above test in sync with iott_readfl */
			if (!utf8_active)
				iott_recall_array_add(tt_ptr, 1, inchar_width, 1, &INPUT_CHAR);
#			ifdef UTF8_SUPPORTED
			else
				iott_recall_array_add(tt_ptr, 1, inchar_width, SIZEOF(INPUT_CHAR), &INPUT_CHAR);
#			endif
		}
		char_is_terminator = IS_TERMINATOR(temp_io_state.mask_term.mask, INPUT_CHAR, utf8_active); //kt
		if (char_is_terminator) //kt mod
		{
			*zb_ptr++ = INPUT_CHAR;
			*zb_ptr++ = 0;
		}
		else
			io_ptr->dollar.zb[0] = '\0';
		if ( !char_is_terminator && echo_mode )  //kt mod
		{
			if ((io_ptr->dollar.x += inchar_width) >= io_ptr->width && io_ptr->wrap)
			{
				++(io_ptr->dollar.y);
				if (io_ptr->length)
					io_ptr->dollar.y %= io_ptr->length;
				io_ptr->dollar.x %= io_ptr->width;
				if (io_ptr->dollar.x == 0)
					DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, STRLEN(NATIVE_TTEOL));
			}
		}
	}
	memcpy(io_ptr->dollar.key, io_ptr->dollar.zb, (zb_ptr - io_ptr->dollar.zb));
	REVERT_GTMIO_CH(&io_curr_device, ch_set);

	iott_restoreterm(io_ptr); //kt added.  Restore ydb's current IO state, because this function modified the TTY IO subsystem.

	return ret;
}
