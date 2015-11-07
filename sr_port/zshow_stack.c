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

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mlkdef.h"
#include "zshow.h"
#include "io.h"

#define ZTRAP_FRAME		"    ($ZTRAP)"
#define ZBRK_FRAME		"    (ZBREAK)"
#define ZINTR_FRAME		"    ($ZINTERRUPT) "
#define DEVERR_FRAME		"    (Device Error)"
#define DIR_MODE_MESS		"    (Direct mode) "
#define UNK_LOC_MESS		"        Indirection"
#define INDR_OVERFLOW		"        (Max indirect frames per counted frame exceeded for ZSHOW ""S"" -"	\
                                " some indirect frames not processed)"

#define MAX_FRAME_MESS_LEN	20	/* Maximum length of any of the frame messages above */
#define MAX_INDR_PER_COUNTED	64	/* Maximum number of indirect frames printed per counted frame */

#define HAS_TRANS_CODE_ERR(fp)	(fp->flags & SFF_ZTRAP_ERR || fp->flags & SFF_DEV_ACT_ERR)

GBLREF stack_frame *frame_pointer;

void zshow_stack(zshow_out *output)
{
	boolean_t	line_reset;
	unsigned char	*addr;
	unsigned short	nocount_frames[MAX_INDR_PER_COUNTED], *nfp;
	stack_frame	*fp;
	mstr 		v;
	unsigned char	buff[MAX_ENTRYREF_LEN + SIZEOF(INDR_OVERFLOW)];

	v.addr = (char *)&buff[0];
	flush_pio();
	nfp = &nocount_frames[0];
	line_reset = FALSE;
	for (fp = frame_pointer; ; fp = fp->old_frame_pointer)
	{
		if (NULL == fp->old_frame_pointer)
		{	/* This frame is a base frame - endpoint or jump it? */
#		ifdef GTM_TRIGGER
			if (fp->type & SFT_TRIGR)
				/* Have a trigger baseframe, pick up stack continuation frame_pointer stored by base_frame() */
				fp = *(stack_frame **)(fp + 1);
			else
#		endif
				break;	/* Endpoint.. */
		}
		if (!(fp->type & SFT_COUNT) || ((fp->type & SFT_ZINTR) && (fp->flags & SFF_INDCE)))
		{	/* SFT_ZINTR is normally indirect but if the frame has been replaced by non-indirect frame via ZGOTO or GOTO
			 * then do not include it in the indirect list here.
			 */
			if (nfp < &nocount_frames[MAX_INDR_PER_COUNTED])
				/* If room in array, save indirect frame type */
				*nfp++ = fp->type;
			else
				nocount_frames[MAX_INDR_PER_COUNTED - 1] = 0xffff;	/* Indicate array overflow */
			if (fp->type & SFT_ZTRAP || fp->type & SFT_DEV_ACT || HAS_TRANS_CODE_ERR(fp))
				line_reset = TRUE;
		} else
		{
			if (HAS_TRANS_CODE_ERR(fp))
			{
				*nfp++ = (fp->flags & SFF_ZTRAP_ERR) ? SFT_ZTRAP : SFT_DEV_ACT;
				line_reset = TRUE;
			}
			if (line_reset && ADDR_IN_CODE(fp->mpc, fp->rvector))
			{
				addr = fp->mpc + 1;
				line_reset = FALSE;
			} else
				addr = fp->mpc;
			v.len = INTCAST(symb_line(addr, &buff[0], 0, fp->rvector) - &buff[0]);
			if (v.len == 0)
			{
				MEMCPY_LIT(&buff[0], UNK_LOC_MESS);
				v.len = SIZEOF(UNK_LOC_MESS) - 1;
			}
			if (nfp != &nocount_frames[0])
			{
				for (--nfp; nfp >= &nocount_frames[0]; nfp--)
				{
					switch(*nfp)
					{
						case SFT_ZBRK_ACT:
							MEMCPY_LIT(&buff[v.len], ZBRK_FRAME);
							v.len += SIZEOF(ZBRK_FRAME) - 1;
							break;
						case SFT_DEV_ACT:
							MEMCPY_LIT(&buff[v.len], DEVERR_FRAME);
							v.len += SIZEOF(DEVERR_FRAME) - 1;
							break;
						case SFT_ZTRAP:
							MEMCPY_LIT(&buff[v.len], ZTRAP_FRAME);
							v.len += SIZEOF(ZTRAP_FRAME) - 1;
							break;
						case SFT_DM:
							MEMCPY_LIT(&buff[v.len], DIR_MODE_MESS);
							v.len += SIZEOF(DIR_MODE_MESS) - 1;
							break;
						case (SFT_COUNT | SFT_ZINTR):
							MEMCPY_LIT(&buff[v.len], ZINTR_FRAME);
							v.len += SIZEOF(DIR_MODE_MESS) - 1;
							break;
						case 0xffff:
							MEMCPY_LIT(&buff[v.len], INDR_OVERFLOW);
							v.len += SIZEOF(INDR_OVERFLOW) - 1;
							break;
						default:
							break;
					}
					output->flush = TRUE;
					zshow_output(output, &v);
					v.len = 0;
				}
				nfp = &nocount_frames[0];
			} else
			{
				output->flush = TRUE;
				zshow_output(output, &v);
			}
		}
	}
	return;
}
