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

#include <sys/ioctl.h>
#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_iconv.h"
#include "gtm_termios.h"
#include "gtm_unistd.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */
#include "gtm_stdlib.h"

#include "io_params.h"
#include "io.h"
#include "iottdef.h"
#include "iosp.h"
#include "trmdef.h"
#include "nametabtyp.h"
#include "copy.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "send_msg.h"
#include "namelook.h"
#include "gtm_conv.h"
#include "error.h"
#include "gtm_tputs.h"
#include "gtm_tparm.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "restrict.h"
#include "op.h"
#include "indir_enum.h"
#include "invocation_mode.h"
#include "sig_init.h"
#include "libyottadb.h"
#include "svnames.h"
#include "util.h"

LITDEF nametabent filter_names[] =
{
	{4, "CHAR*"},
	{3, "ESC*"},
	{6, "NOCHAR*"},
	{5, "NOESC*"}
};
LITDEF unsigned char filter_index[27] =
{
	0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2
	,2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4
};

const io_termmask NULL_TERM_MASK = { {0, 0, 0, 0, 0, 0, 0, 0} };

GBLREF boolean_t		ctrlc_on, hup_on, prin_in_dev_failure, prin_out_dev_failure;
GBLREF char			*CURSOR_ADDRESS, *CLR_EOL, *CLR_EOS;
GBLREF io_pair			io_curr_device, io_std_device;
GBLREF mval			dollar_zstatus;
GBLREF void			(*ctrlc_handler_ptr)();
GBLREF volatile boolean_t	dollar_zininterrupt;

LITREF unsigned char	io_params_size[];

error_def(ERR_DEVPARMNEG);
error_def(ERR_NOPRINCIO);
error_def(ERR_SYSCALL);
error_def(ERR_TCGETATTR);
error_def(ERR_TCSETATTR);
error_def(ERR_TTINVFILTER);
error_def(ERR_WIDTHTOOSMALL);
error_def(ERR_ZINTRECURSEIO);

void iott_use(io_desc * io_ptr, mval * devparms)
{
	d_tt_struct *		tt_ptr;
	int			p_offset;
	struct sigaction	act;
	boolean_t		ch_set;
	mval			mv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	p_offset = 0;
	assert(io_ptr->state == dev_open);
	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	iott_flush(io_ptr);
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	if (*(devparms->str.addr + p_offset) != iop_eol)
	{
		if (tt_ptr->mupintr)
			if (dollar_zininterrupt)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
			else
			{	/* The interrupted read was not properly resumed so clear it now */
				tt_ptr->mupintr = FALSE;
				tt_ptr->tt_state_save.who_saved = ttwhichinvalid;
				io_find_mvstent(io_ptr, TRUE);
			}

		//setup ydb IO state (but not TTY IO subsystem) based on device parameters
		iott_use_params_to_state(io_ptr, devparms);

		//Moved functionality of prior block of code into iott_compile_state_and_set_tty_and_ydb_echo(),
		//  and calling below outside IF block, so that even if there are no devparams, it will still be called.
	} else if (tt_ptr->mupintr && !dollar_zininterrupt)
	{	/* The interrupted read was not properly resumed so clear it now */
		tt_ptr->mupintr = FALSE;
		tt_ptr->tt_state_save.who_saved = ttwhichinvalid;
		io_find_mvstent(io_ptr, TRUE);	/* clear mv stack entry */
	}

	//Send IO state to TTY IO subsystem.  8 is default time, 1 is minimum chars; can be changed later.
	iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, 8, 1, handle_set_tty_err_mode_3);   //Moving funcitonality down from above.

	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
	return;
}

void handle_set_tty_err_mode_3(io_desc* io_ptr, int save_errno, int filedes)  //for SET_TTY_CHECK_ERRORS_MODE_3

{
	assert(WBTEST_YDB_KILL_TERMINAL == ydb_white_box_test_case_number);
	ISSUE_NOPRINCIO_BEFORE_RTS_ERROR_IF_APPROPRIATE(io_ptr);
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCSETATTR, 1, filedes, save_errno);
}


