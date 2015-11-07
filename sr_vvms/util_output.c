/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include <climsgdef.h>
#include <descrip.h>
#include <fab.h>
#include <rab.h>
#include <opcdef.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <stdarg.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "fao_parm.h"
#include "util.h"
#include "gt_timer.h"

static struct RAB *util_output_rab = NULL;
static struct FAB *util_output_fab = NULL;

GBLDEF	unsigned int	sndopr_missed = 0, sndopr_mbfull = 0;

error_def(ERR_DEVOPENFAIL);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void format_special_chars(void);
void util_out_close(void);

boolean_t util_is_log_open(void)
{
	return (NULL != util_output_fab);
}

void util_log_open(char *filename, uint4 len)
{
	unsigned int	status;
	uint4		ustatus;
	char		exp_file_name[MAX_FN_LEN];
	int		exp_file_name_len;

	if(util_output_fab)
		util_out_close();
	memcpy(exp_file_name, filename, len);
	exp_file_name_len = len;
	if (!get_full_path(filename, len, exp_file_name, &exp_file_name_len, SIZEOF(exp_file_name), &ustatus))
		rts_error(VARLSTCNT(5) ERR_DEVOPENFAIL, 2, len, filename, ustatus);
	util_output_fab = malloc(SIZEOF(*util_output_fab));
	util_output_rab = malloc(SIZEOF(*util_output_rab));
	*util_output_fab  = cc$rms_fab;
	*util_output_rab  = cc$rms_rab;
	util_output_rab->rab$l_fab = util_output_fab;
	util_output_rab->rab$w_usz = OUT_BUFF_SIZE;
	util_output_fab->fab$w_mrs = OUT_BUFF_SIZE;
	util_output_fab->fab$b_fac = FAB$M_GET | FAB$M_PUT;
	util_output_fab->fab$b_rat = FAB$M_CR;
	util_output_fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET;
	util_output_fab->fab$l_fna = exp_file_name;
	util_output_fab->fab$b_fns = exp_file_name_len;
	status = sys$create(util_output_fab, 0, 0);
	if (1 & status)
		status = sys$connect(util_output_rab, 0, 0);
	if (RMS$_NORMAL != status)
		rts_error(VARLSTCNT(10) ERR_SYSCALL, 5, LEN_AND_LIT("SYS$CONNECT"), CALLFROM, status, 0,util_output_fab->fab$l_stv);
}

void util_out_open(struct dsc$descriptor_s *file_prompt)
{
	short unsigned			outnamlen;
	unsigned int			status;
	char				output_name[255];
	$DESCRIPTOR(output_name_desc, output_name);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(util_outptr) = TREF(util_outbuff_ptr);
	util_output_fab = NULL;
	util_output_rab = NULL;
	if (file_prompt)
	{
		status = cli$get_value(file_prompt, &output_name_desc, &outnamlen);
		if (SS$_NORMAL == status)
		{
			util_output_fab = malloc(SIZEOF(*util_output_fab));
			util_output_rab = malloc(SIZEOF(*util_output_rab));
			*util_output_fab  = cc$rms_fab;
			*util_output_rab  = cc$rms_rab;
			util_output_rab->rab$l_fab = util_output_fab;
			util_output_rab->rab$w_usz = OUT_BUFF_SIZE;
			util_output_fab->fab$w_mrs = OUT_BUFF_SIZE;
			util_output_fab->fab$b_fac = FAB$M_GET | FAB$M_PUT;
			util_output_fab->fab$b_rat = FAB$M_CR;
			util_output_fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET;
			util_output_fab->fab$l_fna = output_name_desc.dsc$a_pointer;
			util_output_fab->fab$b_fns = outnamlen;
			status = sys$create(util_output_fab, 0, 0);
			if (1 & status)
				status = sys$connect(util_output_rab, 0, 0);
			if (RMS$_NORMAL != status)
				rts_error(VARLSTCNT(10) ERR_SYSCALL, 5, LEN_AND_LIT("SYS$CONNECT"), CALLFROM, status, 0,
						util_output_fab->fab$l_stv);
		}
	}
}

