/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "gtmio.h"
#include "iott_setterm.h"
#include "stringpool.h"
#include "error.h"
#include "op.h"
#include "indir_enum.h"
#include "comline.h"
#include "readline.h"

GBLREF io_pair		io_std_device;
LITREF unsigned char	io_params_size[];

error_def(ERR_SYSCALL);

void iott_close(io_desc *v, mval *pp)
{
	d_tt_struct	*ttptr;
	params		ch;
	int		status;
	int		p_offset;
	boolean_t	ch_set;
	recall_ctxt_t	*recall, *recall_top;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(v->type == tt);
	if (v->state != dev_open)
		return;
	ESTABLISH_GTMIO_CH(&v->pair, ch_set);
	iott_flush(v);
	assert((v->pair.in == v) || (v->pair.out == v));
	ttptr = (d_tt_struct *)v->dev_sp;
	v->state = dev_closed;
	((d_tt_struct *)v->dev_sp)->setterm_done_by = process_id;  //Set flag to ensure resetterm is actually done
	iott_resetterm(v);

	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		if ((ch = *(pp->str.addr + p_offset++)) == iop_exception)
			DEF_EXCEPTION(pp, p_offset, v);
		UPDATE_P_OFFSET(p_offset, ch, pp);	/* updates "p_offset" using "ch" and "pp" */
	}
	if (NULL != ttptr->recall_array)
	{	/* Free up structures allocated in "iott_recall_array_add" */
		for (recall = ttptr->recall_array, recall_top = recall + MAX_RECALL; recall < recall_top; recall++)
		{
			if (NULL != recall->buff)
				free(recall->buff);
		}
		free(ttptr->recall_array);
		ttptr->recall_array = NULL;
	}
	readline_write_history();
	if (v->dollar.devicebuffer)
	{
		free(v->dollar.devicebuffer);
		v->dollar.devicebuffer = NULL;
	}
	if (v == io_std_device.in || (v == io_std_device.out))
	{
		REVERT_GTMIO_CH(&v->pair, ch_set);
		return;
	}

	CLOSEFILE_RESET(ttptr->fildes, status);	/* resets "ttptr->fildes" to FD_INVALID */
	if (0 != status)
	{
		assert(status == errno);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
			RTS_ERROR_LITERAL("iott_close(CLOSEFILE)"), CALLFROM, status);
	}
	REVERT_GTMIO_CH(&v->pair, ch_set);
	return;
}
