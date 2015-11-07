/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <iodef.h>
#include <rms.h>
#include <ssdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "efn.h"
#include "msg.h"
#include "mupip_exit.h"
#include "load.h"

#define MVX_BLK_SIZE 2048
#define M11_BLK_SIZE 1024

GBLREF gd_region *gv_cur_region;

/***********************************************************************************************/
/*					M/VX GOQ Format                                        */
/***********************************************************************************************/
void goq_load(uint4 begin, uint4 end, struct FAB *infab)
{
	int		status;
	msgtype		msg;
	unsigned char	*in_buff, *b;
	unsigned int	n;
	bool		is_begin;
	uint4		rec_count;
	unsigned short	goq_blk_size;
	short		iosb[4];

	error_def(ERR_INVMVXSZ);
	error_def(ERR_MUPIPINFO);
	error_def(ERR_PREMATEOF);
	error_def(ERR_LDGOQFMT);
	error_def(ERR_BEGINST);

	rec_count = 0;
	if (begin > 0)
		is_begin = TRUE;
	else
		is_begin = FALSE;
	goq_blk_size = MVX_BLK_SIZE;
	infab->fab$w_mrs = goq_blk_size;
	in_buff = malloc(goq_blk_size + 8);
	if (is_begin)
	{
		status = sys$qio(efn_bg_qio_read, infab->fab$l_stv, IO$_READVBLK, &iosb[0],
				0, 0, in_buff, goq_blk_size,
				(rec_count * goq_blk_size / 512) + 1, 0, 0, 0);
		if (SS$_NORMAL != status)		/* get header block */
			rts_error(VARLSTCNT(1) status);
		sys$synch(efn_bg_qio_read, &iosb[0]);
		if (SS$_NORMAL != iosb[0])
			rts_error(VARLSTCNT(1) iosb[0]);
		if (iosb[1] != goq_blk_size)
		{
			if (M11_BLK_SIZE != iosb[1])
				rts_error(VARLSTCNT(1) ERR_INVMVXSZ);
			goq_blk_size = M11_BLK_SIZE;
		}
		while ((SS$_ENDOFFILE != iosb[0]) && (rec_count < begin))
		{
			rec_count++;
			status = sys$qio(efn_bg_qio_read, infab->fab$l_stv, IO$_READVBLK, &iosb[0],
					0, 0, in_buff, goq_blk_size,
					(rec_count * goq_blk_size / 512) + 1, 0, 0, 0);
			if (SS$_NORMAL != status)
				rts_error(VARLSTCNT(1) status);
			sys$synch(efn_bg_qio_read, &iosb[0]);
			if ((SS$_NORMAL != iosb[0]) && (SS$_ENDOFFILE != iosb[0]))
			{
				rts_error(VARLSTCNT(1) iosb[0]);
				mupip_exit(iosb[0]);
			}
		}
		for (;rec_count < begin;)
		{
			status = sys$qio(efn_bg_qio_read, infab->fab$l_stv, IO$_READVBLK, &iosb[0],
					0, 0, in_buff, goq_blk_size,
					(rec_count * goq_blk_size / 512) + 1, 0, 0, 0);
			if (SS$_NORMAL != status)
				rts_error(VARLSTCNT(1) status);
			sys$synch(efn_bg_qio_read, &iosb[0]);
			if (SS$_ENDOFFILE == iosb[0])
				rts_error(VARLSTCNT(1) ERR_PREMATEOF);
			if (SS$_NORMAL != iosb[0])
				rts_error(VARLSTCNT(1) iosb[0]);
			rec_count++;
		}
		msg.msg_number = ERR_BEGINST;
		msg.arg_cnt = 3;
		msg.new_opts = msg.def_opts = 1;
		msg.fp_cnt = 1;
		msg.fp[0].n = rec_count;
		sys$putmsg(&msg, 0, 0, 0);
	} else
	{
		status = sys$qio(efn_bg_qio_read, infab->fab$l_stv, IO$_READVBLK, &iosb[0],
				0, 0, in_buff, goq_blk_size,
				(rec_count * goq_blk_size / 512) + 1, 0, 0, 0);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
		sys$synch(efn_bg_qio_read, &iosb[0]);
		if (SS$_NORMAL != iosb[0])
		{
			rts_error(VARLSTCNT(1) iosb[0]);
			mupip_exit(iosb[0]);
		}
		if (iosb[1] != goq_blk_size)
		{
			if (M11_BLK_SIZE != iosb[1])
				rts_error(VARLSTCNT(1) ERR_INVMVXSZ);
			goq_blk_size = M11_BLK_SIZE;
		}
		b = in_buff;
		while ((13 != *b++) && (b - in_buff < goq_blk_size - 28))
			;
		if (memcmp(b - SIZEOF("~%GOQ"), LIT_AND_LEN("~%GOQ")) || (10 != *b))
		{
			rts_error(VARLSTCNT(1) ERR_LDGOQFMT);
			mupip_exit(ERR_LDGOQFMT);
		}
		for (n = 0;  n < 3;  n++)
		{
			while ((13 != *b++) && b - in_buff < goq_blk_size)
				;
			if (10 != *b++)
			{
				rts_error(VARLSTCNT(1) ERR_LDGOQFMT);
				mupip_exit(ERR_LDGOQFMT);
			}
		}
		msg.msg_number = ERR_MUPIPINFO;
		msg.arg_cnt = 4;
		msg.new_opts = msg.def_opts = 1;
		msg.fp_cnt = 2;
		msg.fp[0].n = b - in_buff - 1;
		msg.fp[1].cp = in_buff;
		sys$putmsg(&msg, 0, 0, 0);
		while (SS$_ENDOFFILE != iosb[0])
		{
			rec_count++;
			status = sys$qio(efn_bg_qio_read, infab->fab$l_stv, IO$_READVBLK, &iosb[0],
					0, 0, in_buff, goq_blk_size,
					(rec_count * goq_blk_size / 512) + 1, 0, 0, 0);
			if (SS$_NORMAL != status)
			{
				rts_error(VARLSTCNT(1) status);
				mupip_exit(status);
			}
			sys$synch(efn_bg_qio_read, &iosb[0]);
			if ((SS$_NORMAL != iosb[0]) && (SS$_ENDOFFILE != iosb[0]))
			{
				rts_error(VARLSTCNT(1) iosb[0]);
				mupip_exit(iosb[0]);
			}
		}
	}
	if (MVX_BLK_SIZE == goq_blk_size)
		goq_mvx_load(infab, in_buff, rec_count, end);
	else
		goq_m11_load(infab, in_buff, rec_count, end);
	/***********************************************************************************************/
	/*					Shut Down                                              */
	/***********************************************************************************************/
CLOSE:
	free(in_buff);
	gv_cur_region = NULL;
	status = sys$dassgn(infab->fab$l_stv);
	if (SS$_NORMAL != status)
	{
		rts_error(VARLSTCNT(1) status);
		mupip_exit(status);
	}
	return;
}