void util_out_write(unsigned char *addr, unsigned int len)
{
	unsigned int	status;
	$DESCRIPTOR(output_mess_desc, "");
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == util_output_rab)
	{
		output_mess_desc.dsc$a_pointer = addr;
		output_mess_desc.dsc$w_length = len;
		status = lib$put_output(&output_mess_desc);
		if (SS$_NORMAL != status)
			lib$signal(status);
	} else
	{
		util_output_rab->rab$l_rbf = addr;
		util_output_rab->rab$w_rsz = len;
		assert(!lib$ast_in_prog());
		if (RMS$_NORMAL == (status = sys$put(util_output_rab, 0, 0)))
			status = sys$flush(util_output_rab);
		if (RMS$_NORMAL != status)
		{
			assert(FALSE);
			if (TREF(gtm_environment_init))
				GTMASSERT;
			gtm_putmsg(ERR_TEXT, 2, LEN_AND_LIT("Error in util_output"));
			lib$signal(status, util_output_rab->rab$l_stv);
		}
	}
	return;
}

void util_out_send_oper(char *addr, unsigned int len)
{

	unsigned int	status, thislen;
	int		retry = SNDOPR_TRIES;
	uchar_ptr_t	operptr;
	oper_msg_struct	oper;
	$DESCRIPTOR(opmsg, "");

	if (len > SIZEOF(oper.text))
		len = SIZEOF(oper.text);
	do
	{
		oper.req_code = OPC$_RQ_RQST;
		oper.target = OPC$M_NM_CENTRL | OPC$M_NM_DEVICE | OPC$M_NM_DISKS;
		memcpy(&oper.text, addr, len);
		opmsg.dsc$a_pointer = &oper;
		opmsg.dsc$w_length = SIZEOF(oper) - SIZEOF(oper.text) + len;
		status = sys$sndopr(&opmsg, 0);
		if (SS$_MBFULL == status)
			hiber_start(SNDOPR_DELAY);	/* OPCOM mailbox full so give it a chance to empty */
	} while ((SS$_MBFULL == status) && (0 < --retry));
	assert((SS$_NORMAL == status) || (OPC$_NOPERATOR == status) || (SS$_MBFULL == status));
	/* the documentation of sys$sndopr() indicates that a success status OPC-S-OPC$_NOPERATOR status
	 * gets returned in case OPCOM is not running. hence the explicit || check in the assert above.
	 * If %SYSTEM-W-MBFULL, mailbox is full, we gave it our best try but
	 * the message could be dropped if OPCOM is busy enough.
	 */
	if (SS$_MBFULL == status)
		sndopr_mbfull++;
	else if ((SS$_NORMAL != status) && (OPC$_NOPERATOR != status))
		sndopr_missed++;
	else if (0 < sndopr_mbfull || 0 < sndopr_missed)
	{	/* sndopr with info on missed and mbfull then reset */
		oper.req_code = OPC$_RQ_RQST;
		oper.target = OPC$M_NM_CENTRL | OPC$M_NM_DEVICE | OPC$M_NM_DISKS;
		thislen = SIZEOF(GTMOPCOMMISSED1) - 1;
		memcpy(&oper.text, GTMOPCOMMISSED1, thislen);
		operptr = i2asc((uchar_ptr_t)&oper.text + thislen, sndopr_missed);
		thislen = SIZEOF(GTMOPCOMMISSED2) - 1;
		memcpy(operptr, GTMOPCOMMISSED2, thislen);
		operptr = i2asc(operptr + thislen, sndopr_mbfull);
		thislen = SIZEOF(GTMOPCOMMISSED3) - 1;
		memcpy(operptr, GTMOPCOMMISSED3, thislen);
		opmsg.dsc$a_pointer = &oper;
		opmsg.dsc$w_length = SIZEOF(oper) - SIZEOF(oper.text) + ((operptr - &oper.text) + thislen);
		status = sys$sndopr(&opmsg, 0);
		if ((SS$_NORMAL == status) || (OPC$_NOPERATOR == status))
			sndopr_missed = sndopr_mbfull = 0;
	}
	return;
}