void iott_use_params_to_state(io_desc * io_ptr, mval * devparms)
//NOTE: This saves the IO state into ydb data structures, based on USE parameters.
//       AND it also does some actions, if param calls for action.  It does NOT write anything out to the TTY IO subsystem.
{
	d_tt_struct * 		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	iott_common_params_to_state(io_ptr, &tt_ptr->io_state, devparms, iott_is_valid_use_param);
}

boolean_t iott_is_valid_use_param(io_params_type aparam)  // is aparam valid for USE command?

{
	boolean_t	result;
	result = (
		(aparam == iop_canonical) 	||
		(aparam == iop_nocanonical) 	||
		(aparam == iop_empterm) 	||
		(aparam == iop_noempterm) 	||
		(aparam == iop_cenable) 	||
		(aparam == iop_nocenable) 	||
		(aparam == iop_clearscreen) 	||
		(aparam == iop_convert) 	||
		(aparam == iop_noconvert) 	||
		(aparam == iop_ctrap) 		||
		(aparam == iop_downscroll) 	||
		(aparam == iop_echo) 		||
		(aparam == iop_noecho) 		||
		(aparam == iop_editing) 	||
		(aparam == iop_noediting) 	||
		(aparam == iop_escape) 		||
		(aparam == iop_noescape) 	||
		(aparam == iop_eraseline) 	||
		(aparam == iop_exception) 	||
		(aparam == iop_filter) 		||
		(aparam == iop_nofilter) 	||
		(aparam == iop_flush) 		||
		(aparam == iop_hostsync) 	||
		(aparam == iop_nohostsync) 	||
		(aparam == iop_hupenable) 	||
		(aparam == iop_nohupenable) 	||
		(aparam == iop_insert) 		||
		(aparam == iop_noinsert) 	||
		(aparam == iop_length) 		||
		(aparam == iop_pasthru) 	||
		(aparam == iop_nopasthru) 	||
		(aparam == iop_readsync) 	||
		(aparam == iop_noreadsync) 	||
		(aparam == iop_terminator) 	||
		(aparam == iop_noterminator) 	||
		(aparam == iop_ttsync) 		||
		(aparam == iop_nottsync) 	||
		(aparam == iop_typeahead) 	||
		(aparam == iop_notypeahead) 	||
		(aparam == iop_upscroll) 	||
		(aparam == iop_width) 		||
		(aparam == iop_wrap) 		||
		(aparam == iop_nowrap) 		||
		(aparam == iop_x) 		||
		(aparam == iop_y) 		||
		(aparam == iop_ipchset) 	||
		(aparam == iop_opchset) 	||
		(aparam == iop_chset)	  	||
		//---- below are sometimes added to devparams in io_init, but apparently not processed.
		(aparam == iop_newversion)	||
		(aparam == iop_stream)		||
		(aparam == iop_nl)		||
		(aparam == iop_shared)		||
		(aparam == iop_readonly)		);

	return result;
}

