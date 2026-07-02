/****************************************************************
 *								*
 * Copyright (c) 2026 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef NOPRINCIO_DEF_INCLUDED
#define NOPRINCIO_DEF_INCLUDED

#include "error.h"
#include "gtm_icu_api.h"
#include "op.h"
#include "send_msg.h"
#include "svnames.h"
#include "util.h"

GBLREF	boolean_t	prin_dm_io, prin_in_dev_failure, prin_out_dev_failure;
GBLREF	io_pair		io_std_device;
GBLREF	mval		dollar_zstatus;

error_def(ERR_NOPRINCIO);

/* Set prin_*_dev_failure if a read or write failed on the principal device. If it is a recurrence,
 * issue the NOPRINCIO error.
 */
#define ISSUE_NOPRINCIO_IF_NEEDED(IOD, WRITE, SOCNOERR)							\
MBSTART {												\
	/* the following literal and the game played with it deal with the suppression of the */	\
	/* maintenance of $ZSTATUS while in direct mode */						\
	LITDEF unmanaged_mstr    dm_did_it = {0, LEN_AND_LIT("Direct Mode activity")};			\
	LITDEF unmanaged_mstr    silent_soc = {0, LEN_AND_LIT("socket with IOERROR disabled")};		\
	mval zpos = {{0}};										\
	unmanaged_mstr dev, zstatus;									\
													\
	if ((IOD) == ((WRITE) ? io_std_device.out : io_std_device.in))					\
	{												\
		if ((WRITE) ? prin_out_dev_failure : prin_in_dev_failure)				\
		{											\
			util_out_print("", RESET);	/* Reset output buffer */			\
			op_svget(SV_ZPOS, &zpos);							\
			if (memcmp("+1^GTM$DMOD", zpos.str.addr, STR_LIT_LEN("+1^GTM$DMOD")))		\
			{	/* not in base direct mode so do noisy exit */				\
				dev.len = (WRITE) ? io_std_device.out->trans_name->len			\
					: io_std_device.in->trans_name->len;				\
				dev.addr = (WRITE) ? io_std_device.out->trans_name->dollar_io		\
					: io_std_device.in->trans_name->dollar_io;			\
				if (SOCNOERR)								\
					zstatus = silent_soc;		/* suppressing socket errors */	\
				else									\
					zstatus = dollar_zstatus.str.umstr;				\
				if (0 == zstatus.len)							\
					zstatus = dm_did_it;		/* in case there's no zstatus */\
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_NOPRINCIO, 8, 		\
					RTS_ERROR_STRING((WRITE) ? "WRITE to" : "READ from"),		\
					dev.len, dev.addr, zpos.str.len, zpos.str.addr,			\
					zstatus.len, zstatus.addr);					\
			} else if (!prin_out_dev_failure)	/* in direct mode so go quietly */	\
				flush_pio();			/* unless output is still OK */		\
			stop_image_no_core();								\
		}											\
		if (WRITE)										\
			prin_out_dev_failure = TRUE;							\
		else											\
			prin_in_dev_failure = TRUE;							\
	}												\
} MBEND

#endif /* NOPRINCIO_DEF_INCLUDED */