void util_out_close()
{
	unsigned int	status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(util_outbuff_ptr) != TREF(util_outptr))
		util_out_write(TREF(util_outbuff_ptr), TREF(util_outptr) - TREF(util_outbuff_ptr));
	if (NULL != util_output_fab)
	{
		status = sys$close(util_output_fab, 0, 0);
		free(util_output_fab);
		free(util_output_rab);
		util_output_fab = NULL;
		util_output_rab = NULL;
		if (RMS$_NORMAL != status)
			lib$signal(status);
	}
	return;
}

#define	NOFLUSH	0
#define FLUSH	1
#define RESET	2
#define OPER	4
#define SPRINT	5

void	util_out_print(char *message, int flush, ...)
{
	va_list	var;
	int4	cnt, faocnt, faolist[MAX_FAO_PARMS + 1];
	char	*util_format();
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, flush);
	va_count(cnt);

	cnt -= 2;	/* for message and flush */
	assert(cnt <= MAX_FAO_PARMS);
	faocnt = cnt;
	memset(faolist, 0, SIZEOF(faolist));
	for (cnt = 0;  cnt < faocnt;  cnt++)
		faolist[cnt] = va_arg(var, int4);
	va_end(var);
	if (message)
		TREF(util_outptr) = util_format(message, faolist, TREF(util_outptr),
			OUT_BUFF_SIZE - (TREF(util_outptr) - TREF(util_outbuff_ptr)));
	switch (flush)
	{
	case NOFLUSH :	break;
	case FLUSH   :	util_out_write(TREF(util_outbuff_ptr), TREF(util_outptr) - TREF(util_outbuff_ptr));
			TREF(util_outptr) = TREF(util_outbuff_ptr);
			break;
	case RESET   :	TREF(util_outptr) = TREF(util_outbuff_ptr);
			break;
	case OPER    :	util_out_send_oper(TREF(util_outbuff_ptr), TREF(util_outptr) - TREF(util_outbuff_ptr));
			TREF(util_outptr) = TREF(util_outbuff_ptr);
			break;
	case SPRINT  :  *(TREF(util_outptr)) = '\0';
			format_special_chars();
			TREF(util_outptr) = TREF(util_outbuff_ptr);
			break;
	default      :	break;
	}
	return;
}


char *util_format(char *message, int4 fao[], char *buff, int4 size)
{
	short			faolen;
	unsigned int		status;
	struct dsc$descriptor	desc;
	struct dsc$descriptor	out;

	desc.dsc$a_pointer = message;
	desc.dsc$b_dtype = DSC$K_DTYPE_T;
	desc.dsc$b_class = DSC$K_CLASS_S;
	desc.dsc$w_length = STRLEN(message);
	out.dsc$b_dtype = DSC$K_DTYPE_T;
	out.dsc$b_class = DSC$K_CLASS_S;
	out.dsc$a_pointer = buff;
	out.dsc$w_length = size;
	status = sys$faol(&desc, &faolen, &out, fao);
	if (SS$_NORMAL != status)
		lib$signal(status);
	return buff + faolen;
}

void format_special_chars(void)
{
	/* Taken from util_out_print_vaparm() of Unix (see there for potential truncation of input in case of buffer overflow) */
	char	fmt_buff[OUT_BUFF_SIZE];	/* needs to be same size as that of util_outbuff */
	char	*pout, *pin;
	char	*fmt_top1, *fmt_top2; /* the top of the buffer after leaving 1 (and 2 bytes respectively) at the end */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	fmt_top1 = fmt_buff + SIZEOF(fmt_buff) - 1;	/* leave out last byte for null byte termination */
	fmt_top2 = fmt_top1 - 1;
	for (pin = TREF(util_outbuff_ptr), pout = fmt_buff; ('\0' != *pin) && (pout < fmt_top1); )
	{
		if ('%' == *pin)
		{
			if (pout >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
				break;
			*pout++ = '%'; /* escape for '%' */
		}
		if ('\n' == *pin)
		{
			if (pout >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
				break;
			*pout++ = ',';
			*pout++ = ' ';
			pin++;
			continue;
		}
		*pout++ = *pin++;
	}
	assert(pout <= fmt_top1);
	*pout++ = '\0';
	memcpy(TREF(util_outbuff_ptr), fmt_buff, pout-(char *)fmt_buff);
}