void iott_common_params_to_state(io_desc* io_ptr, ttio_state* io_state_ptr, mval * devparms, devparam_validator is_valid_dev_param 		)
//NOTE: This saves the IO state into ydb data structures, based on OPEN and USE parameters.
//       AND it may also do some actions, if param calls for action.
//       It does NOT write anything out to the TTY IO subsystem.
{
	int			p_offset, fil_type, status;
	int4			len, length, width;
	int4			do_nothing;
	unsigned char		ch;
	io_params_type		aparam;
	char			dc1;
	char *			ttab;
	gtm_chset_t		temp_chset, old_ichset;
	d_tt_struct *		temp_ptr;
	d_tt_struct * 		tt_ptr;
	struct sigaction	act;
	io_desc	*		d_in;
	io_desc	*		d_out;
	io_termmask		mask_term;
	mstr			chset_mstr;
	boolean_t		dev_is_tt;
	boolean_t		flush_input = FALSE;
	boolean_t		terminator_specified = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	d_in = io_ptr->pair.in;
	d_out = io_ptr->pair.out;
	temp_ptr = (d_tt_struct *)d_in->dev_sp;
	dev_is_tt = (tt == d_in->type);
	mask_term = temp_ptr->io_state.mask_term;
	old_ichset = io_ptr->ichset;
	p_offset = 0;

	while (iop_eol != (ch = *(devparms->str.addr + p_offset++)))
	{
		aparam = (io_params_type)ch;
		if (is_valid_dev_param(aparam))
		{
			switch(aparam)
			{
				case iop_canonical:
					io_state_ptr->canonical = TRUE;
					break;
				case iop_nocanonical:
					io_state_ptr->canonical = FALSE;
					break;
				case iop_empterm:
					SET_BIT_FLAG_ON(TT_EMPTERM, io_state_ptr->ext_cap);
					break;
				case iop_noempterm:
					SET_BIT_FLAG_OFF(TT_EMPTERM, io_state_ptr->ext_cap);
					break;
				case iop_cenable:
					/* Note that this parameter is ignored in callin/simpleapi mode because ^C in this mode
					 * is treated as a process termination request.
					 */
					if (!ctrlc_on && !RESTRICTED(cenable) && !(MUMPS_CALLIN & invocation_mode))
					{	/* if it's already cenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* if this is $PRINCIPAL make sure the ctrlc_handler is enabled */
							if (!USING_ALTERNATE_SIGHANDLING)
							{
								sigemptyset(&act.sa_mask);
								act.sa_sigaction = ctrlc_handler_ptr;
								/* FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED (invoked in "ctrlc_handler") relies
								 * on "info" and "context" being passed in.
								 */
								act.sa_flags = YDB_SIGACTION_FLAGS;
								sigaction(SIGINT, &act, NULL);
							} else
							{	/* Make sure this handler is enabled */
								SET_ALTERNATE_SIGHANDLER(SIGINT, &ydb_altmain_sighandler);
							}
							ctrlc_on = TRUE;
						}
					}
					break;
				case iop_nocenable:
					/* Note that this parameter is ignored in callin/simpleapi mode because ^C in this mode
					 * is treated as a process termination request.
					 */
					if (ctrlc_on && !RESTRICTED(cenable) && !(MUMPS_CALLIN & invocation_mode))
					{	/* if it's already nocenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* If this is $PRINCIPAL may disable the ctrlc_handler */
							if (0 == (CTRLC_MSK & tt_ptr->enbld_outofbands.mask))
							{	/* But only if ctrap=$c(3) is not active */
								if (!USING_ALTERNATE_SIGHANDLING)
								{
									sigemptyset(&act.sa_mask);
									act.sa_flags = YDB_SIGACTION_FLAGS;
									/* Setting handler to null handler still allows to be forwarded */
									act.sa_sigaction = null_handler;
									sigaction(SIGINT, &act, NULL);
								} else
								{	/* Disable a handler for this signal (ignored) */
									SET_ALTERNATE_SIGHANDLER(SIGINT, NULL);
								}
							}
							ctrlc_on = FALSE;
						}
					}
					break;
				case iop_clearscreen:
					if (NULL != CLR_EOS)
						gtm_tputs(CLR_EOS, 1, outc);
					break;
				case iop_convert:
					if (dev_is_tt) io_state_ptr->case_convert = TRUE;
					break;
				case iop_noconvert:
					if (dev_is_tt) io_state_ptr->case_convert = FALSE;
					break;
				case iop_ctrap:
					GET_LONG(tt_ptr->enbld_outofbands.mask, devparms->str.addr + p_offset);
					if (!ctrlc_on)
					{	/* if cenable, ctrlc_handler active anyway, otherwise, depends on ctrap=$c(3) */
						sigemptyset(&act.sa_mask);
						if (CTRLC_MSK & tt_ptr->enbld_outofbands.mask)
							act.sa_sigaction = ctrlc_handler_ptr;
						else
							act.sa_sigaction = null_handler;
						act.sa_flags = YDB_SIGACTION_FLAGS;
						sigaction(SIGINT, &act, NULL);
					}
					break;
				case iop_downscroll:
					if (d_out->dollar.y > 0)
					{
						d_out->dollar.y--;
						if (NULL != CURSOR_ADDRESS)
							gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1,
								  outc);
					}
					break;
				case iop_echo:
					if (dev_is_tt) io_state_ptr->devparam_echo = TRUE;
					break;
				case iop_noecho:
					if (dev_is_tt) io_state_ptr->devparam_echo = FALSE;
					break;
				case iop_editing:
					if (io_curr_device.in == io_std_device.in)     //$PRINCIPAL only
					{
						SET_BIT_FLAG_ON(TT_EDITING, io_state_ptr->ext_cap);
					}
					break;
				case iop_noediting:
					if (io_curr_device.in == io_std_device.in)  // $PRINCIPAL only
						SET_BIT_FLAG_OFF(TT_EDITING, io_state_ptr->ext_cap);
					break;
				case iop_escape:
					if (dev_is_tt) io_state_ptr->escape_processing = TRUE;
					break;
				case iop_noescape:
					if (dev_is_tt) io_state_ptr->escape_processing = FALSE;
					break;
				case iop_eraseline:
					if (NULL != CLR_EOL)
						gtm_tputs(CLR_EOL, 1, outc);
					break;
				case iop_exception:
					DEF_EXCEPTION(devparms, p_offset, io_ptr);
					break;
				case iop_filter:
					len = *(devparms->str.addr + p_offset);
					ttab = devparms->str.addr + p_offset + 1;
					if ((fil_type = namelook(filter_index, filter_names, ttab, len)) < 0)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TTINVFILTER);
						return;
					}
					switch (fil_type)
					{
						case 0:
							SET_BIT_FLAG_ON(CHAR_FILTER, io_ptr->write_filter);
							break;
						case 1:
							SET_BIT_FLAG_ON(ESC1, io_ptr->write_filter);
							break;
						case 2:
							SET_BIT_FLAG_OFF(CHAR_FILTER, io_ptr->write_filter);
							break;
						case 3:
							SET_BIT_FLAG_OFF(ESC1, io_ptr->write_filter);
							break;
					}
					break;
				case iop_nofilter:
					io_ptr->write_filter = 0;
					break;
				case iop_flush:
					flush_input = TRUE;
					break;
				case iop_hostsync:
					io_state_ptr->hostsync = TRUE;
					break;
				case iop_nohostsync:
					io_state_ptr->hostsync = FALSE;
					break;
				case iop_hupenable:
					if (!hup_on)
					{	/* if it's already hupenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* if $PRINCIPAL, enable hup_handler; similar code in term_setup.c */
							if (!USING_ALTERNATE_SIGHANDLING)
							{
								sigemptyset(&act.sa_mask);
								act.sa_flags = YDB_SIGACTION_FLAGS;
								act.sa_sigaction = ctrlc_handler_ptr;
								sigaction(SIGHUP, &act, 0);
							} else
							{
								SET_ALTERNATE_SIGHANDLER(SIGHUP, &ydb_altmain_sighandler);
							}
							hup_on = TRUE;
						}
					}
					break;
				case iop_nohupenable:
					if (hup_on)
					{	/* if it's already nohupenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* if $PRINCIPAL, disable the hup_handler */
							if (!USING_ALTERNATE_SIGHANDLING)
							{
								sigemptyset(&act.sa_mask);
								act.sa_flags = 0;
								act.sa_handler = SIG_IGN;
								sigaction(SIGHUP, &act, 0);
							} else
							{
								SET_ALTERNATE_SIGHANDLER(SIGHUP, NULL);
							}
							hup_on = FALSE;
						}
					}
					break;
				case iop_insert:
					if (io_curr_device.in == io_std_device.in)		// $PRINCIPAL only
						SET_BIT_FLAG_OFF(TT_NOINSERT, io_state_ptr->ext_cap);		//turning OFF NOINSERT --> turns ON insert
					break;
				case iop_noinsert:
					if (io_curr_device.in == io_std_device.in)		//  $PRINCIPAL only
						SET_BIT_FLAG_ON(TT_NOINSERT, io_state_ptr->ext_cap);		//turning ON NOINSERT --> turns OFF insert
					break;
				case iop_length:
					GET_LONG(length, devparms->str.addr + p_offset);
					if (0 > length)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DEVPARMNEG);
					d_out->length = length;
					break;
				case iop_pasthru:
					if (dev_is_tt) io_state_ptr->passthru = TRUE;
					break;
				case iop_nopasthru:
					if (dev_is_tt) io_state_ptr->passthru = FALSE;
					break;
				case iop_readsync:
					if (dev_is_tt) io_state_ptr->readsync = TRUE;
					break;
				case iop_noreadsync:
					dc1 = (char)17;
					temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
					DOWRITERC(temp_ptr->fildes, &dc1, 1, status);
					if (0 != status)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
					if (dev_is_tt) io_state_ptr->readsync = FALSE;
					break;
				case iop_terminator:
					memcpy(&mask_term.mask[0], (devparms->str.addr + p_offset), SIZEOF(io_termmask));
					terminator_specified = TRUE;
					temp_ptr = (d_tt_struct *)d_in->dev_sp;
					temp_ptr->io_state.default_mask_term = (STRUCTS_ARE_EQUAL(mask_term, NULL_TERM_MASK));
					break;
				case iop_noterminator:
					temp_ptr = (d_tt_struct *)d_in->dev_sp;
					temp_ptr->io_state.default_mask_term = FALSE;
					mask_term = NULL_TERM_MASK;
					break;
				case iop_ttsync:
					if (dev_is_tt) io_state_ptr->ttsync = TRUE;
					break;
				case iop_nottsync:
					if (dev_is_tt) io_state_ptr->ttsync = FALSE; ;
					break;
				case iop_typeahead:
					if (dev_is_tt) io_state_ptr->no_type_ahead = FALSE;
					break;
				case iop_notypeahead:
					if (dev_is_tt) io_state_ptr->no_type_ahead = TRUE;
					break;
				case iop_upscroll:
					d_out->dollar.y++;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_width:
					GET_LONG(width, devparms->str.addr + p_offset);
					if (0 > width)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DEVPARMNEG);
					/* Do not allow a WIDTH of 1 if UTF mode (ICHSET or OCHSET is not M) */
					if ((1 == width) && ((CHSET_M != d_in->ochset) || (CHSET_M != d_in->ichset)))
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_WIDTHTOOSMALL);
					if (0 == width)
					{
						d_out->wrap = FALSE;
						d_out->width = TTDEF_PG_WIDTH;
					} else
					{
						d_out->width = width;
						d_out->wrap = TRUE;
					}
					break;
				case iop_wrap:
					d_out->wrap = TRUE;
					break;
				case iop_nowrap:
					d_out->wrap = FALSE;
					break;
				case iop_x:
					GET_LONG(d_out->dollar.x, devparms->str.addr + p_offset);
					if (0 > (int4)d_out->dollar.x)
						d_out->dollar.x = 0;
					if (d_out->dollar.x > d_out->width && d_out->wrap)
					{
						d_out->dollar.y += (d_out->dollar.x / d_out->width);
						if (d_out->length)
							d_out->dollar.y %= d_out->length;
						d_out->dollar.x	%= d_out->width;
					}
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_y:
					GET_LONG(d_out->dollar.y, devparms->str.addr + p_offset);
					if (0 > (int4)d_out->dollar.y)
						d_out->dollar.y = 0;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_ipchset:
				{
#					ifdef KEEP_zOS_EBCDIC
					if ((iconv_t)0 != io_ptr->input_conv_cd)
					{
						ICONV_CLOSE_CD(io_ptr->input_conv_cd);
					}
					SET_CODE_SET(io_ptr->in_code_set, (char *)(devparms->str.addr + p_offset + 1));
					if (DEFAULT_CODE_SET != io_ptr->in_code_set)
						ICONV_OPEN_CD(io_ptr->input_conv_cd,
							      (char *)(devparms->str.addr + p_offset + 1), INSIDE_CH_SET);
#					endif
					GET_ADDR_AND_LEN_V2(devparms, p_offset, chset_mstr.addr, chset_mstr.len);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode. */
					if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_BADCHSET, 2,
							      chset_mstr.len, chset_mstr.addr);
					io_ptr->ichset = temp_chset;
					break;
				}
				case iop_opchset:
				{
#					ifdef KEEP_zOS_EBCDIC
					if ((iconv_t)0 != io_ptr->output_conv_cd)
					{
						ICONV_CLOSE_CD(io_ptr->output_conv_cd);
					}
					SET_CODE_SET(io_ptr->out_code_set, (char *)(devparms->str.addr + p_offset + 1));
					if (DEFAULT_CODE_SET != io_ptr->out_code_set)
						ICONV_OPEN_CD(io_ptr->output_conv_cd, INSIDE_CH_SET,
							      (char *)(devparms->str.addr + p_offset + 1));
#					endif
					GET_ADDR_AND_LEN_V2(devparms, p_offset, chset_mstr.addr, chset_mstr.len);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode. */
					if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_BADCHSET, 2,
							      chset_mstr.len, chset_mstr.addr);
					io_ptr->ochset = temp_chset;
					break;
				}
				case iop_chset:
				{
					GET_ADDR_AND_LEN_V2(devparms, p_offset, chset_mstr.addr, chset_mstr.len);
					SET_ENCODING(temp_chset, &chset_mstr);
					if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
						break;	/* ignore UTF chsets if not utf8_mode. */
					if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_BADCHSET, 2,
							      chset_mstr.len, chset_mstr.addr);
					io_ptr->ichset = io_ptr->ochset = temp_chset;
					break;
				}
				//---- below exclusively from iott_open params
				case iop_m:
					io_ptr->ichset = io_ptr->ochset = CHSET_M;
					break;
				case iop_utf8:
					if (gtm_utf8_mode)
						io_ptr->ichset = io_ptr->ochset = CHSET_UTF8;
					break;
				//---- below are sometimes added to devparams in io_init, but apparently not processed.
				case iop_newversion:
					do_nothing = 1; //Just a stopping place for debugger
					break;
				case iop_stream:
					do_nothing = 2; //Just a stopping place for debugger
					break;
				case iop_nl:
					do_nothing = 3; //Just a stopping place for debugger
					break;
				case iop_shared:
					do_nothing = 4; //Just a stopping place for debugger
					break;
				case iop_readonly:
					do_nothing = 5; //Just a stopping place for debugger
					break;
				default:
					break;
			}
		}
		UPDATE_P_OFFSET(p_offset, ch, devparms);	// updates "p_offset" using "ch" and "devparms"
	}

	if (flush_input)
	{
		TCFLUSH(tt_ptr->fildes, TCIFLUSH, status);
		if (0 != status)
		{
			ISSUE_NOPRINCIO_BEFORE_RTS_ERROR_IF_APPROPRIATE(io_ptr);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("tcflush input"),
					      CALLFROM, errno);
		}
	}

	if (dev_is_tt)
	{
		/* if chset was changed without specifying new terminators or Default, then reset the mask to default  */
		if ((!terminator_specified && (old_ichset != io_ptr->ichset)) 		||
		    ( terminator_specified && (tt_ptr->io_state.default_mask_term)))
		{
			iott_set_mask_term_conditional(io_ptr, &mask_term, (CHSET_M != io_ptr->ichset), TRUE);
		}
		tt_ptr->io_state.mask_term = mask_term;
	}
}

void iott_set_mask_term_conditional(io_desc* io_ptr, io_termmask*  mask_term_ptr, boolean_t bool_test, boolean_t set_default)
{
	d_tt_struct * 		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	*mask_term_ptr = NULL_TERM_MASK;
	if (bool_test)
	{
		mask_term_ptr->mask[0] = TERM_MSK_UTF8_0;
		mask_term_ptr->mask[4] = TERM_MSK_UTF8_4;
	} else
	{
		mask_term_ptr->mask[0] = TERM_MSK;
	}
	if (set_default)
	{
		tt_ptr->io_state.default_mask_term = TRUE;
	}

}