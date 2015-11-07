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

#ifndef MDEFSA_included
#define MDEFSA_included
#include "iosb_disk.h"

/* # define SHORT_SLEEP(x) hiber_start(x); */
/* Macros on struct dsc$descriptor_s */
#define LEN_STR_OF_DSC(d)	((d).dsc$w_length), ((d).dsc$a_pointer)
#define STR_LEN_OF_DSC(d)	((d).dsc$a_pointer), ((d).dsc$w_length)
#define STR_OF_DSC(d)		((d).dsc$a_pointer)
#define LEN_OF_DSC(d)		((d).dsc$w_length)
#define	DSC_CPY(to, from)	memcpy(STR_OF_DSC(to), STR_OF_DSC(from), LEN_OF_DSC(from));			\
				LEN_OF_DSC(to) = LEN_OF_DSC(from)
#define DSC_APND_LIT(d, lit)	memcpy(STR_OF_DSC(d) + LEN_OF_DSC(d), lit, SIZEOF(lit) - 1);			\
				LEN_OF_DSC(d) += SIZEOF(lit) - 1
#define DSC_APND_STR(d, str)	memcpy(STR_OF_DSC(d) + LEN_OF_DSC(d), str, strlen(str));			\
				LEN_OF_DSC(d) += strlen(str)
#define DSC_APND_DSC(d, dsc)	memcpy(STR_OF_DSC(d) + LEN_OF_DSC(d), STR_OF_DSC(dsc), LEN_OF_DSC(dsc));	\
				LEN_OF_DSC(d) += LEN_OF_DSC(dsc)
#define MVAL_TO_DSC(v, d)	(d).dsc$a_pointer = (v)->str.addr, (d).dsc$w_length = (v)->str.len
#define DSC_TO_MVAL(d, v)	(v)->str.addr = (d).dsc$a_pointer, (v)->str.len = (d).dsc$w_length

/* DSK_WRITE macro needs "efn.h" to be included. Use this flavor if
   writing from the cache. Note that it is possible that the sys$synch()
   call follows a sys$qiow in dsk_write if in compabitility mode and
   no reformat buffers were available (SE 04/2005 V5.0)
*/
#define	DSK_WRITE(reg, blk, cr, status)				\
{								\
	io_status_block_disk	iosb;				\
								\
	status = dsk_write(reg, blk, cr, 0, 0, &iosb);		\
	if (status & 1)						\
	{							\
		status = sys$synch(efn_bg_qio_write, &iosb);	\
		if (SS$_NORMAL == status)			\
			status = iosb.cond;			\
	}							\
}
/* Use this flavor if writing direct from storage (not cache buffer).
   Note that dsk_write_nocache() always does a synchronous write.
*/
#define	DSK_WRITE_NOCACHE(reg, blk, ptr, odv, status)			\
{									\
	io_status_block_disk	iosb;					\
								        \
	status = dsk_write_nocache(reg, blk, ptr, odv, 0, 0, &iosb);	\
	if (status & 1)							\
		status = iosb.cond;					\
}

#define CHECK_CHANNEL_STATUS(stat, chan_id)			\
{								\
	GBLREF	uint4	gtmDebugLevel;				\
	GBLREF	uint4	check_channel_status;			\
	GBLREF	uint4	check_channel_id;			\
								\
	if ((SS$_IVCHAN == stat) || (SS$_IVIDENT == stat))	\
	{							\
		check_channel_status = stat;			\
		check_channel_id = chan_id;			\
		if (gtmDebugLevel)				\
			verifyAllocatedStorage();		\
		GTMASSERT;					\
	}							\
}

#define DOTM                    ".M"
#define DOTOBJ                  ".OBJ"
#define	GTM_DIST		"GTM$DIST"

#endif /* MDEFSA_included */
