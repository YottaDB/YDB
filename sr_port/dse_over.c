/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_iconv.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "cli.h"
#include "filestruct.h"
#include "jnl.h"
#include "io.h"
#include "iosp.h"
#include "util.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "ebc_xlat.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region        *gv_cur_region;
GBLREF int		update_array_size;
GBLREF gd_addr		*gd_header;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF block_id		patch_curr_blk;
GBLREF iconv_t		dse_over_cvtcd;
GBLREF cw_set_element   cw_set[];
GBLREF unsigned char    *non_tp_jfb_buff_ptr;

void dse_over(void)
{
        block_id        blk;
	char 		data[MAX_LINE];
	sm_uc_ptr_t	bp;
	uchar_ptr_t	lbp;
        int4		offset;
	int		data_len, size;
	blk_segment	*bs1, *bs_ptr;
	cw_set_element  *cse;
	int4		blk_seg_cnt, blk_size;
	unsigned char	*cvt_src_ptr, *cvt_dst_ptr;
	unsigned int	insize, outsize;
	char		chset_name[MAX_CHSET_NAME];
	unsigned short	name_len = 0;
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);
	error_def(ERR_DBRDONLY);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	assert(update_array);
	/* reset new block mechanism */
	update_array_ptr = update_array;
	blk_size = cs_addrs->hdr->blk_size;
        if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK",&blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.",TRUE);
			return;
		}
		patch_curr_blk = blk;
	} else
		blk = patch_curr_blk;
	if (CLI_PRESENT == cli_present("OCHSET"))
	{
		name_len = sizeof(chset_name);
		if (cli_get_str("OCHSET", chset_name, &name_len) && 0 != name_len)
		{
			chset_name[name_len] = 0;
			if ( (iconv_t)0 != dse_over_cvtcd )
			{
				ICONV_CLOSE_CD(dse_over_cvtcd);
			}
			if (!strcmp(chset_name, INSIDE_CH_SET))
				dse_over_cvtcd = (iconv_t)0;
			else
				ICONV_OPEN_CD(dse_over_cvtcd, INSIDE_CH_SET, chset_name);
		}
	} else
	{
		if ((iconv_t)0 != dse_over_cvtcd )
		{
			ICONV_CLOSE_CD(dse_over_cvtcd);
			dse_over_cvtcd = (iconv_t)0;		/* default ASCII, no conversion	*/
		}
	}
	if (cli_present("OFFSET") != CLI_PRESENT)
	{
		util_out_print("Error:  offset must be specified.", TRUE);
		return;
	}
	if (!cli_get_hex("OFFSET",&offset))
		return;
	if (offset < sizeof(blk_hdr))
	{
		util_out_print("Error:  offset too small.", TRUE);
		return;
	}
	if (cli_present("DATA") != CLI_PRESENT)
	{
		util_out_print("Error:  data must be specified.", TRUE);
		return;
	}
	t_begin_crit(ERR_DSEFAIL);
	if(!(bp = t_qread(blk, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);

	size = ((blk_hdr_ptr_t)bp)->bsiz;
	if (size < sizeof(blk_hdr))
		size = sizeof(blk_hdr);
	else if (size >= blk_size)
		size = blk_size;

	if (offset >= size)
	{
		util_out_print("Error:  offset too large.", TRUE);
		t_abort();
		return;
	}
	if (FALSE == dse_data(&data[0], &data_len))
	{
		t_abort();
		return;
	}
	cvt_src_ptr = cvt_dst_ptr = (unsigned char *)data;
	insize = outsize = (unsigned int)data_len;
	if ((iconv_t)0 != dse_over_cvtcd)
		ICONVERT(dse_over_cvtcd, &cvt_src_ptr, &insize, &cvt_dst_ptr, &outsize);         /*      in-place conversion     */
	if (offset + data_len > size)
	{
		util_out_print("Error:  data will not fit in block at given offset.", TRUE);
		t_abort();
		return;
	}
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy (lbp, bp, blk_size);
	memcpy(lbp + offset, &data[0], data_len);

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + sizeof(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - sizeof(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		free(lbp);
		t_abort();
		return;
	}
	t_write(blk, (unsigned char *)bs1, 0, 0, bp, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE);
	BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
	t_end(&dummy_hist, 0);
	return;
}
